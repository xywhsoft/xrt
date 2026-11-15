/* Real production TU: observe its embedded wake while making actual ordinary
 * queues/full allocator budgets unavailable. No mocked success/failure return. */
#include <xrpc/ws.h>
static bool quotaOwnerPost(xnetworker*,xnetpost*,xnettaskproc,ptr);
#define xrtNetPost quotaOwnerPost
#include "../../src/ws/ws.c"
#undef xrtNetPost
#define main quotaPublicMain
#include "../test_ws_post.c"
#undef main
static xatomic32 QuotaOwnerArmed;
static xrpcws* QuotaOwnerPeer;
static bool quotaOwnerPost(xnetworker* W,xnetpost* Post,xnettaskproc Task,ptr Data)
{
#if defined(WS_QUOTA_NEGATIVE)
    /* Deliberately unsafe substitution, used only as an expected negative
     * control. It destroys the embedded channel's resource independence. */
    (void)Post;return xrtNetEnginePost(xrtNetWorkerEngine(W),xrtNetWorkerIndex(W),Task,Data);
#else
    if(xrtAtomic32Load(&QuotaOwnerArmed,XMEMORY_ACQUIRE)==1&&Data==QuotaOwnerPeer&&xrtNetWorkerIsCurrent(W)){
        quotaFull(W);xallocationbudget* B=NULL;xallocationscope Scope={0};quotaRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"incoming wake actual quota");
        bool OK=xrtNetPost(W,Post,Task,Data);quotaRequire(OK&&xrtAllocationBudgetLeave(&Scope),"actual production incoming wake survives ordinary resource exhaustion");xallocationbudgetinfo I={0};I.Size=sizeof(I);quotaRequire(xrtAllocationBudgetInfo(B,&I)&&!I.LiveBytes&&!I.LiveCount&&!I.PeakBytes&&!I.Rejections,"incoming production wake did not request heap");xrtAllocationBudgetFree(B);xrtAtomic32Store(&QuotaOwnerArmed,2,XMEMORY_RELEASE);return OK;
    }
    return xrtNetPost(W,Post,Task,Data);
#endif
}
#if !defined(WS_QUOTA_NEGATIVE)
static void quotaIncomingGate(xnetworker* W,ptr Data)
{quotaBlock(W,Data);}
static void quotaIncoming(bool TLS)
{
    quotaReset(false);rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);quotaGate G={0};xrtAtomic32Init(&G.Ready,0);xrtAtomic32Init(&G.Permit,0);quotaRequire(xrtNetPostInit(&G.Post)&&xrtNetPost(xrtNetEngineWorker(S.Engine,0),&G.Post,quotaIncomingGate,&G),"idle inbound owner barrier");rpcWsServiceAwait(&G.Ready);
    QuotaOwnerPeer=S.Accepted;xrtAtomic32Store(&QuotaOwnerArmed,1,XMEMORY_RELEASE);xfuture* F=NULL;quotaRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7,9]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"incoming-only server request");xrtAtomic32Store(&G.Permit,1,XMEMORY_RELEASE);quotaResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    xrpcwsinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;do{quotaRequire(xrpcWsInfo(S.Accepted,&I)&&xrtTimer()<End,"inbound diagnostic publication");if(I.ReadMessages==1&&I.WriteMessages==1)break;xrtSleep(1);}while(true);quotaRequire(!I.PostFailures&&xrtAtomic32Load(&QuotaOwnerArmed,XMEMORY_ACQUIRE)==2,"inbound-only peer progressed exactly once without Poll/Wait");rpcWsServiceStop(&S);quotaRequire(xrtAtomic32Load(&Probe.Bounded,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&Probe.Embedded,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE)==1,"exact inbound callbacks, no replay and full host join");xrtFutureDestroy(F);
}
#endif
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);xrtAtomic32Init(&QuotaOwnerArmed,0);QuotaOwnerPeer=NULL;Backend=Argc==1||!strcmp(Argv[1],"select")?XNET_PORT_SELECT:!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:XNET_PORT_EPOLL;
#if defined(WS_QUOTA_NEGATIVE)
    quotaRun(false,0);
#else
    quotaIncoming(false);
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    quotaIncoming(true);
#endif
#endif
    puts("[PASS] actual production-TU incoming WS/WSS embedded wake under real command/port/allocator exhaustion");return 0;
}
