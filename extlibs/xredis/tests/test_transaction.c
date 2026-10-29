#include "test.h"
#include "support/transaction_peer.h"

static const xredistransactionresult* transactionWait(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "transaction future wait");
    xfutureresult Result; testRequire(xrtFutureResult(Future, &Result) && Result.State == XFUTURE_RESOLVED && Result.Value,
        "transaction typed outcome"); return Result.Value;
}
static void transactionReady(xredisclient* Client)
{
    xfuture* Ready = xredisClientReady(Client); testRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK &&
        ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "transaction managed readiness"); xrtFutureDestroy(Ready);
}
static xfuture* transactionOrdinary(xredisclient* Client, cstr Name)
{
    xbytesview Word = {(cbytes)Name, strlen(Name)}; xfuture* Future = NULL;
    testRequire(xredisClientCallAsync(Client, &Word, 1, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "transaction ordinary neighbor"); return Future;
}
static void transactionOrdinaryOK(xfuture* Future)
{
    testRequire(xrtFutureWaitFor(Future, 10000) == XWAIT_OK &&
        ((const xredisrequestresult*)xrtFutureValue(Future))->Status == XREDIS_REQUEST_OK, "transaction ordinary correlation"); xrtFutureDestroy(Future);
}
static xfuture* transactionSubmit(xredisclient* Client, const xredistransactioncommand* Commands, size_t Count, int64 Timeout)
{
    xfuture* Future = NULL;
    testRequire(xredisClientTransactionAsync(Client, Commands, Count, NULL, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "transaction admission"); return Future;
}
static void transactionIdle(xredisclient* Client)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredisclientinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisClientInfo(Client, &Info), "transaction idle snapshot");
        if (!Info.Requests && !Info.CommandBytes) return;
        testRequire(xrtTimer() < End, "transaction physical quota retirement"); xrtThreadYield();
    }
}
static void transactionBasic(void)
{
    for (unsigned Secure = 0; Secure < 2; ++Secure) {
        redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, Secure != 0);
        xredisclientconfig Config; xredisClientConfigInit(&Config); Config.WorkRequests = 1; Config.ReadBytes = 2; Config.ReadFrames = 1;
        xredisclient* Client = redisTransactionPeerConnect(&Peer, &Config);
        xfuture* Before = transactionOrdinary(Client, "BEFORE");
        unsigned char Payload[] = {'a', 0, '\r', '\n', 0xff};
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}}, Body = XRT_BYTES_LITERAL("BODY");
        xredistransactioncommand Commands[] = {{Echo, 2}, {&Body, 1}, {Echo, 2}};
        xfuture* Future = transactionSubmit(Client, Commands, 3, 5000); memset(Payload, 0x5a, sizeof(Payload)); memset(Commands, 0, sizeof(Commands));
        xfuture* After = transactionOrdinary(Client, "AFTER"); const xredistransactionresult* Result = transactionWait(Future);
        testRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Request.Delivery == XREDIS_REPLIED &&
            Result->Execution == XREDIS_TRANSACTION_EXECUTED && Result->Queued == 3 && !Result->QueueErrors && !Result->QueueError,
            "transaction execution/queue result");
        xredisreplyvalue Root, Child; testRequire(xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 3 &&
            xredisReplyGet(Result->Request.Reply, Root.FirstChild, &Child) && Child.Data.Size == 5 && !memcmp(Child.Data.Data, "a\0\r\n\xff", 5), "transaction binary input snapshot");
        transactionOrdinaryOK(Before); transactionOrdinaryOK(After);
        xfuture* Empty = NULL; testRequire(xredisClientTransaction(Client, NULL, 0, NULL, 5000, &Empty) == XREDIS_ADMISSION_ACCEPTED &&
            transactionWait(Empty)->Execution == XREDIS_TRANSACTION_EXECUTED, "empty sync transaction"); xrtFutureDestroy(Empty);
        testRequire(xrtAtomic32Load(&Peer.Execs, XMEMORY_ACQUIRE) == 2 && !xrtAtomic32Load(&Peer.Outside, XMEMORY_ACQUIRE), "transaction crossed ordinary FIFO");
        redisTransactionPeerStop(&Peer);
        testRequire(transactionWait(Future)->Request.Reply == Result->Request.Reply, "transaction result retained engine/client"); xrtFutureDestroy(Future);
    }
}
static void transactionErrors(void)
{
    const unsigned Modes[] = {TX_MULTI_ERROR, TX_BAD_MULTI, TX_BAD_QUEUE, TX_EXEC_ERROR, TX_BAD_COUNT,
        TX_ABORT, TX_DISCARD_ERROR, TX_RUNTIME_ERROR, TX_PUSH, TX_SYNC, TX_DROP_MULTI, TX_DROP_COMMAND, TX_DROP_EXEC, TX_EXEC_UNKNOWN};
    for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index) {
        unsigned Mode = Modes[Index]; redistransactionpeer Peer; redisTransactionPeerStart(&Peer, Mode, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionReady(Client);
        xbytesview Body = XRT_BYTES_LITERAL("BODY"), Bad = XRT_BYTES_LITERAL("BAD");
        xredistransactioncommand Commands[] = {{&Body, 1}, {Mode == TX_DISCARD_ERROR ? &Bad : &Body, 1}, {&Body, 1}};
        xfuture* Future = transactionSubmit(Client, Commands, 3, 5000), *After = transactionOrdinary(Client, "AFTER");
        const xredistransactionresult* Result = transactionWait(Future);
        xredisrequeststatus Status = Mode == TX_MULTI_ERROR || Mode == TX_EXEC_ERROR || Mode == TX_DISCARD_ERROR || Mode == TX_EXEC_UNKNOWN ? XREDIS_REQUEST_SERVER_ERROR :
            Mode == TX_BAD_MULTI || Mode == TX_BAD_QUEUE || Mode == TX_BAD_COUNT ? XREDIS_REQUEST_PROTOCOL :
            Mode == TX_DROP_MULTI || Mode == TX_DROP_COMMAND || Mode == TX_DROP_EXEC ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_OK;
        xredistransactionexecution Execution = Mode == TX_EXEC_ERROR || Mode == TX_ABORT ? XREDIS_TRANSACTION_ABORTED :
            Mode == TX_BAD_COUNT || Mode == TX_DROP_EXEC || Mode == TX_EXEC_UNKNOWN ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED :
            Mode == TX_RUNTIME_ERROR || Mode == TX_PUSH || Mode == TX_SYNC ? XREDIS_TRANSACTION_EXECUTED : XREDIS_TRANSACTION_NOT_EXECUTED;
        if (Result->Request.Status != Status || Result->Execution != Execution)
            fprintf(stderr, "mode=%u status=%d/%d execution=%d/%d\n", Mode, Result->Request.Status, Status, Result->Execution, Execution);
        testRequire(Result->Request.Status == Status && Result->Execution == Execution, "transaction typed failure/execution certainty");
        if (Mode == TX_MULTI_ERROR) testRequire(!xrtAtomic32Load(&Peer.Bodies, XMEMORY_ACQUIRE) &&
            !xrtAtomic32Load(&Peer.Outside, XMEMORY_ACQUIRE), "MULTI rejection executed business outside transaction");
        if (Mode == TX_DISCARD_ERROR) testRequire(Result->QueueError && Result->QueueErrors == 1 &&
            xrtAtomic32Load(&Peer.Discards, XMEMORY_ACQUIRE) == 1 && !xrtAtomic32Load(&Peer.Execs, XMEMORY_ACQUIRE), "queue failure failed to discard");
        if (Mode == TX_PUSH) for (unsigned Push = 0; Push < 3; ++Push) {
            xredisreply* Reply = NULL; testRequire(xredisClientTakePush(Client, &Reply), "push shifted transaction reply correlation"); xredisReplyRelease(Reply);
        }
        if (Mode == TX_SYNC) testRequire(xrtAtomic32Load(&Peer.SyncChecked, XMEMORY_ACQUIRE), "sync worker guard not exercised");
        testRequire(xrtFutureWaitFor(After, 10000) == XWAIT_OK, "transaction neighbor terminal outcome");
        bool Usable = Mode == TX_MULTI_ERROR || Mode == TX_EXEC_ERROR || Mode == TX_ABORT || Mode == TX_RUNTIME_ERROR || Mode == TX_PUSH || Mode == TX_SYNC;
        testRequire((((const xredisrequestresult*)xrtFutureValue(After))->Status == XREDIS_REQUEST_OK) == Usable, "dirty transaction connection reused");
        xrtFutureDestroy(After); redisTransactionPeerStop(&Peer); xrtFutureDestroy(Future);
    }
    /* Queue errors are counted individually, with only the first owned frame. */
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); xbytesview Bad = XRT_BYTES_LITERAL("BAD"), Body = XRT_BYTES_LITERAL("BODY");
    xredistransactioncommand Commands[] = {{&Bad, 1}, {&Body, 1}, {&Bad, 1}};
    xfuture* Future = transactionSubmit(Client, Commands, 3, 5000); const xredistransactionresult* Result = transactionWait(Future);
    testRequire(Result->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Result->Execution == XREDIS_TRANSACTION_NOT_EXECUTED &&
        Result->QueueErrors == 2 && Result->Queued == 1 && Result->QueueError && !Result->Request.Reply &&
        xrtAtomic32Load(&Peer.Discards, XMEMORY_ACQUIRE) == 1 && !xrtAtomic32Load(&Peer.Execs, XMEMORY_ACQUIRE), "queue error accumulation/cleanup");
    transactionOrdinaryOK(transactionOrdinary(Client, "AFTER")); redisTransactionPeerStop(&Peer); xrtFutureDestroy(Future);
}
static void transactionAbandon(void)
{
    const unsigned Modes[] = {TX_HOLD_MULTI, TX_HOLD_QUEUE, TX_HOLD_EXEC, TX_HOLD_DISCARD};
    for (unsigned Cancel = 0; Cancel < 2; ++Cancel) for (size_t Index = 0; Index < 4; ++Index) {
        unsigned Mode = Modes[Index]; redistransactionpeer Peer; redisTransactionPeerStart(&Peer, Mode, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionReady(Client);
        xbytesview Body = XRT_BYTES_LITERAL("BODY"), Bad = XRT_BYTES_LITERAL("BAD");
        xredistransactioncommand Commands[] = {{&Body, 1}, {Mode == TX_HOLD_DISCARD ? &Bad : &Body, 1}, {&Body, 1}};
        xfuture* Future = transactionSubmit(Client, Commands, 3, Cancel ? 5000 : 300);
        redisPeerWait(Mode == TX_HOLD_MULTI ? &Peer.Multis : Mode == TX_HOLD_QUEUE ? &Peer.Bodies : Mode == TX_HOLD_EXEC ? &Peer.Execs : &Peer.Discards,
            Mode == TX_HOLD_QUEUE ? 3 : 1);
        if (Cancel) testRequire(xrtFutureCancel(Future), "transaction cooperative cancel");
        const xredistransactionresult* Result = transactionWait(Future);
        testRequire(Result->Request.Status == (Cancel ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT) &&
            Result->Execution == (Mode == TX_HOLD_EXEC ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED : XREDIS_TRANSACTION_NOT_EXECUTED) &&
            Result->Request.Delivery == (Mode == TX_HOLD_EXEC ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT), "abandon execution certainty");
        xfuture* After = transactionOrdinary(Client, "AFTER"); redisPeerReleaseBlocked(Peer.Peer); transactionOrdinaryOK(After);
        testRequire(xrtAtomic32Load(&Peer.Execs, XMEMORY_ACQUIRE) == (Mode == TX_HOLD_EXEC ? 1u : 0u) &&
            xrtAtomic32Load(&Peer.Discards, XMEMORY_ACQUIRE) == (Mode == TX_HOLD_EXEC ? 0u : 1u), "abandon sent EXEC or lost cleanup FIFO");
        testRequire(Result->Execution == (Mode == TX_HOLD_EXEC ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED : XREDIS_TRANSACTION_NOT_EXECUTED), "late reply mutated published certainty");
        redisTransactionPeerStop(&Peer); xrtFutureDestroy(Future);
    }
}
static void transactionBounds(void)
{
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionReady(Client);
    xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Command = {&Body, 1};
    xredistransactionconfig Config; xredisClientTransactionConfigInit(&Config);
    for (unsigned Mode = 0; Mode < 8; ++Mode) {
        xredistransactionconfig Bad = Config; const xredistransactioncommand* Commands = &Command; size_t Count = 1; int64 Timeout = 1000;
        if (Mode == 0) Bad.Size--;
        if (Mode == 1) Bad.MaxCommands = 0;
        if (Mode == 2) Bad.MaxCommands = XREDIS_TRANSACTION_COMMANDS_MAX + 1u;
        if (Mode == 3) Count = SIZE_MAX;
        if (Mode == 4) Commands = NULL;
        if (Mode == 5) Commands = (const xredistransactioncommand*)(uintptr_t)(UINTPTR_MAX - 2u);
        if (Mode == 6) Timeout = -2;
        if (Mode == 7) Command.Count = 0;
        xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xredisClientTransactionAsync(Client, Commands, Count, &Bad, Timeout, &Future) == XREDIS_ADMISSION_ERROR &&
            Future == (xfuture*)(uintptr_t)1, "invalid transaction changed output/admitted"); xrtClearError(); Command.Count = 1;
    }
    const cstr Controls[] = {"MULTI", "EXEC", "DISCARD", "WATCH", "UNWATCH", "RESET"};
    for (size_t Index = 0; Index < 6; ++Index) {
        xbytesview Word = {(cbytes)Controls[Index], strlen(Controls[Index])}; xredistransactioncommand Control = {&Word, 1}; xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xredisClientCallAsync(Client, &Word, 1, 1000, &Future) == XREDIS_ADMISSION_ERROR &&
            xredisClientTransactionAsync(Client, &Control, 1, NULL, 1000, &Future) == XREDIS_ADMISSION_ERROR &&
            Future == (xfuture*)(uintptr_t)1, "manual managed or nested control admitted"); xrtClearError();
    }
    xredistransactionconfig Snapshot = Config;
    testRequire(xredisClientTransactionAsync(Client, &Command, 1, &Config, 1000, (xfuture**)&Config.MaxCommands) == XREDIS_ADMISSION_ERROR &&
        !memcmp(&Config, &Snapshot, sizeof(Config)), "transaction descriptor/output overlap"); xrtClearError();
    unsigned Failed = 0, Accepted = 0;
    for (size_t Limit = 1; Limit < 16384; Limit += 97) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; testRequire(xrtAllocationBudgetCreate(Limit, &Budget) &&
            xrtAllocationBudgetEnter(Budget, &Scope), "transaction allocation budget");
        xfuture* Future = (xfuture*)(uintptr_t)1;
        xredisadmission Admission = xredisClientTransactionAsync(Client, &Command, 1, NULL, 5000, &Future);
        testRequire(xrtAllocationBudgetLeave(&Scope), "transaction allocation scope exit");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) { ++Accepted; testRequire(transactionWait(Future)->Execution == XREDIS_TRANSACTION_EXECUTED, "quota survivor transaction"); xrtFutureDestroy(Future); }
        else { ++Failed; testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "transaction quota mutated output"); }
        xrtClearError(); transactionIdle(Client); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xrtAllocationBudgetInfo(Budget, &Info) && !Info.LiveCount && !Info.LiveBytes, "transaction admission quota rollback"); xrtAllocationBudgetFree(Budget);
    }
    testRequire(Failed && Accepted, "transaction quota sweep misses failure/success"); redisTransactionPeerStop(&Peer);
}

