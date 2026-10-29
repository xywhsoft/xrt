#include <xredis/watch.h>
#include "../internal/client_connect.h"
#include <xrt/memory.h>
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_WATCH)
enum { XREDIS_W_WATCH, XREDIS_W_COMMIT, XREDIS_W_UNWATCH, XREDIS_W_CLIENT_CLOSED, XREDIS_W_SLOTS };
struct xrediswatch {
    xatomic32 References, Reason, Abandoned;
    xmutex Gate;
    xredisexclusiveowner Owner;
    xrediswatchconfig Config;
    xredisclient* Client;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xfuturewatch Watches[XREDIS_W_SLOTS];
    xfuture* Futures[XREDIS_W_SLOTS];
    bool Registered[XREDIS_W_SLOTS];
    xpromise *StartPromise, *ClosePromise;
    xfuture *StartFuture, *CloseFuture;
    xrediswatchresult* StartResult;
    xrediswatchclosedresult* CloseResult;
    xcancelwatch* Cancel;
    uint64 Timer;
    size_t APIs;
    bool Initialized, Queued, Finished, Armed, WatchSeen, CommitSeen, Cleared, UnwatchSeen;
};

static void xredisWatchWork(xnetworker* Worker, ptr Data);
static void xredisWatchWake(ptr Data);
static bool xredisWatchReport(xerrkind Kind, cstr Text)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Text); return false; }
static void xredisWatchRef(ptr Data)
{ (void)xrtAtomic32FetchAdd(&((xrediswatch*)Data)->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisWatchRetain(xrediswatch* Watch)
{
    if (Watch == NULL) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchRef(Watch); return true;
}
XREDIS_API void xredisWatchRelease(xrediswatch* Watch)
{
    if (Watch == NULL || xrtAtomic32FetchSub(&Watch->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Watch->Cancel);
    xrtPromiseDestroy(Watch->StartPromise); xrtFutureDestroy(Watch->StartFuture);
    xrtPromiseDestroy(Watch->ClosePromise); xrtFutureDestroy(Watch->CloseFuture);
    xrtFree(Watch->StartResult); xrtFree(Watch->CloseResult);
    xrtMutexUnit(&Watch->Gate); xrtFree(Watch);
}
static void xredisWatchDrop(ptr Data) { xredisWatchRelease(Data); }

/* Notifications never call the session while holding the client Gate. The
 * separate engine Pin protects posts even after an external ClientClose. */
static void xredisWatchWake(ptr Data)
{
    xrediswatch* Watch = Data; xrtMutexLock(&Watch->Gate);
    if (Watch->Initialized && !Watch->Finished && !Watch->Queued) {
        Watch->Queued = true; xredisWatchRef(Watch);
        if (!xrtNetPost(Watch->Worker, &Watch->Work, xredisWatchWork, Watch)) {
            Watch->Queued = false; xredisWatchRelease(Watch);
        }
    }
    xrtMutexUnlock(&Watch->Gate);
}
static void xredisWatchReason(xrediswatch* Watch, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    (void)xrtAtomic32CompareExchange(&Watch->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisWatchStop(xrediswatch* Watch, xredisrequeststatus Status)
{
    xrtMutexLock(&Watch->Gate);
    if (!Watch->Finished) {
        xredisWatchReason(Watch, Status);
        xrtAtomic32Store(&Watch->Abandoned, 1, XMEMORY_RELEASE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
}
static void xredisWatchCancel(ptr Data) { xredisWatchStop(Data, XREDIS_REQUEST_CANCELLED); }
static void xredisWatchTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    if (Watch->Timer == Id) Watch->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    xredisWatchWake(Watch);
    xredisWatchRelease(Watch);
}
static bool xredisWatchObserve(xrediswatch* Watch, unsigned Slot)
{
    xredisWatchRef(Watch);
    xfuturewatchresult Result = xrtFutureWatchAdd(Watch->Futures[Slot], &Watch->Watches[Slot]);
    Watch->Registered[Slot] = Result == XFUTURE_WATCH_PENDING;
    if (Result != XFUTURE_WATCH_PENDING) xredisWatchRelease(Watch);
    return Result != XFUTURE_WATCH_ERROR;
}
static bool xredisWatchOK(const xredisrequestresult* Result)
{
    xredisreplyvalue Root;
    return Result && Result->Status == XREDIS_REQUEST_OK && Result->Reply &&
        xredisReplyGet(Result->Reply, 0, &Root) && Root.Type == XREDIS_REPLY_SIMPLE_STRING &&
        Root.Data.Size == 2 && memcmp(Root.Data.Data, "OK", 2) == 0;
}
static const xredisrequestresult* xredisWatchOutcome(xfuture* Future)
{
    return Future && xrtFutureState(Future) != XFUTURE_PENDING ? xrtFutureValue(Future) : NULL;
}
static void xredisWatchResultDrop(ptr Value, ptr Data)
{
    (void)Data; xrediswatchresult* Result = Value;
    xredisReplyRelease(Result->Request.Reply); xrtErrorFree((xerror*)Result->Request.Cause);
    xredisWatchRelease(Result->Watch); xrtFree(Result);
}
static void xredisWatchCloseResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }

static void xredisWatchPublish(xrediswatch* Watch, xredisrequeststatus Status, const xredisrequestresult* Source)
{
    if (Watch->StartPromise == NULL) return;
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    xpromise* Promise = Watch->StartPromise; Watch->StartPromise = NULL;
    xrediswatchresult* Result = Watch->StartResult; Watch->StartResult = NULL;
    Result->Request = (xredisrequestresult){Status, Source ? Source->Delivery : XREDIS_MAY_HAVE_EXECUTED,
        Source ? Source->Reply : NULL, Source ? xrtErrorRef(Source->Cause) : NULL};
    if (Result->Request.Reply) xredisReplyRetain(Result->Request.Reply);
    /* Even a failed/cancelled start must expose its physical cleanup barrier.
     * The inert handle retains metadata, not the client/engine after Closed. */
    xredisWatchRef(Watch); Result->Watch = Watch;
    xfuture* Future = Watch->StartFuture; Watch->StartFuture = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchResultDrop, NULL)) xredisWatchResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}

/* Called only by the owning worker, with all native reservations physically
 * idle. Remove notifications before releasing the independent engine Pin.
 * A post queued by any racing notification/API must run first. */
static void xredisWatchFinish(xrediswatch* Watch, bool Reusable)
{
    /* Sealing prevents new APIs; an already admitted commit may still be
     * attaching its observer. Do not inspect/detach that storage until its
     * API tail has published and left. No wait occurs under either Gate. */
    xrtMutexLock(&Watch->Gate);
    bool Busy = Watch->APIs != 0;
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) return;
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) if (Watch->Registered[Slot]) {
        xrtFutureWatchRemove(Watch->Futures[Slot], &Watch->Watches[Slot]); Watch->Registered[Slot] = false;
    }
    xrtMutexLock(&Watch->Gate);
    if (Watch->APIs || Watch->Queued || Watch->Timer != 0) { xrtMutexUnlock(&Watch->Gate); return; }
    if (!__xredisClientUnclaim(Watch->Client, &Watch->Owner)) { xrtMutexUnlock(&Watch->Gate); return; }
    Watch->Finished = true; xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisclient* Client = Watch->Client; Watch->Client = NULL;
    xnetengine* Engine = Watch->Engine; Watch->Engine = NULL; Watch->Worker = NULL;
    xrtMutexUnlock(&Watch->Gate);
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) {
        xrtFutureDestroy(Watch->Futures[Slot]); Watch->Futures[Slot] = NULL;
    }
    xredisClientRelease(Client); xrtNetEngineUnpin(Engine);
    xrediswatchclosedresult* Result = Watch->CloseResult; Watch->CloseResult = NULL;
    *Result = (xrediswatchclosedresult){(xredisrequeststatus)(xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) - 1u), Reusable};
    xpromise* Promise = Watch->ClosePromise; Watch->ClosePromise = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchCloseResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    /* The parent's pool/lease barrier must include this scope's own Pin,
     * observers and physical Closed publication, not just native request=0. */
    __xredisClientChildDone(&Watch->Owner);
