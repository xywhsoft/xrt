#include "test.h"
#include "support/watch_peer.h"

static const xrediswatchresult* watchStartResult(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK && xrtFutureState(Future) == XFUTURE_RESOLVED, "watch start wait");
    const xrediswatchresult* Result = xrtFutureValue(Future); testRequire(Result != NULL, "watch typed start"); return Result;
}
static const xredisrequestresult* watchRequest(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "watch request wait");
    const xredisrequestresult* Result = xrtFutureValue(Future); testRequire(Result != NULL, "watch typed request"); return Result;
}
static const xrediswatchclosedresult* watchClosed(xrediswatch* Watch, xfuture** Future)
{
    *Future = xredisWatchClosed(Watch); testRequire(*Future && xrtFutureWaitFor(*Future, 10000) == XWAIT_OK, "watch physical close");
    const xrediswatchclosedresult* Result = xrtFutureValue(*Future); testRequire(Result != NULL, "watch typed close"); return Result;
}
static xfuture* watchBegin(xredisclient* Client, const xrediswatchconfig* Config)
{
    unsigned char Binary[] = {'k', 0, '\r', '\n', 0xff}; xbytesview Keys[] = {{Binary, sizeof(Binary)}, XRT_BYTES_LITERAL("")};
    xfuture* Future = NULL;
    testRequire(xredisClientWatchAsync(Client, Keys, 2, Config, &Future) == XREDIS_ADMISSION_ACCEPTED, "WATCH admission");
    memset(Binary, 0x5a, sizeof(Binary)); memset(Keys, 0, sizeof(Keys)); return Future;
}
static void watchOrdinary(xredisclient* Client)
{
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
    testRequire(xredisClientCall(Client, &Ping, 1, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED && watchRequest(Future)->Status == XREDIS_REQUEST_OK,
        "watch connection reusable"); xrtFutureDestroy(Future);
}
static void watchIsolation(xredisclient* Client)
{
    xbytesview Ping = XRT_BYTES_LITERAL("PING"), Key = XRT_BYTES_LITERAL("other"); xfuture* Future = (xfuture*)(uintptr_t)1;
    testRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Future) == XREDIS_ADMISSION_OVERLOADED && Future == (xfuture*)(uintptr_t)1,
        "ordinary request entered WATCH scope");
    testRequire(xredisClientTransactionAsync(Client, NULL, 0, NULL, 5000, &Future) == XREDIS_ADMISSION_OVERLOADED && Future == (xfuture*)(uintptr_t)1,
        "unowned transaction entered WATCH scope");
    testRequire(xredisClientWatchAsync(Client, &Key, 1, NULL, &Future) == XREDIS_ADMISSION_OVERLOADED && Future == (xfuture*)(uintptr_t)1,
        "second WATCH entered owned connection");
}
static void watchBasic(void)
{
    unsigned Runs = 1;
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    Runs = 2;
#endif
    for (unsigned Secure = 0; Secure < Runs; ++Secure) for (unsigned Abort = 0; Abort < 2; ++Abort) {
        rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, Secure != 0);
        xredisclientconfig Config; xredisClientConfigInit(&Config); Config.WorkRequests = 1; Config.ReadBytes = 2; Config.ReadFrames = 1;
        xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, &Config); redisWatchPeerReady(Client);
        xfuture* Started = watchBegin(Client, NULL); const xrediswatchresult* Start = watchStartResult(Started);
        testRequire(Start->Request.Status == XREDIS_REQUEST_OK && Start->Watch && xredisWatchRetain(Start->Watch), "WATCH readiness/retained handle");
        xrediswatch* Watch = Start->Watch; watchIsolation(Client);
        unsigned char Payload[] = {'a', 0, '\r', '\n', 0xff};
        xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}}; xfuture* Read = NULL;
        testRequire(xredisWatchCallAsync(Watch, Args, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED, "scoped read");
        memset(Payload, 0, sizeof(Payload)); xfuture* Commit = NULL;
        if (Abort) {
            testRequire(watchRequest(Read)->Status == XREDIS_REQUEST_OK, "scoped read result"); redisWatchPeerReady(Client);
            testRequire(xredisWatchAbort(Watch) && xredisWatchAbort(Watch), "idempotent idle abort");
        } else {
            xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Command = {&Body, 1};
            testRequire(xredisWatchCommitAsync(Watch, &Command, 1, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "scoped commit");
            xfuture* Reject = (xfuture*)(uintptr_t)1;
            testRequire(xredisWatchCallAsync(Watch, Args, 2, 5000, &Reject) == XREDIS_ADMISSION_CLOSED && Reject == (xfuture*)(uintptr_t)1,
                "read admitted after commit seal");
            testRequire(xredisWatchCommitAsync(Watch, NULL, 0, 5000, &Reject) == XREDIS_ADMISSION_CLOSED && Reject == (xfuture*)(uintptr_t)1,
                "second commit admitted");
            const xredistransactionresult* Result = (const xredistransactionresult*)watchRequest(Commit);
            testRequire(Result->Execution == XREDIS_TRANSACTION_EXECUTED && Result->Request.Status == XREDIS_REQUEST_OK, "scoped EXEC");
        }
        const xredisrequestresult* ReadResult = watchRequest(Read); xredisreplyvalue Value;
        testRequire(ReadResult->Status == XREDIS_REQUEST_OK && xredisReplyGet(ReadResult->Reply, 0, &Value) && Value.Data.Size == 5 &&
            !memcmp(Value.Data.Data, "a\0\r\n\xff", 5), "scoped binary snapshot/correlation");
        xfuture* Closed = NULL; const xrediswatchclosedresult* Result = watchClosed(Watch, &Closed);
        if (!Result->Reusable || Result->Reason != (Abort ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_OK))
            fprintf(stderr, "watch secure=%u abort=%u reason=%d reusable=%d starts=%u cleanup=%u exec=%u\n", Secure, Abort,
                Result->Reason, Result->Reusable, xrtAtomic32Load(&Peer.Starts, XMEMORY_ACQUIRE),
                xrtAtomic32Load(&Peer.Cleanups, XMEMORY_ACQUIRE), xrtAtomic32Load(&Peer.Tx.Execs, XMEMORY_ACQUIRE));
        testRequire(Result->Reusable && Result->Reason == (Abort ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_OK), "watch clean terminal reason");
        testRequire(xrtAtomic32Load(&Peer.Cleanups, XMEMORY_ACQUIRE) == Abort, "unnecessary or missing UNWATCH");
        watchOrdinary(Client); redisTransactionPeerStop(&Peer.Tx);
        testRequire(xredisWatchAbort(Watch) && xrtFutureValue(Closed) == Result && watchStartResult(Started)->Watch == Watch,
            "completed scope retained engine or changed outcomes");
        xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed); xredisWatchRelease(Watch);
    }
}
static void watchFailures(void)
{
    for (unsigned Mode = W_BAD_START; Mode <= W_HOLD_CLEANUP; ++Mode) {
        rediswatchpeer Peer; redisWatchPeerStart(&Peer, Mode, TX_NORMAL, false);
        xredisclientconfig ClientConfig; xredisClientConfigInit(&ClientConfig); ClientConfig.DrainTimeout = 20;
        xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, &ClientConfig); redisWatchPeerReady(Client);
        xrediswatchconfig Config; xredisWatchConfigInit(&Config); Config.CleanupTimeout = 20;
        xfuture* Started = watchBegin(Client, &Config); const xrediswatchresult* Start = watchStartResult(Started);
        if (Mode <= W_DROP_START) {
            testRequire(Start->Watch && Start->Request.Status == (Mode == W_BAD_START ? XREDIS_REQUEST_PROTOCOL :
                Mode == W_DENY_START ? XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_DISCONNECTED), "WATCH start failure status");
            xfuture* FailedClosed = NULL;
            const xrediswatchclosedresult* Failed = watchClosed(Start->Watch, &FailedClosed);
            testRequire(Failed->Reusable == (Mode == W_DENY_START) && Failed->Reason == Start->Request.Status, "failed start cleanup barrier");
            xrtFutureDestroy(FailedClosed);
            if (Mode == W_DENY_START) {
                double End = xrtTimer() + 10; xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Reuse = NULL;
                while (xredisClientCallAsync(Client, &Ping, 1, 5000, &Reuse) == XREDIS_ADMISSION_OVERLOADED) {
                    testRequire(xrtTimer() < End, "denied WATCH cleanup"); xrtThreadYield();
                }
                testRequire(Reuse && watchRequest(Reuse)->Status == XREDIS_REQUEST_OK, "denied WATCH left dirty claim"); xrtFutureDestroy(Reuse);
            }
        } else {
            testRequire(Start->Watch && xredisWatchAbort(Start->Watch), "cleanup failure abort");
            xfuture* Closed = NULL; testRequire(!watchClosed(Start->Watch, &Closed)->Reusable, "failed UNWATCH reused connection"); xrtFutureDestroy(Closed);
        }
        redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started);
    }
    const unsigned TxModes[] = {TX_ABORT, TX_MULTI_ERROR, TX_EXEC_ERROR, TX_EXEC_UNKNOWN, TX_NORMAL, TX_RUNTIME_ERROR};
    for (size_t Index = 0; Index < sizeof(TxModes) / sizeof(TxModes[0]); ++Index) {
        unsigned Mode = TxModes[Index]; rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, Mode, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
        xfuture* Started = watchBegin(Client, NULL); xrediswatch* Watch = watchStartResult(Started)->Watch;
        testRequire(Watch != NULL, "transaction failure WATCH readiness");
        xbytesview Body = XRT_BYTES_LITERAL("BODY"), Bad = XRT_BYTES_LITERAL("BAD");
        xredistransactioncommand Commands[] = {{&Body, 1}, {Mode == TX_NORMAL ? &Bad : &Body, 1}}; xfuture* Commit = NULL;
        testRequire(xredisWatchCommit(Watch, Commands, 2, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "watch failing commit");
        const xredistransactionresult* Result = (const xredistransactionresult*)watchRequest(Commit);
        testRequire(Result->Execution == (Mode == TX_ABORT || Mode == TX_EXEC_ERROR ? XREDIS_TRANSACTION_ABORTED :
            Mode == TX_MULTI_ERROR || Mode == TX_NORMAL ? XREDIS_TRANSACTION_NOT_EXECUTED :
            Mode == TX_EXEC_UNKNOWN ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED : XREDIS_TRANSACTION_EXECUTED), "WATCH EXEC certainty");
        xfuture* Closed = NULL; testRequire(watchClosed(Watch, &Closed)->Reusable == (Mode != TX_EXEC_UNKNOWN), "WATCH transaction error reuse");
        testRequire(xrtAtomic32Load(&Peer.Cleanups, XMEMORY_ACQUIRE) == (Mode == TX_MULTI_ERROR), "MULTI rejection failed to UNWATCH");
        if (Mode != TX_EXEC_UNKNOWN) watchOrdinary(Client);
        redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed);
    }
}
static void watchAbandon(void)
{
    for (unsigned Mode = 0; Mode < 6; ++Mode) {
        rediswatchpeer Peer; redisWatchPeerStart(&Peer, Mode == 0 ? W_HOLD_START : W_NORMAL, Mode == 5 ? TX_HOLD_EXEC : TX_NORMAL, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
        xrediswatchconfig Config; xredisWatchConfigInit(&Config); if (Mode == 1 || Mode == 3) Config.Timeout = 100;
        xfuture* Started = watchBegin(Client, &Config), *Read = NULL, *Commit = NULL, *Closed = NULL;
        if (Mode == 0) {
            redisPeerWait(&Peer.Starts, 1); testRequire(xrtFutureCancel(Started), "cancel pending WATCH");
            testRequire(watchStartResult(Started)->Request.Status == XREDIS_REQUEST_CANCELLED && watchStartResult(Started)->Watch,
                "pending WATCH cancellation result");
            const xrediswatchclosedresult* Result = watchClosed(watchStartResult(Started)->Watch, &Closed);
            testRequire(!Result->Reusable && Result->Reason == XREDIS_REQUEST_CANCELLED, "cancelled start physical barrier");
        } else {
            xrediswatch* Watch = watchStartResult(Started)->Watch; testRequire(Watch != NULL, "abandon ready scope");
            if (Mode == 2 || Mode == 3) {
                uint32 Before = xrtAtomic32Load(&Peer.Tx.Peer->Commands, XMEMORY_ACQUIRE); xbytesview Hold = XRT_BYTES_LITERAL("HOLD");
                testRequire(xredisWatchCallAsync(Watch, &Hold, 1, XRT_WAIT_FOREVER, &Read) == XREDIS_ADMISSION_ACCEPTED, "busy scope read");
                redisPeerWait(&Peer.Tx.Peer->Commands, Before + 1u);
            }
            if (Mode == 2) xredisWatchAbort(Watch);
            if (Mode == 4) xredisClientClose(Client);
            if (Mode == 5) {
                testRequire(xredisWatchCommitAsync(Watch, NULL, 0, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "cancel commit admission");
                redisPeerWait(&Peer.Tx.Execs, 1); xrtFutureCancel(Commit);
                testRequire(((const xredistransactionresult*)watchRequest(Commit))->Execution == XREDIS_TRANSACTION_MAY_HAVE_EXECUTED,
                    "cancelled EXEC certainty");
            }
            const xrediswatchclosedresult* Result = watchClosed(Watch, &Closed);
            testRequire(Result->Reason == (Mode == 1 || Mode == 3 ? XREDIS_REQUEST_TIMEOUT : Mode == 4 ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_CANCELLED) &&
                Result->Reusable == (Mode == 1), "abandon reason/dirty reuse");
            if (Read) testRequire(watchRequest(Read)->Status == XREDIS_REQUEST_TIMEOUT || watchRequest(Read)->Status == XREDIS_REQUEST_CLOSED, "abandoned scoped read");
            if (Mode == 1) watchOrdinary(Client);
        }
        redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed);
    }
}
static void watchBounds(void)
{
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xbytesview Key = XRT_BYTES_LITERAL("key"); xrediswatchconfig Config; xredisWatchConfigInit(&Config);
    for (unsigned Mode = 0; Mode < 11; ++Mode) {
        xrediswatchconfig Bad = Config; size_t Count = 1; const xbytesview* Keys = &Key; xfuture* Future = (xfuture*)(uintptr_t)1;
        if (Mode == 0) Bad.Size--;
        if (Mode == 1) Bad.Timeout = 0;
        if (Mode == 2) Bad.Timeout = 86400001;
        if (Mode == 3) Bad.CleanupTimeout = 60001;
        if (Mode == 4) Bad.MaxKeys = 65537;
        if (Mode == 5) Count = 0;
        if (Mode == 6) Count = SIZE_MAX;
        if (Mode == 7) Keys = NULL;
        if (Mode == 8) Bad.MaxWireBytes = 1;
        if (Mode == 9) Bad.Transaction.Size--;
        if (Mode == 10) Keys = (const xbytesview*)(uintptr_t)(UINTPTR_MAX - 2u);
        testRequire(xredisClientWatchAsync(Client, Keys, Count, &Bad, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
            "invalid WATCH changed output or admitted"); xrtClearError();
    }
    xfuture* Started = watchBegin(Client, NULL); xrediswatch* Watch = watchStartResult(Started)->Watch;
    const cstr Words[] = {"MULTI", "EXEC", "DISCARD", "WATCH", "UNWATCH", "SELECT", "AUTH", "HELLO", "QUIT", "READONLY", "ASKING", "SUBSCRIBE", "MONITOR"};
    for (size_t Index = 0; Index < sizeof(Words) / sizeof(Words[0]); ++Index) {
        xbytesview Word = {(cbytes)Words[Index], strlen(Words[Index])}; xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xredisWatchCallAsync(Watch, &Word, 1, 5000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
            "scoped control escaped"); xrtClearError();
    }
    xredisWatchAbort(Watch); xfuture* Closed = NULL; testRequire(watchClosed(Watch, &Closed)->Reusable, "bounded scope cleanup");
    redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(Closed);
}

static void watchBudgetDrained(xallocationbudget* Budget)
{
    /* Closed releases the claim/pin before waking observers. Its publisher
     * and embedded-post callback can still be returning on the owning worker;
     * do not mistake those real reference tails for a permanent leak. */
    double End = xrtTimer() + 10;
    for (;;) {
        xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xrtAllocationBudgetInfo(Budget, &Info), "WATCH budget snapshot");
        if (!Info.LiveCount && !Info.LiveBytes) return;
        testRequire(xrtTimer() < End, "WATCH constructor/result allocation leak after callback retirement"); xrtThreadYield();
    }
}

static void watchMemory(void)
{
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xbytesview Key = XRT_BYTES_LITERAL("key"); unsigned Accepted = 0, Rejected = 0;
    for (size_t Limit = 1; Limit <= 16384; Limit += 97) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "watch constructor budget");
        xredisadmission Admission = xredisClientWatchAsync(Client, &Key, 1, NULL, &Future);
        testRequire(xrtAllocationBudgetLeave(&Scope), "watch budget leave");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) {
            ++Accepted; xrediswatch* Watch = watchStartResult(Future)->Watch; testRequire(Watch != NULL, "accepted budget WATCH failed");
            redisWatchPeerReady(Client); xredisWatchAbort(Watch); xfuture* Closed = NULL;
            testRequire(watchClosed(Watch, &Closed)->Reusable, "budget WATCH cleanup");
            xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
        } else {
            ++Rejected; testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "budget failure changed output/claim");
            xrtClearError();
        }
        watchBudgetDrained(Budget);
        xrtAllocationBudgetFree(Budget); watchOrdinary(Client); redisWatchPeerReady(Client);
    }
    testRequire(Accepted && Rejected, "watch quota windows missed success/failure"); redisTransactionPeerStop(&Peer.Tx);
}
typedef struct watchworkerbudget { xallocationbudget* Budget; xallocationscope Scope; xatomic32 Done; bool Enter; } watchworkerbudget;
static void watchWorkerBudget(xnetworker* Worker, ptr Data)
{
    (void)Worker; watchworkerbudget* Budget = Data;
    testRequire(Budget->Enter ? xrtAllocationBudgetEnter(Budget->Budget, &Budget->Scope) : xrtAllocationBudgetLeave(&Budget->Scope), "watch worker budget scope");
    xrtAtomic32Store(&Budget->Done, 1, XMEMORY_RELEASE);
}
static void watchWorkerMemory(void)
{
    for (unsigned Mode = 0; Mode < 3; ++Mode) {
        rediswatchpeer Peer; redisWatchPeerStart(&Peer, Mode == 0 ? W_HOLD_START : Mode == 1 ? W_HOLD_CLEANUP : W_NORMAL,
            Mode == 2 ? TX_HOLD_EXEC : TX_NORMAL, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
        xfuture* Started = watchBegin(Client, NULL), *Commit = NULL, *Closed = NULL; xrediswatch* Watch = NULL;
        if (Mode == 0) redisPeerWait(&Peer.Starts, 1);
        else {
            Watch = watchStartResult(Started)->Watch; testRequire(Watch != NULL, "worker OOM ready scope"); redisWatchPeerReady(Client);
            if (Mode == 1) { xredisWatchAbort(Watch); redisPeerWait(&Peer.Cleanups, 1); }
            else {
                testRequire(xredisWatchCommitAsync(Watch, NULL, 0, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED, "worker OOM commit");
                redisPeerWait(&Peer.Tx.Execs, 1);
            }
        }
        watchworkerbudget Budget = {0}; Budget.Enter = true;
        testRequire(xrtAllocationBudgetCreate(1, &Budget.Budget) && xrtNetEnginePost(Peer.Tx.Peer->Engine, 0, watchWorkerBudget, &Budget), "watch worker quota post");
        redisPeerWait(&Budget.Done, 1); redisPeerReleaseBlocked(Peer.Tx.Peer);
        if (!Mode) {
            testRequire(watchStartResult(Started)->Request.Status == XREDIS_REQUEST_MEMORY && watchStartResult(Started)->Watch, "WATCH OOM typed failure");
            Watch = watchStartResult(Started)->Watch;
        }
        testRequire(!watchClosed(Watch, &Closed)->Reusable, "OOM reused WATCH connection");
        if (Commit) {
            const xredistransactionresult* Result = (const xredistransactionresult*)watchRequest(Commit);
            testRequire(Result->Request.Status == XREDIS_REQUEST_MEMORY && Result->Execution == XREDIS_TRANSACTION_MAY_HAVE_EXECUTED, "WATCH OOM execution certainty");
        }
        xfuture* NativeClosed = xredisClientClosed(Client); testRequire(xrtFutureWaitFor(NativeClosed, 10000) == XWAIT_OK, "WATCH OOM native retirement");
        Budget.Enter = false; xrtAtomic32Store(&Budget.Done, 0, XMEMORY_RELAXED);
        testRequire(xrtNetEnginePost(Peer.Tx.Peer->Engine, 0, watchWorkerBudget, &Budget), "watch worker quota leave"); redisPeerWait(&Budget.Done, 1);
        xrtFutureDestroy(Started); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed); xrtFutureDestroy(NativeClosed);
        redisTransactionPeerStop(&Peer.Tx);
        xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xrtAllocationBudgetInfo(Budget.Budget, &Info) && !Info.LiveCount && !Info.LiveBytes, "WATCH worker OOM allocation leak"); xrtAllocationBudgetFree(Budget.Budget);
    }
}
typedef struct watchproducer { xrediswatch* Watch; xatomic32 *Ready, *Go, *Commits; unsigned Index; } watchproducer;
static int32 watchProduce(ptr Data)
{
    watchproducer* Producer = Data; xrtAtomic32FetchAdd(Producer->Ready, 1, XMEMORY_RELEASE); redisPeerWait(Producer->Go, 1);
    xfuture* Future = (xfuture*)(uintptr_t)1; xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("race\0binary")};
    xredisadmission Admission = Producer->Index < 2 ? xredisWatchCommitAsync(Producer->Watch, NULL, 0, 5000, &Future) :
        xredisWatchCallAsync(Producer->Watch, Args, 2, 5000, &Future);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        testRequire(watchRequest(Future)->Status == XREDIS_REQUEST_OK, "concurrent WATCH request correlation");
        if (Producer->Index < 2) xrtAtomic32FetchAdd(Producer->Commits, 1, XMEMORY_RELEASE);
        xrtFutureDestroy(Future);
    } else testRequire(Admission == XREDIS_ADMISSION_CLOSED && Future == (xfuture*)(uintptr_t)1, "race admitted late/duplicate commit");
    return 0;
}
static void watchConcurrent(void)
{
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    for (unsigned Round = 0; Round < 16; ++Round) {
        xfuture* Started = watchBegin(Client, NULL); xrediswatch* Watch = watchStartResult(Started)->Watch; testRequire(Watch != NULL, "race scope readiness");
        xatomic32 Ready, Go, Commits; xrtAtomic32Init(&Ready, 0); xrtAtomic32Init(&Go, 0); xrtAtomic32Init(&Commits, 0);
        watchproducer Producers[8]; xthread* Threads[8];
        for (unsigned Index = 0; Index < 8; ++Index) {
            Producers[Index] = (watchproducer){Watch, &Ready, &Go, &Commits, Index};
            Threads[Index] = xrtThreadCreate(watchProduce, &Producers[Index], 0); testRequire(Threads[Index] != NULL, "WATCH producer thread");
        }
        redisPeerWait(&Ready, 8); xrtAtomic32Store(&Go, 1, XMEMORY_RELEASE);
        for (unsigned Index = 0; Index < 8; ++Index) {
            testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && !xrtThreadExitCode(Threads[Index]), "WATCH producer join"); xrtThreadDestroy(Threads[Index]);
        }
        testRequire(xrtAtomic32Load(&Commits, XMEMORY_ACQUIRE) == 1, "concurrent WATCH accepted multiple commits");
        xfuture* Closed = NULL; testRequire(watchClosed(Watch, &Closed)->Reusable, "race scope retirement");
        xrtFutureDestroy(Started); xrtFutureDestroy(Closed);
    }
    redisTransactionPeerStop(&Peer.Tx);
}
static void watchObserverLifetime(void)
{
    for (unsigned Pending = 0; Pending < 2; ++Pending) {
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, Pending ? W_HOLD_START : W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xrediswatchconfig Config; xredisWatchConfigInit(&Config); Config.Timeout = 50;
    xfuture* Started = watchBegin(Client, &Config);
    if (!Pending) testRequire(watchStartResult(Started)->Watch != NULL, "dropped observer scope");
    else redisPeerWait(&Peer.Starts, 1);
    xrtFutureDestroy(Started);
    if (Pending) redisPeerReleaseBlocked(Peer.Tx.Peer);
    double End = xrtTimer() + 5; xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
    xredisadmission Admission;
    while ((Admission = xredisClientCallAsync(Client, &Ping, 1, 5000, &Future)) == XREDIS_ADMISSION_OVERLOADED) {
        testRequire(xrtTimer() < End, "dropped WATCH observer leaked claim/engine"); xrtThreadYield();
    }
    testRequire(Admission == XREDIS_ADMISSION_ACCEPTED && watchRequest(Future)->Status == XREDIS_REQUEST_OK &&
        xrtAtomic32Load(&Peer.Cleanups, XMEMORY_ACQUIRE) == 1, "dropped observer failed bounded UNWATCH");
    xrtFutureDestroy(Future); redisTransactionPeerStop(&Peer.Tx);
    }
}
typedef struct watchreentry { xredisclient* Client; xrediswatch* Watch; xatomic32 Done; } watchreentry;
static void watchWorkerReentry(xnetworker* Worker, ptr Data)
{
    (void)Worker; watchreentry* State = Data; xfuture* Future = (xfuture*)(uintptr_t)1; xbytesview Key = XRT_BYTES_LITERAL("key");
    testRequire(xredisClientWatch(State->Client, &Key, 1, NULL, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "sync WATCH blocked an engine worker");
    xrtClearError();
    testRequire(xredisWatchCall(State->Watch, &Key, 1, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "sync scoped call blocked an engine worker");
    xrtClearError();
    testRequire(xredisWatchCommit(State->Watch, NULL, 0, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "sync scoped commit blocked an engine worker");
    xrtClearError(); xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}
static void watchAdmission(void)
{
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xbytesview Hold = XRT_BYTES_LITERAL("HOLD"), Key = XRT_BYTES_LITERAL("key"); xfuture* Held = NULL;
    uint32 Before = xrtAtomic32Load(&Peer.Tx.Peer->Commands, XMEMORY_ACQUIRE);
    testRequire(xredisClientCallAsync(Client, &Hold, 1, 5000, &Held) == XREDIS_ADMISSION_ACCEPTED, "WATCH idle precondition holder");
    redisPeerWait(&Peer.Tx.Peer->Commands, Before + 1u); xfuture* Future = (xfuture*)(uintptr_t)1;
    testRequire(xredisClientWatchAsync(Client, &Key, 1, NULL, &Future) == XREDIS_ADMISSION_OVERLOADED && Future == (xfuture*)(uintptr_t)1,
        "WATCH claimed a busy connection");
    redisPeerReleaseBlocked(Peer.Tx.Peer); testRequire(watchRequest(Held)->Status == XREDIS_REQUEST_OK, "WATCH preceding request");
    xrtFutureDestroy(Held); redisWatchPeerReady(Client);
    testRequire(xredisClientWatchAsync(Client, &Key, 1, NULL, (xfuture**)Client) == XREDIS_ADMISSION_ERROR, "WATCH output overlapped client storage"); xrtClearError();
    xbytesview Alias = {(cbytes)&Future, sizeof(Future)};
    testRequire(xredisClientWatchAsync(Client, &Alias, 1, NULL, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
        "WATCH output overlapped key payload"); xrtClearError();
    xfuture* Started = watchBegin(Client, NULL); xrediswatch* Watch = watchStartResult(Started)->Watch; testRequire(Watch != NULL, "reentry scope");
    testRequire(xredisWatchCallAsync(Watch, &Key, 1, 5000, (xfuture**)Watch) == XREDIS_ADMISSION_ERROR &&
        xredisWatchCommitAsync(Watch, NULL, 0, 5000, (xfuture**)Watch) == XREDIS_ADMISSION_ERROR, "scoped output overlapped session storage"); xrtClearError();
    watchreentry State = {0}; State.Client = Client; State.Watch = Watch;
    testRequire(xrtNetEnginePost(Peer.Tx.Peer->Engine, 1, watchWorkerReentry, &State), "WATCH different-worker reentry post"); redisPeerWait(&State.Done, 1);
    xredisWatchAbort(Watch); xfuture* Closed = NULL; testRequire(watchClosed(Watch, &Closed)->Reusable, "reentry sealed or changed scope");
    redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(Closed);
    redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    testRequire(xredisClientConnect(Peer.Tx.Peer->Engine, &Peer.Tx.Peer->Address, 0, NULL, &Client), "raw client admission fixture");
    Peer.Tx.Client = Client; redisWatchPeerReady(Client);
    testRequire(xredisClientWatchAsync(Client, &Key, 1, NULL, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1,
        "WATCH accepted unmanaged client"); xrtClearError(); redisTransactionPeerStop(&Peer.Tx);
}
int main(void)
{
    watchBasic(); watchFailures(); watchAbandon(); watchBounds(); watchMemory(); watchWorkerMemory(); watchConcurrent(); watchObserverLifetime(); watchAdmission();
    puts("[PASS] xredis WATCH: exclusive ready/idle claim; binary reads; sealed commit; conflict/queue/runtime errors; abort/deadline/cleanup/physical retirement"); return 0;
}
