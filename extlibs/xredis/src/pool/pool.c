#include <xredis/pool.h>
#include "../internal/client_connect.h"
#include "../internal/pool_admit.h"
#include "../internal/pool_connect_internal.h"
#include <xrt/memory.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispoolslot xredispoolslot;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
typedef struct xredispoolconnectstate xredispoolconnectstate;
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xredispoolconnectstate* Connector;
#endif
    xredispoolslot Slots[];
};

static void xredisPoolWork(xnetworker* Worker, ptr Data);
static void xredisPoolLeaseNotify(xredislease* Lease);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot);
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status);
static void xredisPoolConnectDrive(xredispool* Pool);
static bool xredisPoolConnectMore(xredispool* Pool);
static void xredisPoolConnectSchedule(xredispool* Pool);
static bool xredisPoolConnectDispose(xredispool* Pool);
#endif
static bool xredisPoolCanWait(xredispool* Pool)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) return true;
#endif
    return Pool->Info.Live != 0;
}
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
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xrtFree(Pool->Connector);
#endif
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
    bool Live = xredisPoolCanWait(Pool), Immediate = Pool->WaitHead == Lease && Pool->IdleHead != NULL;
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
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (!__xredisClientOwnerReady(Slot->Client, &Slot->Owner, true)) {
        if (Reason || Abort || Closing) xredisClientClose(Slot->Client);
        else {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) xredisClientClose(Slot->Client);
        }
        return;
    }
#endif
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

static bool xredisPoolDropMember(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Registered) { xrtFutureWatchRemove(Slot->Closed, &Slot->Watch); Slot->Registered = false; }
    if (Slot->Claimed) {
        if (!__xredisClientUnclaim(Slot->Client, &Slot->Owner)) return false;
        Slot->Claimed = false; xrtNetEngineUnpin(Pool->Engine);
    }
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisClientRelease(Slot->Client); Slot->Client = NULL;
    xrtFutureDestroy(Slot->Closed); Slot->Closed = NULL;
    xrtMutexLock(&Pool->Gate); ++Pool->Info.Retired; xrtMutexUnlock(&Pool->Gate); return true;
}
static void xredisPoolProcessSlot(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Retired) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && xredisPoolConnectProcess(Pool, Slot)) return;
#endif
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        xrtMutexLock(&Pool->Gate);
        if (!Slot->Dead) { Slot->Dead = true; --Pool->Info.Live; xredisPoolIdleRemove(Pool, Slot); }
        xrtMutexUnlock(&Pool->Gate);
    }
    if (Slot->Lease) { xredisPoolQueueLease(Pool, Slot->Lease); return; }
    if (Slot->Dead) {
        if (Info.Requests || xrtFutureState(Slot->Closed) == XFUTURE_PENDING) return;
        if (!xredisPoolDropMember(Pool, Slot)) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
        if (Pool->Connector) { xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus); return; }
#endif
        Slot->Retired = true;
        xrtMutexLock(&Pool->Gate); ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
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
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && !xredisPoolConnectDispose(Pool)) return;
#endif
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
        if (!Slot->Retired) { if (Slot->Client) xredisClientClose(Slot->Client); xredisPoolProcessSlot(Pool, Slot); }
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
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectDrive(Pool);
#endif
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->WaitHead;
        xredispoolslot* Slot = Pool->Info.State == XREDIS_POOL_OPEN ? Pool->IdleHead : NULL;
        bool NoLive = !xredisPoolCanWait(Pool);
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
    bool More = Pool->SlotEvents || Pool->LeaseEvents || (Pool->WaitHead && (Pool->IdleHead || Closing || !xredisPoolCanWait(Pool))) ||
        (Closing ? Pool->CloseNext < Pool->Count : Pool->InitializeNext < Pool->Count);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) More = More || xredisPoolConnectMore(Pool);
#endif
    if (More) xredisPoolWakeLocked(Pool);
    xrtMutexUnlock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectSchedule(Pool);
