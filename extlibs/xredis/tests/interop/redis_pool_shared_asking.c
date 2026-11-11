#include <xredis/pool_shared_asking.h>
#include <xredis/pool_shared_connect.h>
#include <xredis/dial.h>
#include <xredis/cluster_topology.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void arRequire(bool B,cstr T){if(!B){fprintf(stderr,"[FAIL] real ASKING: %s\n",T);exit(1);}}
static const void* arWait(xfuture* F){arRequire(F && xrtFutureWaitFor(F,15000)==XWAIT_OK && xrtFutureValue(F),"typed native Future");return xrtFutureValue(F);}
static xfuture* arCall(xredispool* L,const xbytesview* A,size_t N)
{xfuture* F=NULL;arRequire(xredisSharedPoolCallAsync(L,A,N,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"ordinary lease call");return F;}
static xredisreplyvalue arValue(xredisreply* R)
{xredisreplyinfo I={0};I.Size=sizeof(I);xredisreplyvalue V;arRequire(xredisReplyInfo(R,&I) && xredisReplyGet(R,I.Root,&V),"immutable reply root");return V;}
static void arMoved(xfuture* F,uint16 Source)
{const xredisrequestresult* R=arWait(F);xredisclusterredirect D;uint16 Slot;arRequire(xredisClusterKeySlot(XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key"),&Slot),"real key slot");arRequire(R->Status==XREDIS_REQUEST_SERVER_ERROR && R->Delivery==XREDIS_REPLIED && xredisClusterRedirectRead(R->Reply,1024,&D)==XREDIS_CLUSTER_VALUE && D.Kind==XREDIS_CLUSTER_MOVED && D.Port==Source && D.Slot==Slot,"ordinary request cannot inherit ASKING flag");}
static bool arContains(xbytesview V,cstr Text)
{size_t N=strlen(Text);for(size_t I=0;I+N<=V.Size;++I)if(!memcmp(V.Data+I,Text,N))return true;return false;}
static void arPair(xfuture* F,bool Denied)
{const xredisaskingresult* R=arWait(F);arRequire(R->ControlReceived && R->Control.Status==(Denied?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK) && R->Control.Delivery==XREDIS_REPLIED && R->Query.Status==XREDIS_REQUEST_OK && R->Query.Delivery==XREDIS_REPLIED,"independent real control/query outcomes");xredisreplyvalue V=arValue(R->Query.Reply);arRequire(V.Data.Size==7 && !memcmp(V.Data.Data,"value\0x",7),"owned binary business reply");}
typedef struct arfactory {
    xnetresolver* Resolver; xnetaddr Address; xredisclientinitconfig Init; unsigned Kind;
    xfuture* Closed[8]; xatomic32 Calls, Drops;
} arfactory;
static bool arConnect(xnetengine* E, size_t Slot, uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Out)
{
    (void)Slot; (void)Attempt; arfactory* F = Data; uint32 Call = xrtAtomic32Load(&F->Calls, XMEMORY_RELAXED);
    arRequire(Call < 8, "unexpected factory churn"); xrtAtomic32Store(&F->Calls, Call + 1u, XMEMORY_RELEASE);
    xredisclientinitconfig I = F->Init; I.Timeout = Timeout; xredisclientdialconfig D; xredisClientDialConfigInit(&D); D.Family = XNET_FAMILY_IPV4; bool OK;
    if (F->Kind == 1) OK = xredisClientDial(E, F->Resolver, XRT_STR_LITERAL("localhost"), F->Address.Port, 0, NULL, &I, &D, Out);
    else if (F->Kind == 2) {xnetaddrlist* L = xrtNetAddrListCreate(&F->Address, 1); if (!L) return false;
        OK = xredisClientDialAddresses(E, L, 0, NULL, &I, &D, Out); xrtNetAddrListDestroy(L);}
    else OK = xredisClientConnectInit(E, &F->Address, 0, NULL, &I, Out);
    if (OK) F->Closed[Call] = xredisClientClosed(*Out);
    return OK;
}
static void arDrop(ptr Data)
{
    arfactory* F = Data;
    for (unsigned N = 0; N < 8; ++N) if (F->Closed[N]) {arRequire(xrtFutureState(F->Closed[N]) == XFUTURE_RESOLVED, "factory drop before physical member close"); xrtFutureDestroy(F->Closed[N]); F->Closed[N] = NULL;}
    xrtAtomic32FetchAdd(&F->Drops, 1, XMEMORY_RELEASE);
}
typedef struct arproducer {xredispool* Pool; uint16 Source; bool Denied;} arproducer;
static int32 arProduce(ptr Data)
{
    arproducer* S = Data;
    xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key")};
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("value\0x")};
    for (unsigned N = 0; N < 12; ++N) {
        xfuture* Pair = NULL; arRequire(xredisSharedPoolAskingCallAsync(S->Pool, S->Denied ? Echo : Get, 2, 10000, &Pair) == XREDIS_ADMISSION_ACCEPTED, "real concurrent shared group");
        xfuture* Ordinary = arCall(S->Pool, Get, 2); arPair(Pair, S->Denied); arMoved(Ordinary, S->Source);
        xrtFutureDestroy(Pair); xrtFutureDestroy(Ordinary);
    }
    return 0;
}
int main(int Argc, char** Argv)
{
    if (Argc != 4 || strlen(Argv[3]) != 40) return 2;
    unsigned long Port = strtoul(Argv[1], NULL, 10), Source = strtoul(Argv[2], NULL, 10); if (!Port || Port > 65535 || !Source || Source > 65535) return 2;
    xnetengineconfig EC; xrtNetEngineConfigInit(&EC); EC.Workers = 2; xnetengine* E = xrtNetEngineCreate(&EC); arRequire(E && xrtNetEngineStart(E), "engine start");
    xnetresolver* Resolver = xrtNetResolverCreate(NULL); arRequire(Resolver != NULL, "resolver");
    const xredisinitmode Modes[] = {XREDIS_INIT_RESP2, XREDIS_INIT_RESP3, XREDIS_INIT_AUTO}; xfuture* Retained[36] = {0}; bool Denials[36] = {0}; size_t Saved = 0;
    for (unsigned Maintained = 0; Maintained < 2; ++Maintained) for (unsigned Factory = 0; Factory < 3; ++Factory) for (unsigned M = 0; M < 3; ++M) for (unsigned Denied = 0; Denied < 2; ++Denied) {
        arfactory F = {0}; F.Resolver = Resolver; F.Kind = Factory; arRequire(xrtNetAddrParse(&F.Address, "127.0.0.1", (uint16)Port), "owned target address");
        xredisClientInitConfigInit(&F.Init); F.Init.Mode = Modes[M]; F.Init.Timeout = 10000;
        if (Denied) {F.Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME; F.Init.Username = (xbytesview)XRT_BYTES_LITERAL("asking-denied");}
        xredisclient* Client = NULL; xredispool* Pool = NULL;
        if (Maintained) {
            xredispoolconnectconfig C; xredisPoolConnectConfigInit(&C); C.Connect = arConnect; C.Drop = arDrop; C.Data = &F; C.Protocol = M == 0 ? XREDIS_RESP2 : XREDIS_RESP3;
            C.RetryMin = C.RetryMax = 5; arRequire(xredisSharedPoolConnect(E, 1, 1, NULL, &C, &Pool) == XREDIS_ADMISSION_ACCEPTED, "automatic shared member");
            double Due = xrtTimer() + 15;
            for (;;) {xredispoolinfo I = {0}; I.Size = sizeof(I); arRequire(xredisPoolInfo(Pool, &I) && I.State == XREDIS_POOL_OPEN && xrtTimer() < Due, "automatic shared readiness"); if (I.Idle == 1) break; xrtThreadYield();}
        } else {
            arRequire(arConnect(E, 0, 1, 10000, &F, &Client), "managed fixed member"); xfuture* Ready = xredisClientReady(Client);
            arRequire(((const xredisreadyresult*)arWait(Ready))->Status == XREDIS_READY_OK, "fixed member readiness"); xrtFutureDestroy(Ready);
            arRequire(xredisSharedPoolCreate(E, 1, &Client, 1, NULL, &Pool) == XREDIS_ADMISSION_ACCEPTED, "fixed shared member");
        }
        xbytesview Info[] = {XRT_BYTES_LITERAL("INFO"), XRT_BYTES_LITERAL("SERVER")}; xfuture* ID = arCall(Pool, Info, 2);
        const xredisrequestresult* IR = arWait(ID); arRequire(IR->Status == XREDIS_REQUEST_OK && arContains(arValue(IR->Reply).Data, Argv[3]), "actual owned importer identity"); xrtFutureDestroy(ID);
        xbytesview Get[] = {XRT_BYTES_LITERAL("GET"), XRT_BYTES_LITERAL("xrt-owned:asking:{fixed}\0key")}; xfuture* Before = arCall(Pool, Get, 2); arMoved(Before, (uint16)Source); xrtFutureDestroy(Before);
        xbytesview ClientID[] = {XRT_BYTES_LITERAL("CLIENT"), XRT_BYTES_LITERAL("ID")}; ID = arCall(Pool, ClientID, 2); int64 Original = arValue(((const xredisrequestresult*)arWait(ID))->Reply).Integer; xrtFutureDestroy(ID);
        xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("value\0x")}; xfuture* Pair = NULL;
        arRequire(xredisSharedPoolAskingCallAsync(Pool, Denied ? Echo : Get, 2, 10000, &Pair) == XREDIS_ADMISSION_ACCEPTED, "shared native ASKING group"); arPair(Pair, Denied != 0);
        arproducer Work = {Pool, (uint16)Source, Denied != 0}; xthread* Threads[4];
        for (unsigned N = 0; N < 4; ++N) {Threads[N] = xrtThreadCreate(arProduce, &Work, 0); arRequire(Threads[N] != NULL, "real shared producers");}
        for (unsigned N = 0; N < 4; ++N) {arRequire(xrtThreadWaitFor(Threads[N], 30000) == XWAIT_OK && !xrtThreadExitCode(Threads[N]), "real shared producers joined"); xrtThreadDestroy(Threads[N]);}
        xfuture* After = arCall(Pool, Get, 2); arMoved(After, (uint16)Source); xrtFutureDestroy(After);
        ID = arCall(Pool, ClientID, 2); arRequire(arValue(((const xredisrequestresult*)arWait(ID))->Reply).Integer == Original, "groups/ordinary calls retain same physical connection"); xrtFutureDestroy(ID);
        xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); arRequire(xrtFutureWaitFor(Closed, 15000) == XWAIT_OK, "shared physical close");
        xredispoolinfo I = {0}; I.Size = sizeof(I); arRequire(xredisPoolInfo(Pool, &I) && !I.Requests && !I.CommandBytes && !I.Leased && !I.Pending && !I.Live && !I.Idle, "shared physical storage retired");
        if (Client) {xredisclientinfo CI = {0}; CI.Size = sizeof(CI); arRequire(xredisClientInfo(Client, &CI) && !CI.Requests && !CI.CommandBytes && CI.Accepted == CI.Completed, "fixed client tails retired"); xredisClientRelease(Client); arDrop(&F);}
        arRequire(xrtAtomic32Load(&F.Drops, XMEMORY_ACQUIRE) == 1 && xrtAtomic32Load(&F.Calls, XMEMORY_ACQUIRE) == 1, "exact factory lifetime without unexpected replay/reconnect");
        xredisPoolRelease(Pool); xrtFutureDestroy(Closed); Retained[Saved] = Pair; Denials[Saved++] = Denied != 0;
    }
    arRequire(Saved == 36 && xrtNetResolverDestroy(Resolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E), "all shared native owners joined");
    for (size_t N = 0; N < Saved; ++N) {arPair(Retained[N], Denials[N]); xrtFutureDestroy(Retained[N]);}
    puts("[PASS] real shared ASKING: 36 fixed/maintained numeric/DNS/prepared RESP2/3/AUTO ACL cases, concurrent ASKING/ordinary traffic, no one-shot leak and retained outcomes"); return 0;
}
