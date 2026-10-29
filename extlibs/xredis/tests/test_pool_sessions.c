#include "test.h"
#include "support/pool_sessions_peer.h"
static const void* sessionsWait(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK && xrtFutureState(Future) == XFUTURE_RESOLVED,
        "pool session future wait"); const void* Value = xrtFutureValue(Future); testRequire(Value != NULL, "typed pool session result"); return Value;
}
static xredislease* sessionsAcquire(redispoolsessionspeer* Peer, xfuture** Future)
{
    testRequire(xredisPoolAcquire(Peer->Pool, 5000, Future) == XREDIS_ADMISSION_ACCEPTED, "session acquire");
    const xredisleaseresult* Result = sessionsWait(*Future); testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Lease, "session lease"); return Result->Lease;
}
static void sessionsQuota(redispoolsessionspeer* Peer, uint64 Count)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info); xredisPoolInfo(Peer->Pool, &Info);
        if (Info.Requests == Count) return;
        testRequire(xrtTimer() < End, "session physical quota"); xrtThreadYield();
    }
}
static xfuture* sessionsReturn(xredislease* Lease, bool Reusable)
{
    xfuture* Closed = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease);
    const xredisleaseclosedresult* Result = sessionsWait(Closed); testRequire(Result->Reusable == Reusable, "session lease reuse"); return Closed;
}
static void sessionsPing(xredislease* Lease)
{
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
    testRequire(xredisLeaseCall(Lease, &Ping, 1, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisrequestresult*)sessionsWait(Future))->Status == XREDIS_REQUEST_OK, "session successor request"); xrtFutureDestroy(Future);
}
static void sessionsTransaction(void)
{
    unsigned Runs = 1;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    Runs = 2;
#endif
    for (unsigned Secure = 0; Secure < Runs; ++Secure) {
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, Secure != 0, NULL);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired);
        unsigned char Binary[] = {'x', 0, '\r', '\n', 0xff}; xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Binary, sizeof(Binary)}};
        xfuture* Before = NULL; testRequire(xredisLeaseCallAsync(Lease, Echo, 2, 5000, &Before) == XREDIS_ADMISSION_ACCEPTED, "transaction predecessor");
        xredistransactioncommand Commands[] = {{Echo, 2}, {Echo, 2}}; xfuture* Transaction = NULL;
        testRequire(xredisLeaseTransactionAsync(Lease, Commands, 2, NULL, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED, "leased transaction");
        memset(Binary, 0x5a, sizeof(Binary)); xfuture* Rejected = (xfuture*)(uintptr_t)1;
        testRequire(xredisLeaseCallAsync(Lease, Echo, 2, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1 &&
            xredisLeaseTransactionAsync(Lease, NULL, 0, NULL, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1,
            "leased transaction did not seal parent");
        const xredistransactionresult* Result = sessionsWait(Transaction); xredisreplyvalue Root, First;
        testRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Execution == XREDIS_TRANSACTION_EXECUTED &&
            xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 2 && xredisReplyGet(Result->Request.Reply, Root.FirstChild, &First) &&
            First.Data.Size == 5 && !memcmp(First.Data.Data, "x\0\r\n\xff", 5), "leased binary EXEC");
        testRequire(((const xredisrequestresult*)sessionsWait(Before))->Status == XREDIS_REQUEST_OK, "leased predecessor correlation");
        xfuture* Closed = sessionsReturn(Lease, true), *Next = NULL; xredislease* Successor = sessionsAcquire(&Peer, &Next); sessionsPing(Successor);
        xfuture* Returned = sessionsReturn(Successor, true); redisPoolSessionsPeerStop(&Peer);
        testRequire(((const xredistransactionresult*)sessionsWait(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "transaction metadata pinned engine");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Before); xrtFutureDestroy(Transaction); xrtFutureDestroy(Closed); xrtFutureDestroy(Next); xrtFutureDestroy(Returned);
    }
}
static void sessionsWatch(void)
{
    unsigned Runs = 2;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    Runs = 4;
#endif
    for (unsigned Case = 0; Case < Runs; ++Case) {
        bool Abort = (Case & 1u) != 0;
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, Case >= 2, NULL);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xbytesview Key = XRT_BYTES_LITERAL("watch\0pool");
        xfuture* Started = NULL; testRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "nested WATCH admission");
        const xrediswatchresult* Start = sessionsWait(Started); testRequire(Start->Request.Status == XREDIS_REQUEST_OK && Start->Watch, "nested WATCH ready");
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("nested\0read")}; xfuture* Read = NULL;
        testRequire(xredisWatchCall(Start->Watch, Echo, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED &&
            ((const xredisrequestresult*)sessionsWait(Read))->Status == XREDIS_REQUEST_OK, "nested read");
        sessionsQuota(&Peer, 0); xfuture* Rejected = (xfuture*)(uintptr_t)1;
        testRequire(xredisLeaseCallAsync(Lease, Echo, 2, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1 &&
            xredisLeaseWatchAsync(Lease, &Key, 1, NULL, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1,
            "nested WATCH did not seal parent lease");
        xfuture *WatchClosed = xredisWatchClosed(Start->Watch), *Transaction = NULL;
        if (Abort) xredisWatchAbort(Start->Watch);
        else {
            xredistransactioncommand Command = {Echo, 2};
            testRequire(xredisWatchCommit(Start->Watch, &Command, 1, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED &&
                ((const xredistransactionresult*)sessionsWait(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "nested watch EXEC");
        }
        testRequire(((const xrediswatchclosedresult*)sessionsWait(WatchClosed))->Reusable, "nested WATCH physical cleanup");
        xfuture* Waiting = NULL; testRequire(xredisPoolAcquireAsync(Peer.Pool, 5000, &Waiting) == XREDIS_ADMISSION_ACCEPTED &&
            xrtFutureWaitFor(Waiting, 20) == XWAIT_TIMEOUT, "scope close prematurely released explicit lease");
        xfuture* Closed = sessionsReturn(Lease, true); xredislease* Next = ((const xredisleaseresult*)sessionsWait(Waiting))->Lease;
        testRequire(Next != NULL, "nested successor"); sessionsPing(Next); xfuture* Returned = sessionsReturn(Next, true);
        redisPoolSessionsPeerStop(&Peer);
        testRequire(xredisWatchAbort(Start->Watch), "nested completed metadata accessed freed pool");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(WatchClosed); xrtFutureDestroy(Transaction);
        xrtFutureDestroy(Waiting); xrtFutureDestroy(Closed); xrtFutureDestroy(Returned);
    }
}
typedef struct sessionsorder { xfuture* Scope; xatomic32 Checked; } sessionsorder;
static void sessionsOrderNotify(ptr Data)
{
    sessionsorder* Order = Data; testRequire(xrtFutureState(Order->Scope) == XFUTURE_RESOLVED, "lease Closed preceded nested physical Closed");
    xrtAtomic32Store(&Order->Checked, 1, XMEMORY_RELEASE);
}
static void sessionsOrderDrop(ptr Data) { (void)Data; }
static void sessionsParentReturn(void)
{
    const unsigned Modes[] = {W_NORMAL, W_BAD_CLEANUP, W_DENY_CLEANUP, W_HOLD_CLEANUP};
    for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index) {
        xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.ReturnTimeout = 30;
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, Modes[Index], TX_NORMAL, false, &Config);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xbytesview Key = XRT_BYTES_LITERAL("parent-return");
        xfuture* Started = NULL; testRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "parent-return watch");
        const xrediswatchresult* Result = sessionsWait(Started); testRequire(Result->Request.Status == XREDIS_REQUEST_OK, "parent-return ready"); sessionsQuota(&Peer, 0);
        xfuture* ScopeClosed = xredisWatchClosed(Result->Watch), *LeaseClosed = xredisLeaseClosed(Lease); sessionsorder Order = {0}; Order.Scope = ScopeClosed;
        xfuturewatch Watch; testRequire(xrtFutureWatchInit(&Watch, sessionsOrderNotify, sessionsOrderDrop, &Order) &&
            xrtFutureWatchAdd(LeaseClosed, &Watch) == XFUTURE_WATCH_PENDING, "parent close observer");
        xredisLeaseReturn(Lease);
        const xredisleaseclosedresult* Returned = sessionsWait(LeaseClosed);
        testRequire(Returned->Reusable == (Modes[Index] == W_NORMAL) &&
            ((const xrediswatchclosedresult*)sessionsWait(ScopeClosed))->Reusable == (Modes[Index] == W_NORMAL), "parent returned dirty scope");
        xrtFutureWatchRemove(LeaseClosed, &Watch); testRequire(xrtAtomic32Load(&Order.Checked, XMEMORY_ACQUIRE), "parent close observer did not finish");
        testRequire(xrtAtomic32Load(&Peer.Redis.Cleanups, XMEMORY_ACQUIRE) == 1, "parent Return omitted/replayed UNWATCH");
        redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(LeaseClosed);
    }
}
static void sessionsErrors(void)
{
    const unsigned Modes[] = {TX_MULTI_ERROR, TX_BAD_MULTI, TX_BAD_QUEUE, TX_EXEC_ERROR, TX_EXEC_UNKNOWN,
        TX_BAD_COUNT, TX_ABORT, TX_DISCARD_ERROR, TX_RUNTIME_ERROR, TX_DROP_COMMAND, TX_DROP_EXEC};
    for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index) {
        unsigned Mode = Modes[Index]; redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, Mode, false, NULL);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired);
        xbytesview Body = XRT_BYTES_LITERAL("BODY"), Bad = XRT_BYTES_LITERAL("BAD");
        xredistransactioncommand Commands[] = {{&Body, 1}, {Mode == TX_DISCARD_ERROR ? &Bad : &Body, 1}}; xfuture* Transaction = NULL;
        testRequire(xredisLeaseTransaction(Lease, Commands, 2, NULL, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED, "pooled transaction fault admission");
        const xredistransactionresult* Result = sessionsWait(Transaction);
        bool Reusable = Mode == TX_MULTI_ERROR || Mode == TX_EXEC_ERROR || Mode == TX_ABORT || Mode == TX_RUNTIME_ERROR;
        testRequire(Result->Execution == (Mode == TX_EXEC_ERROR || Mode == TX_ABORT ? XREDIS_TRANSACTION_ABORTED : Mode == TX_RUNTIME_ERROR ? XREDIS_TRANSACTION_EXECUTED :
            Mode == TX_EXEC_UNKNOWN || Mode == TX_BAD_COUNT || Mode == TX_DROP_EXEC ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED : XREDIS_TRANSACTION_NOT_EXECUTED), "pooled execution certainty");
        xfuture* Returned = sessionsReturn(Lease, Reusable); redisPoolSessionsPeerStop(&Peer);
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Transaction); xrtFutureDestroy(Returned);
    }
    for (unsigned Mode = W_BAD_START; Mode <= W_DROP_START; ++Mode) {
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, Mode, TX_NORMAL, false, NULL);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xbytesview Key = XRT_BYTES_LITERAL("fault"); xfuture* Started = NULL;
        testRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "pooled rejected watch admission");
        const xrediswatchresult* Result = sessionsWait(Started); testRequire(Result->Request.Status != XREDIS_REQUEST_OK && Result->Watch, "failed nested start omitted cleanup handle");
        xfuture* ScopeClosed = xredisWatchClosed(Result->Watch); bool Reusable = Mode == W_DENY_START;
        testRequire(((const xrediswatchclosedresult*)sessionsWait(ScopeClosed))->Reusable == Reusable, "nested failure clearing boundary");
        xfuture* Returned = sessionsReturn(Lease, Reusable); redisPoolSessionsPeerStop(&Peer);
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned);
    }
}
static void sessionsQuotaAndBusy(void)
{
    xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.MaxRequests = 1; Config.MaxCommandBytes = 128;
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, &Config);
    xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired);
    xbytesview Body = XRT_BYTES_LITERAL("BODY"), Key = XRT_BYTES_LITERAL("key"); xredistransactioncommand Commands[] = {{&Body, 1}, {&Body, 1}};
    xfuture* Output = (xfuture*)(uintptr_t)1;
    testRequire(xredisLeaseTransactionAsync(Lease, Commands, 2, NULL, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1,
        "pooled transaction metadata escaped group byte cap"); sessionsPing(Lease); sessionsQuota(&Peer, 0);
    xbytesview Hold = XRT_BYTES_LITERAL("HOLD"); xfuture* Held = NULL;
    testRequire(xredisLeaseCallAsync(Lease, &Hold, 1, 5000, &Held) == XREDIS_ADMISSION_ACCEPTED, "nested busy predecessor");
    redisPeerWait(&Peer.Redis.Tx.Peer->Commands, 3);
    testRequire(xredisLeaseWatchAsync(Lease, &Key, 1, NULL, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1,
        "nested watch claimed a busy parent");
    redisPeerReleaseBlocked(Peer.Redis.Tx.Peer); testRequire(((const xredisrequestresult*)sessionsWait(Held))->Status == XREDIS_REQUEST_OK, "busy predecessor drain");
    sessionsQuota(&Peer, 0); xfuture* Started = NULL;
    testRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "busy rejection did not preserve lease");
    const xrediswatchresult* Start = sessionsWait(Started); testRequire(Start->Request.Status == XREDIS_REQUEST_OK, "quota WATCH ready"); sessionsQuota(&Peer, 0);
    xfuture* Read = NULL; uint32 Before = xrtAtomic32Load(&Peer.Redis.Tx.Peer->Commands, XMEMORY_ACQUIRE);
    testRequire(xredisWatchCallAsync(Start->Watch, &Hold, 1, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "quota nested read"); redisPeerWait(&Peer.Redis.Tx.Peer->Commands, Before + 1u);
    testRequire(xredisWatchCommitAsync(Start->Watch, NULL, 0, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1,
        "nested transaction escaped aggregate request cap");
    testRequire(xrtFutureCancel(Read) && ((const xredisrequestresult*)sessionsWait(Read))->Status == XREDIS_REQUEST_CANCELLED, "nested sent read cancel"); sessionsQuota(&Peer, 1);
    testRequire(xredisWatchCommitAsync(Start->Watch, NULL, 0, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1,
        "nested logical cancellation returned physical quota");
    xfuture* Returned = sessionsReturn(Lease, false), *ScopeClosed = xredisWatchClosed(Start->Watch);
    testRequire(!((const xrediswatchclosedresult*)sessionsWait(ScopeClosed))->Reusable, "busy nested cancellation reused stream");
    redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Held); xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Returned); xrtFutureDestroy(ScopeClosed);
}
static void sessionsDeadlinesAndClose(void)
{
    for (unsigned Mode = 0; Mode < 5; ++Mode) {
        xredispoolconfig Config; xredisPoolConfigInit(&Config); if (Mode == 1) Config.LeaseTimeout = 80;
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, Mode == 4 ? W_HOLD_START : W_NORMAL, TX_NORMAL, false, &Config);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xbytesview Key = XRT_BYTES_LITERAL("deadline"); xrediswatchconfig WatchConfig;
        xredisWatchConfigInit(&WatchConfig); if (Mode == 0) WatchConfig.Timeout = 80;
        xfuture* Started = NULL; testRequire(xredisLeaseWatchAsync(Lease, &Key, 1, &WatchConfig, &Started) == XREDIS_ADMISSION_ACCEPTED, "nested lifecycle admission");
        if (Mode == 4) { redisPeerWait(&Peer.Redis.Starts, 1); xrtFutureCancel(Started); }
        const xrediswatchresult* Result = sessionsWait(Started); testRequire(Result->Watch != NULL, "nested lifecycle handle");
        if (Mode != 4) testRequire(Result->Request.Status == XREDIS_REQUEST_OK, "nested lifecycle ready");
        if (Mode == 2) xredisLeaseAbort(Lease);
        if (Mode == 3) xredisPoolClose(Peer.Pool);
        xfuture* ScopeClosed = xredisWatchClosed(Result->Watch); const xrediswatchclosedresult* Scope = sessionsWait(ScopeClosed);
        testRequire(Scope->Reusable == (Mode == 0), "nested hold/close cleanup");
        xfuture* Returned = sessionsReturn(Lease, Mode == 0);
        if (Mode == 0) testRequire(Scope->Reason == XREDIS_REQUEST_TIMEOUT, "nested deadline renewed by parent hold");
        if (Mode == 1) testRequire(((const xredisleaseclosedresult*)sessionsWait(Returned))->Reason == XREDIS_REQUEST_TIMEOUT, "parent hold not capped");
        redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned);
    }
    xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.ReturnTimeout = 25;
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_HOLD_EXEC, false, &Config);
    xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xfuture* Transaction = NULL;
    testRequire(xredisLeaseTransactionAsync(Lease, NULL, 0, NULL, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED, "cancel leased EXEC"); redisPeerWait(&Peer.Redis.Tx.Execs, 1);
    xrtFutureCancel(Transaction); const xredistransactionresult* Result = sessionsWait(Transaction);
    testRequire(Result->Request.Status == XREDIS_REQUEST_CANCELLED && Result->Execution == XREDIS_TRANSACTION_MAY_HAVE_EXECUTED, "pooled cancelled EXEC certainty");
    xfuture* Returned = sessionsReturn(Lease, false); redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Transaction); xrtFutureDestroy(Returned);
}
static void sessionsBudgetZero(xallocationbudget* Budget)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xrtAllocationBudgetInfo(Budget, &Info), "session budget snapshot");
        if (!Info.LiveCount && !Info.LiveBytes) return;
        testRequire(xrtTimer() < End, "nested session allocation retained after physical tails"); xrtThreadYield();
    }
}
static void sessionsMemory(void)
{
    for (unsigned Watch = 0; Watch < 2; ++Watch) {
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL); unsigned Accepted = 0, Rejected = 0;
        for (size_t Limit = 1; Limit <= 8192; Limit += 113) {
            xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xbytesview Key = XRT_BYTES_LITERAL("oom");
            xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xfuture* Future = (xfuture*)(uintptr_t)1;
            testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "nested admission budget");
            xredisadmission Admission = Watch ? xredisLeaseWatchAsync(Lease, &Key, 1, NULL, &Future) : xredisLeaseTransactionAsync(Lease, NULL, 0, NULL, 5000, &Future);
            testRequire(xrtAllocationBudgetLeave(&Scope), "nested budget leave");
            if (Admission == XREDIS_ADMISSION_ACCEPTED) {
                ++Accepted;
                if (Watch) {
                    const xrediswatchresult* Started = sessionsWait(Future); testRequire(Started->Request.Status == XREDIS_REQUEST_OK, "budget nested start");
                    xfuture* Closed = xredisWatchClosed(Started->Watch); xredisWatchAbort(Started->Watch);
                    testRequire(((const xrediswatchclosedresult*)sessionsWait(Closed))->Reusable, "budget nested cleanup"); xrtFutureDestroy(Closed);
                } else testRequire(((const xredistransactionresult*)sessionsWait(Future))->Execution == XREDIS_TRANSACTION_EXECUTED, "budget pooled EXEC");
                xrtFutureDestroy(Future);
            } else {
                ++Rejected; testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "nested OOM changed output/state"); xrtClearError(); sessionsPing(Lease);
            }
            xfuture* Returned = sessionsReturn(Lease, true); xrtFutureDestroy(Returned); xrtFutureDestroy(Acquired); sessionsBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
        }
        testRequire(Accepted && Rejected, "nested OOM windows missed boundary"); redisPoolSessionsPeerStop(&Peer);
    }
}
typedef struct sessionsrace {
    xredislease* Lease;
    xatomic32 *Ready, *Go;
    unsigned Role;
    xredisadmission Admission;
    xfuture* Future;
} sessionsrace;
static int32 sessionsRaceProc(ptr Data)
{
    sessionsrace* Race = Data; xrtAtomic32FetchAdd(Race->Ready, 1, XMEMORY_RELEASE); redisPeerWait(Race->Go, 1);
    xbytesview Key = XRT_BYTES_LITERAL("race"), Ping = XRT_BYTES_LITERAL("PING"); Race->Future = (xfuture*)(uintptr_t)1;
    if (Race->Role == 0) Race->Admission = xredisLeaseWatchAsync(Race->Lease, &Key, 1, NULL, &Race->Future);
    else if (Race->Role == 1) Race->Admission = xredisLeaseTransactionAsync(Race->Lease, NULL, 0, NULL, 5000, &Race->Future);
    else if (Race->Role == 2) Race->Admission = xredisLeaseCallAsync(Race->Lease, &Ping, 1, 5000, &Race->Future);
    else { xredisLeaseReturn(Race->Lease); Race->Admission = XREDIS_ADMISSION_CLOSED; }
    return 0;
}
static void sessionsRaces(void)
{
    for (unsigned Round = 0; Round < 20; ++Round) {
        redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL);
        xfuture* Acquired = NULL; xredislease* Lease = sessionsAcquire(&Peer, &Acquired); xfuture* Returned = xredisLeaseClosed(Lease);
        xatomic32 Ready, Go; xrtAtomic32Init(&Ready, 0); xrtAtomic32Init(&Go, 0); sessionsrace Races[4]; xthread* Threads[4];
        for (unsigned Role = 0; Role < 4; ++Role) {
            Races[Role] = (sessionsrace){Lease, &Ready, &Go, Role, XREDIS_ADMISSION_ERROR, NULL};
            Threads[Role] = xrtThreadCreate(sessionsRaceProc, &Races[Role], 0); testRequire(Threads[Role] != NULL, "session race thread");
        }
        redisPeerWait(&Ready, 4); xrtAtomic32Store(&Go, 1, XMEMORY_RELEASE);
        for (unsigned Role = 0; Role < 4; ++Role) { testRequire(xrtThreadWait(Threads[Role]) == XWAIT_OK && !xrtThreadExitCode(Threads[Role]), "session race join"); xrtThreadDestroy(Threads[Role]); }
        xredisLeaseReturn(Lease); const xredisleaseclosedresult* Closed = sessionsWait(Returned);
        for (unsigned Role = 0; Role < 3; ++Role) {
            if (Races[Role].Admission == XREDIS_ADMISSION_ACCEPTED) {
                const void* Result = sessionsWait(Races[Role].Future);
                if (Role == 0) {
                    const xrediswatchresult* Start = Result; testRequire(Start->Watch != NULL, "racing WATCH cleanup handle");
                    xfuture* ScopeClosed = xredisWatchClosed(Start->Watch);
                    testRequire(xrtFutureState(ScopeClosed) == XFUTURE_RESOLVED, "parent race released a live nested owner"); xrtFutureDestroy(ScopeClosed);
                }
                xrtFutureDestroy(Races[Role].Future);
            } else testRequire((Races[Role].Admission == XREDIS_ADMISSION_CLOSED || Races[Role].Admission == XREDIS_ADMISSION_OVERLOADED) &&
                Races[Role].Future == (xfuture*)(uintptr_t)1, "racing admission mutated rejected output");
        }
        if (Closed->Reusable) { xfuture* Next = NULL; xredislease* Successor = sessionsAcquire(&Peer, &Next); sessionsPing(Successor); xfuture* Again = sessionsReturn(Successor, true); xrtFutureDestroy(Next); xrtFutureDestroy(Again); }
        redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Returned);
    }
}
typedef struct sessionssync { xredislease* Lease; xatomic32 Done; } sessionssync;
static void sessionsSyncWorker(xnetworker* Worker, ptr Data)
{
    (void)Worker; sessionssync* State = Data; xfuture* Future = (xfuture*)(uintptr_t)1; xbytesview Key = XRT_BYTES_LITERAL("sync");
    testRequire(xredisLeaseTransaction(State->Lease, NULL, 0, NULL, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
        "sync pooled transaction blocked an engine worker"); xrtClearError();
    testRequire(xredisLeaseWatch(State->Lease, &Key, 1, NULL, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
        "sync nested watch blocked an engine worker"); xrtClearError(); xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}
static void sessionsSync(void)
{
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL); xfuture* Acquired = NULL;
    xredislease* Lease = sessionsAcquire(&Peer, &Acquired); sessionssync State = {0}; State.Lease = Lease;
    testRequire(xrtNetEnginePost(Peer.Redis.Tx.Peer->Engine, 1, sessionsSyncWorker, &State), "session sync reentry post"); redisPeerWait(&State.Done, 1);
    sessionsPing(Lease); xfuture* Returned = sessionsReturn(Lease, true); redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Returned);
}
int main(void)
{
    sessionsTransaction(); sessionsWatch(); sessionsParentReturn(); sessionsErrors(); sessionsQuotaAndBusy(); sessionsDeadlinesAndClose(); sessionsMemory(); sessionsRaces(); sessionsSync();
    puts("[PASS] pooled transactions/WATCH: direct native outcomes; sealed lease; nested physical barrier; quota-preserving cleanup and no dirty handoff"); return 0;
}
