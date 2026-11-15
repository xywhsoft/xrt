#include <xrpc/ws.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct quotaProbe {xatomic32 Ctor,Bounded,Embedded; xnetpost Post[2];} quotaProbe;
static quotaProbe Probe;
static xnetportbackend Backend;
static void quotaRequire(bool OK,cstr M)
{if(!OK){fprintf(stderr,"[FAIL] %s: %d/%s\n",M,xrtErrorKind(xrtGetError()),xrtErrorMessage(xrtGetError()));exit(1);}}
static void quotaBounded(xnetworker* W,ptr Data)
{(void)W;quotaProbe* P=Data;xrtAtomic32FetchAdd(&P->Bounded,1,XMEMORY_RELEASE);}
static void quotaEmbedded(xnetworker* W,ptr Data)
{quotaProbe* P=Data;quotaRequire(!xrtNetPostPending(P->Post+xrtNetWorkerIndex(W)),"Pending cleared before embedded callback begins");xrtAtomic32FetchAdd(&P->Embedded,1,XMEMORY_RELEASE);}
static void quotaFull(xnetworker* W)
{
    xnetengine* E=xrtNetWorkerEngine(W);uint64 Index=xrtNetWorkerIndex(W);xerror* Ambient=xrtTakeError();
    for(unsigned N=0;N<2;++N)quotaRequire(xrtNetEnginePost(E,Index,quotaBounded,&Probe),"fill actual public command queue");
    quotaRequire(!xrtNetEnginePost(E,Index,quotaBounded,&Probe)&&xrtErrorKind(xrtGetError())==XERR_AGAIN,"third public command rejected by capacity2");xrtClearError();
    xnetport* Port=xrtNetWorkerPort(W);quotaRequire(xrtNetPortPost(Port,1,NULL),"fill actual port post quota");quotaRequire(!xrtNetPortPost(Port,2,NULL)&&xrtErrorKind(xrtGetError())==XERR_AGAIN,"second port post rejected by capacity1");xrtClearError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};quotaRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"real allocation quota1");
    quotaRequire(!xrtNetEnginePost(E,Index,quotaBounded,&Probe)&&xrtErrorKind(xrtGetError())==XERR_MEMORY,"ordinary post allocation rejected under actual budget");xrtClearError();
    quotaRequire(xrtNetPostInit(Probe.Post+Index)&&xrtNetPost(W,Probe.Post+Index,quotaEmbedded,&Probe)&&xrtNetPostPending(Probe.Post+Index),"embedded post accepted with all ordinary resources exhausted");
    quotaRequire(xrtAllocationBudgetLeave(&Scope),"leave allocation quota");
    quotaRequire(!xrtNetPost(W,Probe.Post+Index,quotaEmbedded,&Probe)&&xrtErrorKind(xrtGetError())==XERR_STATE,"one queued credit per embedded post");xrtClearError();
xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);quotaRequire(xrtAllocationBudgetInfo(Budget,&Info)&&!Info.LiveBytes&&!Info.LiveCount&&!Info.PeakBytes&&Info.Rejections,"embedded post made no requested heap allocation");xrtAllocationBudgetFree(Budget);xrtClearError();xrtSetErrorTake(Ambient);
}
static void quotaConfig(xnetengineconfig* C)
{xrtNetEngineConfigInit(C);C->CommandCapacity=2;C->PortPostLimit=1;C->NodeCacheBytes=0;C->Backend=Backend;}
static bool quotaAttachTcp(xnetstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out)
{bool Fill=xrtAtomic32Load(&Probe.Ctor,XMEMORY_ACQUIRE)!=0;if(Fill)quotaFull(xrtNetStreamWorker(S));return xrpcWsAttachTcp(S,Initial,C,Out);}
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
static bool quotaAttachTls(xtlsstream* S,xnetbuf* Initial,const xrpcwsconfig* C,xrpcws** Out)
{bool Fill=xrtAtomic32Load(&Probe.Ctor,XMEMORY_ACQUIRE)!=0;if(Fill)quotaFull(xrtNetStreamWorker(xrtTlsStreamTransport(S)));return xrpcWsAttachTls(S,Initial,C,Out);}
#define xrpcWsAttachTls quotaAttachTls
#endif
#define xrtNetEngineConfigInit quotaConfig
#define xrpcWsAttachTcp quotaAttachTcp
#include "../examples/common/ws_service.h"
#undef xrpcWsAttachTcp
#undef xrtNetEngineConfigInit
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
#undef xrpcWsAttachTls
#endif
typedef struct quotaGate {xnetpost Post;xatomic32 Ready,Permit;} quotaGate;
static void quotaBlock(xnetworker* W,ptr Data)
{(void)W;quotaGate* G=Data;xrtAtomic32Store(&G->Ready,1,XMEMORY_RELEASE);rpcWsServiceAwait(&G->Permit);}
static void quotaGateStart(rpcWsService* S,quotaGate* G)
{memset(G,0,sizeof(*G));xrtAtomic32Init(&G->Ready,0);xrtAtomic32Init(&G->Permit,0);quotaRequire(xrtNetPostInit(&G->Post)&&xrtNetPost(xrtNetEngineWorker(S->Engine,1),&G->Post,quotaBlock,G),"owner barrier");rpcWsServiceAwait(&G->Ready);}
static void quotaReset(bool Ctor)
{memset(&Probe,0,sizeof(Probe));xrtAtomic32Init(&Probe.Ctor,Ctor?1:0);xrtAtomic32Init(&Probe.Bounded,0);xrtAtomic32Init(&Probe.Embedded,0);}
static void quotaResult(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{const xrpccallresult* R=rpcWsServiceResult(F);quotaRequire(R->Status==Status&&R->Delivery==Delivery,"typed result without peer Poll/Wait nudges");if(Status==XRPC_CALL_OK){xrpcresponseinfo I={0};I.Size=sizeof(I);quotaRequire(xrpcResponsesInfo(R->Responses,&I)&&R->Index<I.Count&&I.Messages[R->Index].Result.Raw.Size==5&&!memcmp(I.Messages[R->Index].Result.Raw.Data,"[7,9]",5),"exact reply under post exhaustion");}}
static void quotaRun(bool TLS,unsigned Mode)
{
    printf("[case] tls=%u mode=%u\n",TLS,Mode);quotaReset(Mode==0);rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);xfuture* F=NULL;quotaGate Gate;
    if(Mode){quotaGateStart(&S,&Gate);quotaFull(xrtNetEngineWorker(S.Engine,1));}
    xrpcduplexadmission A=Mode==5?xrpcWsNotify(S.Client,XRT_STR_LITERAL("note"),XRT_STR_LITERAL("[7,9]"),5000,&F):xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7,9]"),Mode==3?0:5000,&F);quotaRequire(A==XRPC_DUPLEX_ACCEPTED,"first call admitted under queue saturation");
    if(Mode==1||Mode==3||Mode==4||Mode==5){if(Mode!=3)quotaRequire(xrtFutureWaitFor(F,0)==XWAIT_TIMEOUT,"owner remains gated");if(Mode==4)quotaRequire(xrtFutureCancel(F),"queued cancellation under saturated posts");xrtAtomic32Store(&Gate.Permit,1,XMEMORY_RELEASE);}
    if(Mode==2){quotaRequire(xrpcWsClose(S.Client),"Close embedded wake survives saturated public queues");xrtAtomic32Store(&Gate.Permit,1,XMEMORY_RELEASE);}
    quotaResult(F,Mode==2?XRPC_CALL_CLOSED:Mode==3?XRPC_CALL_TIMEOUT:Mode==4?XRPC_CALL_CANCELLED:Mode==5?XRPC_CALL_NOTIFY_DONE:XRPC_CALL_OK,Mode==2||Mode==3||Mode==4?XRPC_NOT_SENT:Mode==5?XRPC_MAY_HAVE_EXECUTED:XRPC_REPLIED);
    xrpcwsinfo I={0};I.Size=sizeof(I);quotaRequire(xrpcWsInfo(S.Client,&I)&&!I.PostFailures,"RPC private wake did not share ordinary post limits");rpcWsServiceStop(&S);
    quotaRequire(xrtAtomic32Load(&Probe.Bounded,XMEMORY_ACQUIRE)==(Mode==0?4:2)&&xrtAtomic32Load(&Probe.Embedded,XMEMORY_ACQUIRE)==(Mode==0?2:1),"exact admitted task/embedded callbacks and joins");xrtFutureDestroy(F);
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);Backend=Argc==1||!strcmp(Argv[1],"select")?XNET_PORT_SELECT:!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:XNET_PORT_EPOLL;
    for(unsigned Mode=0;Mode<6;++Mode){quotaRun(false,Mode);
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
        quotaRun(true,Mode);
#endif
    }
    puts("[PASS] actual command/port/allocator exhaustion, embedded wake/constructor/first call/Close progress, no physical Poll/Wait result nudges");return 0;
}
