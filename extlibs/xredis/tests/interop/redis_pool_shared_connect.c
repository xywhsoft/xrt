#include <xredis/pool_shared_connect.h>
#include <xredis/dial.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned rcCase;
static void rcRequire(bool B, cstr T) {if (!B) {fprintf(stderr, "[FAIL] real shared reconnect case=%u: %s\n", rcCase, T); exit(1);}}
static const xredisrequestresult* rcWait(xfuture* F)
{rcRequire(F && xrtFutureWaitFor(F, 15000) == XWAIT_OK && xrtFutureValue(F), "native request wait"); return xrtFutureValue(F);}
static xredisreplyvalue rcValue(xfuture* F)
{
    const xredisrequestresult* R = rcWait(F); xredisreplyvalue V; xredisreplyinfo I = {0}; I.Size = sizeof(I);
    if (R->Status != XREDIS_REQUEST_OK) fprintf(stderr, "status=%d delivery=%d system=%d cause=%s\n", (int)R->Status, (int)R->Delivery, xrtErrorSystemCode(R->Cause), xrtErrorMessage(R->Cause));
    rcRequire(R->Status == XREDIS_REQUEST_OK && xredisReplyInfo(R->Reply, &I) && xredisReplyGet(R->Reply, I.Root, &V), "native reply root"); return V;
}
static bool rcContains(xbytesview V, cstr T)
{size_t N = strlen(T); for (size_t I = 0; I + N <= V.Size; ++I) if (!memcmp(V.Data + I, T, N)) return true; return false;}
typedef struct rcfactory {
    xnetaddr Address; xnetresolver* Resolver; xredisclientinitconfig Init; unsigned Kind;
    xatomic32 Calls, Drops; xfuture* Closed[8];
} rcfactory;
static bool rcConnect(xnetengine* E, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
    (void)Attempt; rcfactory* F = Data; uint32 N = xrtAtomic32Load(&F->Calls, XMEMORY_RELAXED);
    rcRequire(N < 8, "unexpected reconnect churn"); xrtAtomic32Store(&F->Calls, N + 1u, XMEMORY_RELEASE);
    xredisclientinitconfig I = F->Init; I.Timeout = Timeout; xredisclientdialconfig D; xredisClientDialConfigInit(&D); D.Family = XNET_FAMILY_IPV4; bool OK;
    if (F->Kind == 1) OK = xredisClientDial(E, F->Resolver, XRT_STR_LITERAL("localhost"), F->Address.Port, Slot % 2u, NULL, &I, &D, Out);
    else if (F->Kind == 2) {xnetaddrlist* L = xrtNetAddrListCreate(&F->Address, 1); if (!L) return false;
        OK = xredisClientDialAddresses(E, L, Slot % 2u, NULL, &I, &D, Out); xrtNetAddrListDestroy(L);}
    else OK = xredisClientConnectInit(E, &F->Address, Slot % 2u, NULL, &I, Out);
    if (OK) F->Closed[N] = xredisClientClosed(*Out);
    return OK;
}
static void rcDrop(ptr Data)
{
    rcfactory* F = Data;
    for (size_t N = 0; N < 8; ++N) if (F->Closed[N]) {rcRequire(xrtFutureState(F->Closed[N]) == XFUTURE_RESOLVED, "context dropped before client closed"); xrtFutureDestroy(F->Closed[N]); F->Closed[N] = NULL;}
    xrtAtomic32FetchAdd(&F->Drops, 1, XMEMORY_RELEASE);
}
static void rcOnline(xredispool* P, uint64 Generation)
{
    double Due = xrtTimer() + 15;
    for (;;) {xredispoolinfo I = {0}; I.Size = sizeof(I); xredispoolconnectinfo C = {0}; C.Size = sizeof(C);
        rcRequire(xredisPoolInfo(P, &I) && xredisPoolConnectInfo(P, &C) && I.State == XREDIS_POOL_OPEN && xrtTimer() < Due, "shared reconnect readiness");
        if (I.Idle == 2 && C.Connected >= Generation) return;
        xrtThreadYield();}
}
static xfuture* rcCall(xredispool* P, const xbytesview* A, size_t N)
{xfuture* F = NULL; rcRequire(xredisSharedPoolCallAsync(P, A, N, 10000, &F) == XREDIS_ADMISSION_ACCEPTED, "shared native admission"); return F;}
static xfuture* rcAdmin(xredisclient* C, const xbytesview* A, size_t N)
{xfuture* F = NULL; rcRequire(xredisClientCall(C, A, N, 10000, &F) == XREDIS_ADMISSION_ACCEPTED, "owned admin admission"); return F;}
int main(int Argc, char** Argv)
{
    if (Argc != 3 || strlen(Argv[2]) != 40) return 2;
    unsigned long Port = strtoul(Argv[1], NULL, 10); if (!Port || Port > 65535) return 2;
    xnetengineconfig EC; xrtNetEngineConfigInit(&EC); EC.Workers = 2; xnetengine* E = xrtNetEngineCreate(&EC);
    rcRequire(E && xrtNetEngineStart(E), "engine start"); xnetresolver* Resolver = xrtNetResolverCreate(NULL); rcRequire(Resolver != NULL, "resolver");
    xnetaddr Address; rcRequire(xrtNetAddrParse(&Address, "127.0.0.1", (uint16)Port), "owned target address"); xredisclient* Admin = NULL;
    rcRequire(xredisClientConnectInit(E, &Address, 0, NULL, NULL, &Admin), "owned admin client"); xfuture* Ready = xredisClientReady(Admin);
    rcRequire(xrtFutureWaitFor(Ready, 15000) == XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status == XREDIS_READY_OK, "owned admin ready"); xrtFutureDestroy(Ready);
    xbytesview Info[] = {XRT_BYTES_LITERAL("INFO"), XRT_BYTES_LITERAL("SERVER")}; xfuture* F = rcAdmin(Admin, Info, 2);
    rcRequire(rcContains(rcValue(F).Data, Argv[2]), "actual owned Redis identity before mutation"); xrtFutureDestroy(F);
    const xredisinitmode Modes[] = {XREDIS_INIT_RESP2, XREDIS_INIT_RESP3, XREDIS_INIT_AUTO}; xfuture *Retained[18] = {0}, *Lost[18] = {0}; size_t Saved = 0;
    for (unsigned Kind = 0; Kind < 3; ++Kind) for (unsigned Mode = 0; Mode < 3; ++Mode) for (unsigned ACL = 0; ACL < 2; ++ACL) {
        rcCase = Kind * 6 + Mode * 2 + ACL; rcfactory Factory = {0}; Factory.Address = Address; Factory.Resolver = Resolver; Factory.Kind = Kind;
        xredisClientInitConfigInit(&Factory.Init); Factory.Init.Mode = Modes[Mode];
        if (ACL) {Factory.Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME; Factory.Init.Username = (xbytesview)XRT_BYTES_LITERAL("shared-fixture");}
        xredispoolconnectconfig C; xredisPoolConnectConfigInit(&C); C.Connect = rcConnect; C.Drop = rcDrop; C.Data = &Factory; C.Protocol = Mode == 0 ? XREDIS_RESP2 : XREDIS_RESP3;
        C.RetryMin = C.RetryMax = 5; xredispool* P = NULL;
        rcRequire(xredisSharedPoolConnect(E, 1, 2, NULL, &C, &P) == XREDIS_ADMISSION_ACCEPTED, "automatic shared pool"); rcOnline(P, 2);
        for (unsigned Member = 0; Member < 2; ++Member) {F = rcCall(P, Info, 2); rcRequire(rcContains(rcValue(F).Data, Argv[2]), "each member actual run ID"); xrtFutureDestroy(F);}
        xbytesview ID[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; int64 Ids[2];
        for (unsigned Member = 0; Member < 2; ++Member) {F = rcCall(P, ID, 2); Ids[Member] = rcValue(F).Integer; xrtFutureDestroy(F);} rcRequire(Ids[0] != Ids[1], "two physical clients");
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("reconnect\0x")}; xfuture* Calls[64] = {0};
        for (unsigned N = 0; N < 64; ++N) Calls[N] = rcCall(P, Echo, 2);
        for (unsigned N = 0; N < 64; ++N) {xredisreplyvalue V = rcValue(Calls[N]); rcRequire(V.Data.Size == 11 && !memcmp(V.Data.Data, "reconnect\0x", 11), "shared binary pipeline"); if (N) xrtFutureDestroy(Calls[N]);}
        /* Even64 rotations select Ids[0]. Prove BLPOP was executed/blocking on
         * this exact owned connection before killing it via a separate admin. */
        xbytesview Block[] = {XRT_BYTES_LITERAL("BLPOP"), XRT_BYTES_LITERAL("xrt-owned:shared-repair:block\0key"), XRT_BYTES_LITERAL("0")}; xfuture* Pending = rcCall(P, Block, 3);
        char Number[32]; int Digits = snprintf(Number, sizeof(Number), "%lld", (long long)Ids[0]); rcRequire(Digits > 0, "client ID format");
        xbytesview List[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("LIST"), XRT_BYTES_LITERAL("ID"), {(const unsigned char*)Number, (size_t)Digits}};
        double Due = xrtTimer() + 10;
        for (;;) {F = rcAdmin(Admin, List, 4); bool Blocked = rcContains(rcValue(F).Data, "cmd=blpop"); xrtFutureDestroy(F); if (Blocked) break; rcRequire(xrtTimer() < Due, "owned BLPOP not observed"); xrtThreadYield();}
        xbytesview Kill[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("KILL"), XRT_BYTES_LITERAL("ID"), {(const unsigned char*)Number, (size_t)Digits}};
        F = rcAdmin(Admin, Kill, 4); rcRequire(rcValue(F).Integer == 1, "kill exact owned shared member"); xrtFutureDestroy(F);
        const xredisrequestresult* R = rcWait(Pending); rcRequire(R->Status == XREDIS_REQUEST_DISCONNECTED && R->Delivery == XREDIS_MAY_HAVE_EXECUTED, "native blocked outcome preserved"); rcOnline(P, 3);
        xbytesview All[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("LIST")}; F = rcAdmin(Admin, All, 2); rcRequire(!rcContains(rcValue(F).Data, "cmd=blpop"), "accepted blocking command replayed"); xrtFutureDestroy(F);
        int64 After[2]; for (unsigned Member = 0; Member < 2; ++Member) {F = rcCall(P, ID, 2); After[Member] = rcValue(F).Integer; xrtFutureDestroy(F);}
        rcRequire(After[0] != After[1] && After[0] != Ids[0] && After[1] != Ids[0] && ((After[0] == Ids[1]) != (After[1] == Ids[1])), "only killed member replaced");
        xfuture* Closed = xredisPoolClosed(P); xredisPoolClose(P); rcRequire(xrtFutureWaitFor(Closed, 15000) == XWAIT_OK, "native shared close");
        xredispoolinfo PI = {0}; PI.Size = sizeof(PI); rcRequire(xredisPoolInfo(P, &PI) && !PI.Live && !PI.Idle && !PI.Requests && !PI.CommandBytes && !PI.Leased && !PI.Pending && xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE) == 1, "physical generation/context obligations");
        rcRequire(xrtAtomic32Load(&Factory.Calls, XMEMORY_ACQUIRE) == 3, "unexpected factory replay/churn"); xrtFutureDestroy(Closed); xredisPoolRelease(P);
        Retained[Saved] = Calls[0]; Lost[Saved++] = Pending;
    }
    xfuture* Closed = xredisClientClosed(Admin); xredisClientClose(Admin); rcRequire(xrtFutureWaitFor(Closed, 15000) == XWAIT_OK, "admin physical close"); xrtFutureDestroy(Closed); xredisClientRelease(Admin);
    rcRequire(xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E), "engine owners retired");
    for (size_t N = 0; N < Saved; ++N) {rcRequire(rcValue(Retained[N]).Data.Size == 11 && rcWait(Lost[N])->Status == XREDIS_REQUEST_DISCONNECTED, "retained immutable outcomes"); xrtFutureDestroy(Retained[N]); xrtFutureDestroy(Lost[N]);}
    puts("[PASS] real shared reconnect: 18 numeric/DNS/prepared RESP2/3/AUTO ACL cases; observed blocked member killed, only its generation replaced, no replay, joined context and retained replies"); return 0;
}
