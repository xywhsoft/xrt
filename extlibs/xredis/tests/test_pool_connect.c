#include "test.h"
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/pool_connect_peer.h"
static const void* pcWait(xfuture* Future)
{ testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK && xrtFutureValue(Future), "automatic pool typed wait"); return xrtFutureValue(Future); }
static xfuture* pcAcquire(xredispool* Pool, int64 Timeout)
{ xfuture* Future = NULL; testRequire(xredisPoolAcquireAsync(Pool, Timeout, &Future) == XREDIS_ADMISSION_ACCEPTED, "automatic acquire admission"); return Future; }
static xredislease* pcLease(xfuture* Future)
{ const xredisleaseresult* Result = pcWait(Future); testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Lease, "automatic lease ready"); return Result->Lease; }
static xfuture* pcReturn(xredislease* Lease, bool Abort)
{
    xfuture* Closed = xredisLeaseClosed(Lease); testRequire(Abort ? xredisLeaseAbort(Lease) : xredisLeaseReturn(Lease), "automatic return/abort");
    const xredisleaseclosedresult* Result = pcWait(Closed); testRequire(Result->Reusable != Abort, "automatic return boundary"); return Closed;
}
static void pcPing(xredislease* Lease)
{
    xbytesview Command[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("pool\0repair\xff")}; xfuture* Future = NULL;
    testRequire(xredisLeaseCall(Lease, Command, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "automatic native call");
    const xredisrequestresult* Result = pcWait(Future); xredisreplyvalue Text;
    testRequire(Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Text) && Text.Data.Size == Command[1].Size &&
        !memcmp(Text.Data.Data, Command[1].Data, Text.Data.Size), "automatic native binary reply"); xrtFutureDestroy(Future);
}
static xredispoolconnectinfo pcInfo(xredispool* Pool)
{ xredispoolconnectinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xredisPoolConnectInfo(Pool, &Info), "automatic snapshot"); return Info; }
static void pcClose(xredispool* Pool, redisconnectfactory* Factory)
{
    xfuture* Closed = xredisPoolClosed(Pool); testRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "automatic physical close");
    xredispoolconnectinfo State = pcInfo(Pool); xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
    testRequire(xredisPoolInfo(Pool, &Info) && !Info.Live && !Info.Idle && !Info.Leased && !Info.Pending && !Info.Requests && !Info.CommandBytes &&
        !State.Connecting && !State.Waiting && xrtAtomic32Load(&Factory->Drops, XMEMORY_ACQUIRE) == 1, "automatic close context/accounting");
    uint32 Calls = xrtAtomic32Load(&Factory->Calls, XMEMORY_ACQUIRE); xredisPoolClose(Pool);
    testRequire(xrtAtomic32Load(&Factory->Calls, XMEMORY_ACQUIRE) == Calls, "closed pool restarted factory"); xrtFutureDestroy(Closed);
}
static xredispool* pcCreate(redisconnectpeer* Peer, size_t Count, xredispoolconfig* PoolConfig, xredispoolconnectconfig* Config)
{
    xredispool* Pool = NULL; testRequire(xredisPoolConnect(Peer->Engine, 0, Count, PoolConfig, Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "automatic create");
    xrtAtomicPtrStore(&((redisconnectfactory*)Config->Data)->Pool, Pool, XMEMORY_RELEASE); return Pool;
}
static void pcRepair(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.FailFirst = 3; Factory.Observe = true; Config.RetryMin = 10; Config.RetryMax = 30;
    xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Acquired = pcAcquire(Pool, 5000); xredislease* Lease = pcLease(Acquired); pcPing(Lease);
    testRequire(pcInfo(Pool).Attempts == 4 && pcInfo(Pool).Failures == 3, "factory failure retry accounting");
    for (size_t Index = 1; Index < 4; ++Index) testRequire(Factory.Times[Index] - Factory.Times[Index - 1u] >= .009, "retry spun without backoff");
    xfuture* Returned = pcReturn(Lease, true); xfuture* Again = pcAcquire(Pool, 5000); xredislease* Successor = pcLease(Again); pcPing(Successor);
    xfuture* Clean = pcReturn(Successor, false); testRequire(pcInfo(Pool).Connected == 2, "retired slot not replenished");
    xfuture* Rejected = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisLeaseCallAsync(Lease, &Ping, 1, 5000, &Rejected) == XREDIS_ADMISSION_CLOSED && Rejected == (xfuture*)(uintptr_t)1, "old generation crossed reconnect");
    pcClose(Pool, &Factory); redisConnectPeerStop(&Peer); testRequire(xredisLeaseReturn(Lease) && pcInfo(Pool).Connected == 2, "retained metadata kept engine");
    xredisPoolRelease(Pool); xrtFutureDestroy(Acquired); xrtFutureDestroy(Again); xrtFutureDestroy(Returned); xrtFutureDestroy(Clean);
}
static void pcConcurrency(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); xrtAtomic32Store(&Peer.Mode, PC_HOLD, XMEMORY_RELEASE);
    redisconnectfactory Factory; xredispoolconnectconfig Config; redisConnectFactoryConfig(&Factory, &Peer, &Config);
    Config.MaxConnecting = 2; Config.AttemptTimeout = 150; Config.RetryMin = Config.RetryMax = 20;
    xredispoolconfig Options; xredisPoolConfigInit(&Options); Options.WorkItems = 1;
    xredispool* Pool = pcCreate(&Peer, 4, &Options, &Config); redisPeerWait(&Factory.Calls, 2); redisPeerWait(&Peer.Hello, 2);
    xredispoolconnectinfo Info = pcInfo(Pool); testRequire(Info.Attempts == 2 && Info.Connecting == 2 && Info.Waiting == 2, "connecting concurrency exceeded");
    xfuture* Expired = pcAcquire(Pool, 10), *Cancelled = pcAcquire(Pool, XRT_WAIT_FOREVER);
    testRequire(((const xredisleaseresult*)pcWait(Expired))->Status == XREDIS_REQUEST_TIMEOUT, "outage ignored acquire deadline");
    testRequire(xrtFutureCancel(Cancelled) && ((const xredisleaseresult*)pcWait(Cancelled))->Status == XREDIS_REQUEST_CANCELLED, "outage cancel");
    xrtAtomic32Store(&Peer.Mode, PC_NORMAL, XMEMORY_RELEASE);
    xfuture* Granted[4]; for (size_t Index = 0; Index < 4; ++Index) Granted[Index] = pcAcquire(Pool, 5000);
    for (size_t Index = 0; Index < 4; ++Index) { xredislease* Lease = pcLease(Granted[Index]); pcPing(Lease); }
    testRequire(pcInfo(Pool).Failures >= 2 && pcInfo(Pool).Connected == 4, "connect timeout hidden behind due retries");
    for (size_t Index = 0; Index < 4; ++Index) { xfuture* Closed = pcReturn(pcLease(Granted[Index]), false); xrtFutureDestroy(Closed); xrtFutureDestroy(Granted[Index]); }
    pcClose(Pool, &Factory); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer); xrtFutureDestroy(Expired); xrtFutureDestroy(Cancelled);
}
static void pcModesAndClose(void)
{
    for (unsigned Mode = 0; Mode < 6; ++Mode) {
        redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
        redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 10; Config.AttemptTimeout = 80;
        if (Mode == 0) Factory.Raw = true;
        if (Mode == 1) Factory.Resp2 = true;
        if (Mode == 2) xrtAtomic32Store(&Peer.Mode, PC_BAD_HELLO, XMEMORY_RELEASE);
        if (Mode == 3) xrtAtomic32Store(&Peer.Mode, PC_DROP_HELLO, XMEMORY_RELEASE);
        if (Mode == 4) xrtAtomic32Store(&Peer.Mode, PC_HOLD, XMEMORY_RELEASE);
        if (Mode == 5) { Factory.FailFirst = 100; Config.RetryMin = Config.RetryMax = 10000; }
        xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Waiting = pcAcquire(Pool, XRT_WAIT_FOREVER);
        redisPeerWait(&Factory.Calls, Mode == 4 || Mode == 5 ? 1 : 2);
        testRequire(pcInfo(Pool).Connected == 0, "raw/profile/failed setup became idle");
        pcClose(Pool, &Factory); testRequire(((const xredisleaseresult*)pcWait(Waiting))->Status == XREDIS_REQUEST_CLOSED, "outage close lost waiter");
        xredisPoolRelease(Pool); redisConnectPeerStop(&Peer); xrtFutureDestroy(Waiting);
    }
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.Resp2 = true; Config.Protocol = XREDIS_RESP2;
    xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Acquired = pcAcquire(Pool, 5000); pcPing(pcLease(Acquired));
    pcClose(Pool, &Factory); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer); xrtFutureDestroy(Acquired);
}
static void pcNoReplay(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 5;
    xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Acquired = pcAcquire(Pool, 5000); xredislease* Lease = pcLease(Acquired);
    xbytesview Drop = XRT_BYTES_LITERAL("DROP"); xfuture* Call = NULL;
    testRequire(xredisLeaseCall(Lease, &Drop, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED, "uncertain command admit");
    const xredisrequestresult* Result = pcWait(Call); testRequire(Result->Status == XREDIS_REQUEST_DISCONNECTED && Result->Delivery == XREDIS_MAY_HAVE_EXECUTED, "disconnect uncertainty");
    xfuture* Returned = xredisLeaseClosed(Lease); testRequire(!((const xredisleaseclosedresult*)pcWait(Returned))->Reusable, "dirty member reused");
    xfuture* Next = pcAcquire(Pool, 5000); xredislease* Successor = pcLease(Next);
    testRequire(xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 1, "old command replayed on replacement"); pcPing(Successor);
    xfuture* Clean = pcReturn(Successor, false); pcClose(Pool, &Factory); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer);
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Call); xrtFutureDestroy(Returned); xrtFutureDestroy(Next); xrtFutureDestroy(Clean);
}
static void pcBoundsAndMemory(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config);
    for (unsigned Mode = 0; Mode < 13; ++Mode) {
        xredispoolconnectconfig Bad = Config; size_t Count = 1; xredispool* Output = (xredispool*)(uintptr_t)1;
        if (Mode == 0) Bad.Size--;
        if (Mode == 1) Bad.Connect = NULL;
        if (Mode == 2) Bad.Protocol = XREDIS_PROTOCOL_UNKNOWN;
        if (Mode == 3) Bad.MaxConnecting = 0;
        if (Mode == 4) Bad.MaxConnecting = 65537;
        if (Mode == 5) Bad.AttemptTimeout = 0;
        if (Mode == 6) Bad.AttemptTimeout = 60001;
        if (Mode == 7) Bad.RetryMin = 0;
        if (Mode == 8) Bad.RetryMax = 60001;
        if (Mode == 9) Bad.RetryMax = 1;
        if (Mode == 10) Count = 0;
        if (Mode == 11) Count = SIZE_MAX;
        xredispoolconfig PoolOptions; xredisPoolConfigInit(&PoolOptions); if (Mode == 12) PoolOptions.MaxPending = 0;
        testRequire(xredisPoolConnect(Peer.Engine, 0, Count, &PoolOptions, &Bad, &Output) == XREDIS_ADMISSION_ERROR && Output == (xredispool*)(uintptr_t)1 &&
            !xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE) && !xrtAtomic32Load(&Factory.Calls, XMEMORY_ACQUIRE), "constructor failure consumed factory/output"); xrtClearError();
    }
    unsigned Accepted = 0, Failed = 0;
    for (size_t Limit = 1; Limit <= 16384; Limit += 127) {
        redisConnectFactoryConfig(&Factory, &Peer, &Config); xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "automatic constructor budget");
        xredispool* Pool = (xredispool*)(uintptr_t)1;
        xredisadmission Admission = xredisPoolConnect(Peer.Engine, 0, 2, NULL, &Config, &Pool);
        testRequire(xrtAllocationBudgetLeave(&Scope), "automatic constructor budget leave");
        if (Admission == XREDIS_ADMISSION_ACCEPTED) { ++Accepted; pcClose(Pool, &Factory); xredisPoolRelease(Pool); }
        else { ++Failed; testRequire(Admission == XREDIS_ADMISSION_ERROR && Pool == (xredispool*)(uintptr_t)1 && !xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE), "OOM consumed factory/output"); xrtClearError(); }
        double End = xrtTimer() + 10;
        for (;;) { xallocationbudgetinfo Stats = {0}; Stats.Size = sizeof(Stats); testRequire(xrtAllocationBudgetInfo(Budget, &Stats), "automatic budget stats"); if (!Stats.LiveBytes && !Stats.LiveCount) break;
            testRequire(xrtTimer() < End, "automatic constructor budget leaked"); xrtThreadYield(); }
        xrtAllocationBudgetFree(Budget);
    }
    testRequire(Accepted && Failed, "automatic budget windows missed boundary"); redisConnectPeerStop(&Peer);
}
typedef struct pcworkerbudget { xallocationbudget* Budget; xallocationscope Scope; xatomic32 Done; bool Enter; } pcworkerbudget;
static void pcWorkerBudget(xnetworker* Worker, ptr Data)
{
    (void)Worker; pcworkerbudget* State = Data;
    testRequire(State->Enter ? xrtAllocationBudgetEnter(State->Budget, &State->Scope) : xrtAllocationBudgetLeave(&State->Scope), "maintenance worker allocation scope");
    xrtAtomic32Store(&State->Done, 1, XMEMORY_RELEASE);
}
static void pcMaintenanceMemory(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); pcworkerbudget Budget = {0}; Budget.Enter = true;
    testRequire(xrtAllocationBudgetCreate(1, &Budget.Budget) && xrtNetEnginePost(Peer.Engine, 0, pcWorkerBudget, &Budget), "maintenance budget enter post"); redisPeerWait(&Budget.Done, 1);
    redisconnectfactory Factory; xredispoolconnectconfig Config; redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.FailFirst = UINT32_MAX;
    xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Closed = xredisPoolClosed(Pool);
    testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && pcInfo(Pool).LastFailure == XREDIS_REQUEST_MEMORY && pcInfo(Pool).Attempts == 1,
        "timer OOM must close once without retry spin");
    pcClose(Pool, &Factory); xredisPoolRelease(Pool); xrtFutureDestroy(Closed);
    Budget.Enter = false; xrtAtomic32Store(&Budget.Done, 0, XMEMORY_RELEASE);
    testRequire(xrtNetEnginePost(Peer.Engine, 0, pcWorkerBudget, &Budget), "maintenance budget leave post"); redisPeerWait(&Budget.Done, 1);
    xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xrtAllocationBudgetInfo(Budget.Budget, &Info) && !Info.LiveBytes && !Info.LiveCount, "maintenance OOM leaked");
    xrtAllocationBudgetFree(Budget.Budget); redisConnectPeerStop(&Peer);
}
static bool pcReentrantFactory(xnetengine* Engine, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client)
{
    redisconnectfactory* Factory = Data; xredispool* Pool = xrtAtomicPtrLoad(&Factory->Pool, XMEMORY_ACQUIRE); if (!Pool) return false;
    xfuture* Future = (xfuture*)(uintptr_t)1;
    testRequire(xredisPoolAcquire(Pool, 100, &Future) == XREDIS_ADMISSION_ERROR && Future == (xfuture*)(uintptr_t)1, "factory sync self-worker deadlock");
    xrtClearError(); testRequire(xredisPoolClose(Pool), "factory reentrant close");
    return redisConnectFactory(Engine, Slot, Attempt, Timeout, Data, Client);
}
static void pcReentry(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.Observe = true; Config.Connect = pcReentrantFactory; Config.RetryMin = Config.RetryMax = 5;
    xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Closed = xredisPoolClosed(Pool);
    testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && xrtAtomic32Load(&Factory.Calls, XMEMORY_ACQUIRE) == 1 &&
        xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE) == 1, "factory close lost transferred candidate/context");
    xrtFutureDestroy(Closed); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer);
}
static void pcNested(void)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    for (unsigned Abort = 0; Abort < 2; ++Abort) {
        redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
        redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.Observe = true; Config.RetryMin = Config.RetryMax = 5;
        xredispool* Pool = pcCreate(&Peer, 1, NULL, &Config); xfuture* Acquired = pcAcquire(Pool, 5000); xredislease* Lease = pcLease(Acquired);
        xbytesview Key = XRT_BYTES_LITERAL("auto-watch"); xfuture* Started = NULL;
        testRequire(xredisLeaseWatch(Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "automatic nested WATCH");
        const xrediswatchresult* Result = pcWait(Started); testRequire(Result->Request.Status == XREDIS_REQUEST_OK && Result->Watch, "automatic nested WATCH readiness");
        xfuture* ChildClosed = xredisWatchClosed(Result->Watch); xfuture* ParentClosed = pcReturn(Lease, Abort != 0);
        testRequire(xrtFutureState(ChildClosed) == XFUTURE_RESOLVED, "replacement passed live nested owner");
        xfuture* Again = pcAcquire(Pool, 5000); pcPing(pcLease(Again)); xfuture* Returned = pcReturn(pcLease(Again), false);
        testRequire(pcInfo(Pool).Connected == 1u + Abort, "nested physical replacement count");
        pcClose(Pool, &Factory); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer);
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(ChildClosed); xrtFutureDestroy(ParentClosed); xrtFutureDestroy(Again); xrtFutureDestroy(Returned);
    }
