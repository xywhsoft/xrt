/* Instrument the actual production TU only at its receive allocation boundary.
 * This invokes the real allocator under a real quota, not a fake NULL return. */
#include <xrpc/ws.h>
static ptr wsBudgetMalloc(size_t);
#define xrtMalloc wsBudgetMalloc
#include "../../src/ws/ws.c"
#undef xrtMalloc
#define main wsOrdinaryMain
#include "../test_ws.c"
#undef main
static xatomic32 WsOOMArmed;
static ptr wsBudgetMalloc(size_t Size)
{
    uint32 Expected=1;if(!xrtAtomic32CompareExchange(&WsOOMArmed,&Expected,2,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return xrtMalloc(Size);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"actual receive owner quota");ptr R=xrtMalloc(Size);xerror* Error=xrtTakeError();testRequire(!R&&Error&&xrtErrorKind(Error)==XERR_MEMORY&&xrtAllocationBudgetLeave(&Scope),"actual production receive allocator rejection");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I)&&!I.LiveBytes&&!I.LiveCount&&I.Rejections,"receive allocation quota leak");xrtAllocationBudgetFree(B);xrtClearError();xrtSetErrorTake(Error);return R;
}
static void wsOwnerOOM(bool TLS)
{
    rpcWsService S;rpcWsServiceStart(&S,TLS,NULL,false);xrtAtomic32Store(&WsOOMArmed,1,XMEMORY_RELEASE);xfuture* F=NULL;testRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"owner OOM call");testRequire(xrpcWsWaitFor(S.Accepted,5000)==XWAIT_OK&&xrtAtomic32Load(&WsOOMArmed,XMEMORY_ACQUIRE)==2,"owner OOM native retirement and injection");xrpcwsinfo I={0};I.Size=sizeof(I);testRequire(xrpcWsInfo(S.Accepted,&I)&&I.Reason==XRPC_WS_MEMORY&&!I.ReadMessages&&!I.ReceiveCapacity&&!xrtAtomic32Load(&S.Calls,XMEMORY_RELAXED),"receive OOM closes before business admission");wsResult(F,XRPC_CALL_TRANSPORT,XRPC_MAY_HAVE_EXECUTED);rpcWsServiceStop(&S);xrtFutureDestroy(F);
}
int main(void)
{
    xrtAtomic32Init(&WsOOMArmed,0);wsOwnerOOM(false);
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    wsOwnerOOM(true);
#endif
    puts("[PASS] WS/WSS actual production-TU owner receive OOM/no business execution/no replay/native physical retirement");return 0;
}