static void transactionQueueAndDrain(void)
{
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_HOLD_BEFORE, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionReady(Client);
    xfuture* Before = transactionOrdinary(Client, "BEFORE"); redisPeerWait(&Peer.Peer->Commands, 2);
    xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Command = {&Body, 1};
    xfuture* Future = transactionSubmit(Client, &Command, 1, 30); const xredistransactionresult* Result = transactionWait(Future);
    testRequire(Result->Request.Status == XREDIS_REQUEST_TIMEOUT && Result->Request.Delivery == XREDIS_NOT_SENT &&
        Result->Execution == XREDIS_TRANSACTION_NOT_EXECUTED && !xrtAtomic32Load(&Peer.Multis, XMEMORY_ACQUIRE), "queue wait restarted transaction deadline");
    redisPeerReleaseBlocked(Peer.Peer); transactionOrdinaryOK(Before); redisTransactionPeerStop(&Peer); xrtFutureDestroy(Future);
    redisTransactionPeerStart(&Peer, TX_HOLD_MULTI, false);
    xredisclientconfig Config; xredisClientConfigInit(&Config); Config.DrainTimeout = 30;
    Client = redisTransactionPeerConnect(&Peer, &Config); transactionReady(Client);
    Future = transactionSubmit(Client, &Command, 1, 5000); redisPeerWait(&Peer.Multis, 1);
    testRequire(xrtFutureCancel(Future), "undrained transaction cancel"); Result = transactionWait(Future);
    xfuture* After = transactionOrdinary(Client, "AFTER"), *Closed = xredisClientClosed(Client);
    testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && xrtFutureWaitFor(After, 10000) == XWAIT_OK &&
        ((const xredisrequestresult*)xrtFutureValue(After))->Status == XREDIS_REQUEST_DISCONNECTED &&
        Result->Request.Status == XREDIS_REQUEST_CANCELLED && Result->Execution == XREDIS_TRANSACTION_NOT_EXECUTED,
        "drain grace falsely expired another request or changed execution certainty");
    redisTransactionPeerStop(&Peer); xrtFutureDestroy(Closed); xrtFutureDestroy(After); xrtFutureDestroy(Future);
    redisTransactionPeerStart(&Peer, TX_NORMAL, false); xredisClientConfigInit(&Config); Config.MaxCommandBytes = 1;
    Client = redisTransactionPeerConnect(&Peer, &Config); transactionReady(Client); Future = (xfuture*)(uintptr_t)1;
    testRequire(xredisClientTransactionAsync(Client, &Command, 1, NULL, 5000, &Future) == XREDIS_ADMISSION_OVERLOADED &&
        Future == (xfuture*)(uintptr_t)1 && !xrtAtomic32Load(&Peer.Multis, XMEMORY_ACQUIRE), "transaction wire/metadata cap admitted or changed output");
    redisTransactionPeerStop(&Peer);
}