#endif
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
static bool xredisPoolBounds(const xredispoolconfig* Options)
{
    return Options->Size == sizeof(*Options) && Options->MaxPending && Options->MaxPending <= 65536 && Options->MaxRequests && Options->MaxCommandBytes &&
        Options->LeaseTimeout > 0 && Options->LeaseTimeout <= 86400000 && Options->ReturnTimeout > 0 && Options->ReturnTimeout <= 60000 &&
        Options->WorkItems && Options->WorkItems <= 65536;
}
static xredispool* xredisPoolAllocate(xnetengine* Engine, uint64 Affinity, size_t Count, const xredispoolconfig* Options)
{
    if (!xrtNetEnginePin(Engine)) return NULL;
    size_t Bytes = sizeof(xredispool) + Count * sizeof(xredispoolslot);
    xredispool* Pool = xrtCalloc(1, Bytes);
    if (!Pool || !xrtMutexInit(&Pool->Gate)) { xrtFree(Pool); xrtNetEngineUnpin(Engine); return NULL; }
    xrtAtomic32Init(&Pool->References, 1); Pool->Engine = Engine; Pool->Count = Count; Pool->StorageBytes = Bytes; Pool->Config = *Options;
    Pool->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
    Pool->Info.Size = sizeof(Pool->Info); Pool->Info.Members = Count;
    xrtAtomic64Init(&Pool->Quota.Requests, 0); xrtAtomic64Init(&Pool->Quota.Bytes, 0); xrtAtomic32Init(&Pool->Quota.Closing, 0);
    Pool->Quota.MaxRequests = (uint64)Options->MaxRequests; Pool->Quota.MaxBytes = (uint64)Options->MaxCommandBytes;
    if (!xrtNetPostInit(&Pool->Work) || !(Pool->ClosePromise = xrtPromiseCreate(&Pool->CloseFuture, NULL))) {
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return NULL;
    }
    return Pool;
}
static void xredisPoolSlotInit(xredispool* Pool, xredispoolslot* Slot)
{
    Slot->Pool = Pool; Slot->Owner.Data = Slot;
    Slot->Owner.Retain = xredisPoolSlotRef; Slot->Owner.Release = xredisPoolSlotDrop; Slot->Owner.Wake = xredisPoolSlotNotify;
    Slot->Owner.Quota = &Pool->Quota; Slot->Owner.Deadline = INFINITY; xrtAtomic32Init(&Slot->Owner.State, XREDIS_OWNER_CLOSING);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xrtAtomic32Init(&Slot->Owner.Nested, 0);
#endif
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
    if (!xredisPoolBounds(&Options)) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool bounds/deadlines"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient** Sorted = xrtMalloc(Count * sizeof(*Sorted)); if (!Sorted) return XREDIS_ADMISSION_ERROR;
    memcpy(Sorted, Clients, Count * sizeof(*Sorted)); qsort(Sorted, Count, sizeof(*Sorted), xredisPoolCompareClient);
    bool Valid = Sorted[0] != NULL;
    for (size_t Index = 1; Valid && Index < Count; ++Index) Valid = Sorted[Index] != Sorted[Index - 1];
    xrtFree(Sorted);
    if (!Valid) { xredisPoolReport(XERR_ARGUMENT, "NULL or duplicate pool member"); return XREDIS_ADMISSION_ERROR; }
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options);
    if (!Pool) return XREDIS_ADMISSION_ERROR;
    Pool->Info.Live = Count;
    xredisadmission Admission = XREDIS_ADMISSION_ACCEPTED;
    xredisprotocol Protocol = XREDIS_PROTOCOL_UNKNOWN;
    for (size_t Index = 0; Admission == XREDIS_ADMISSION_ACCEPTED && Index < Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Index]; xredisPoolSlotInit(Pool, Slot);
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

