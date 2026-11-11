#include "test.h"
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/pool_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "support/tls_peer.h"
#endif

static const xredisrequestresult* sharedWait(xfuture* Future)
{
    testRequire(Future && xrtFutureWaitFor(Future, 10000) == XWAIT_OK, "shared request wait");
    const xredisrequestresult* Result = xrtFutureValue(Future);
    testRequire(Result != NULL, "shared typed native result"); return Result;
}
static xredispool* sharedStart(redispoolpeer* Peer, size_t Count, const xredispoolconfig* Config)
{
    redisPoolPeerStart(Peer, Count); xredispool* Pool = NULL;
    testRequire(xredisSharedPoolCreate(Peer->Peers[0].Engine, 0, Peer->Clients, Count, Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "shared pool create");
    return Pool;
}
static void sharedZero(xredispool* Pool)
{
    double End = xrtTimer() + 10;
    for (;;) {
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xredisPoolInfo(Pool, &Info) && !Info.Pending && !Info.Leased && !Info.Acquired && !Info.Returned, "shared pool has lease overhead");
        if (!Info.Requests && !Info.CommandBytes) return;
        testRequire(xrtTimer() < End, "shared physical tails did not retire"); xrtThreadYield();
    }
}
static void sharedStop(redispoolpeer* Peer, xredispool* Pool)
{
    xfuture* Closed = xredisPoolClosed(Pool);
    testRequire(xredisPoolClose(Pool) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "shared physical close");
    sharedZero(Pool); xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
    testRequire(xredisPoolInfo(Pool, &Info) && Info.State == XREDIS_POOL_CLOSED && !Info.Idle && !Info.Live && Info.Retired == Peer->Count, "shared retired members");
    xfuture* Output = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisSharedPoolCallAsync(Pool, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_CLOSED && Output == (xfuture*)(uintptr_t)1, "closed shared admission");
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisPoolPeerStop(Peer);
}
static void sharedBasic(void)
{
    redispoolpeer Peer; xredispool* Pool = sharedStart(&Peer, 3, NULL);
    xfuture* Output = (xfuture*)(uintptr_t)1;
    testRequire(xredisPoolAcquireAsync(Pool, 5000, &Output) == XREDIS_ADMISSION_ERROR && Output == (xfuture*)(uintptr_t)1, "shared pool granted an exclusive lease"); xrtClearError();
    unsigned char Payload[] = {'s', 0, '\r', '\n', 0xff}; xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), {Payload, sizeof(Payload)}};
    xfuture* Calls[12] = {0};
    for (size_t I = 0; I < 12; ++I) testRequire(xredisSharedPoolCallAsync(Pool, Echo, 2, 5000, &Calls[I]) == XREDIS_ADMISSION_ACCEPTED, "shared immediate pipelining");
    memset(Payload, 'X', sizeof(Payload));
    for (size_t I = 0; I < 12; ++I) {
        const xredisrequestresult* R = sharedWait(Calls[I]); xredisreplyvalue V;
        testRequire(R->Status == XREDIS_REQUEST_OK && xredisReplyGet(R->Reply, 0, &V) && V.Data.Size == 5 && !memcmp(V.Data.Data, "s\0\r\n\xff", 5), "shared binary ownership");
    }
    for (size_t I = 0; I < 3; ++I) testRequire(xrtAtomic32Load(&Peer.Peers[I].Commands, XMEMORY_ACQUIRE) == 5, "shared round robin");
    const char* Controls[] = {"MULTI", "WATCH", "SUBSCRIBE", "ASKING", "QUIT", "READONLY", "SELECT", "AUTH"};
    for (size_t I = 0; I < sizeof(Controls) / sizeof(Controls[0]); ++I) {
        xbytesview Command = {(const unsigned char*)Controls[I], strlen(Controls[I])};
        testRequire(xredisSharedPoolCallAsync(Pool, &Command, 1, 5000, &Output) == XREDIS_ADMISSION_ERROR && Output == (xfuture*)(uintptr_t)1, "shared mode control accepted"); xrtClearError();
    }
    xbytesview Error = XRT_BYTES_LITERAL("ERROR"); xfuture* Failed = NULL;
    testRequire(xredisSharedPoolCall(Pool, &Error, 1, 5000, &Failed) == XREDIS_ADMISSION_ACCEPTED && sharedWait(Failed)->Status == XREDIS_REQUEST_SERVER_ERROR, "shared server error");
    sharedStop(&Peer, Pool);
    for (size_t I = 0; I < 12; ++I) {testRequire(sharedWait(Calls[I])->Status == XREDIS_REQUEST_OK, "retained results depend on engine"); xrtFutureDestroy(Calls[I]);}
    xrtFutureDestroy(Failed);
    redisPoolPeerStart(&Peer, 1); Pool = NULL;
    testRequire(xredisPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "exclusive control pool");
    xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisSharedPoolCall(Pool, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_ERROR && Output == (xfuture*)(uintptr_t)1, "shared call accepted exclusive pool"); xrtClearError();
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "exclusive control close");
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisPoolPeerStop(&Peer);
}
static void sharedQuotaStops(void)
{
    for (unsigned Stop = 0; Stop < 3; ++Stop) {
        redispoolpeer Peer; xredispoolconfig C; xredisPoolConfigInit(&C); C.MaxRequests = 2;
        C.MaxCommandBytes = 28; C.WorkItems = 1; /* HOLD wire is exactly14 bytes. */
        xredispool* Pool = sharedStart(&Peer, 2, &C); xbytesview Hold = XRT_BYTES_LITERAL("HOLD"); xfuture* Calls[2] = {0};
        for (size_t I = 0; I < 2; ++I) testRequire(xredisSharedPoolCallAsync(Pool, &Hold, 1, Stop == 1 ? 30 : 5000, &Calls[I]) == XREDIS_ADMISSION_ACCEPTED, "shared held admission");
        for (size_t I = 0; I < 2; ++I) redisPeerWait(&Peer.Peers[I].Commands, 2);
        xredispoolinfo Info = {0}; Info.Size = sizeof(Info);
        testRequire(xredisPoolInfo(Pool, &Info) && Info.Requests == 2 && Info.CommandBytes == 28 && Info.Idle == 2, "shared physical aggregate quota");
        xfuture* Output = (xfuture*)(uintptr_t)1;
        testRequire(xredisSharedPoolCallAsync(Pool, &Hold, 1, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1, "shared hard quota");
        if (Stop == 0) for (size_t I = 0; I < 2; ++I) xrtFutureCancel(Calls[I]);
        if (Stop == 2) xredisPoolClose(Pool);
        for (size_t I = 0; I < 2; ++I) {
            const xredisrequestresult* R = sharedWait(Calls[I]);
            testRequire(R->Status == (Stop == 0 ? XREDIS_REQUEST_CANCELLED : Stop == 1 ? XREDIS_REQUEST_TIMEOUT : XREDIS_REQUEST_CLOSED) && R->Delivery == XREDIS_MAY_HAVE_EXECUTED, "shared sent stop outcome");
        }
        if (Stop != 2) {
            testRequire(xredisPoolInfo(Pool, &Info) && Info.Requests == 2 && Info.CommandBytes == 28, "logical stop released physical quota");
            for (size_t I = 0; I < 2; ++I) redisPeerReleaseBlocked(&Peer.Peers[I]);
            sharedZero(Pool); xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Next = NULL;
            testRequire(xredisSharedPoolCall(Pool, &Ping, 1, 5000, &Next) == XREDIS_ADMISSION_ACCEPTED && sharedWait(Next)->Status == XREDIS_REQUEST_OK, "shared post-drain reuse"); xrtFutureDestroy(Next);
        }
        sharedStop(&Peer, Pool); for (size_t I = 0; I < 2; ++I) xrtFutureDestroy(Calls[I]);
    }
}
static void sharedRetire(void)
{
    redispoolpeer Peer; xredispool* Pool = sharedStart(&Peer, 2, NULL);
    xbytesview Bad = XRT_BYTES_LITERAL("MALFORMED"); xfuture* Call = NULL;
    testRequire(xredisSharedPoolCall(Pool, &Bad, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && sharedWait(Call)->Status == XREDIS_REQUEST_PROTOCOL, "shared protocol retirement"); xrtFutureDestroy(Call);
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); Call = NULL;
    testRequire(xredisSharedPoolCall(Pool, &Ping, 1, 5000, &Call) == XREDIS_ADMISSION_ACCEPTED && sharedWait(Call)->Status == XREDIS_REQUEST_OK, "shared surviving member"); xrtFutureDestroy(Call);
    xfuture* Closed = xredisClientClosed(Peer.Clients[1]); xredisClientClose(Peer.Clients[1]); testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "shared last member close"); xrtFutureDestroy(Closed);
    xfuture* Output = (xfuture*)(uintptr_t)1;
    testRequire(xredisSharedPoolCallAsync(Pool, &Ping, 1, 5000, &Output) == XREDIS_ADMISSION_OVERLOADED && Output == (xfuture*)(uintptr_t)1, "empty shared pool admission"); sharedStop(&Peer, Pool);
}
static void sharedProbeBound(void)
{
    for (size_t Work = 1; Work <= 2; ++Work) {
        redispoolpeer Peer; redisPoolPeerStart(&Peer, 2);
        xfuture* Closed = xredisClientClosed(Peer.Clients[0]); xredisClientClose(Peer.Clients[0]);
        testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "limited member replacement close");
        xrtFutureDestroy(Closed); xredisClientRelease(Peer.Clients[0]); redisPeerWait(&Peer.Peers[0].Closed, 1);
        xrtNetStreamDestroy(Peer.Peers[0].Stream); Peer.Peers[0].Stream = NULL;
        xrtAtomic32Store(&Peer.Peers[0].Closed, 0, XMEMORY_RELAXED); xrtAtomic32Store(&Peer.Peers[0].Commands, 0, XMEMORY_RELAXED);
        xredisclientconfig Native; xredisClientConfigInit(&Native); Native.MaxRequests = Native.MaxInFlight = 1;
        testRequire(xredisClientConnectInit(Peer.Peers[0].Engine, &Peer.Peers[0].Address, 0, &Native, NULL, &Peer.Clients[0]), "limited shared member");
        xfuture* Ready = xredisClientReady(Peer.Clients[0]);
        testRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "limited ready"); xrtFutureDestroy(Ready);
        xredispoolconfig Config; xredisPoolConfigInit(&Config); Config.WorkItems = Work; xredispool* Pool = NULL;
        testRequire(xredisSharedPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, &Config, &Pool) == XREDIS_ADMISSION_ACCEPTED, "bounded probe pool");
        xbytesview Hold = XRT_BYTES_LITERAL("HOLD"), Ping = XRT_BYTES_LITERAL("PING"); xfuture *Held = NULL, *Other = NULL;
        testRequire(xredisSharedPoolCallAsync(Pool, &Hold, 1, 5000, &Held) == XREDIS_ADMISSION_ACCEPTED, "limited hold"); redisPeerWait(&Peer.Peers[0].Commands, 2);
        testRequire(xredisSharedPoolCall(Pool, &Ping, 1, 5000, &Other) == XREDIS_ADMISSION_ACCEPTED && sharedWait(Other)->Status == XREDIS_REQUEST_OK, "other member call"); xrtFutureDestroy(Other);
        Other = (xfuture*)(uintptr_t)1; xredisadmission A = xredisSharedPoolCallAsync(Pool, &Ping, 1, 5000, &Other);
        if (Work == 1) testRequire(A == XREDIS_ADMISSION_OVERLOADED && Other == (xfuture*)(uintptr_t)1, "shared probe exceeded WorkItems");
        else {testRequire(A == XREDIS_ADMISSION_ACCEPTED && sharedWait(Other)->Status == XREDIS_REQUEST_OK, "shared rejected admission did not probe available member"); xrtFutureDestroy(Other);}
        testRequire(xrtAtomic32Load(&Peer.Peers[0].Commands, XMEMORY_ACQUIRE) == 2, "overloaded member received query");
        xrtFutureCancel(Held); testRequire(sharedWait(Held)->Status == XREDIS_REQUEST_CANCELLED, "limited cancel"); redisPeerReleaseBlocked(&Peer.Peers[0]);
        sharedZero(Pool); xrtFutureDestroy(Held); sharedStop(&Peer, Pool);
    }
}
static void sharedBudgetZero(xallocationbudget* Budget)
{
    double End = xrtTimer() + 10;
    for (;;) {xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info); testRequire(xrtAllocationBudgetInfo(Budget, &Info), "shared budget info");
        if (!Info.LiveBytes) return;
        testRequire(xrtTimer() < End, "shared budget retained storage"); xrtThreadYield();}
}
static void sharedMemory(void)
{
    redispoolpeer Peer; redisPoolPeerStart(&Peer, 2); xredispool* Pool = NULL; unsigned Failed = 0;
    for (size_t Limit = 1; Limit <= 8192; Limit += 37) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; Pool = (xredispool*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "shared create OOM scope");
        xredisadmission A = xredisSharedPoolCreate(Peer.Peers[0].Engine, 0, Peer.Clients, 2, NULL, &Pool);
        testRequire(xrtAllocationBudgetLeave(&Scope), "shared create scope leave");
        if (A == XREDIS_ADMISSION_ACCEPTED) {sharedStop(&Peer, Pool); sharedBudgetZero(Budget); xrtAllocationBudgetFree(Budget); break;}
        ++Failed; testRequire(A == XREDIS_ADMISSION_ERROR && Pool == (xredispool*)(uintptr_t)1, "shared create OOM changed output/claims"); xrtClearError();
        sharedBudgetZero(Budget); xrtAllocationBudgetFree(Budget); Pool = NULL;
    }
    testRequire(Pool && Failed, "shared create OOM boundary");
    Pool = sharedStart(&Peer, 1, NULL); unsigned Accepted = 0; Failed = 0; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    for (size_t Limit = 1; Limit <= 2048; Limit += 17) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xfuture* F = (xfuture*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "shared call OOM scope");
        xredisadmission A = xredisSharedPoolCallAsync(Pool, &Ping, 1, 5000, &F); testRequire(xrtAllocationBudgetLeave(&Scope), "shared call scope leave");
        if (A == XREDIS_ADMISSION_ACCEPTED) {++Accepted; testRequire(sharedWait(F)->Status == XREDIS_REQUEST_OK, "shared OOM accepted outcome"); xrtFutureDestroy(F);}
        else {++Failed; testRequire(A == XREDIS_ADMISSION_ERROR && F == (xfuture*)(uintptr_t)1, "shared call OOM changed output"); xrtClearError();}
        sharedZero(Pool); sharedBudgetZero(Budget); xrtAllocationBudgetFree(Budget);
    }
    testRequire(Accepted && Failed, "shared call OOM boundary"); sharedStop(&Peer, Pool);
}
typedef struct sharedproducer {xredispool* Pool; xatomic32 Accepted, Closed;} sharedproducer;
static int32 sharedProduce(ptr Data)
{
    sharedproducer* P = Data; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    for (unsigned I = 0; I < 200; ++I) {
        xfuture* F = NULL; xredisadmission A = xredisSharedPoolCallAsync(P->Pool, &Ping, 1, 5000, &F);
        if (A == XREDIS_ADMISSION_CLOSED) {xrtAtomic32FetchAdd(&P->Closed, 1, XMEMORY_RELAXED); continue;}
        testRequire(A == XREDIS_ADMISSION_ACCEPTED, "shared producer admission");
        const xredisrequestresult* R = sharedWait(F); testRequire(R->Status == XREDIS_REQUEST_OK || R->Status == XREDIS_REQUEST_CLOSED, "shared producer close outcome");
        xrtAtomic32FetchAdd(&P->Accepted, 1, XMEMORY_RELEASE); xrtFutureDestroy(F);
    }
    return 0;
}
typedef struct sharedreentry {xredispool* Pool; xfuture* Future; xatomic32 Done;} sharedreentry;
static void sharedReentry(xnetworker* Worker, ptr Data)
{
    (void)Worker; sharedreentry* P = Data; xfuture* Out = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(xredisSharedPoolCall(P->Pool, &Ping, 1, 5000, &Out) == XREDIS_ADMISSION_ERROR && Out == (xfuture*)(uintptr_t)1, "shared sync worker accepted"); xrtClearError();
    testRequire(xredisSharedPoolCallAsync(P->Pool, &Ping, 1, 5000, &P->Future) == XREDIS_ADMISSION_ACCEPTED, "shared async worker refused");
    xrtAtomic32Store(&P->Done, 1, XMEMORY_RELEASE);
}
static void sharedConcurrent(void)
{
    redispoolpeer Peer; xredispool* Pool = sharedStart(&Peer, 3, NULL); sharedreentry Reentry = {Pool, NULL, {0}};
    testRequire(xrtNetEnginePost(Peer.Peers[0].Engine, 1, sharedReentry, &Reentry), "shared worker post"); redisPeerWait(&Reentry.Done, 1);
    testRequire(sharedWait(Reentry.Future)->Status == XREDIS_REQUEST_OK, "shared worker result"); xrtFutureDestroy(Reentry.Future);
    sharedproducer Producer = {Pool, {0}, {0}}; xthread* Threads[4];
    for (size_t I = 0; I < 4; ++I) {Threads[I] = xrtThreadCreate(sharedProduce, &Producer, 0); testRequire(Threads[I] != NULL, "shared producer thread");}
    double End = xrtTimer() + 10;
    while (xrtAtomic32Load(&Producer.Accepted, XMEMORY_ACQUIRE) < 20) {testRequire(xrtTimer() < End, "shared producer progress"); xrtThreadYield();}
    xredisPoolClose(Pool);
    for (size_t I = 0; I < 4; ++I) {testRequire(xrtThreadWait(Threads[I]) == XWAIT_OK && !xrtThreadExitCode(Threads[I]), "shared producer join"); xrtThreadDestroy(Threads[I]);}
    testRequire(xrtAtomic32Load(&Producer.Accepted, XMEMORY_ACQUIRE) + xrtAtomic32Load(&Producer.Closed, XMEMORY_ACQUIRE) == 800, "shared lost producer admission");
    sharedStop(&Peer, Pool);
}
static void sharedTLS(void)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    redistlspeer Peer; redisTlsPeerStart(&Peer, false, false, 0); xredisclienttlsconfig TLS; redisTlsPeerConfig(&Peer, &TLS); TLS.WriteBytes = 3;
    xredisclient* Client = NULL; testRequire(xredisClientTlsDial(Peer.Dial.Init.Peer.Engine, Peer.Dial.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Dial.Init.Peer.Address.Port, 0, NULL, NULL, &TLS, &Client), "shared TLS connect");
    xfuture* Ready = xredisClientReady(Client); testRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "shared TLS ready"); xrtFutureDestroy(Ready);
    xredispool* Pool = NULL; testRequire(xredisSharedPoolCreate(Peer.Dial.Init.Peer.Engine, 0, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "shared TLS pool");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("shared\0TLS")}; xfuture* Calls[16] = {0};
    for (size_t I = 0; I < 16; ++I) testRequire(xredisSharedPoolCallAsync(Pool, Echo, 2, 5000, &Calls[I]) == XREDIS_ADMISSION_ACCEPTED, "shared TLS pipelined call");
    for (size_t I = 0; I < 16; ++I) {const xredisrequestresult* R = sharedWait(Calls[I]); xredisreplyvalue V;
        testRequire(R->Status == XREDIS_REQUEST_OK && xredisReplyGet(R->Reply, 0, &V) && V.Data.Size == 10 && !memcmp(V.Data.Data, "shared\0TLS", 10), "shared TLS binary result");}
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "shared TLS physical close"); sharedZero(Pool);
    xredisPoolRelease(Pool); xredisClientRelease(Client); xrtFutureDestroy(Closed); redisTlsPeerStop(&Peer);
    for (size_t I = 0; I < 16; ++I) xrtFutureDestroy(Calls[I]);
#endif
}
int main(void)
{
    sharedBasic(); sharedQuotaStops(); sharedRetire(); sharedProbeBound(); sharedMemory(); sharedConcurrent(); sharedTLS();
    puts("[PASS] xredis shared pool: native pipelining, bounded selection, physical quotas, stops, OOM, producers and lifetime"); return 0;
}
