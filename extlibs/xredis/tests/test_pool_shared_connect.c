#include "test.h"
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/pool_connect_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "support/tls_peer.h"
#endif
static const xredisrequestresult* scWait(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F, 15000) == XWAIT_OK && xrtFutureValue(F), "shared reconnect native wait"); return xrtFutureValue(F);}
static xredispoolinfo scInfo(xredispool* P)
{xredispoolinfo I = {0}; I.Size = sizeof(I); testRequire(xredisPoolInfo(P, &I), "shared reconnect snapshot"); return I;}
static xredispoolconnectinfo scConnectInfo(xredispool* P)
{xredispoolconnectinfo I = {0}; I.Size = sizeof(I); testRequire(xredisPoolConnectInfo(P, &I), "shared connector snapshot"); return I;}
static void scOnline(xredispool* P, size_t Members, uint64 Generations)
{
    double Due = xrtTimer() + 15;
    for (;;) {xredispoolinfo I = scInfo(P); xredispoolconnectinfo C = scConnectInfo(P);
        if (I.Idle == Members && C.Connected >= Generations) return;
        testRequire(I.State == XREDIS_POOL_OPEN && xrtTimer() < Due, "shared replacement not ready"); xrtThreadYield();}
}
static xredispool* scCreate(redisconnectpeer* Peer, redisconnectfactory* F, size_t N,
    const xredispoolconfig* P, xredispoolconnectconfig* C)
{
    xredispool* Pool = NULL;
    testRequire(xredisSharedPoolConnect(Peer->Engine, 0, N, P, C, &Pool) == XREDIS_ADMISSION_ACCEPTED, "shared reconnect create");
    xrtAtomicPtrStore(&F->Pool, Pool, XMEMORY_RELEASE); return Pool;
}
static void scClose(xredispool* P, redisconnectfactory* F)
{
    xfuture* Closed = xredisPoolClosed(P); testRequire(xredisPoolClose(P) && xrtFutureWaitFor(Closed, 15000) == XWAIT_OK, "shared reconnect physical close");
    xredispoolinfo I = scInfo(P); xredispoolconnectinfo C = scConnectInfo(P);
    testRequire(!I.Live && !I.Idle && !I.Requests && !I.CommandBytes && !I.Pending && !I.Leased && !I.Acquired && !I.Returned &&
        !C.Waiting && !C.Connecting && xrtAtomic32Load(&F->Drops, XMEMORY_ACQUIRE) == 1, "shared reconnect duties/context not retired");
    uint32 Calls = xrtAtomic32Load(&F->Calls, XMEMORY_ACQUIRE); xredisPoolClose(P);
    testRequire(xrtAtomic32Load(&F->Calls, XMEMORY_ACQUIRE) == Calls, "closed shared pool restarted factory"); xrtFutureDestroy(Closed);
}
static xfuture* scCall(xredispool* P, const xbytesview* A, size_t N, int64 Timeout)
{xfuture* F = NULL; testRequire(xredisSharedPoolCallAsync(P, A, N, Timeout, &F) == XREDIS_ADMISSION_ACCEPTED, "shared reconnect admission"); return F;}
static void scEcho(xredispool* P)
{
    xbytesview A[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("repair\0bytes\xff")}; xfuture* F = scCall(P, A, 2, 5000);
    const xredisrequestresult* R = scWait(F); xredisreplyvalue V;
    testRequire(R->Status == XREDIS_REQUEST_OK && xredisReplyGet(R->Reply, 0, &V) && V.Data.Size == A[1].Size && !memcmp(V.Data.Data, A[1].Data, V.Data.Size), "shared repaired binary reply"); xrtFutureDestroy(F);
}
static void scRepair(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C);
    F.FailFirst = 3; F.Observe = true; C.RetryMin = 5; C.RetryMax = 20; xredispool* P = scCreate(&Peer, &F, 1, NULL, &C); scOnline(P, 1, 1);
    testRequire(scConnectInfo(P).Attempts == 4 && scConnectInfo(P).Failures == 3, "shared retry accounting"); scEcho(P);
    for (unsigned Generation = 1; Generation <= 4; ++Generation) {
#if defined(XREDIS_SHARED_CONNECT_WHITEBOX)
        xredisclient* Old = P->Slots[0].Client; xredisClientRetain(Old); xredisexclusiveowner* Owner = &P->Slots[0].Owner;
#endif
        xbytesview Drop = XRT_BYTES_LITERAL("DROP"); xfuture* Lost = scCall(P, &Drop, 1, 5000);
        const xredisrequestresult* R = scWait(Lost);
        testRequire(R->Status == XREDIS_REQUEST_DISCONNECTED && R->Delivery == XREDIS_MAY_HAVE_EXECUTED, "shared disconnect lost uncertainty");
        scOnline(P, 1, Generation + 1u);
#if defined(XREDIS_SHARED_CONNECT_WHITEBOX)
        xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Out = (xfuture*)(uintptr_t)1;
        testRequire(__xredisClientOwnedCall(Old, Owner, &Ping, 1, 5000, xrtTimer(), false, &Out) == XREDIS_ADMISSION_CLOSED && Out == (xfuture*)(uintptr_t)1,
            "stale selected client crossed slot generation"); xredisClientRelease(Old);
#endif
        testRequire(xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 2u * Generation, "accepted request was replayed");
        scEcho(P); xrtFutureDestroy(Lost);
    }
    scClose(P, &F); redisConnectPeerStop(&Peer); testRequire(scConnectInfo(P).Connected == 5, "retained shared metadata held engine"); xredisPoolRelease(P);
}
static void scOutage(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); xrtAtomic32Store(&Peer.Mode, PC_HOLD, XMEMORY_RELEASE);
    redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C); F.Observe = true;
    C.MaxConnecting = 2; C.AttemptTimeout = 150; C.RetryMin = C.RetryMax = 10;
    xredispoolconfig B; xredisPoolConfigInit(&B); B.WorkItems = 1; xredispool* P = scCreate(&Peer, &F, 4, &B, &C);
    redisPeerWait(&Peer.Hello, 2); xfuture* Out = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
    testRequire(scConnectInfo(P).Connecting == 2 && xredisSharedPoolCallAsync(P, &Ping, 1, 5000, &Out) == XREDIS_ADMISSION_OVERLOADED && Out == (xfuture*)(uintptr_t)1,
        "shared outage queued work or exceeded connection bound");
    testRequire(xredisPoolAcquireAsync(P, 5000, &Out) == XREDIS_ADMISSION_ERROR && Out == (xfuture*)(uintptr_t)1, "maintained shared pool granted lease"); xrtClearError();
    redisPeerWait(&F.Calls, 4); xrtAtomic32Store(&Peer.Mode, PC_NORMAL, XMEMORY_RELEASE); scOnline(P, 4, 4); scEcho(P);
    testRequire(scConnectInfo(P).Failures >= 2, "shared attempt deadline hidden behind retries"); scClose(P, &F); xredisPoolRelease(P); redisConnectPeerStop(&Peer);
    for (unsigned Mode = 0; Mode < 4; ++Mode) {
        redisConnectPeerStart(&Peer); redisConnectFactoryConfig(&F, &Peer, &C); F.Observe = true; C.RetryMin = C.RetryMax = 20;
        if (Mode == 0) F.Raw = true;
        if (Mode == 1) F.Resp2 = true;
        if (Mode == 2) xrtAtomic32Store(&Peer.Mode, PC_BAD_HELLO, XMEMORY_RELEASE);
        if (Mode == 3) {F.FailFirst = 100; C.RetryMin = C.RetryMax = 10000;}
        P = scCreate(&Peer, &F, 1, NULL, &C); redisPeerWait(&F.Calls, Mode == 3 ? 1 : 2);
        testRequire(scInfo(P).Idle == 0, "invalid shared generation became callable"); scClose(P, &F); xredisPoolRelease(P); redisConnectPeerStop(&Peer);
    }
}
static void scStoppedDrain(void)
{
    for (unsigned Timeout = 0; Timeout < 2; ++Timeout) {
        redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C);
        xredisclientconfig Native; xredisClientConfigInit(&Native); Native.DrainTimeout = 500; F.ClientConfig = &Native; F.Observe = true; C.RetryMin = C.RetryMax = 5;
        xredispoolconfig B; xredisPoolConfigInit(&B); B.MaxRequests = 1; xredispool* P = scCreate(&Peer, &F, 1, &B, &C); scOnline(P, 1, 1);
        xbytesview Hold = XRT_BYTES_LITERAL("HOLD"); xfuture* Held = scCall(P, &Hold, 1, Timeout ? 200 : 5000); redisPeerWait(&Peer.Peers[0].Commands, 2);
        if (!Timeout) xrtFutureCancel(Held);
        const xredisrequestresult* R = scWait(Held); testRequire(R->Status == (Timeout ? XREDIS_REQUEST_TIMEOUT : XREDIS_REQUEST_CANCELLED) && R->Delivery == XREDIS_MAY_HAVE_EXECUTED, "shared logical stop snapshot");
        testRequire(scInfo(P).Requests == 1 && scConnectInfo(P).Connected == 1, "shared stop released native physical quota");
        xfuture* Out = (xfuture*)(uintptr_t)1; xbytesview Ping = XRT_BYTES_LITERAL("PING");
        testRequire(xredisSharedPoolCallAsync(P, &Ping, 1, 5000, &Out) == XREDIS_ADMISSION_OVERLOADED && Out == (xfuture*)(uintptr_t)1, "shared stop bypassed quota");
        scOnline(P, 1, 2); testRequire(scInfo(P).Requests == 0, "replacement passed old request storage"); scEcho(P);
        testRequire(xrtAtomic32Load(&Peer.Commands, XMEMORY_ACQUIRE) == 2, "stopped request replayed on replacement");
        scClose(P, &F); xredisPoolRelease(P); redisConnectPeerStop(&Peer);
        testRequire(R->Status == (Timeout ? XREDIS_REQUEST_TIMEOUT : XREDIS_REQUEST_CANCELLED), "retained stopped result changed"); xrtFutureDestroy(Held);
    }
}
static void scBudgetZero(xallocationbudget* B)
{
    double Due = xrtTimer() + 10;
    for (;;) {xallocationbudgetinfo I = {0}; I.Size = sizeof(I); testRequire(xrtAllocationBudgetInfo(B, &I), "shared connector budget info");
        if (!I.LiveCount && !I.LiveBytes) return;
        testRequire(xrtTimer() < Due, "shared connector retained budget storage"); xrtThreadYield();}
}
static void scMemory(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C); unsigned Failed = 0; bool Accepted = false;
    for (size_t Limit = 1; Limit <= 8192; Limit += 37) {
        xallocationbudget* Budget = NULL; xallocationscope Scope = {0}; xredispool* Out = (xredispool*)(uintptr_t)1;
        testRequire(xrtAllocationBudgetCreate(Limit, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope), "shared connector constructor budget");
        xredisadmission A = xredisSharedPoolConnect(Peer.Engine, 0, 2, NULL, &C, &Out); testRequire(xrtAllocationBudgetLeave(&Scope), "shared connector budget leave");
        if (A == XREDIS_ADMISSION_ACCEPTED) {Accepted = true; xrtAtomicPtrStore(&F.Pool, Out, XMEMORY_RELEASE); scClose(Out, &F); xredisPoolRelease(Out);}
        else {++Failed; testRequire(A == XREDIS_ADMISSION_ERROR && Out == (xredispool*)(uintptr_t)1 && !xrtAtomic32Load(&F.Calls, XMEMORY_ACQUIRE) && !xrtAtomic32Load(&F.Drops, XMEMORY_ACQUIRE), "failed constructor consumed context/output"); xrtClearError();}
        scBudgetZero(Budget); xrtAllocationBudgetFree(Budget); if (Accepted) break;
    }
    testRequire(Accepted && Failed, "shared connector OOM boundary"); redisConnectPeerStop(&Peer);
}
typedef struct scproducer {xredispool* Pool; xatomic32 Accepted, Rejected; unsigned Role;} scproducer;
static int32 scProduce(ptr Data)
{
    scproducer* S = Data; xbytesview Ping = XRT_BYTES_LITERAL("PING"), Drop = XRT_BYTES_LITERAL("DROP");
    for (unsigned I = 0; I < 80; ++I) {
        xfuture* F = (xfuture*)(uintptr_t)1; xredisadmission A = xredisSharedPoolCallAsync(S->Pool, S->Role == 0 && I % 20u == 0 ? &Drop : &Ping, 1, 5000, &F);
        if (A == XREDIS_ADMISSION_OVERLOADED || A == XREDIS_ADMISSION_CLOSED) {testRequire(F == (xfuture*)(uintptr_t)1, "shared rejected producer changed output"); xrtAtomic32FetchAdd(&S->Rejected, 1, XMEMORY_RELAXED); xrtThreadYield(); continue;}
        testRequire(A == XREDIS_ADMISSION_ACCEPTED, "shared recovery producer admit"); const xredisrequestresult* R = scWait(F);
        testRequire(R->Status == XREDIS_REQUEST_OK || R->Status == XREDIS_REQUEST_DISCONNECTED || R->Status == XREDIS_REQUEST_CLOSED, "shared producer recovery result");
        xrtAtomic32FetchAdd(&S->Accepted, 1, XMEMORY_RELAXED); xrtFutureDestroy(F);
    }
    return 0;
}
static void scConcurrent(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F, &Peer, &C); F.Observe = true; C.RetryMin = C.RetryMax = 1;
    xredispool* P = scCreate(&Peer, &F, 4, NULL, &C); scOnline(P, 4, 4); scproducer S[4]; xthread* Threads[4];
    for (unsigned I = 0; I < 4; ++I) {S[I] = (scproducer){P, {0}, {0}, I}; Threads[I] = xrtThreadCreate(scProduce, &S[I], 0); testRequire(Threads[I] != NULL, "shared recovery thread");}
    for (unsigned I = 0; I < 4; ++I) {testRequire(xrtThreadWait(Threads[I]) == XWAIT_OK && !xrtThreadExitCode(Threads[I]), "shared recovery join"); xrtThreadDestroy(Threads[I]);
        testRequire(xrtAtomic32Load(&S[I].Accepted, XMEMORY_ACQUIRE) + xrtAtomic32Load(&S[I].Rejected, XMEMORY_ACQUIRE) == 80, "shared recovery lost admission");}
    scClose(P, &F); xredisPoolRelease(P); redisConnectPeerStop(&Peer);
}
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
typedef struct sctlsfactory {redisconnectfactory Base; xredisclienttlsconfig TLS;} sctlsfactory;
static bool scTLSFactory(xnetengine* E, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
    (void)Attempt; sctlsfactory* F = Data; uint32 Call = xrtAtomic32FetchAdd(&F->Base.Calls, 1, XMEMORY_RELEASE);
    xredisclientinitconfig I; xredisClientInitConfigInit(&I); I.Timeout = Timeout;
    bool OK = xredisClientTlsConnect(E, &F->Base.Peer->Address, Slot % 2u, NULL, &I, &F->TLS, Out);
    if (OK && Call < 256) F->Base.Closed[Call] = xredisClientClosed(*Out);
    return OK;
}
#endif
static void scTLS(void)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redistlspeer Credentials = {0}; redisTlsPeerCredentials(&Credentials, &Peer.Peers[0], false, false); Peer.TLS = &Credentials.Options;
    sctlsfactory F; xredispoolconnectconfig C; redisConnectFactoryConfig(&F.Base, &Peer, &C); C.Connect = scTLSFactory; redisTlsPeerConfig(&Credentials, &F.TLS); F.TLS.WriteBytes = 3;
    C.RetryMin = C.RetryMax = 5; xredispool* P = scCreate(&Peer, &F.Base, 2, NULL, &C); scOnline(P, 2, 2); scEcho(P);
    xbytesview Drop = XRT_BYTES_LITERAL("DROP"); xfuture* Lost = scCall(P, &Drop, 1, 5000);
    testRequire(scWait(Lost)->Status == XREDIS_REQUEST_DISCONNECTED, "shared TLS disconnect"); scOnline(P, 2, 3); scEcho(P);
    scClose(P, &F.Base); xredisPoolRelease(P); redisConnectPeerStop(&Peer); redisTlsPeerCredentialsDrop(&Credentials); xrtFutureDestroy(Lost);
#endif
}
int main(void)
{
    scRepair(); scOutage(); scStoppedDrain(); scMemory(); scConcurrent(); scTLS();
    puts("[PASS] shared reconnect: bounded attempts/readiness, physical generations, no replay, stopped drain, OOM and concurrent recovery"); return 0;
}