#endif
    xredisWatchRelease(Watch); /* Active lifetime. */
}

static void xredisWatchWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    xrtMutexLock(&Watch->Gate); Watch->Queued = false;
    bool Busy = Watch->APIs != 0; xfuture* Commit = Watch->Futures[XREDIS_W_COMMIT];
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) { xredisWatchRelease(Watch); return; }
    if (!Watch->Armed) {
        Watch->Armed = true; xredisWatchRef(Watch);
        Watch->Timer = __xrtNetEngineSchedule(Watch->Engine, xrtNetWorkerIndex(Watch->Worker), Watch->Owner.Deadline, xredisWatchTimer, Watch);
        if (!Watch->Timer) { xredisWatchRelease(Watch); xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY); }
    }
    if (!xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) && xrtTimer() >= Watch->Owner.Deadline)
        xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    const xredisrequestresult* Start = xredisWatchOutcome(Watch->Futures[XREDIS_W_WATCH]);
    if (Start && !Watch->WatchSeen) {
        Watch->WatchSeen = true;
        uint32 Expected = XREDIS_OWNER_STARTING;
        if (xredisWatchOK(Start) && !xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) &&
            xrtAtomic32CompareExchange(&Watch->Owner.State, &Expected, XREDIS_OWNER_ACTIVE, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
            xredisWatchPublish(Watch, XREDIS_REQUEST_OK, Start);
        } else {
            uint32 Stopped = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
            xredisrequeststatus Status = Stopped ? (xredisrequeststatus)(Stopped - 1u) :
                Start->Status == XREDIS_REQUEST_OK ? XREDIS_REQUEST_PROTOCOL : Start->Status;
            xredisWatchReason(Watch, Status);
            xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
            if (!Stopped && Status == XREDIS_REQUEST_PROTOCOL) xredisClientClose(Watch->Client);
        }
    }
    const xredistransactionresult* Transaction = (const xredistransactionresult*)xredisWatchOutcome(Commit);
    if (Transaction && !Watch->CommitSeen) {
        Watch->CommitSeen = true; xredisWatchReason(Watch, Transaction->Request.Status);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
        Watch->Cleared = Transaction->Execution == XREDIS_TRANSACTION_EXECUTED || Transaction->Execution == XREDIS_TRANSACTION_ABORTED ||
            (Transaction->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Transaction->QueueErrors != 0 &&
             Transaction->Execution == XREDIS_TRANSACTION_NOT_EXECUTED);
        if (Transaction->Request.Status == XREDIS_REQUEST_TIMEOUT || Transaction->Request.Status == XREDIS_REQUEST_CANCELLED)
            xredisWatchStop(Watch, Transaction->Request.Status);
    }
    uint32 Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Watch->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING && !Reason) {
        xredisWatchReason(Watch, Info.CloseStatus); Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    if (!Reason) { xredisWatchRelease(Watch); return; }
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xredisWatchPublish(Watch, (xredisrequeststatus)(Reason - 1u), Start);
    if (xrtAtomic32Load(&Watch->Abandoned, XMEMORY_ACQUIRE)) {
        if (Watch->Futures[XREDIS_W_WATCH]) xrtFutureCancel(Watch->Futures[XREDIS_W_WATCH]);
        if (Commit) xrtFutureCancel(Commit);
        /* Once cleanup itself owns the last native reservation, allow its
         * bounded deadline/drain to finish. Repeated Abort is idempotent. */
        if (Info.Requests && Watch->Futures[XREDIS_W_UNWATCH] == NULL) xredisClientClose(Watch->Client);
        xredisClientInfo(Watch->Client, &Info);
    }
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        if (!Info.Requests && xrtFutureState(Watch->Futures[XREDIS_W_CLIENT_CLOSED]) != XFUTURE_PENDING) xredisWatchFinish(Watch, false);
    } else if (!Info.Requests) {
        if (Watch->Cleared) xredisWatchFinish(Watch, true);
        else if (Watch->Futures[XREDIS_W_UNWATCH] == NULL) {
            xbytesview Word = XRT_BYTES_LITERAL("UNWATCH");
            xredisadmission Admission = __xredisClientOwnedCall(Watch->Client, &Watch->Owner, &Word, 1,
                Watch->Config.CleanupTimeout, xrtTimer(), true, &Watch->Futures[XREDIS_W_UNWATCH]);
            if (Admission != XREDIS_ADMISSION_ACCEPTED || !xredisWatchObserve(Watch, XREDIS_W_UNWATCH)) xredisClientClose(Watch->Client);
        } else if (!Watch->UnwatchSeen) {
            const xredisrequestresult* Result = xredisWatchOutcome(Watch->Futures[XREDIS_W_UNWATCH]);
            if (Result) {
                Watch->UnwatchSeen = true;
                if (xredisWatchOK(Result)) { Watch->Cleared = true; xredisWatchFinish(Watch, true); }
                else xredisClientClose(Watch->Client);
            }
        }
    }
    xredisWatchRelease(Watch);
}

XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisWatchReport(XERR_ARGUMENT, "invalid watch config");
    *Config = (xrediswatchconfig){sizeof(*Config), 5000, 5000, 256, 65536, {sizeof(xredistransactionconfig), XREDIS_TRANSACTION_COMMANDS_DEFAULT}};
    return true;
}

static xredisadmission xredisWatchBegin(xredisclient* Client, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, xredisexclusiveowner* Parent)
{
    xrediswatchconfig Options;
    if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch arguments/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config == NULL) xredisWatchConfigInit(&Options); else Options = *Config;
    if (Options.Size != sizeof(Options) || Options.Timeout <= 0 || Options.Timeout > 86400000 ||
        Options.CleanupTimeout <= 0 || Options.CleanupTimeout > 60000 || !Options.MaxKeys || Options.MaxKeys > 65536 ||
        !Options.MaxWireBytes || !Count || Count > Options.MaxKeys || !Keys || !xrtMemRangeValid(Keys, Count * sizeof(*Keys)) ||
        xrtMemRangesOverlap(Keys, Count * sizeof(*Keys), Future, sizeof(*Future)) ||
        Options.Transaction.Size != sizeof(Options.Transaction) || !Options.Transaction.MaxCommands || Options.Transaction.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch config/keys"); return XREDIS_ADMISSION_ERROR;
    }
    for (size_t Index = 0; Index < Count; ++Index) if (!xrtMemRangeValid(Keys[Index].Data, Keys[Index].Size) ||
        xrtMemRangesOverlap(Keys[Index].Data, Keys[Index].Size, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch key/output alias"); return XREDIS_ADMISSION_ERROR;
    }
    xbytesview* Arguments = xrtMalloc((Count + 1u) * sizeof(*Arguments));
    if (!Arguments) return XREDIS_ADMISSION_ERROR;
    Arguments[0] = (xbytesview)XRT_BYTES_LITERAL("WATCH"); memcpy(Arguments + 1, Keys, Count * sizeof(*Keys));
    size_t Bytes = 0;
    if (!xredisCommandSize(Arguments, Count + 1u, &Bytes) || Bytes > Options.MaxWireBytes) {
        xrtFree(Arguments); xredisWatchReport(XERR_RANGE, "WATCH exceeds wire limit"); return XREDIS_ADMISSION_ERROR;
    }
    xrediswatch* Watch = xrtMalloc(sizeof(*Watch));
    if (!Watch) { xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    memset(Watch, 0, sizeof(*Watch));
    if (!xrtMutexInit(&Watch->Gate)) { xrtFree(Watch); xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    xrtAtomic32Init(&Watch->References, 1); xrtAtomic32Init(&Watch->Reason, 0); xrtAtomic32Init(&Watch->Abandoned, 0);
    Watch->Config = Options; Watch->Owner.Data = Watch;
    xrtAtomic32Init(&Watch->Owner.State, XREDIS_OWNER_STARTING);
    Watch->Owner.Deadline = nextafter(Entered + (double)Options.Timeout * 0.001, INFINITY);
    Watch->Owner.Retain = xredisWatchRef; Watch->Owner.Release = xredisWatchDrop; Watch->Owner.Wake = xredisWatchWake;
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    Watch->Owner.Abandon = xredisWatchCancel;
    xrtAtomic32Init(&Watch->Owner.Nested, 0);
#else
    (void)Parent;
#endif
    Watch->StartResult = xrtMalloc(sizeof(*Watch->StartResult)); Watch->CloseResult = xrtMalloc(sizeof(*Watch->CloseResult));
    if (Watch->StartResult) memset(Watch->StartResult, 0, sizeof(*Watch->StartResult));
    bool Ready = Watch->StartResult && Watch->CloseResult && xrtNetPostInit(&Watch->Work) &&
        (Watch->StartPromise = xrtPromiseCreate(&Watch->StartFuture, NULL)) &&
        (Watch->ClosePromise = xrtPromiseCreate(&Watch->CloseFuture, NULL));
    for (unsigned Slot = 0; Ready && Slot < XREDIS_W_SLOTS; ++Slot)
        Ready = xrtFutureWatchInit(&Watch->Watches[Slot], xredisWatchWake, xredisWatchDrop, Watch);
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Watch->StartFuture);
        if (Token) Watch->Cancel = xrtCancelWatch(Token, xredisWatchCancel, Watch);
        xrtCancelDestroy(Token); Ready = Watch->Cancel != NULL;
    }
    xredisadmission Admission = XREDIS_ADMISSION_ERROR;
    if (Ready) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        if (Parent != NULL) Admission = __xredisClientClaimNested(Client, Parent, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
        else
#endif
        Admission = __xredisClientClaim(Client, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
    }
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        Watch->Client = Client; Watch->Futures[XREDIS_W_CLIENT_CLOSED] = xredisClientClosed(Client);
        Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count + 1u, Options.Timeout, Entered, true, &Watch->Futures[XREDIS_W_WATCH]);
        if (Admission != XREDIS_ADMISSION_ACCEPTED) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
            (void)__xredisClientUnclaimUnused(Client, &Watch->Owner);
#else
            (void)__xredisClientUnclaim(Client, &Watch->Owner);
#endif
            xredisClientRelease(Client); xrtNetEngineUnpin(Watch->Engine);
            xrtFutureDestroy(Watch->Futures[XREDIS_W_CLIENT_CLOSED]); Watch->Client = NULL; Watch->Engine = NULL;
        }
    }
    xrtFree(Arguments);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { xredisWatchRelease(Watch); return Admission; }
    /* Native acceptance is the effects boundary. Every subsequent failure is
     * a typed asynchronous outcome with physical cleanup, never ERROR. */
    bool Observed = xredisWatchObserve(Watch, XREDIS_W_WATCH) && xredisWatchObserve(Watch, XREDIS_W_CLIENT_CLOSED);
    xfuture* Observer = xrtFutureRef(Watch->StartFuture);
    xredisWatchRef(Watch); /* Constructor tail may race an immediate failure. */
    xrtMutexLock(&Watch->Gate); Watch->Initialized = true; xrtMutexUnlock(&Watch->Gate);
    if (!Observed) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    *Future = Observer; xredisWatchWake(Watch); xredisWatchRelease(Watch); return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, xrtTimer(), NULL); }
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{
    double Entered = xrtTimer();
    if (Client && __xredisClientIsWorker(Client)) { xredisWatchReport(XERR_STATE, "sync WATCH cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
    xredisadmission Admission = xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, NULL);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, Parent); }
