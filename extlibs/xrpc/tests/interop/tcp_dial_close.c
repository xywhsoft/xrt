/* Production-TU only: public binding interposition, no production test hook. */
#include <xrpc/tcp_dial.h>
static bool tcpDialProbeBind(xnetstream*,ptr,const xnetstreameventsownershipv1*);
#define xrtNetStreamSetEventsOwnedV1 tcpDialProbeBind
#ifndef XRPC_TCP_DIAL_PROBE_SOURCE
#define XRPC_TCP_DIAL_PROBE_SOURCE "../../src/tcp/tcp.c"
#endif
#include XRPC_TCP_DIAL_PROBE_SOURCE
#undef xrtNetStreamSetEventsOwnedV1
#define main tcpDialProbeOrdinaryMain
#include "../test_tcp_dial.c"
#undef main
static xatomic32 TcpDialProbeArmed;
static bool tcpDialProbeBind(xnetstream* Stream,ptr Data,const xnetstreameventsownershipv1* Policy)
{
    xrpctcp* P=Data;
    if(Policy==&xrpcTcpEvents && P->Connect && xrtAtomic32Load(&TcpDialProbeArmed,XMEMORY_ACQUIRE)==1){
        xrtAtomic32Store(&TcpDialProbeArmed,2,XMEMORY_RELEASE);testRequire(xrpcTcpClose(P),"publication-race close");
        xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(P,&I) && I.State==XRPC_TCP_CLOSING,"close did not win before publication");
    }
    return xrtNetStreamSetEventsOwnedV1(Stream,Data,Policy);
}
static void tcpDialProbeRace(unsigned Format)
{
    rpcTcpService S;rpcTcpServiceStart(&S);S.Config.Frame.Format=(xrpcframeformat)Format;tcpDialGate G;tcpDialGateStart(&S,&G);
    xnetaddrlist* Addresses=xrtNetAddrListCreate(&S.Address,1);xrpctcp* P=NULL;xrtAtomic32Store(&TcpDialProbeArmed,1,XMEMORY_RELEASE);
    testRequire(Addresses && xrpcTcpDialAddresses(S.Engine,Addresses,1,&S.Config,NULL,&P),"race factory");xrtNetAddrListDestroy(Addresses);xfuture* F=tcpDialCall(P,5000);xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);
    tcpDialResult(F,XRPC_CALL_CLOSED,XRPC_NOT_SENT);testRequire(xrpcTcpWaitFor(P,5000)==XWAIT_OK && xrtAtomic32Load(&TcpDialProbeArmed,XMEMORY_ACQUIRE)==2,"race duty/injection not observed");
    tcpDialWait(&S.Ready);xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(P,&I) && I.State==XRPC_TCP_CLOSED && I.Reason==XRPC_TCP_LOCAL && !I.WriteFrames,"late winner reopened or sent after close");
    rpcTcpServiceCycleClose(&S,P);testRequire(!xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"closed request executed");xrtFutureDestroy(F);rpcTcpServiceStop(&S);
}
typedef struct tcpDialProbeBudget {xallocationbudget* Budget;xallocationscope Scope;xnetpost Enter,Leave;xatomic32 Ready,Permit,Left;} tcpDialProbeBudget;
static void tcpDialProbeEnter(xnetworker* W,ptr Data)
{
    (void)W;tcpDialProbeBudget* B=Data;testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"owner budget enter");xrtAtomic32Store(&B->Ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&B->Permit,XMEMORY_ACQUIRE))xrtSleep(1);
}
static void tcpDialProbeLeave(xnetworker* W,ptr Data)
{(void)W;tcpDialProbeBudget* B=Data;testRequire(xrtAllocationBudgetLeave(&B->Scope),"owner budget leave");xrtAtomic32Store(&B->Left,1,XMEMORY_RELEASE);}
static void tcpDialProbeOwnerOOM(unsigned Format,bool Prepared)
{
    rpcTcpService S;rpcTcpServiceStart(&S);S.Config.Frame.Format=(xrpcframeformat)Format;tcpDialLookup L;xnetresolver* R=tcpDialResolver(&S,&L,false,false);
    tcpDialProbeBudget B={0};xrtAtomic32Init(&B.Ready,0);xrtAtomic32Init(&B.Permit,0);xrtAtomic32Init(&B.Left,0);
    testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetPostInit(&B.Enter) && xrtNetPostInit(&B.Leave) && xrtNetPost(xrtNetEngineWorker(S.Engine,1),&B.Enter,tcpDialProbeEnter,&B),"owner budget barrier");tcpDialWait(&B.Ready);
    xrpctcp* P=NULL;xnetaddrlist* List=Prepared?xrtNetAddrListCreate(&S.Address,1):NULL;
    bool OK=Prepared?xrpcTcpDialAddresses(S.Engine,List,1,&S.Config,NULL,&P):xrpcTcpDial(S.Engine,R,XRT_STR_LITERAL("rpc.test"),S.Address.Port,1,&S.Config,NULL,&P);
    testRequire(OK,"caller factory independent from owner OOM");xrtNetAddrListDestroy(List);xfuture* F=tcpDialCall(P,5000);
    testRequire(xrtNetPost(xrtNetEngineWorker(S.Engine,1),&B.Leave,tcpDialProbeLeave,&B),"owner leave queued after source start");xrtAtomic32Store(&B.Permit,1,XMEMORY_RELEASE);tcpDialWait(&B.Left);tcpDialResult(F,XRPC_CALL_MEMORY,XRPC_NOT_SENT);
    testRequire(xrpcTcpWaitFor(P,5000)==XWAIT_OK,"owner OOM retirement");xrpctcpinfo I={0};I.Size=sizeof(I);testRequire(xrpcTcpInfo(P,&I) && I.Reason==XRPC_TCP_MEMORY && I.NativeErrorKind==XERR_MEMORY,"owner OOM native cause");
    rpcTcpServiceCycleClose(&S,P);xrtFutureDestroy(F);testRequire(!xrtAtomic32Load(&L.Calls,XMEMORY_RELAXED) && xrtNetResolverDestroy(R),"failed owner submission performed DNS");
    xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B.Budget,&Info) && !Info.LiveBytes && !Info.LiveCount,"owner source allocation leak");xrtAllocationBudgetFree(B.Budget);rpcTcpServiceStop(&S);
}
int main(int Argc,char** Argv)
{
    (void)Argv;setbuf(stdout,NULL);xrtAtomic32Init(&TcpDialProbeArmed,0);
    for(unsigned Format=1;Format<=2;++Format){tcpDialProbeRace(Format);if(Argc>1)continue;tcpDialProbeOwnerOOM(Format,false);tcpDialProbeOwnerOOM(Format,true);}
    puts("[PASS] TCP DNS production-TU: close during winner binding cannot reopen/send; real owner allocation failure/native diagnostics/reference retirement");return 0;
}
