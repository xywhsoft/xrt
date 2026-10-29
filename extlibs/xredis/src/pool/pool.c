#include <xredis/pool.h>
#include "../internal/client_connect.h"
#include <xrt/memory.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispoolslot xredispoolslot;
enum { XREDIS_L_WAITING, XREDIS_L_GRANTED, XREDIS_L_DONE };
struct xredislease {
    xatomic32 References, Cancelled, Reason;
    xmutex Gate;
    xredispool* Pool;
    xredispoolslot* Slot;
    xredislease *Next, *Previous, *EventNext;
    xpromise *Promise, *ClosePromise;
    xfuture *Future, *CloseFuture;
    xredisleaseresult* Result;
    xredisleaseclosedresult* CloseResult;
    xcancelwatch* Cancel;
    double Deadline, HoldDeadline, ReturnBy, TimerDeadline;
    int64 ReturnTimeout;
    uint64 Timer;
    size_t APIs;
    unsigned Phase;
    bool EventQueued, Returning, Abort;
};
struct xredispoolslot {
    xredispool* Pool;
    xredisclient* Client;
    xredislease* Lease;
    xredisexclusiveowner Owner;
    xfuture* Closed;
    xfuturewatch Watch;
    xredispoolslot *NextIdle, *PreviousIdle, *EventNext;
    bool Claimed, Registered, Idle, EventQueued, Dead, Retired;
};
struct xredispool {
    xatomic32 References;
    xmutex Gate;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xredispoolconfig Config;
    xredispoolinfo Info;
    xredisownerquota Quota;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xredislease *WaitHead, *WaitTail, *LeaseEvents, *LeaseEventTail;
    xredispoolslot *IdleHead, *IdleTail, *SlotEvents, *SlotEventTail;
    size_t Count, StorageBytes, InitializeNext, CloseNext, Retired;
    bool Initialized, Queued, Finished;
    xredispoolslot Slots[];
};

