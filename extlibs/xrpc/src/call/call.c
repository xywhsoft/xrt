#include <xrpc/call.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <math.h>

#if defined(XRPC_FEATURE_XRPC_CALL)
struct xrpccall {
	xatomic32 References;
	xrpccallconfig Config;
	double Deadline;
	xpromise* Promise;
	xcancel* Cancel;
	xrpccallresult* Result;
	xrpcdelivery Delivery;
	bool ContextOwned, Destroying;
};
static bool xrpcCallReport(xerrkind Kind,xrpccallerror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static void xrpcCallResultDrop(ptr Value,ptr User)
{
	(void)User;xrpccallresult* R=Value;
	xrpcResponsesRelease(R->Responses);xrtErrorFree((xerror*)R->Cause);xrtFree(R);
}
static void xrpcCallUnref(xrpccall* C)
{
	if(xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
	xrtCancelDestroy(C->Cancel);
	if(C->ContextOwned && C->Config.Drop)C->Config.Drop(C->Config.User);
	xrtFree(C);
}
static void xrpcCallWake(const void* User)
{const xrpccall* C=User;if(C->Config.Wake)C->Config.Wake(C->Config.User);}
static bool xrpcCallRefTry(const void* Value)
{
	xrpccall* C=(xrpccall*)Value;uint32 Count=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
	while(Count!=UINT32_MAX){if(xrtAtomic32CompareExchange(&C->References,&Count,Count+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
	return false;
}
static void xrpcCallProducerDrop(const void* Value){xrpcCallUnref((xrpccall*)Value);}
static bool xrpcCallCount(const void* Value,size_t* Count)
{if(!Value || !Count)return false;*Count=xrtAtomic32Load(&((const xrpccall*)Value)->References,XMEMORY_RELAXED);return true;}
static bool xrpcCallOpaque(const void* Value,xrtownershipvisitor Visit,ptr Context)
{
	/* This is a real native RC node, not a synthetic leaf. Its activation and
	 * arbitrary native context have no collector admission/closed trace policy.
	 * Refuse traversal rather than certify an incomplete ownership graph. */
	(void)Value;(void)Visit;(void)Context;return false;
}
static const xrtownershipops xrpcCallOps={xrpcCallCount,xrpcCallOpaque};
static const xfutureproducerownershipv1 xrpcCallProducer={sizeof(xfutureproducerownershipv1),xrpcCallProducerDrop};
static const xfutureproducercancellationv1 xrpcCallCancellation={sizeof(xfutureproducercancellationv1),xrpcCallRefTry,xrpcCallWake};
XRPC_API bool xrpcCallConfigInit(xrpccallconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call config output");
	*Config=(xrpccallconfig){sizeof(*Config),5000,NULL,NULL,NULL};return true;
}
XRPC_API bool xrpcCallCreate(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future)
{
	double Began=xrtTimer();
	if(!Call || !Future || !xrtMemRangeValid(Call,sizeof(*Call)) || !xrtMemRangeValid(Future,sizeof(*Future)) ||
		xrtMemRangesOverlap(Call,sizeof(*Call),Future,sizeof(*Future)) || (Config &&
		(!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Call,sizeof(*Call),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Future,sizeof(*Future),Config,sizeof(*Config)))))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call constructor spans/overlap");
	xrpccallconfig Limits;if(Config)Limits=*Config;else xrpcCallConfigInit(&Limits);
	if(Limits.Size!=sizeof(Limits) || (Limits.Timeout<0 && Limits.Timeout!=XRT_WAIT_FOREVER))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call configuration");
	xrpccall* C=xrtMalloc(sizeof(*C));if(!C)return xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call allocation failed");
	*C=(xrpccall){0};xrtAtomic32Init(&C->References,2);C->Config=Limits;
	C->Deadline=Limits.Timeout==XRT_WAIT_FOREVER?INFINITY:Limits.Timeout==0?Began:
		nextafter(Began+(double)(Limits.Timeout/1000)+(double)(Limits.Timeout%1000)*.001,INFINITY);
	xfuture* Observer=NULL;C->Result=xrtMalloc(sizeof(*C->Result));
	if(!C->Result)xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call result allocation failed");
	if(C->Result){*C->Result=(xrpccallresult){0};C->Promise=xrtPromiseCreate(&Observer,NULL);}
	if(C->Promise)C->Cancel=xrtPromiseCancelToken(C->Promise);
	bool Bound=C->Cancel && xrtPromiseProducerBindTakeV2(C->Promise,(xrtownershipref){C,&xrpcCallOps},&xrpcCallProducer,&xrpcCallCancellation);
	if(!Bound){
		xerror* Error=xrtTakeError();xrtPromiseDestroy(C->Promise);xrtFutureDestroy(Observer);xrtFree(C->Result);xrtCancelDestroy(C->Cancel);xrtFree(C);
		xrtClearError();xrtSetErrorTake(Error);return false;
	}
	C->ContextOwned=true;*Call=C;*Future=Observer;return true;
}
XRPC_API bool xrpcCallMarkSent(xrpccall* Call)
{
	if(!Call)return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");
	if(!Call->Promise)return false;
	Call->Delivery=XRPC_MAY_HAVE_EXECUTED;return true;
}
XRPC_API double xrpcCallDeadline(const xrpccall* Call)
{if(!Call){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return NAN;}return Call->Deadline;}
XRPC_API bool xrpcCallCancellationRequested(const xrpccall* Call)
{if(!Call)return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return xrtCancelRequested(Call->Cancel);}
XRPC_API xrpccallfinish xrpcCallFinish(xrpccall* C,xrpccallstatus Status,xrpcresponses* Responses,size_t Index,const xerror* Cause)
{
	if(!C){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return XRPC_CALL_FINISH_ERROR;}
	if(!C->Promise)return XRPC_CALL_ALREADY_DONE;
	bool Reply=Status==XRPC_CALL_OK || Status==XRPC_CALL_REMOTE_ERROR;
	if(Status<XRPC_CALL_OK || Status>XRPC_CALL_MEMORY || (Reply?!Responses:(Responses!=NULL || Index!=0)))goto Argument;
	if(Reply){xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
		if(!xrpcResponsesInfo(Responses,&Info))return XRPC_CALL_FINISH_ERROR;
		if(Index>=Info.Count || Info.Messages[Index].Kind!=(Status==XRPC_CALL_OK?XRPC_MESSAGE_RESULT:XRPC_MESSAGE_ERROR))goto Argument;
		if(!xrpcResponsesRetain(Responses))return XRPC_CALL_FINISH_ERROR;
	}
	xerror* Held=Cause?xrtErrorRef(Cause):NULL;
	if(Cause && !Held){xrpcResponsesRelease(Responses);return XRPC_CALL_FINISH_ERROR;}
	if(!xrpcCallRefTry(C)){
		xrpcResponsesRelease(Responses);xrtErrorFree(Held);xrpcCallReport(XERR_RANGE,XRPC_CALL_STATE,"RPC call references overflow");return XRPC_CALL_FINISH_ERROR;}
	if(xrtTimer()>=C->Deadline)Status=XRPC_CALL_TIMEOUT;
	else if(xrtCancelRequested(C->Cancel))Status=XRPC_CALL_CANCELLED;
	if(Status!=XRPC_CALL_OK && Status!=XRPC_CALL_REMOTE_ERROR){xrpcResponsesRelease(Responses);Responses=NULL;Index=0;}
	xpromise* Promise=C->Promise;xrpccallresult* Result=C->Result;
	*Result=(xrpccallresult){Status,Reply?XRPC_REPLIED:C->Delivery,Responses,Index,Held};
	C->Promise=NULL;C->Result=NULL;
	xerror* Previous=xrtTakeError();bool Published=xrtPromiseResolveOwned(Promise,Result,xrpcCallResultDrop,NULL);
	if(Published)xrtPromiseDestroy(Promise);
	else {C->Promise=Promise;C->Result=Result;xrpcResponsesRelease(Result->Responses);xrtErrorFree((xerror*)Result->Cause);*Result=(xrpccallresult){0};}
	if(Published){xrpcCallUnref(C);xrtClearError();xrtSetErrorTake(Previous);return XRPC_CALL_FINISHED;}
	xerror* Error=xrtTakeError();xrpcCallUnref(C);xrtErrorFree(Previous);xrtClearError();xrtSetErrorTake(Error);return XRPC_CALL_FINISH_ERROR;
Argument:
	xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call terminal status/response/index");return XRPC_CALL_FINISH_ERROR;
}
XRPC_API xrpccallfinish xrpcCallPoll(xrpccall* C)
{
	if(!C){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return XRPC_CALL_FINISH_ERROR;}
	if(!C->Promise)return XRPC_CALL_ALREADY_DONE;
	if(xrtTimer()>=C->Deadline)return xrpcCallFinish(C,XRPC_CALL_TIMEOUT,NULL,0,NULL);
	if(xrtCancelRequested(C->Cancel))return xrpcCallFinish(C,XRPC_CALL_CANCELLED,NULL,0,NULL);
	return XRPC_CALL_PENDING;
}
XRPC_API bool xrpcCallDestroy(xrpccall* C)
{
	if(!C || C->Destroying)return true;
	xerror* Previous=xrtTakeError();
	C->Destroying=true;
	if(C->Promise && xrpcCallFinish(C,XRPC_CALL_CLOSED,NULL,0,NULL)==XRPC_CALL_FINISH_ERROR){
		C->Destroying=false;xerror* Error=xrtTakeError();xrtErrorFree(Previous);xrtClearError();xrtSetErrorTake(Error);return false;}
	xrpcCallUnref(C);xrtClearError();xrtSetErrorTake(Previous);return true;
}
#endif