#endif
}
typedef struct pcproducer { xredispool* Pool; xatomic32 *Ready, *Go; unsigned Role; bool Close; } pcproducer;
static int32 pcProduce(ptr Data)
{
    pcproducer* State = Data; xrtAtomic32FetchAdd(State->Ready, 1, XMEMORY_RELEASE); redisPeerWait(State->Go, 1);
    for (unsigned Index = 0; Index < 8; ++Index) {
        xfuture* Future = NULL; xredisadmission Admission = xredisPoolAcquire(State->Pool, 5000, &Future);
        if (Admission == XREDIS_ADMISSION_CLOSED) break;
        testRequire(Admission == XREDIS_ADMISSION_ACCEPTED, "recovery producer admission"); const xredisleaseresult* Result = pcWait(Future);
        if (Result->Status == XREDIS_REQUEST_CLOSED) { xrtFutureDestroy(Future); break; }
        testRequire(Result->Status == XREDIS_REQUEST_OK && Result->Lease, "recovery producer grant");
        xfuture* Closed = xredisLeaseClosed(Result->Lease); xfuture* Call = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
        if (State->Close && State->Role == 3) xredisPoolClose(State->Pool);
        Admission = xredisLeaseCallAsync(Result->Lease, &Ping, 1, 5000, &Call);
        if (Admission == XREDIS_ADMISSION_ACCEPTED) { const xredisrequestresult* Request = pcWait(Call);
            testRequire(Request->Status == XREDIS_REQUEST_OK || Request->Status == XREDIS_REQUEST_CLOSED, "recovery producer native result"); xrtFutureDestroy(Call); }
        else testRequire(Admission == XREDIS_ADMISSION_CLOSED && Call == (xfuture*)(uintptr_t)1, "closed producer changed output");
        if (!(Index % 3u)) xredisLeaseAbort(Result->Lease); else xredisLeaseReturn(Result->Lease);
        pcWait(Closed); xrtFutureDestroy(Closed); xrtFutureDestroy(Future);
    }
    return 0;
}
static void pcRaces(void)
{
    for (unsigned Round = 0; Round < 10; ++Round) {
        redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
        redisConnectFactoryConfig(&Factory, &Peer, &Config); Factory.Observe = true; Config.MaxConnecting = 2; Config.RetryMin = Config.RetryMax = 1;
        xredispoolconfig Options; xredisPoolConfigInit(&Options); Options.WorkItems = 1;
        xredispool* Pool = pcCreate(&Peer, 4, &Options, &Config); xatomic32 Ready, Go; xrtAtomic32Init(&Ready, 0); xrtAtomic32Init(&Go, 0);
        pcproducer States[4]; xthread* Threads[4];
        for (unsigned Index = 0; Index < 4; ++Index) { States[Index] = (pcproducer){Pool, &Ready, &Go, Index, Round % 2u != 0};
            Threads[Index] = xrtThreadCreate(pcProduce, &States[Index], 0); testRequire(Threads[Index] != NULL, "recovery producer thread"); }
        redisPeerWait(&Ready, 4); xrtAtomic32Store(&Go, 1, XMEMORY_RELEASE);
        for (unsigned Index = 0; Index < 4; ++Index) { testRequire(xrtThreadWait(Threads[Index]) == XWAIT_OK && !xrtThreadExitCode(Threads[Index]), "recovery producer join"); xrtThreadDestroy(Threads[Index]); }
        pcClose(Pool, &Factory); xredisPoolRelease(Pool); redisConnectPeerStop(&Peer);
    }
}
int main(void)
{
    pcRepair(); pcConcurrency(); pcModesAndClose(); pcNoReplay(); pcBoundsAndMemory(); pcMaintenanceMemory(); pcReentry(); pcNested(); pcRaces();
    puts("[PASS] automatic pools: bounded setup/refill, jittered retry, hard deadlines, no replay and physical close/context barrier"); return 0;
}