static void xredisPoolWork(xnetworker* Worker, ptr Data);
static void xredisPoolLeaseNotify(xredislease* Lease);
static void xredisPoolRef(xredispool* Pool)
{ xrtAtomic32FetchAdd(&Pool->References, 1, XMEMORY_RELAXED); }
static bool xredisPoolReport(xerrkind Kind, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Message); return false; }
static double xredisPoolDeadline(double Entered, int64 Timeout)
{ return Timeout == XRT_WAIT_FOREVER ? INFINITY : Timeout == 0 ? Entered : nextafter(Entered + (double)Timeout * 0.001, INFINITY); }
static void xredisLeaseRef(xredislease* Lease)
{ xrtAtomic32FetchAdd(&Lease->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisPoolRetain(xredispool* Pool)
{ if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); xredisPoolRef(Pool); return true; }
XREDIS_API void xredisPoolRelease(xredispool* Pool)
{
    if (!Pool || xrtAtomic32FetchSub(&Pool->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtPromiseDestroy(Pool->ClosePromise); xrtFutureDestroy(Pool->CloseFuture);
    xrtMutexUnit(&Pool->Gate); xrtFree(Pool);
}
XREDIS_API bool xredisLeaseRetain(xredislease* Lease)
{ if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); xredisLeaseRef(Lease); return true; }
XREDIS_API void xredisLeaseRelease(xredislease* Lease)
{
    if (!Lease || xrtAtomic32FetchSub(&Lease->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Lease->Cancel);
    xrtPromiseDestroy(Lease->Promise); xrtFutureDestroy(Lease->Future);
    xrtPromiseDestroy(Lease->ClosePromise); xrtFutureDestroy(Lease->CloseFuture);
    xrtFree(Lease->Result); xrtFree(Lease->CloseResult);
    xrtMutexUnit(&Lease->Gate); xrtFree(Lease);
}

/* Pool Gate held. The runtime Pin and one reference per post protect Worker. */
static bool xredisPoolWakeLocked(xredispool* Pool)
{
    if (!Pool->Initialized || Pool->Finished || Pool->Queued) return true;
    Pool->Queued = true; xredisPoolRef(Pool);
    if (xrtNetPost(Pool->Worker, &Pool->Work, xredisPoolWork, Pool)) return true;
    Pool->Queued = false; xredisPoolRelease(Pool); return false;
}
static void xredisPoolSlotRef(ptr Data) { xredisPoolRef(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotDrop(ptr Data) { xredisPoolRelease(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotNotify(ptr Data)
{
    xredispoolslot* Slot = Data; xredispool* Pool = Slot->Pool;
    xrtMutexLock(&Pool->Gate);
    if (Pool->Initialized && !Pool->Finished && !Slot->EventQueued) {
        Slot->EventQueued = true; Slot->EventNext = NULL;
        if (Pool->SlotEventTail) Pool->SlotEventTail->EventNext = Slot; else Pool->SlotEvents = Slot;
        Pool->SlotEventTail = Slot; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolQueueLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Pool->Gate);
    if (!Pool->Finished && !Lease->EventQueued) {
        Lease->EventQueued = true; Lease->EventNext = NULL; xredisLeaseRef(Lease);
        if (Pool->LeaseEventTail) Pool->LeaseEventTail->EventNext = Lease; else Pool->LeaseEvents = Lease;
        Pool->LeaseEventTail = Lease; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolLeaseNotify(xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate); xredispool* Pool = Lease->Pool;
    if (Pool) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Pool) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static void xredisLeaseReason(xredislease* Lease, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    xrtAtomic32CompareExchange(&Lease->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisLeaseCancel(ptr Data)
{ xredislease* Lease = Data; xrtAtomic32Store(&Lease->Cancelled, 1, XMEMORY_RELEASE); xredisPoolLeaseNotify(Lease); }
static void xredisLeaseTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredislease* Lease = Data;
    if (Lease->Timer == Id) Lease->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    else if (Result != XNET_RESULT_CANCELLED) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    xredisPoolLeaseNotify(Lease); xredisLeaseRelease(Lease);
}
/* Owning pool worker only; cancellation callbacks only queue events. */
static bool xredisLeaseArm(xredispool* Pool, xredislease* Lease, double Deadline)
{
    if (Lease->Timer && Lease->TimerDeadline == Deadline) return true;
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    if (Lease->Timer) return false;
    if (Deadline == INFINITY) return true;
    Lease->TimerDeadline = Deadline; xredisLeaseRef(Lease);
    Lease->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisLeaseTimer, Lease);
    if (Lease->Timer) return true;
    xredisLeaseRelease(Lease); xredisLeaseReason(Lease, XREDIS_REQUEST_MEMORY); return false;
}
static bool xredisLeaseDisarm(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    return Lease->Timer == 0;
}

static void xredisPoolIdleRemove(xredispool* Pool, xredispoolslot* Slot)
{
    /* Owning worker, with Pool Gate held for public counters. */
    if (!Slot->Idle) return;
    if (Slot->PreviousIdle) Slot->PreviousIdle->NextIdle = Slot->NextIdle; else Pool->IdleHead = Slot->NextIdle;
    if (Slot->NextIdle) Slot->NextIdle->PreviousIdle = Slot->PreviousIdle; else Pool->IdleTail = Slot->PreviousIdle;
    Slot->NextIdle = Slot->PreviousIdle = NULL; Slot->Idle = false; --Pool->Info.Idle;
}
static void xredisPoolIdleAppend(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Idle || Slot->Dead || Slot->Lease || Pool->Info.State != XREDIS_POOL_OPEN) return;
    Slot->Idle = true; Slot->PreviousIdle = Pool->IdleTail; Slot->NextIdle = NULL;
    if (Pool->IdleTail) Pool->IdleTail->NextIdle = Slot; else Pool->IdleHead = Slot;
    Pool->IdleTail = Slot; ++Pool->Info.Idle;
}
static void xredisPoolWaitRemove(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Previous) Lease->Previous->Next = Lease->Next; else Pool->WaitHead = Lease->Next;
    if (Lease->Next) Lease->Next->Previous = Lease->Previous; else Pool->WaitTail = Lease->Previous;
    Lease->Next = Lease->Previous = NULL; --Pool->Info.Pending;
}
static void xredisPoolAcquireResultDrop(ptr Value, ptr Data)
{ (void)Data; xredisleaseresult* Result = Value; xredisLeaseRelease(Result->Lease); xrtFree(Result); }
static void xredisPoolClosedResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }
static void xredisPoolPublishAcquire(xredislease* Lease, xredisrequeststatus Status)
{
    xpromise* Promise = Lease->Promise; Lease->Promise = NULL;
    xfuture* Future = Lease->Future; Lease->Future = NULL;
    xredisleaseresult* Result = Lease->Result; Lease->Result = NULL;
    *Result = (xredisleaseresult){Status, NULL};
    if (Status == XREDIS_REQUEST_OK) { xredisLeaseRef(Lease); Result->Lease = Lease; }
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolAcquireResultDrop, NULL)) xredisPoolAcquireResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}
static void xredisPoolPublishReturn(xredislease* Lease, xredisrequeststatus Status, bool Reusable)
{
    xpromise* Promise = Lease->ClosePromise; Lease->ClosePromise = NULL;
    xredisleaseclosedresult* Result = Lease->CloseResult; Lease->CloseResult = NULL;
    *Result = (xredisleaseclosedresult){Status, Reusable};
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolClosedResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
}
static void xredisPoolFailAcquire(xredispool* Pool, xredislease* Lease, xredisrequeststatus Status)
{
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Pool->Gate); xredisPoolWaitRemove(Pool, Lease); xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; xrtMutexUnlock(&Lease->Gate);
    xredisPoolRelease(Pool); /* Acquisition's runtime reference. */
    xredisPoolPublishAcquire(Lease, Status); xredisPoolPublishReturn(Lease, Status, false); xredisLeaseRelease(Lease);
}

static void xredisPoolProcessLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate);
    unsigned Phase = Lease->Phase; xredispoolslot* Slot = Lease->Slot;
    bool Returning = Lease->Returning, Abort = Lease->Abort; size_t APIs = Lease->APIs;
    double ReturnBy = Lease->ReturnBy;
    xrtMutexUnlock(&Lease->Gate);
    if (Phase == XREDIS_L_DONE) return;
    xrtMutexLock(&Pool->Gate); bool Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool Live = Pool->Info.Live != 0, Immediate = Pool->WaitHead == Lease && Pool->IdleHead != NULL;
    xrtMutexUnlock(&Pool->Gate);
    if (Closing) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Phase == XREDIS_L_WAITING) {
        if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
        if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
        if (!Live) xredisLeaseReason(Lease, XREDIS_REQUEST_DISCONNECTED);
        Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
        if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
        if (!Immediate && !xredisLeaseArm(Pool, Lease, Lease->Deadline) && !Lease->Timer) xredisPoolLeaseNotify(Lease);
        return;
    }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) xredisLeaseReason(Lease, Info.CloseStatus);
    if (xrtTimer() >= Lease->HoldDeadline || (Returning && xrtTimer() >= ReturnBy)) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (!Returning && !Reason) return;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    if (Abort || Closing || (Reason && Info.Requests)) xredisClientClose(Slot->Client);
    if (APIs) return; /* Leave always queues another event. */
    xredisClientInfo(Slot->Client, &Info);
    if (Info.Requests) {
        if (!Reason && !Abort && !Closing) {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) { xredisClientClose(Slot->Client); xredisPoolLeaseNotify(Lease); }
        }
        return;
    }
    bool Reusable = Info.State == XREDIS_CLIENT_OPEN && !Abort && !Closing;
    if (!Reusable && xrtFutureState(Slot->Closed) == XFUTURE_PENDING) { xredisClientClose(Slot->Client); return; }
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Lease->Gate);
    if (Lease->APIs) { xrtMutexUnlock(&Lease->Gate); return; }
    if (Lease->Abort && Reusable) { xrtMutexUnlock(&Lease->Gate); xredisClientClose(Slot->Client); return; }
    Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; Lease->Slot = NULL;
    xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = NULL;
    xrtMutexLock(&Pool->Gate); --Pool->Info.Leased; ++Pool->Info.Returned;
    if (Reusable) xredisPoolIdleAppend(Pool, Slot);
    xrtMutexUnlock(&Pool->Gate);
    xredisPoolPublishReturn(Lease, Reason ? (xredisrequeststatus)(Reason - 1u) : XREDIS_REQUEST_OK, Reusable);
    xredisPoolRelease(Pool); xredisLeaseRelease(Lease);
    if (!Reusable) xredisPoolSlotNotify(Slot);
}