static xredisadmission xredisLeaseEnter(xredislease* Lease, bool Sync, xfuture** Future,
    xredispool** ResultPool, xredispoolslot** ResultSlot)
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
    if (Sync && __xredisClientIsWorker(Slot->Client)) {
        xrtMutexLock(&Lease->Gate); --Lease->APIs; xredisPoolRef(Pool); xrtMutexUnlock(&Lease->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool);
        xredisPoolReport(XERR_STATE, "sync leased operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    *ResultPool = Pool; *ResultSlot = Slot; return XREDIS_ADMISSION_ACCEPTED;
}
static void xredisLeaseLeave(xredislease* Lease, xredispool* Pool)
{
    xrtMutexLock(&Lease->Gate); --Lease->APIs;
    bool Wake = Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE);
    if (Wake) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Wake) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static xredisadmission xredisLeaseCallAdmit(xredislease* Lease, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Slot->Client, &Slot->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Slot->Client, &Slot->Owner, Commands, Count, Config, Timeout, Entered, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisWatchBeginOwned(Slot->Client, &Slot->Owner, Keys, Count, Config, Future, Entered);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
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
/* Optional cold maintenance: no transport dispatch, adapter Future or command
 * replay on hot calls. All mutable maintenance state belongs to this worker. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
enum { XREDIS_PC_WAIT, XREDIS_PC_CONNECTING, XREDIS_PC_DRAIN, XREDIS_PC_ONLINE, XREDIS_PC_DONE };
typedef struct xredispoolconnectentry {
    xfuture* Ready;
    xfuturewatch Watch;
    size_t Heap, Previous, Next;
    double Due;
    uint64 Attempt;
    unsigned Phase, Failures;
    bool Registered, Queued, InFlight;
} xredispoolconnectentry;
struct xredispoolconnectstate {
    xredispoolconnectconfig Config;
    xredispoolconnectinfo Info;
    xredispoolconnectentry* Entries;
    size_t* Heap;
    size_t HeapCount, Head, Tail;
    uint64 Random, Timer;
    double TimerDeadline;
    bool Dropped;
};
static void xredisPoolConnectIncrement(uint64* Value) { if (*Value != UINT64_MAX) ++*Value; }
static bool xredisPoolConnectOpen(xredispool* Pool)
{ return !xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE); }
static void xredisPoolConnectHeapSwap(xredispoolconnectstate* State, size_t A, size_t B)
{
    size_t Item = State->Heap[A]; State->Heap[A] = State->Heap[B]; State->Heap[B] = Item;
    State->Entries[State->Heap[A]].Heap = A; State->Entries[State->Heap[B]].Heap = B;
}
static bool xredisPoolConnectBefore(xredispoolconnectstate* State, size_t A, size_t B)
{
    double Left = State->Entries[State->Heap[A]].Due, Right = State->Entries[State->Heap[B]].Due;
    return Left < Right || (Left == Right && State->Heap[A] < State->Heap[B]);
}
static void xredisPoolConnectHeapFix(xredispoolconnectstate* State, size_t Index)
{
    while (Index && xredisPoolConnectBefore(State, Index, (Index - 1u) / 2u)) {
        size_t Parent = (Index - 1u) / 2u; xredisPoolConnectHeapSwap(State, Index, Parent); Index = Parent;
    }
    for (;;) {
        size_t Left = Index * 2u + 1u; if (Left >= State->HeapCount) break;
        size_t Best = Left;
        if (Left + 1u < State->HeapCount && xredisPoolConnectBefore(State, Left + 1u, Left)) Best = Left + 1u;
        if (!xredisPoolConnectBefore(State, Best, Index)) break;
        xredisPoolConnectHeapSwap(State, Index, Best); Index = Best;
    }
}
static void xredisPoolConnectHeapRemove(xredispoolconnectstate* State, size_t Item)
{
    size_t Index = State->Entries[Item].Heap; if (Index == SIZE_MAX) return;
    size_t Last = --State->HeapCount; State->Entries[Item].Heap = SIZE_MAX;
    if (Index == Last) return;
    State->Heap[Index] = State->Heap[Last]; State->Entries[State->Heap[Index]].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectHeapPut(xredispoolconnectstate* State, size_t Item, double Due)
{
    xredisPoolConnectHeapRemove(State, Item); State->Entries[Item].Due = Due;
    size_t Index = State->HeapCount++; State->Heap[Index] = Item; State->Entries[Item].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectQueueRemove(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (!Entry->Queued) return;
    if (Entry->Previous != SIZE_MAX) State->Entries[Entry->Previous].Next = Entry->Next; else State->Head = Entry->Next;
    if (Entry->Next != SIZE_MAX) State->Entries[Entry->Next].Previous = Entry->Previous; else State->Tail = Entry->Previous;
    Entry->Previous = Entry->Next = SIZE_MAX; Entry->Queued = false;
}
static void xredisPoolConnectQueue(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (Entry->Queued) return;
    Entry->Queued = true; Entry->Next = SIZE_MAX; Entry->Previous = State->Tail;
    if (State->Tail != SIZE_MAX) State->Entries[State->Tail].Next = Item; else State->Head = Item;
    State->Tail = Item;
}
static void xredisPoolConnectReadyDrop(xredispool* Pool, size_t Item)
{
    xredispoolconnectentry* Entry = &Pool->Connector->Entries[Item];
    if (Entry->Registered) { xrtFutureWatchRemove(Entry->Ready, &Entry->Watch); Entry->Registered = false; }
    xrtFutureDestroy(Entry->Ready); Entry->Ready = NULL;
}
static xredisrequeststatus xredisPoolConnectReadyStatus(xredisreadystatus Status)
{
    switch (Status) {
    case XREDIS_READY_TIMEOUT: return XREDIS_REQUEST_TIMEOUT;
    case XREDIS_READY_CLOSED: return XREDIS_REQUEST_CLOSED;
    case XREDIS_READY_DISCONNECTED: return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_READY_PROTOCOL: return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_READY_LIMIT: return XREDIS_REQUEST_LIMIT;
    case XREDIS_READY_MEMORY: return XREDIS_REQUEST_MEMORY;
    default: return XREDIS_REQUEST_INITIALIZATION;
    }
}
static void xredisPoolConnectFailed(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Failures < 31) ++Entry->Failures;
    xrtMutexLock(&Pool->Gate); xredisPoolConnectIncrement(&State->Info.Failures); State->Info.LastFailure = Status; xrtMutexUnlock(&Pool->Gate);
}
static double xredisPoolConnectRetry(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    uint64 Cap = (uint64)State->Config.RetryMin << Entry->Failures;
    if (Cap > (uint64)State->Config.RetryMax) Cap = (uint64)State->Config.RetryMax;
    /* Jitter is for retry dephasing, never for cryptographic purposes. */
    uint64 Value = State->Random; Value ^= Value << 13; Value ^= Value >> 7; Value ^= Value << 17; State->Random = Value;
    uint64 Delay = (uint64)State->Config.RetryMin + Value % (Cap - (uint64)State->Config.RetryMin + 1u);
    return xredisPoolDeadline(xrtTimer(), (int64)Delay);
}
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
    xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
    xredisPoolConnectReadyDrop(Pool, Item);
    if (Entry->Phase == XREDIS_PC_ONLINE && xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
    xrtMutexLock(&Pool->Gate);
    if (Entry->InFlight) { Entry->InFlight = false; --State->Info.Connecting; }
    bool Open = Pool->Info.State == XREDIS_POOL_OPEN;
    if (Open) { Entry->Phase = XREDIS_PC_WAIT; ++State->Info.Waiting; }
    else { Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true; ++Pool->Retired; }
    xrtMutexUnlock(&Pool->Gate);
    if (Open) xredisPoolConnectHeapPut(State, Item, xredisPoolConnectRetry(Pool, Item));
}
static void xredisPoolConnectDrain(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Phase != XREDIS_PC_DRAIN) {
        if (xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
        Entry->Phase = XREDIS_PC_DRAIN; xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectReadyDrop(Pool, Item);
    }
    xredisClientClose(Pool->Slots[Item].Client); xredisPoolSlotNotify(&Pool->Slots[Item]);
}
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Phase == XREDIS_PC_ONLINE) return false;
    if (Entry->Phase == XREDIS_PC_WAIT) {
        if (!xredisPoolConnectOpen(Pool)) {
            xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
            Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true;
            xrtMutexLock(&Pool->Gate); --State->Info.Waiting; ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
        }
        return true;
    }
    if (Entry->Phase == XREDIS_PC_DONE) return true;
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Entry->Phase == XREDIS_PC_CONNECTING) {
        if (!xredisPoolConnectOpen(Pool)) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_CLOSED);
        else if (xrtTimer() >= Entry->Due) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_TIMEOUT);
        else if (xrtFutureState(Entry->Ready) != XFUTURE_PENDING) {
            const xredisreadyresult* Ready = xrtFutureValue(Entry->Ready);
            if (Ready && Ready->Status == XREDIS_READY_OK && Ready->Protocol == State->Config.Protocol &&
                Slot->Claimed && Info.State == XREDIS_CLIENT_OPEN && !Info.Requests) {
                xredisPoolConnectReadyDrop(Pool, Item); xredisPoolConnectHeapRemove(State, Item);
                Entry->Phase = XREDIS_PC_ONLINE; Entry->Failures = 0; Entry->InFlight = false; Slot->Dead = false;
                xrtMutexLock(&Pool->Gate); --State->Info.Connecting; ++Pool->Info.Live; xredisPoolConnectIncrement(&State->Info.Connected); xrtMutexUnlock(&Pool->Gate);
                return false;
            }
            xredisPoolConnectDrain(Pool, Item, Ready ? xredisPoolConnectReadyStatus(Ready->Status) : XREDIS_REQUEST_INITIALIZATION);
        } else if (Info.State >= XREDIS_CLIENT_CLOSING) xredisPoolConnectDrain(Pool, Item, Info.CloseStatus);
    }
    if (Entry->Phase == XREDIS_PC_DRAIN) {
        if (!Info.Requests && xrtFutureState(Slot->Closed) != XFUTURE_PENDING && xredisPoolDropMember(Pool, Slot))
            xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus);
        else if (!Slot->Registered) xredisPoolConnectHeapPut(State, Item, xredisPoolDeadline(xrtTimer(), 10));
    }
    return true;
}
static bool xredisPoolConnectWatch(xredispool* Pool, xredispoolslot* Slot, xfuture* Future, xfuturewatch* Watch, bool* Registered)
{
    if (!xrtFutureWatchInit(Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) return false;
    xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Future, Watch);
    *Registered = Watched == XFUTURE_WATCH_PENDING;
    if (!*Registered) xredisPoolRelease(Pool);
    return Watched != XFUTURE_WATCH_ERROR;
}
static void xredisPoolConnectStart(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item]; xredispoolslot* Slot = &Pool->Slots[Item];
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) { xrtMutexUnlock(&Pool->Gate); return; }
    --State->Info.Waiting; ++State->Info.Connecting; xredisPoolConnectIncrement(&State->Info.Attempts);
    Entry->InFlight = true; Entry->Phase = XREDIS_PC_CONNECTING; xredisPoolConnectIncrement(&Entry->Attempt);
    xrtMutexUnlock(&Pool->Gate);
    Entry->Due = xredisPoolDeadline(xrtTimer(), State->Config.AttemptTimeout);
    xredisclient* Client = NULL;
    bool Started = State->Config.Connect(Pool->Engine, Item, Entry->Attempt, State->Config.AttemptTimeout, State->Config.Data, &Client);
    if (!Started || !Client) {
        if (xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
        xredisPoolConnectRetired(Pool, Slot, XREDIS_REQUEST_INITIALIZATION); return;
    }
    Slot->Client = Client; Slot->Dead = true;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE); Slot->Owner.Deadline = INFINITY;
    if (__xredisClientClaimStarting(Client, &Slot->Owner, Pool->Engine) == XREDIS_ADMISSION_ACCEPTED) {
        Slot->Claimed = true; xredisClientRelease(Client); /* Keep the native claim reference. */
    }
    Slot->Closed = xredisClientClosed(Client); Entry->Ready = xredisClientReady(Client);
    bool ClosedWatch = xredisPoolConnectWatch(Pool, Slot, Slot->Closed, &Slot->Watch, &Slot->Registered);
    bool ReadyWatch = xredisPoolConnectWatch(Pool, Slot, Entry->Ready, &Entry->Watch, &Entry->Registered);
    if (!Slot->Claimed || !ClosedWatch || !ReadyWatch) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
    else xredisPoolConnectHeapPut(State, Item, Entry->Due);
    xredisPoolSlotNotify(Slot);
}
static void xredisPoolConnectDrive(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    /* Move due retries out of the heap even when concurrency is full. They
     * must never hide a later connecting attempt's hard timeout at the root. */
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->HeapCount; ++Count) {
        size_t Item = State->Heap[0]; xredispoolconnectentry* Entry = &State->Entries[Item];
        if (Entry->Due > xrtTimer()) break;
        xredisPoolConnectHeapRemove(State, Item);
        if (Entry->Phase == XREDIS_PC_WAIT && xredisPoolConnectOpen(Pool)) xredisPoolConnectQueue(State, Item);
        else xredisPoolSlotNotify(&Pool->Slots[Item]);
    }
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->Head != SIZE_MAX; ++Count) {
        if (State->Info.Connecting >= State->Config.MaxConnecting || !xredisPoolConnectOpen(Pool)) break;
        size_t Item = State->Head; xredisPoolConnectQueueRemove(State, Item); xredisPoolConnectStart(Pool, Item);
    }
}
static bool xredisPoolConnectMore(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    return (xredisPoolConnectOpen(Pool) && State->Head != SIZE_MAX && State->Info.Connecting < State->Config.MaxConnecting) ||
        (State->HeapCount && State->Entries[State->Heap[0]].Due <= xrtTimer());
}
static void xredisPoolConnectTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredispool* Pool = Data; xredispoolconnectstate* State = Pool->Connector;
    if (State->Timer == Id) State->Timer = 0;
    if (Result != XNET_RESULT_OK && Result != XNET_RESULT_CANCELLED) xredisPoolClose(Pool);
    xrtMutexLock(&Pool->Gate); xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); xredisPoolRelease(Pool);
}
static void xredisPoolConnectSchedule(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    double Deadline = State->HeapCount ? State->Entries[State->Heap[0]].Due : INFINITY;
    if (Deadline <= xrtTimer()) Deadline = INFINITY; /* Bounded post continuation handles due work. */
    if (State->Timer && State->TimerDeadline == Deadline) return;
    if (State->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, State->Timer);
    if (State->Timer || Deadline == INFINITY) return;
    State->TimerDeadline = Deadline; xredisPoolRef(Pool);
    State->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisPoolConnectTimer, Pool);
    if (!State->Timer) {
        xrtMutexLock(&Pool->Gate); State->Info.LastFailure = XREDIS_REQUEST_MEMORY; xrtMutexUnlock(&Pool->Gate);
        xredisPoolClose(Pool); xredisPoolRelease(Pool);
    }
}
static bool xredisPoolConnectDispose(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector; if (State->Timer) return false;
    xrtMutexLock(&Pool->Gate);
    bool Ready = !Pool->Queued && !Pool->Info.Pending && !Pool->Info.Leased && Pool->Retired == Pool->Count && !Pool->LeaseEvents && !Pool->SlotEvents;
    xrtMutexUnlock(&Pool->Gate);
    if (!Ready) return false;
    if (!State->Dropped) {
        xredispoolconnectdrop Drop = State->Config.Drop; ptr Data = State->Config.Data;
        State->Dropped = true; State->Config.Connect = NULL; State->Config.Drop = NULL; State->Config.Data = NULL;
        if (Drop) Drop(Data);
    }
    return true;
}
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{
    if (!Engine || !Count || Count > 65536 || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) ||
        !Connect || !xrtMemRangeValid(Connect, sizeof(*Connect)) || xrtMemRangesOverlap(Connect, sizeof(*Connect), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool config/output"); return XREDIS_ADMISSION_ERROR;
    }
    xredispoolconfig Options; if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    xredispoolconnectconfig Factory = *Connect;
    if (!xredisPoolBounds(&Options) || Factory.Size != sizeof(Factory) || !Factory.Connect ||
        (Factory.Protocol != XREDIS_RESP2 && Factory.Protocol != XREDIS_RESP3) || !Factory.MaxConnecting || Factory.MaxConnecting > 65536 ||
        Factory.AttemptTimeout <= 0 || Factory.AttemptTimeout > 60000 || Factory.RetryMin <= 0 || Factory.RetryMax < Factory.RetryMin || Factory.RetryMax > 60000) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool bounds/factory/profile"); return XREDIS_ADMISSION_ERROR;
    }
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options); if (!Pool) return XREDIS_ADMISSION_ERROR;
    size_t Bytes = sizeof(xredispoolconnectstate) + Count * (sizeof(xredispoolconnectentry) + sizeof(size_t));
    xredispoolconnectstate* State = Pool->Connector = xrtCalloc(1, Bytes);
    if (!State) { xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR; }
    State->Config = Factory; if (State->Config.MaxConnecting > Count) State->Config.MaxConnecting = Count;
    State->Entries = (xredispoolconnectentry*)(State + 1); State->Heap = (size_t*)(State->Entries + Count);
    State->Head = State->Tail = SIZE_MAX; State->HeapCount = Count;
    State->Random = ((uint64)(xrtTimer() * 1000000.0) ^ (uint64)(uintptr_t)Pool) | 1u;
    State->Info.Size = sizeof(State->Info); State->Info.Waiting = Count;
    double Now = xrtTimer();
    for (size_t Item = 0; Item < Count; ++Item) {
        xredisPoolSlotInit(Pool, &Pool->Slots[Item]); Pool->Slots[Item].Dead = true;
        State->Entries[Item].Heap = State->Heap[Item] = Item; State->Entries[Item].Due = Now;
        State->Entries[Item].Previous = State->Entries[Item].Next = SIZE_MAX;
    }
    Pool->InitializeNext = Count; xredisPoolRef(Pool);
    xrtMutexLock(&Pool->Gate); Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool connector snapshot output");
    xrtMutexLock(&Pool->Gate); bool Valid = Pool->Connector != NULL; if (Valid) *Info = Pool->Connector->Info; xrtMutexUnlock(&Pool->Gate);
    return Valid ? true : xredisPoolReport(XERR_STATE, "pool has no automatic connector");
}
#endif

#endif