typedef struct transactionproducer { xredisclient* Client; } transactionproducer;
static int32 transactionProduce(ptr Data)
{
    transactionproducer* Producer = Data; xbytesview Body = XRT_BYTES_LITERAL("BODY"); xredistransactioncommand Commands[] = {{&Body, 1}, {&Body, 1}};
    for (unsigned Index = 0; Index < 8; ++Index) {
        xfuture* Future = NULL;
        testRequire(xredisClientTransaction(Producer->Client, Commands, 2, NULL, 10000, &Future) == XREDIS_ADMISSION_ACCEPTED &&
            transactionWait(Future)->Execution == XREDIS_TRANSACTION_EXECUTED, "concurrent transaction boundary"); xrtFutureDestroy(Future);
    }
    return 0;
}
static void transactionConcurrent(void)
{
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionproducer Producer = {Client}; xthread* Threads[4];
    for (unsigned Index = 0; Index < 4; ++Index) { Threads[Index] = xrtThreadCreate(transactionProduce, &Producer, 0); testRequire(Threads[Index] != NULL, "transaction producer thread"); }
    for (unsigned Index = 0; Index < 4; ++Index) {
        testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && !xrtThreadExitCode(Threads[Index]), "transaction producer join"); xrtThreadDestroy(Threads[Index]);
    }
    testRequire(xrtAtomic32Load(&Peer.Multis, XMEMORY_ACQUIRE) == 32 && xrtAtomic32Load(&Peer.Execs, XMEMORY_ACQUIRE) == 32 &&
        xrtAtomic32Load(&Peer.Effects, XMEMORY_ACQUIRE) == 64 && !xrtAtomic32Load(&Peer.Outside, XMEMORY_ACQUIRE), "concurrent transactions interleaved or replayed");
    redisTransactionPeerStop(&Peer);
}