static void xredisPoolProcessSlot(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Retired) return;
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        xrtMutexLock(&Pool->Gate);
        if (!Slot->Dead) { Slot->Dead = true; --Pool->Info.Live; xredisPoolIdleRemove(Pool, Slot); }
        xrtMutexUnlock(&Pool->Gate);
    }
    if (Slot->Lease) { xredisPoolQueueLease(Pool, Slot->Lease); return; }
    if (Slot->Dead) {
        if (Info.Requests || xrtFutureState(Slot->Closed) == XFUTURE_PENDING) return;
        if (Slot->Registered) { xrtFutureWatchRemove(Slot->Closed, &Slot->Watch); Slot->Registered = false; }
        if (!__xredisClientUnclaim(Slot->Client, &Slot->Owner)) return;
        Slot->Claimed = false; xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
        xredisClientRelease(Slot->Client); Slot->Client = NULL; xrtNetEngineUnpin(Pool->Engine);
        xrtFutureDestroy(Slot->Closed); Slot->Closed = NULL; Slot->Retired = true;
        xrtMutexLock(&Pool->Gate); ++Pool->Retired; ++Pool->Info.Retired; xrtMutexUnlock(&Pool->Gate);
    } else if (!Info.Requests) {
        xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate);
    }
}

static void xredisPoolGrant(xredispool* Pool, xredislease* Lease, xredispoolslot* Slot)
{
    /* No application can use this lease yet. Retire the acquisition cancel
     * callback before the final grant check; future cancellation afterwards
     * cannot revoke an already granted lease. */
    if (!xredisLeaseDisarm(Pool, Lease)) { xredisPoolLeaseNotify(Lease); return; }
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
    if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State != XREDIS_CLIENT_OPEN || Info.Requests) { xredisPoolSlotNotify(Slot); return; }
    Lease->HoldDeadline = xredisPoolDeadline(xrtTimer(), Pool->Config.LeaseTimeout); Lease->ReturnTimeout = Pool->Config.ReturnTimeout;
    if (!xredisLeaseArm(Pool, Lease, Lease->HoldDeadline)) {
        xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_MEMORY); return;
    }
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_CLOSED); return;
    }
    xredisPoolWaitRemove(Pool, Lease); ++Pool->Info.Leased; ++Pool->Info.Acquired;
    xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_GRANTED; Lease->Slot = Slot; xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = Lease; Slot->Owner.Deadline = Lease->HoldDeadline;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_ACTIVE, XMEMORY_RELEASE);
    xredisPoolPublishAcquire(Lease, XREDIS_REQUEST_OK);
}

