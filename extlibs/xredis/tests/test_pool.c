#include "test.h"
/* Force actual timer allocation failures instead of satisfying them from
 * nodes recycled by managed initialization. Other fixtures keep defaults. */
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/client_init_peer.h"
#include "support/pool_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "support/tls_peer.h"
#endif
static const redisinitpeerstep poolHello[] = {
    {{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3")}, 2, XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), false, false}
};

static const xredisleaseresult* poolWait(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "pool acquisition wait");
    const xredisleaseresult* Result = xrtFutureValue(Future); testRequire(Result != NULL, "typed acquisition"); return Result;
}
static const xredisrequestresult* poolCallWait(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "pooled request wait");
    const xredisrequestresult* Result = xrtFutureValue(Future); testRequire(Result != NULL, "typed pooled request"); return Result;
}
static void poolReady(xredisclient* Client)
{
    xfuture* Ready = xredisClientReady(Client); testRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK &&
        ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "pool member readiness"); xrtFutureDestroy(Ready);
}
static xredispool* poolCreate(redisinitpeer* Peer, const xredispoolconfig* Config, xredisclient** Client)
{
    xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
    testRequire(xredisClientConnectInit(Peer->Peer.Engine, &Peer->Peer.Address, 0, NULL, &Init, Client), "pool member connect"); poolReady(*Client);
    xredispool* Pool = NULL; testRequire(xredisPoolCreate(Peer->Peer.Engine, 0, Client, 1, Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "pool creation"); return Pool;
}
static void poolStop(redisinitpeer* Peer, xredispool* Pool, xredisclient* Client)
{
    xfuture* Closed = xredisPoolClosed(Pool);
    testRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "pool physical close");
    xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
    testRequire(xredisPoolInfo(Pool, &Info) && Info.State == XREDIS_POOL_CLOSED && !Info.Leased && !Info.Pending && !Info.Requests &&
        !Info.CommandBytes && Info.Retired == Info.Members, "pool close accounting");
    xredisClientRelease(Client); xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisPeerStop(&Peer->Peer);
}
static xfuture* poolAcquire(xredispool* Pool, int64 Timeout)
{
    xfuture* Future = NULL; testRequire(xredisPoolAcquireAsync(Pool, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "acquire admission"); return Future;
}
static xfuture* poolReturn(xredislease* Lease, xredisrequeststatus Reason, bool Reusable)
{
    xfuture* Closed = xredisLeaseClosed(Lease); testRequire(xredisLeaseReturn(Lease) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "lease physical return");
    const xredisleaseclosedresult* Result = xrtFutureValue(Closed);
    testRequire(Result && Result->Reason == Reason && Result->Reusable == Reusable, "lease return outcome"); return Closed;
}
static void poolBasic(void)
{
    redisinitpeer Peer; redisInitPeerStart(&Peer, poolHello, 1); xredisclient* Client = NULL;
    xredispool* Pool = poolCreate(&Peer, NULL, &Client); xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease;
    testRequire(Lease && xredisLeaseRetain(Lease), "lease owned handle");
    xfuture* Rejected = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1, "unowned call entered pool");
    unsigned char Payload[] = {'x', 0, '\r', '\n', 0xff}; xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}}; xfuture* Call = NULL;
    testRequire(xredisLeaseCallAsync(Lease, Echo, 2, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED, "leased binary call"); memset(Payload, 0x5a, sizeof(Payload));
    const xredisrequestresult* Result = poolCallWait(Call); xredisreplyvalue Value;
    testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 5 && !memcmp(Value.Data.Data, "x\0\r\n\xff", 5), "leased binary ownership");
    xfuture* Closed = poolReturn(Lease, XREDIS_REQUEST_OK, true);
    testRequire(xredisLeaseCallAsync(Lease, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1, "old lease reused slot generation");
    xfuture* Again = NULL; testRequire(xredisPoolAcquire(Pool, 5000, &Again) == XREDIS_ADMISSION_ACCEPTED && poolWait(Again)->Lease, "sync pool acquisition");
    xfuture* AgainClosed = poolReturn(poolWait(Again)->Lease, XREDIS_REQUEST_OK, true);
    poolStop(&Peer, Pool, Client); testRequire(xredisLeaseReturn(Lease) && xredisLeaseAbort(Lease), "completed metadata retained pool/engine");
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Again); xrtFutureDestroy(Call); xrtFutureDestroy(Closed); xrtFutureDestroy(AgainClosed); xredisLeaseRelease(Lease);
}
static xredispool* poolFixtureCreate(redispoolpeer* Peer, const xredispoolconfig* Config)
{
    xredispool* Pool = NULL; testRequire(xredisPoolCreate(Peer->Peers[0].Engine, 0, Peer->Clients, Peer->Count, Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "multi-member pool creation"); return Pool;
}
static void poolFixtureStop(redispoolpeer* Peer, xredispool* Pool)
{
    xfuture* Closed = xredisPoolClosed(Pool); testRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "multi-member physical pool close");
    xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
    testRequire(xredisPoolInfo(Pool, &Info) && !Info.Requests && !Info.CommandBytes && !Info.Pending && !Info.Leased &&
        Info.Retired == Peer->Count, "multi-member physical accounting");
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisPoolPeerStop(Peer);
}
static void poolCounter(xredispool* Pool, uint64 Requests, uint64 Bytes)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisPoolInfo(Pool, &Info), "pool quota snapshot");
        if (Info.Requests == Requests && Info.CommandBytes == Bytes) return;
        testRequire(xrtTimer() < End, "pool physical quota did not settle"); xrtThreadYield();
    }
}
static void poolFIFO(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.WorkItems = 1; Config.MaxPending = 3;
    xredispool* Pool = poolFixtureCreate(&Peer, &Config); xfuture* First = poolAcquire(Pool, 5000); xredislease* Held = poolWait(First)->Lease;
    testRequire(Held != NULL, "FIFO first lease");
    xfuture* Waiters[3] = {poolAcquire(Pool, 5000), poolAcquire(Pool, 40), poolAcquire(Pool, 5000)};
    xfuture* Rejected = (xfuture*)(uintptr_t)1;
    testRequire(xredisPoolAcquireAsync(Pool, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1, "bounded waiter admission");
    testRequire(poolWait(Waiters[1])->Status == XREDIS_REQUEST_TIMEOUT && !poolWait(Waiters[1])->Lease, "queued original deadline");
    testRequire(xrtFutureState(Waiters[0]) == XFUTURE_PENDING && xrtFutureState(Waiters[2]) == XFUTURE_PENDING, "acquisition bypassed held lease");
    xfuture* FirstClosed = poolReturn(Held, XREDIS_REQUEST_OK, true); xredislease* Next = poolWait(Waiters[0])->Lease;
    testRequire(Next && xrtFutureState(Waiters[2]) == XFUTURE_PENDING, "FIFO order skipped head");
    xfuture* NextClosed = poolReturn(Next, XREDIS_REQUEST_OK, true); xredislease* Last = poolWait(Waiters[2])->Lease;
    testRequire(Last != NULL, "FIFO last acquisition"); xfuture* LastClosed = poolReturn(Last, XREDIS_REQUEST_OK, true);
    xfuture* Due = poolAcquire(Pool, 0); testRequire(poolWait(Due)->Status == XREDIS_REQUEST_TIMEOUT && !poolWait(Due)->Lease, "already-due acquisition granted");
    xrtFutureDestroy(Due); xrtFutureDestroy(FirstClosed); xrtFutureDestroy(NextClosed); xrtFutureDestroy(LastClosed); xrtFutureDestroy(First);
    for (size_t Index = 0; Index < 3; ++Index) xrtFutureDestroy(Waiters[Index]);
    poolFixtureStop(&Peer, Pool);
}
static void poolQuotas(void)
{
    for (unsigned Bytes = 0; Bytes < 2; ++Bytes) {
        redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispoolconfig Config; xredisPoolConfigInit(&Config);
        if (Bytes) Config.MaxCommandBytes = 15; else Config.MaxRequests = 1;
        xredispool* Pool = poolFixtureCreate(&Peer, &Config);
        xfuture *A = poolAcquire(Pool, 5000), *B = poolAcquire(Pool, 5000); xredislease *LA = poolWait(A)->Lease, *LB = poolWait(B)->Lease;
        testRequire(LA && LB && LA != LB, "distinct concurrent member leases");
        xbytesview Hold = XRT_BYTES_LITERAL("HOLD"), Ping = XRT_BYTES_LITERAL("PING"); xfuture* Slow = NULL;
        testRequire(xredisLeaseCallAsync(LA, &Hold, 1, 5000, &Slow) == XREDIS_ADMISSION_ACCEPTED, "quota holder");
        redisPeerWait(&Peer.Peers[0].Commands, 2); poolCounter(Pool, 1, 14);
        xfuture* Rejected = (xfuture*)(uintptr_t)1;
        testRequire(xredisLeaseCallAsync(LB, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1,
            "aggregate request/byte quota admitted");
        testRequire(xrtFutureCancel(Slow) && poolCallWait(Slow)->Status == XREDIS_REQUEST_CANCELLED, "cancel pooled sent request");
        poolCounter(Pool, 1, 14);
        testRequire(xredisLeaseCallAsync(LB, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_OVERLOADED && Rejected == (xfuture*)(uintptr_t)1,
            "logical cancellation released physical group quota");
        redisPeerReleaseBlocked(&Peer.Peers[0]); poolCounter(Pool, 0, 0);
        xfuture* Call = NULL; testRequire(xredisLeaseCall(LB, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && poolCallWait(Call)->Status == XREDIS_REQUEST_OK,
            "group quota was not reclaimed");
        xfuture *CA = poolReturn(LA, XREDIS_REQUEST_OK, true), *CB = poolReturn(LB, XREDIS_REQUEST_OK, true);
        xrtFutureDestroy(CA); xrtFutureDestroy(CB); xrtFutureDestroy(A); xrtFutureDestroy(B); xrtFutureDestroy(Slow); xrtFutureDestroy(Call);
        poolFixtureStop(&Peer, Pool);
    }
}
static void poolAbandon(void)
{
    for (unsigned Mode = 0; Mode < 5; ++Mode) {
        redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispoolconfig Config; xredisPoolConfigInit(&Config);
        Config.ReturnTimeout = 30; if (Mode == 1 || Mode == 2) Config.LeaseTimeout = 100;
        xredispool* Pool = poolFixtureCreate(&Peer, &Config); xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease;
        testRequire(Lease != NULL, "abandon lease"); xfuture* Slow = NULL;
        if (Mode != 1) {
            xbytesview Hold = XRT_BYTES_LITERAL("HOLD"); testRequire(xredisLeaseCallAsync(Lease, &Hold, 1, XRT_WAIT_FOREVER, &Slow) == XREDIS_ADMISSION_ACCEPTED, "abandon blocking call");
            redisPeerWait(&Peer.Peers[0].Commands, 2);
        }
        xfuture* Waiting = poolAcquire(Pool, 5000), *Closed = xredisLeaseClosed(Lease);
        if (Mode == 0) xredisLeaseReturn(Lease);
        if (Mode == 3) xredisLeaseAbort(Lease);
        if (Mode == 4) xredisPoolClose(Pool);
        testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "abandon physical retirement");
        const xredisleaseclosedresult* Result = xrtFutureValue(Closed);
        testRequire(Result && Result->Reason == (Mode == 4 ? XREDIS_REQUEST_CLOSED : Mode == 3 ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT) &&
            Result->Reusable == (Mode == 1), "abandon reuse/reason");
        const xredisleaseresult* Next = poolWait(Waiting);
        if (Mode == 1) {
            testRequire(Next->Lease != NULL, "idle-expired member not reusable"); xfuture* Returned = poolReturn(Next->Lease, XREDIS_REQUEST_OK, true); xrtFutureDestroy(Returned);
        } else testRequire(!Next->Lease && Next->Status == (Mode == 4 ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_DISCONNECTED), "dead member was leased or acquisition stuck");
        if (Slow) testRequire(poolCallWait(Slow)->Status == XREDIS_REQUEST_CLOSED || poolCallWait(Slow)->Status == XREDIS_REQUEST_TIMEOUT,
            "abandoned blocking request status");
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Waiting); xrtFutureDestroy(Closed); xrtFutureDestroy(Slow); poolFixtureStop(&Peer, Pool);
    }
}
static void poolGraceful(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispool* Pool = poolFixtureCreate(&Peer, NULL);
    xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease; testRequire(Lease != NULL, "grace lease");
    xbytesview Hold = XRT_BYTES_LITERAL("HOLD"), Ping = XRT_BYTES_LITERAL("PING"); xfuture* Call = NULL;
    testRequire(xredisLeaseCallAsync(Lease, &Hold, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED, "graceful outstanding call"); redisPeerWait(&Peer.Peers[0].Commands, 2);
    xfuture* Waiting = poolAcquire(Pool, 5000), *Closed = xredisLeaseClosed(Lease);
    testRequire(xredisLeaseReturn(Lease) && xredisLeaseReturn(Lease) && xrtFutureWaitFor(Closed, 20) == XWAIT_TIMEOUT &&
        xrtFutureState(Waiting) == XFUTURE_PENDING, "return released unfinished FIFO slot");
    xfuture* Reject = (xfuture*)(uintptr_t)1;
    testRequire(xredisLeaseCallAsync(Lease, &Ping, 1, 5000, &Reject) == XREDIS_ADMISSION_CLOSED && Reject == (xfuture*)(uintptr_t)1, "return failed to seal admission");
    redisPeerReleaseBlocked(&Peer.Peers[0]); testRequire(poolCallWait(Call)->Status == XREDIS_REQUEST_OK && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK &&
        ((const xredisleaseclosedresult*)xrtFutureValue(Closed))->Reusable, "graceful drain failed reuse");
    xredislease* Next = poolWait(Waiting)->Lease; testRequire(Next != NULL, "graceful follower"); xfuture* Returned = poolReturn(Next, XREDIS_REQUEST_OK, true);
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Waiting); xrtFutureDestroy(Closed); xrtFutureDestroy(Returned); xrtFutureDestroy(Call); poolFixtureStop(&Peer, Pool);
}
static void poolCancelledAndClosed(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispool* Pool = poolFixtureCreate(&Peer, NULL);
    xfuture *A = poolAcquire(Pool, 5000), *B = poolAcquire(Pool, 5000); xredislease *LA = poolWait(A)->Lease, *LB = poolWait(B)->Lease;
    testRequire(LA && LB, "close held leases"); xfuture* Cancelled = poolAcquire(Pool, XRT_WAIT_FOREVER);
    testRequire(xrtFutureCancel(Cancelled) && poolWait(Cancelled)->Status == XREDIS_REQUEST_CANCELLED && !poolWait(Cancelled)->Lease, "queued acquisition cancellation");
    xfuture* Waiting = poolAcquire(Pool, XRT_WAIT_FOREVER);
    xfuture *CA = xredisLeaseClosed(LA), *CB = xredisLeaseClosed(LB), *Closed = xredisPoolClosed(Pool);
    testRequire(xredisPoolClose(Pool) && xredisPoolClose(Pool), "idempotent pool close");
    xfuture* Rejected = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisPoolAcquireAsync(Pool, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1 &&
        xredisLeaseCallAsync(LA, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1,
        "pool close admitted later acquire/request");
    testRequire(poolWait(Waiting)->Status == XREDIS_REQUEST_CLOSED && xrtFutureWaitFor(CA, 10000) == XWAIT_OK &&
        xrtFutureWaitFor(CB, 10000) == XWAIT_OK && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "pool close incomplete leases/waiter");
    testRequire(!((const xredisleaseclosedresult*)xrtFutureValue(CA))->Reusable && !((const xredisleaseclosedresult*)xrtFutureValue(CB))->Reusable,
        "closed pool returned reusable lease");
    xrtFutureDestroy(A); xrtFutureDestroy(B); xrtFutureDestroy(Cancelled); xrtFutureDestroy(Waiting); xrtFutureDestroy(CA); xrtFutureDestroy(CB); xrtFutureDestroy(Closed);
    poolFixtureStop(&Peer, Pool);
    redisPoolPeerStart(&Peer, 1); Pool = poolFixtureCreate(&Peer, NULL); A = poolAcquire(Pool, 5000); LA = poolWait(A)->Lease;
    testRequire(LA && xredisClientClose(Peer.Clients[0]), "external member close"); CA = xredisLeaseClosed(LA);
    testRequire(xrtFutureWaitFor(CA, 10000) == XWAIT_OK && !((const xredisleaseclosedresult*)xrtFutureValue(CA))->Reusable, "externally closed member reused");
    Waiting = poolAcquire(Pool, 5000); testRequire(poolWait(Waiting)->Status == XREDIS_REQUEST_DISCONNECTED && !poolWait(Waiting)->Lease, "empty-live pool hung acquisition");
    xrtFutureDestroy(A); xrtFutureDestroy(CA); xrtFutureDestroy(Waiting); poolFixtureStop(&Peer, Pool);
}
static void poolBudgetZero(xallocationbudget* Budget)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xrtAllocationBudgetInfo(Budget, &Info), "pool budget snapshot");
        if (!Info.LiveCount && !Info.LiveBytes) return;
        testRequire(xrtTimer() < End, "pool allocation retained after callback tails"); xrtThreadYield();
    }
}
static void poolMemory(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispool* Pool = poolFixtureCreate(&Peer, NULL);
    unsigned Accepted = 0, Failed = 0;
    for (size_t Limit = 1; Limit <= 16384; Limit += 97) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "pool acquire budget");
        xredisadmission Admission = xredisPoolAcquireAsync(Pool, 5000, &Future); testRequire(xrtAllocationBudgetLeave(&Scope), "pool acquire budget leave");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) {
            ++Accepted; xredislease* Lease = poolWait(Future)->Lease; testRequire(Lease != NULL, "budget accepted lease");
            xfuture* Closed = poolReturn(Lease, XREDIS_REQUEST_OK, true); xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
        } else { ++Failed; testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "acquire OOM changed output/count"); xrtClearError(); }
        poolBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
    }
    testRequire(Accepted && Failed, "pool acquire budget missed boundary");
    xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease; testRequire(Lease != NULL, "request budget lease");
    xbytesview Ping = XRT_BYTES_LITERAL("PING");
    for (size_t Limit = 1; Limit <= 2048; Limit += 31) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xfuture* Future = (xfuture*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "leased native request budget");
        xredisadmission Admission = xredisLeaseCallAsync(Lease, &Ping, 1, 5000, &Future); testRequire(xrtAllocationBudgetLeave(&Scope), "leased request budget leave");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) { testRequire(poolCallWait(Future)->Status == XREDIS_REQUEST_OK, "budget native request"); xrtFutureDestroy(Future); }
        else { testRequire(Admission == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "native request OOM output"); xrtClearError(); }
        poolCounter(Pool, 0, 0); poolBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
    }
    xfuture* Closed = poolReturn(Lease, XREDIS_REQUEST_OK, true); xrtFutureDestroy(Acquired); xrtFutureDestroy(Closed); poolFixtureStop(&Peer, Pool);
}
typedef struct poolproducer { xredispool* Pool; unsigned Count; } poolproducer;
static int32 poolProduce(ptr Data)
{
    poolproducer* Producer = Data;
    for (unsigned Index = 0; Index < Producer->Count; ++Index) {
        xfuture* Acquired = NULL; testRequire(xredisPoolAcquire(Producer->Pool, 10000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "concurrent acquire");
        xredislease* Lease = poolWait(Acquired)->Lease; testRequire(Lease != NULL, "concurrent granted lease");
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("producer\0binary")}; xfuture* Calls[4];
        for (size_t Call = 0; Call < 4; ++Call) testRequire(xredisLeaseCallAsync(Lease, Echo, 2, 5000, &Calls[Call]) == XREDIS_ADMISSION_ACCEPTED, "concurrent leased pipeline");
        xfuture* Closed = xredisLeaseClosed(Lease); xredisLeaseReturn(Lease);
        for (size_t Call = 0; Call < 4; ++Call) { testRequire(poolCallWait(Calls[Call])->Status == XREDIS_REQUEST_OK, "concurrent pipeline correlation"); xrtFutureDestroy(Calls[Call]); }
        testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Closed))->Reusable, "concurrent lease return");
        xrtFutureDestroy(Closed); xrtFutureDestroy(Acquired);
    }
    return 0;
}
static void poolConcurrent(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 4); xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.WorkItems = 1;
    xredispool* Pool = poolFixtureCreate(&Peer, &Config); poolproducer Producer = {Pool, 16}; xthread* Threads[8];
    for (size_t Index = 0; Index < 8; ++Index) { Threads[Index] = xrtThreadCreate(poolProduce, &Producer, 0); testRequire(Threads[Index] != NULL, "pool producer thread"); }
    for (size_t Index = 0; Index < 8; ++Index) { testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && !xrtThreadExitCode(Threads[Index]), "pool producer join"); xrtThreadDestroy(Threads[Index]); }
    xredispoolinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisPoolInfo(Pool, &Info) && Info.Acquired == 128 && Info.Returned == 128 && !Info.Pending && !Info.Leased,
        "concurrent pool lost acquisitions"); poolFixtureStop(&Peer, Pool);
}
typedef struct poolworkerbudget { xallocationbudget* Budget; xallocationscope Scope; xatomic32 Done; bool Enter; } poolworkerbudget;
static void poolWorkerBudget(xnetworker* Worker, ptr Data)
{
    (void)Worker; poolworkerbudget* Budget = Data;
    testRequire(Budget->Enter ? xrtAllocationBudgetEnter(Budget->Budget, &Budget->Scope) : xrtAllocationBudgetLeave(&Budget->Scope), "pool worker quota scope");
    xrtAtomic32Store(&Budget->Done, 1, XMEMORY_RELEASE);
}
static void poolWorkerMemory(void)
{
    for (unsigned Held = 0; Held < 2; ++Held) {
        redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispool* Pool = poolFixtureCreate(&Peer, NULL); xfuture* First = NULL;
        if (Held) { First = poolAcquire(Pool, 5000); testRequire(poolWait(First)->Lease != NULL, "worker OOM held lease"); }
        poolworkerbudget Budget = {0}; Budget.Enter = true;
        testRequire(xrtAllocationBudgetCreate(1, &Budget.Budget) && xrtNetEnginePost(Peer.Peers[0].Engine, 0, poolWorkerBudget, &Budget), "pool worker OOM enter");
        redisPeerWait(&Budget.Done, 1); xfuture* Future = poolAcquire(Pool, 5000);
        if (poolWait(Future)->Status != XREDIS_REQUEST_MEMORY || poolWait(Future)->Lease)
            fprintf(stderr, "worker timer quota held=%u status=%d lease=%d\n", Held, poolWait(Future)->Status, poolWait(Future)->Lease != NULL);
        testRequire(poolWait(Future)->Status == XREDIS_REQUEST_MEMORY && !poolWait(Future)->Lease, "worker queue/hold timer OOM lost typed outcome");
        Budget.Enter = false; xrtAtomic32Store(&Budget.Done, 0, XMEMORY_RELAXED);
        testRequire(xrtNetEnginePost(Peer.Peers[0].Engine, 0, poolWorkerBudget, &Budget), "pool worker OOM leave"); redisPeerWait(&Budget.Done, 1);
        if (First) { xfuture* Closed = poolReturn(poolWait(First)->Lease, XREDIS_REQUEST_OK, true); xrtFutureDestroy(Closed); xrtFutureDestroy(First); }
        xrtFutureDestroy(Future); poolFixtureStop(&Peer, Pool); poolBudgetZero(Budget.Budget); xrtAllocationBudgetFree(Budget.Budget);
    }
}
typedef struct poolreentry { xredispool* Pool; xredislease* Lease; xatomic32 Done; } poolreentry;
static void poolWorkerReentry(xnetworker* Worker, ptr Data)
{
    (void)Worker; poolreentry* State = Data; xfuture* Future = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisPoolAcquire(State->Pool, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "sync acquisition blocked another engine worker"); xrtClearError();
    testRequire(xredisLeaseCall(State->Lease, &Ping, 1, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "sync leased call blocked another engine worker"); xrtClearError();
    xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}
static void poolBounds(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispoolconfig Config; xredisPoolConfigInit(&Config);
    for (unsigned Mode = 0; Mode < 11; ++Mode) {
        xredispoolconfig Bad = Config; xredispool* Output = (xredispool*)(uintptr_t)1; size_t Count = Peer.Count; xredisclient* const* Clients = Peer.Clients;
        if (Mode == 0) Bad.Size--;
        if (Mode == 1) Bad.MaxPending = 0;
        if (Mode == 2) Bad.MaxRequests = 0;
        if (Mode == 3) Bad.MaxCommandBytes = 0;
        if (Mode == 4) Bad.LeaseTimeout = 0;
        if (Mode == 5) Bad.LeaseTimeout = 86400001;
        if (Mode == 6) Bad.ReturnTimeout = 60001;
        if (Mode == 7) Bad.WorkItems = 0;
        if (Mode == 8) Count = 0;
        if (Mode == 9) Count = SIZE_MAX;
        if (Mode == 10) Clients = NULL;
        testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Clients, Count, &Bad, &Output) == XREDIS_ADMISSION_ERROR && Output == (xredispool*)(uintptr_t)1,
            "invalid pool construction changed output"); xrtClearError();
    }
    xredisclient* Duplicate[] = {Peer.Clients[0], Peer.Clients[0]}; xredispool* Output = (xredispool*)(uintptr_t)1;
    testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Duplicate, 2, NULL, &Output) == XREDIS_ADMISSION_ERROR && Output == (xredispool*)(uintptr_t)1,
        "duplicate pool membership"); xrtClearError();
    /* A later busy member must not leave the earlier successful claim held. */
    xbytesview Hold = XRT_BYTES_LITERAL("HOLD"), Ping = XRT_BYTES_LITERAL("PING"); xfuture* Held = NULL;
    testRequire(xredisClientCallAsync(Peer.Clients[1], &Hold, 1, 5000, &Held) == XREDIS_ADMISSION_ACCEPTED, "partial claim holder"); redisPeerWait(&Peer.Peers[1].Commands, 2);
    testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, NULL, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xredispool*)(uintptr_t)1,
        "busy partial claim did not roll back");
    xfuture* Call = NULL; testRequire(xredisClientCall(Peer.Clients[0], &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && poolCallWait(Call)->Status == XREDIS_REQUEST_OK,
        "partial claim retained first member"); xrtFutureDestroy(Call);
    redisPeerReleaseBlocked(&Peer.Peers[1]); testRequire(poolCallWait(Held)->Status == XREDIS_REQUEST_OK, "partial claim holder release"); xrtFutureDestroy(Held);
    double End = xrtTimer() + 10;
    for (;;) {
        xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Peer.Clients[1], &Info);
        if (!Info.Requests) break;
        testRequire(xrtTimer() < End, "partial claim native tail"); xrtThreadYield();
    }
    xredispool* Pool = poolFixtureCreate(&Peer, NULL); xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease;
    testRequire(Lease != NULL, "bounds lease"); poolreentry State = {0}; State.Pool = Pool; State.Lease = Lease;
    testRequire(xrtNetEnginePost(Peer.Peers[0].Engine, 1, poolWorkerReentry, &State), "pool reentry post"); redisPeerWait(&State.Done, 1);
    xfuture* Future = (xfuture*)(uintptr_t)1;
    testRequire(xredisPoolAcquireAsync(Pool, 1000, (xfuture**)Pool) == XREDIS_ADMISSION_ERROR &&
        xredisLeaseCallAsync(Lease, &Ping, 1, 1000, (xfuture**)Lease) == XREDIS_ADMISSION_ERROR, "pool/lease output aliases storage"); xrtClearError();
    testRequire(xredisPoolAcquireAsync(Pool, -2, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "invalid acquire timeout changed output"); xrtClearError();
    const cstr Controls[] = {"AUTH","HELLO","SELECT","MULTI","EXEC","DISCARD","WATCH","UNWATCH","RESET","QUIT","READONLY","READWRITE","ASKING","SUBSCRIBE","MONITOR"};
    for (size_t Index = 0; Index < sizeof(Controls) / sizeof(Controls[0]); ++Index) {
        xbytesview Word = {(cbytes)Controls[Index], strlen(Controls[Index])};
        testRequire(xredisLeaseCallAsync(Lease, &Word, 1, 1000, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "pooled control escaped managed profile"); xrtClearError();
    }
    xfuture* Closed = poolReturn(Lease, XREDIS_REQUEST_OK, true); xrtFutureDestroy(Acquired); xrtFutureDestroy(Closed); poolFixtureStop(&Peer, Pool);
}
static void poolConstructorMemory(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); unsigned Accepted = 0, Failed = 0; bool Alive = true;
    /* Recreate members after each successful pool because Close owns their
     * physical retirement. OOM rollback leaves both original clients usable. */
    for (size_t Limit = 1; Limit <= 8192; Limit += 127) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xredispool* Pool = (xredispool*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "pool constructor budget");
        xredisadmission Admission = xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, Peer.Count, NULL, &Pool);
        testRequire(xrtAllocationBudgetLeave(&Scope), "pool constructor budget leave");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) {
            ++Accepted; poolFixtureStop(&Peer, Pool); Alive = false; poolBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
            if (Limit + 127 <= 8192) { redisPoolPeerStart(&Peer, 2); Alive = true; }
        } else {
            ++Failed; testRequire(Admission == XREDIS_ADMISSION_ERROR && Pool == (xredispool*)(uintptr_t)1, "pool constructor OOM changed output/claims"); xrtClearError();
            poolBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
        }
    }
    if (Alive) redisPoolPeerStop(&Peer);
    testRequire(Accepted && Failed, "pool constructor quota missed boundary");
}
static void poolDroppedObserver(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 1); xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.LeaseTimeout = 50;
    xredispool* Pool = poolFixtureCreate(&Peer, &Config); xfuture* First = poolAcquire(Pool, 5000);
    testRequire(poolWait(First)->Lease != NULL, "drop granted lease"); xrtFutureDestroy(First);
    xfuture* Next = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Next)->Lease;
    testRequire(Lease != NULL, "dropped lease leaked slot"); xfuture* Closed = poolReturn(Lease, XREDIS_REQUEST_OK, true);
    xrtFutureDestroy(Next); xrtFutureDestroy(Closed); poolFixtureStop(&Peer, Pool);
}
static void poolProfileBoundaries(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2);
    xfuture* Closed = xredisClientClosed(Peer.Clients[1]); xredisClientClose(Peer.Clients[1]);
    testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "profile fixture original close"); xrtFutureDestroy(Closed); xredisClientRelease(Peer.Clients[1]);
    redisPeerWait(&Peer.Peers[1].Closed, 1); xrtNetStreamDestroy(Peer.Peers[1].Stream); Peer.Peers[1].Stream = NULL;
    xrtAtomic32Store(&Peer.Peers[1].Closed, 0, XMEMORY_RELAXED);
    xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); Init.Mode = XREDIS_INIT_RESP2;
    testRequire(xredisClientConnectInit(Peer.Peers[1].Engine, &Peer.Peers[1].Address, 1, NULL, &Init, &Peer.Clients[1]), "profile RESP2 member"); poolReady(Peer.Clients[1]);
    xredispool* Output = (xredispool*)(uintptr_t)1;
    testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, NULL, &Output) == XREDIS_ADMISSION_ERROR && Output == (xredispool*)(uintptr_t)1,
        "mixed RESP profiles accepted"); xrtClearError();
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Call = NULL;
    testRequire(xredisClientCall(Peer.Clients[0], &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && poolCallWait(Call)->Status == XREDIS_REQUEST_OK,
        "profile rejection retained first claim"); xrtFutureDestroy(Call); redisPoolPeerStop(&Peer);
    redispoolpeer Other; redisPoolPeerStart(&Peer, 1); redisPoolPeerStart(&Other, 1);
    xredisclient* Mixed[] = {Peer.Clients[0], Other.Clients[0]}; Output = (xredispool*)(uintptr_t)1;
    testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Mixed, 2, NULL, &Output) == XREDIS_ADMISSION_ERROR && Output == (xredispool*)(uintptr_t)1,
        "different engine members accepted"); xrtClearError(); redisPoolPeerStop(&Other); redisPoolPeerStop(&Peer);
}
static void poolTLS(void)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0);
    xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS); TLS.WriteBytes = 3; xredisclient* Client = NULL;
    testRequire(xredisClientTlsDial(Peer.Dial.Init.Peer.Engine, Peer.Dial.Resolver, XRT_STR_LITERAL("redis.fixture"),
        Peer.Dial.Init.Peer.Address.Port, 0, NULL, NULL, &TLS, &Client), "TLS pool member"); poolReady(Client);
    xredispool* Pool = NULL; testRequire(xredisPoolCreate(Peer.Dial.Init.Peer.Engine, 0, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "TLS pool create");
    xfuture* Acquired = poolAcquire(Pool, 5000); xredislease* Lease = poolWait(Acquired)->Lease; testRequire(Lease != NULL, "TLS pool lease");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("pool\0TLS")}; xfuture* Call = NULL;
    testRequire(xredisLeaseCall(Lease, Echo, 2, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && poolCallWait(Call)->Status == XREDIS_REQUEST_OK, "TLS leased binary request");
    xfuture* Returned = poolReturn(Lease, XREDIS_REQUEST_OK, true), *Closed = xredisPoolClosed(Pool);
    testRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "TLS pool close");
    xredisClientRelease(Client); xredisPoolRelease(Pool); redisTlsPeerStop(&Peer);
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Returned); xrtFutureDestroy(Closed); xrtFutureDestroy(Call);
#endif
}
int main(void)
{
    poolBasic(); poolFIFO(); poolQuotas(); poolAbandon(); poolGraceful(); poolCancelledAndClosed(); poolMemory(); poolConcurrent();
    poolWorkerMemory(); poolBounds(); poolConstructorMemory(); poolDroppedObserver(); poolProfileBoundaries(); poolTLS();
    puts("[PASS] xredis pool: exclusive FIFO leases; native requests; aggregate physical quota; bounded abandonment and retained metadata"); return 0;
}
