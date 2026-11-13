/* Exercise the actual production translation unit at public binding boundaries. */
#include <xrpc/tls_dial.h>
static bool tlsDialProbeBind(xnetstream*,ptr,const xnetstreameventsownershipv1*);
#define xrtNetStreamSetEventsOwnedV1 tlsDialProbeBind
#ifndef XRPC_TLS_DIAL_PROBE_SOURCE
#define XRPC_TLS_DIAL_PROBE_SOURCE "../../src/tls/tls.c"
#endif
#include XRPC_TLS_DIAL_PROBE_SOURCE
#undef xrtNetStreamSetEventsOwnedV1
#define main tlsDialProbeOrdinaryMain
#include "../test_tls_dial.c"
#undef main
static xatomic32 TlsDialProbeArmed;
static bool tlsDialProbeBind(xnetstream* Stream,ptr Data,const xnetstreameventsownershipv1* Policy)
{
    xrpctls* P=Data;
    if(Policy==&xrpcTlsPeerRawEvents && P->Connect && xrtAtomic32Load(&TlsDialProbeArmed,XMEMORY_ACQUIRE)==1){
        xrtAtomic32Store(&TlsDialProbeArmed,2,XMEMORY_RELEASE);testRequire(xrpcTlsClose(P),"TLS winner publication close");
        xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(P,&I) && I.State==XRPC_TLS_CLOSING,"TLS Close did not win before publication");
    }
    return xrtNetStreamSetEventsOwnedV1(Stream,Data,Policy);
}
static void tlsDialProbeRace(unsigned Format)
{
    rpcTlsService S;rpcTlsServiceStart(&S,Format,false,false,false);tlsDialGate G;tlsDialGateStart(&S,&G);
    xtlsclientconfig C;rpcTlsServiceClient(&S,&C);xnetaddrlist* List=xrtNetAddrListCreate(&S.Address,1);xrpctls* P=NULL;xrtAtomic32Store(&TlsDialProbeArmed,1,XMEMORY_RELEASE);
    testRequire(List && xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,NULL,&P),"TLS race factory");xrtNetAddrListDestroy(List);xfuture* F=tlsDialCall(P,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
    tlsDialResult(F,XRPC_CALL_CLOSED,XRPC_NOT_SENT);testRequire(xrpcTlsWaitFor(P,5000)==XWAIT_OK && xrtAtomic32Load(&TlsDialProbeArmed,XMEMORY_ACQUIRE)==2,"TLS race duty/injection");
    tlsDialWait(&S.Ready);xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(P,&I) && I.State==XRPC_TLS_CLOSED && I.Reason==XRPC_TLS_LOCAL && !I.WriteFrames && !xrtAtomic32Load(&S.VerifyCalls,XMEMORY_RELAXED),"late TLS winner reopened or began handshake after Close");
    rpcTlsServiceCycleClose(&S,P);testRequire(!xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"closed TLS request executed");xrtFutureDestroy(F);rpcTlsServiceStop(&S);
}
typedef struct tlsDialProbeBudget {xallocationbudget* Budget;xallocationscope Scope;xnetpost Enter,Leave;xatomic32 Ready,Permit,Left;} tlsDialProbeBudget;
static void tlsDialProbeEnter(xnetworker* W,ptr Data)
{(void)W;tlsDialProbeBudget* B=Data;testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"TLS owner budget enter");xrtAtomic32Store(&B->Ready,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&B->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void tlsDialProbeLeave(xnetworker* W,ptr Data)
{(void)W;tlsDialProbeBudget* B=Data;testRequire(xrtAllocationBudgetLeave(&B->Scope),"TLS owner budget leave");xrtAtomic32Store(&B->Left,1,XMEMORY_RELEASE);}
static void tlsDialProbeOwnerOOM(unsigned Format,bool Prepared)
{
    rpcTlsService S;rpcTlsServiceStart(&S,Format,false,false,false);tlsDialLookup L;xnetresolver* R=tlsDialResolver(&S,&L,false,false);
    tlsDialProbeBudget B={0};xrtAtomic32Init(&B.Ready,0);xrtAtomic32Init(&B.Permit,0);xrtAtomic32Init(&B.Left,0);
    testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetPostInit(&B.Enter) && xrtNetPostInit(&B.Leave) && xrtNetPost(xrtNetEngineWorker(S.Engine,1),&B.Enter,tlsDialProbeEnter,&B),"TLS owner budget barrier");tlsDialWait(&B.Ready);
    xtlsclientconfig C;rpcTlsServiceClient(&S,&C);xrpctls* P=NULL;xnetaddrlist* List=Prepared?xrtNetAddrListCreate(&S.Address,1):NULL;
    bool OK=Prepared?xrpcTlsDialAddresses(S.Engine,List,1,&C,&S.Config,NULL,&P):xrpcTlsDial(S.Engine,R,XRT_STR_LITERAL("route.test"),S.Address.Port,1,&C,&S.Config,NULL,&P);
    testRequire(OK,"TLS caller snapshot independent from owner OOM");xrtNetAddrListDestroy(List);xfuture* F=tlsDialCall(P,5000);
    testRequire(xrtNetPost(xrtNetEngineWorker(S.Engine,1),&B.Leave,tlsDialProbeLeave,&B),"TLS owner leave after source start");xrtAtomic32Store(&B.Permit,1,XMEMORY_RELEASE);tlsDialWait(&B.Left);tlsDialResult(F,XRPC_CALL_MEMORY,XRPC_NOT_SENT);
    testRequire(xrpcTlsWaitFor(P,5000)==XWAIT_OK,"TLS owner OOM retirement");xrpctlsinfo I={0};I.Size=sizeof(I);testRequire(xrpcTlsInfo(P,&I) && I.Reason==XRPC_TLS_MEMORY && I.NativeErrorKind==XERR_MEMORY,"TLS owner OOM diagnostics");
    rpcTlsServiceCycleClose(&S,P);xrtFutureDestroy(F);testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"TLS owner OOM submitted DNS");
    xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B.Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"TLS source owner allocation leak");xrtAllocationBudgetFree(B.Budget);rpcTlsServiceStop(&S);
}
int main(int Argc,char** Argv)
{
    (void)Argv;setbuf(stdout,NULL);xrtAtomic32Init(&TlsDialProbeArmed,0);
    for(unsigned Format=1;Format<=2;++Format){tlsDialProbeRace(Format);if(Argc>1)continue;tlsDialProbeOwnerOOM(Format,false);tlsDialProbeOwnerOOM(Format,true);}
    puts("[PASS] TLS DNS production-TU: concurrent winner publication Close and real Worker OOM, no handshake/replay, physical source/native retirement");return 0;
}