static void xredisPoolFinish(xredispool* Pool)
{
    xrtMutexLock(&Pool->Gate);
    if (Pool->Queued || Pool->Info.Pending || Pool->Info.Leased || Pool->Retired != Pool->Count || Pool->LeaseEvents || Pool->SlotEvents) {
        xrtMutexUnlock(&Pool->Gate); return;
    }
    Pool->Finished = true; Pool->Info.State = XREDIS_POOL_CLOSED;
    xnetengine* Engine = Pool->Engine; Pool->Engine = NULL; Pool->Worker = NULL;
    xrtMutexUnlock(&Pool->Gate);
    xrtNetEngineUnpin(Engine);
    xpromise* Promise = Pool->ClosePromise; Pool->ClosePromise = NULL;
    xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise); xredisPoolRelease(Pool); /* Runtime. */
}
static void xredisPoolWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xredispool* Pool = Data;
    xrtMutexLock(&Pool->Gate); Pool->Queued = false; bool Closing = Pool->Info.State != XREDIS_POOL_OPEN; xrtMutexUnlock(&Pool->Gate);
    size_t Budget = Pool->Config.WorkItems;
    if (Closing) for (size_t Index = 0; Index < Budget && Pool->CloseNext < Pool->Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Pool->CloseNext++];
        if (!Slot->Retired) { xredisClientClose(Slot->Client); xredisPoolProcessSlot(Pool, Slot); }
    }
    else for (size_t Index = 0; Index < Budget && Pool->InitializeNext < Pool->Count; ++Index)
        xredisPoolProcessSlot(Pool, &Pool->Slots[Pool->InitializeNext++]);
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredispoolslot* Slot = Pool->SlotEvents;
        if (Slot) { Pool->SlotEvents = Slot->EventNext; if (!Pool->SlotEvents) Pool->SlotEventTail = NULL; Slot->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Slot) break; xredisPoolProcessSlot(Pool, Slot);
    }
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->LeaseEvents;
        if (Lease) { Pool->LeaseEvents = Lease->EventNext; if (!Pool->LeaseEvents) Pool->LeaseEventTail = NULL; Lease->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Lease) break; xredisPoolProcessLease(Pool, Lease); xredisLeaseRelease(Lease);
    }
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->WaitHead;
        xredispoolslot* Slot = Pool->Info.State == XREDIS_POOL_OPEN ? Pool->IdleHead : NULL;
        bool NoLive = Pool->Info.Live == 0;
        if (Slot && Lease) xredisPoolIdleRemove(Pool, Slot);
        xrtMutexUnlock(&Pool->Gate);
        if (!Lease) break;
        if (Closing || NoLive) { xredisLeaseReason(Lease, Closing ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_DISCONNECTED); xredisPoolProcessLease(Pool, Lease); }
        else if (Slot) {
            xredisPoolGrant(Pool, Lease, Slot);
            if (!Slot->Lease && !Slot->Dead) { xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate); }
        } else {
            if (!Lease->Timer && !xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) xredisPoolLeaseNotify(Lease);
            break;
        }
    }
    xrtMutexLock(&Pool->Gate);
    Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool More = Pool->SlotEvents || Pool->LeaseEvents || (Pool->WaitHead && (Pool->IdleHead || Closing || !Pool->Info.Live)) ||
        (Closing ? Pool->CloseNext < Pool->Count : Pool->InitializeNext < Pool->Count);
    if (More) xredisPoolWakeLocked(Pool);
    xrtMutexUnlock(&Pool->Gate);
    if (Closing) xredisPoolFinish(Pool);
    xredisPoolRelease(Pool);
}

XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool config output");
    *Config = (xredispoolconfig){sizeof(*Config), 256, 4096, 64u * 1024u * 1024u, 30000, 5000, 64}; return true;
}
static int xredisPoolCompareClient(const void* Left, const void* Right)
{
    uintptr_t A = (uintptr_t)*(xredisclient* const*)Left, B = (uintptr_t)*(xredisclient* const*)Right;
    return (A > B) - (A < B);
}
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result)
{
    xredispoolconfig Options;
    if (!Engine || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) || !Count || Count > 65536 || !Clients ||
        !xrtMemRangeValid(Clients, Count * sizeof(*Clients)) || xrtMemRangesOverlap(Clients, Count * sizeof(*Clients), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool members/config/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    if (Options.Size != sizeof(Options) || !Options.MaxPending || Options.MaxPending > 65536 || !Options.MaxRequests || !Options.MaxCommandBytes ||
        Options.LeaseTimeout <= 0 || Options.LeaseTimeout > 86400000 || Options.ReturnTimeout <= 0 || Options.ReturnTimeout > 60000 ||
        !Options.WorkItems || Options.WorkItems > 65536) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool bounds/deadlines"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient** Sorted = xrtMalloc(Count * sizeof(*Sorted)); if (!Sorted) return XREDIS_ADMISSION_ERROR;
    memcpy(Sorted, Clients, Count * sizeof(*Sorted)); qsort(Sorted, Count, sizeof(*Sorted), xredisPoolCompareClient);
    bool Valid = Sorted[0] != NULL;
    for (size_t Index = 1; Valid && Index < Count; ++Index) Valid = Sorted[Index] != Sorted[Index - 1];
    xrtFree(Sorted);
    if (!Valid) { xredisPoolReport(XERR_ARGUMENT, "NULL or duplicate pool member"); return XREDIS_ADMISSION_ERROR; }
    if (!xrtNetEnginePin(Engine)) return XREDIS_ADMISSION_ERROR;
    size_t Bytes = sizeof(xredispool) + Count * sizeof(xredispoolslot);
    xredispool* Pool = xrtCalloc(1, Bytes);
    if (!Pool || !xrtMutexInit(&Pool->Gate)) { xrtFree(Pool); xrtNetEngineUnpin(Engine); return XREDIS_ADMISSION_ERROR; }
    xrtAtomic32Init(&Pool->References, 1); Pool->Engine = Engine; Pool->Count = Count; Pool->StorageBytes = Bytes; Pool->Config = Options;
    Pool->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
    Pool->Info.Size = sizeof(Pool->Info); Pool->Info.Members = Pool->Info.Live = Count;
    xrtAtomic64Init(&Pool->Quota.Requests, 0); xrtAtomic64Init(&Pool->Quota.Bytes, 0);
    xrtAtomic32Init(&Pool->Quota.Closing, 0);
    Pool->Quota.MaxRequests = (uint64)Options.MaxRequests; Pool->Quota.MaxBytes = (uint64)Options.MaxCommandBytes;
    bool Ready = xrtNetPostInit(&Pool->Work) && (Pool->ClosePromise = xrtPromiseCreate(&Pool->CloseFuture, NULL));
    xredisadmission Admission = Ready ? XREDIS_ADMISSION_ACCEPTED : XREDIS_ADMISSION_ERROR;
    xredisprotocol Protocol = XREDIS_PROTOCOL_UNKNOWN;
    for (size_t Index = 0; Admission == XREDIS_ADMISSION_ACCEPTED && Index < Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Index]; Slot->Pool = Pool; Slot->Owner.Data = Slot;
        Slot->Owner.Retain = xredisPoolSlotRef; Slot->Owner.Release = xredisPoolSlotDrop; Slot->Owner.Wake = xredisPoolSlotNotify;
        Slot->Owner.Quota = &Pool->Quota; Slot->Owner.Deadline = INFINITY; xrtAtomic32Init(&Slot->Owner.State, XREDIS_OWNER_CLOSING);
        xnetengine* MemberEngine = NULL; xnetworker* MemberWorker = NULL;
        Admission = __xredisClientClaim(Clients[Index], &Slot->Owner, &MemberEngine, &MemberWorker, Result);
        (void)MemberWorker;
        if (Admission != XREDIS_ADMISSION_ACCEPTED) break;
        Slot->Claimed = true; Slot->Client = Clients[Index];
        if (MemberEngine != Engine) {
            __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); Slot->Client = NULL; Slot->Claimed = false;
            xrtNetEngineUnpin(MemberEngine); xredisPoolReport(XERR_ARGUMENT, "pool members belong to different engines"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        xredisclientinfo MemberInfo = {0}; MemberInfo.Size = sizeof(MemberInfo); xredisClientInfo(Slot->Client, &MemberInfo);
        if (Index == 0) Protocol = MemberInfo.Protocol;
        else if (Protocol != MemberInfo.Protocol) {
            xredisPoolReport(XERR_ARGUMENT, "pool members use different RESP protocols"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        Slot->Closed = xredisClientClosed(Slot->Client);
        if (!xrtFutureWatchInit(&Slot->Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) { Admission = XREDIS_ADMISSION_ERROR; break; }
        xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Slot->Closed, &Slot->Watch);
        Slot->Registered = Watched == XFUTURE_WATCH_PENDING;
        if (!Slot->Registered) xredisPoolRelease(Pool);
        if (Watched == XFUTURE_WATCH_ERROR) Admission = XREDIS_ADMISSION_ERROR;
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) {
        for (size_t Index = 0; Index < Count; ++Index) {
            xredispoolslot* Slot = &Pool->Slots[Index];
            if (Slot->Registered) xrtFutureWatchRemove(Slot->Closed, &Slot->Watch);
            if (Slot->Claimed) { __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); xrtNetEngineUnpin(Engine); }
            xrtFutureDestroy(Slot->Closed);
        }
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return Admission;
    }
    xredisPoolRef(Pool); /* Active lifetime, independent of caller. */
    xrtMutexLock(&Pool->Gate); Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}

static xredisadmission xredisPoolAcquireAdmit(xredispool* Pool, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    if (!Pool || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool acquisition/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Pool->Gate);
    if (Sync && !Pool->Finished && xrtNetEngineCurrent(Pool->Engine)) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolReport(XERR_STATE, "sync acquire cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    xredisadmission Admission = Pool->Info.State != XREDIS_POOL_OPEN ? XREDIS_ADMISSION_CLOSED :
        Pool->Info.Pending >= Pool->Config.MaxPending ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Pool->Info.Rejected; xrtMutexUnlock(&Pool->Gate); return Admission; }
    ++Pool->Info.Pending; xredisPoolRef(Pool); xrtMutexUnlock(&Pool->Gate);
    xredislease* Lease = xrtCalloc(1, sizeof(*Lease));
    if (!Lease || !xrtMutexInit(&Lease->Gate)) {
        xrtFree(Lease); xrtMutexLock(&Pool->Gate); --Pool->Info.Pending; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
        xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR;
    }
    xrtAtomic32Init(&Lease->References, 1); xrtAtomic32Init(&Lease->Cancelled, 0); xrtAtomic32Init(&Lease->Reason, 0);
    Lease->Pool = Pool; Lease->Deadline = xredisPoolDeadline(Entered, Timeout);
    Lease->Result = xrtMalloc(sizeof(*Lease->Result)); Lease->CloseResult = xrtMalloc(sizeof(*Lease->CloseResult));
    bool Ready = Lease->Result && Lease->CloseResult && (Lease->Promise = xrtPromiseCreate(&Lease->Future, NULL)) &&
        (Lease->ClosePromise = xrtPromiseCreate(&Lease->CloseFuture, NULL));
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Lease->Future); if (Token) Lease->Cancel = xrtCancelWatch(Token, xredisLeaseCancel, Lease);
        xrtCancelDestroy(Token); Ready = Lease->Cancel != NULL;
    }
    xrtMutexLock(&Pool->Gate);
    if (Ready && Pool->Info.State == XREDIS_POOL_OPEN) {
        Lease->Previous = Pool->WaitTail; if (Pool->WaitTail) Pool->WaitTail->Next = Lease; else Pool->WaitHead = Lease; Pool->WaitTail = Lease;
        xfuture* Observer = xrtFutureRef(Lease->Future); xredisLeaseRef(Lease); xredisPoolRef(Pool); /* Admission tail. */
        *Future = Observer; xrtMutexUnlock(&Pool->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisLeaseRelease(Lease); xredisPoolRelease(Pool);
        if (Sync) xrtFutureWait(Observer);
        return XREDIS_ADMISSION_ACCEPTED;
    }
    Admission = Pool->Info.State == XREDIS_POOL_OPEN ? XREDIS_ADMISSION_ERROR : XREDIS_ADMISSION_CLOSED;
    --Pool->Info.Pending; if (Admission == XREDIS_ADMISSION_CLOSED) ++Pool->Info.Rejected;
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    xredisLeaseRelease(Lease); xredisPoolRelease(Pool); return Admission;
}
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), true); }
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool snapshot output");
    xrtMutexLock(&Pool->Gate); *Info = Pool->Info;
    Info->Requests = xrtAtomic64Load(&Pool->Quota.Requests, XMEMORY_RELAXED); Info->CommandBytes = xrtAtomic64Load(&Pool->Quota.Bytes, XMEMORY_RELAXED);
    xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API bool xredisPoolClose(xredispool* Pool)
{
    if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL");
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State == XREDIS_POOL_OPEN) Pool->Info.State = XREDIS_POOL_CLOSING;
    xrtAtomic32Store(&Pool->Quota.Closing, 1, XMEMORY_RELEASE);
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool)
{ if (!Pool) { xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); return NULL; } return xrtFutureRef(Pool->CloseFuture); }