typedef struct transactionworkerbudget { xallocationbudget* Budget; xallocationscope Scope; xatomic32 Done; bool Enter; } transactionworkerbudget;
static void transactionWorkerBudget(xnetworker* Worker, ptr Data)
{
    (void)Worker; transactionworkerbudget* State = Data;
    testRequire(State->Enter ? xrtAllocationBudgetEnter(State->Budget, &State->Scope) : xrtAllocationBudgetLeave(&State->Scope), "transaction worker quota scope");
    xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}
static void transactionWorkerMemory(void)
{
    const unsigned Modes[] = {TX_HOLD_QUEUE, TX_HOLD_EXEC, TX_HOLD_DISCARD};
    for (unsigned Index = 0; Index < 3; ++Index) {
        unsigned Mode = Modes[Index]; redistransactionpeer Peer; redisTransactionPeerStart(&Peer, Mode, false);
        xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL); transactionReady(Client);
        xbytesview Body = XRT_BYTES_LITERAL("BODY"), Bad = XRT_BYTES_LITERAL("BAD");
        xredistransactioncommand Commands[] = {{&Body, 1}, {Mode == TX_HOLD_DISCARD ? &Bad : &Body, 1}, {&Body, 1}};
        xfuture* Future = transactionSubmit(Client, Commands, 3, 5000);
        redisPeerWait(Mode == TX_HOLD_QUEUE ? &Peer.Bodies : Mode == TX_HOLD_EXEC ? &Peer.Execs : &Peer.Discards, Mode == TX_HOLD_QUEUE ? 3 : 1);
        transactionworkerbudget Budget = {0}; Budget.Enter = true;
        testRequire(xrtAllocationBudgetCreate(1, &Budget.Budget) && xrtNetEnginePost(Peer.Peer->Engine, 0, transactionWorkerBudget, &Budget), "worker OOM gate");
        redisPeerWait(&Budget.Done, 1); redisPeerReleaseBlocked(Peer.Peer);
        const xredistransactionresult* Result = transactionWait(Future);
        testRequire(Result->Request.Status == XREDIS_REQUEST_MEMORY && Result->Execution ==
            (Mode == TX_HOLD_EXEC ? XREDIS_TRANSACTION_MAY_HAVE_EXECUTED : XREDIS_TRANSACTION_NOT_EXECUTED), "worker OOM lost typed execution certainty");
        xfuture* Closed = xredisClientClosed(Client); testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "worker OOM physical close");
        Budget.Enter = false; xrtAtomic32Store(&Budget.Done, 0, XMEMORY_RELAXED);
        testRequire(xrtNetEnginePost(Peer.Peer->Engine, 0, transactionWorkerBudget, &Budget), "worker quota leave post"); redisPeerWait(&Budget.Done, 1);
        redisTransactionPeerStop(&Peer); xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
        xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xrtAllocationBudgetInfo(Budget.Budget, &Info) && !Info.LiveCount && !Info.LiveBytes, "worker OOM transaction/queue-error leak");
        xrtAllocationBudgetFree(Budget.Budget);
    }
}
int main(void)
{
    transactionBasic(); transactionErrors(); transactionAbandon(); transactionBounds(); transactionQueueAndDrain(); transactionConcurrent(); transactionWorkerMemory();
    puts("[PASS] xredis atomic transactions: staged MULTI/QUEUED/EXEC/DISCARD; exclusive FIFO; typed uncertainty; TLS; failure/cancel/quota/lifetimes"); return 0;
}