#endif

/* APIs protect the engine/client borrow through encoding and final admission.
 * Commit seals under the native client Gate, not at the start of encoding. */
static xredisadmission xredisWatchEnter(xrediswatch* Watch, bool Sync, xredisclient** Client)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return XREDIS_ADMISSION_ERROR; }
    xrtMutexLock(&Watch->Gate);
    xredisadmission Admission = Watch->Finished || xrtAtomic32Load(&Watch->Owner.State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ?
        XREDIS_ADMISSION_CLOSED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) { ++Watch->APIs; *Client = Watch->Client; }
    xrtMutexUnlock(&Watch->Gate);
    if (Admission == XREDIS_ADMISSION_ACCEPTED && Sync && __xredisClientIsWorker(*Client)) {
        xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
        xredisWatchReport(XERR_STATE, "sync WATCH operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    return Admission;
}
static void xredisWatchLeave(xrediswatch* Watch)
{ xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch); }

static xredisadmission xredisWatchCallAdmit(xrediswatch* Watch, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, true, xrtTimer()); }

static xredisadmission xredisWatchCommitAdmit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Client, &Watch->Owner, Commands, Count, &Watch->Config.Transaction, Timeout, Entered, Future);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        xrtMutexLock(&Watch->Gate); Watch->Futures[XREDIS_W_COMMIT] = xrtFutureRef(*Future); xrtMutexUnlock(&Watch->Gate);
        if (!xredisWatchObserve(Watch, XREDIS_W_COMMIT)) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    }
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, true, xrtTimer()); }
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch)
{
    if (!Watch) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchStop(Watch, XREDIS_REQUEST_CANCELLED); return true;
}
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return NULL; }
    return xrtFutureRef(Watch->CloseFuture);
}
#endif