static xredisadmission xredisLeaseCallAdmit(xredislease* Lease, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    if (!Lease || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || xrtMemRangesOverlap(Lease, Lease ? sizeof(*Lease) : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid leased call/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase != XREDIS_L_GRANTED || Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) {
        xrtMutexUnlock(&Lease->Gate); return XREDIS_ADMISSION_CLOSED;
    }
    xredispoolslot* Slot = Lease->Slot; xredispool* Pool = Lease->Pool; ++Lease->APIs;
    xrtMutexUnlock(&Lease->Gate);
    xredisadmission Admission;
    if (Sync && __xredisClientIsWorker(Slot->Client)) { xredisPoolReport(XERR_STATE, "sync leased call cannot block an engine worker"); Admission = XREDIS_ADMISSION_ERROR; }
    else Admission = __xredisClientOwnedCall(Slot->Client, &Slot->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xrtMutexLock(&Lease->Gate); --Lease->APIs;
    bool Wake = Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE);
    if (Wake) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Wake) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), true); }
static bool xredisLeaseSeal(xredislease* Lease, bool Abort)
{
    if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL");
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase == XREDIS_L_GRANTED) {
        if (!Lease->Returning) { Lease->Returning = true; Lease->ReturnBy = xredisPoolDeadline(xrtTimer(), Lease->ReturnTimeout); }
        if (Abort) { Lease->Abort = true; xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED); }
        xrtAtomic32Store(&Lease->Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Lease->Gate); xredisPoolLeaseNotify(Lease); return true;
}
XREDIS_API bool xredisLeaseReturn(xredislease* Lease) { return xredisLeaseSeal(Lease, false); }
XREDIS_API bool xredisLeaseAbort(xredislease* Lease) { return xredisLeaseSeal(Lease, true); }
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease)
{ if (!Lease) { xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); return NULL; } return xrtFutureRef(Lease->CloseFuture); }
#endif
