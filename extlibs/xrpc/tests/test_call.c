#include "test.h"
#include <math.h>

typedef struct rpccallcontext {unsigned Wakes,Drops;} rpccallcontext;
static void rpcCallWake(ptr User){++((rpccallcontext*)User)->Wakes;}
static void rpcCallDrop(ptr User){++((rpccallcontext*)User)->Drops;}
static xrpccallconfig rpcCallConfig(void)
{xrpccallconfig C;testRequire(xrpcCallConfigInit(&C),"call config");C.Timeout=XRT_WAIT_FOREVER;return C;}
static xrpcresponses* rpcCallReplies(void)
{
	xrpcmessage Scratch[2];xrpcresponses* R=NULL;
	testRequire(xrpcResponsesRead(XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[1,2,3]},{\"jsonrpc\":\"2.0\",\"id\":2,\"error\":{\"code\":-1,\"message\":\"m\"}}]"),NULL,Scratch,2,&R)==XRPC_RESPONSE_DONE,"call replies");return R;
}
static const xrpccallresult* rpcCallResult(xfuture* F)
{xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value && !R.Error,"typed call Future terminal");return R.Value;}
static void testRpcCallOutcomes(void)
{
	xrpcresponses* Responses=rpcCallReplies();
	for(unsigned Status=XRPC_CALL_OK;Status<=XRPC_CALL_MEMORY;++Status)for(unsigned Sent=0;Sent<2;++Sent){
		rpccallcontext Context={0};xrpccallconfig Config=rpcCallConfig();Config.Wake=rpcCallWake;Config.Drop=rpcCallDrop;Config.User=&Context;
		xrpccall* C=NULL;xfuture* F=NULL;testRequire(xrpcCallCreate(&Config,&C,&F) && xrtFutureState(F)==XFUTURE_PENDING && isinf(xrpcCallDeadline(C)),"call create");
		if(Sent)testRequire(xrpcCallMarkSent(C),"call mark sent");
		bool Reply=Status==XRPC_CALL_OK || Status==XRPC_CALL_REMOTE_ERROR;size_t Index=Status==XRPC_CALL_REMOTE_ERROR?1:0;
		testRequire(xrpcCallFinish(C,(xrpccallstatus)Status,Reply?Responses:NULL,Index,NULL)==XRPC_CALL_FINISHED,"call finish status");
		const xrpccallresult* R=rpcCallResult(F);testRequire(R->Status==(xrpccallstatus)Status &&
			R->Delivery==(Reply?XRPC_REPLIED:Sent?XRPC_MAY_HAVE_EXECUTED:XRPC_NOT_SENT) && R->Responses==(Reply?Responses:NULL) && R->Index==Index && !R->Cause,"call typed outcome/knowledge");
		testRequire(!xrpcCallMarkSent(C) && xrpcCallPoll(C)==XRPC_CALL_ALREADY_DONE &&
			xrpcCallFinish(C,(xrpccallstatus)999,(xrpcresponses*)(uintptr_t)1,999,NULL)==XRPC_CALL_ALREADY_DONE,"terminal call inspected/recompleted payload");
		testRequire(xrpcCallDestroy(C) && Context.Drops==1 && Context.Wakes==0,"producer context retirement");
		R=rpcCallResult(F);testRequire(R->Status==(xrpccallstatus)Status,"result did not outlive producer/context");xrtFutureDestroy(F);
	}
	xrpcResponsesRelease(Responses);
	xrpccall* C=NULL;xfuture* F=NULL;xrpccallconfig Config=rpcCallConfig();
	testRequire(xrpcCallCreate(&Config,&C,&F) && xrpcCallDestroy(C),"unfinished call destruction");testRequire(rpcCallResult(F)->Status==XRPC_CALL_CLOSED,"destroy did not close typed Future");xrtFutureDestroy(F);
	testRequire(xrpcCallCreate(&Config,&C,&F),"dropped observer call");xrtFutureDestroy(F);
	testRequire(!xrpcCallCancellationRequested(C) && xrpcCallPoll(C)==XRPC_CALL_PENDING && xrpcCallFinish(C,XRPC_CALL_CLOSED,NULL,0,NULL)==XRPC_CALL_FINISHED && xrpcCallDestroy(C),"observer drop cancelled accepted producer");
}
static void testRpcCallStopAndCause(void)
{
	rpccallcontext Context={0};xrpccallconfig Config=rpcCallConfig();Config.User=&Context;Config.Wake=rpcCallWake;
	xrpccall* C=NULL;xfuture* F=NULL;testRequire(xrpcCallCreate(&Config,&C,&F) && xrtFutureCancel(F) && !xrtFutureCancel(F) && Context.Wakes==1 &&
		xrtFutureState(F)==XFUTURE_PENDING && xrpcCallCancellationRequested(C),"cooperative cancellation fabricated terminal/repeated wake");
	testRequire(xrpcCallPoll(C)==XRPC_CALL_FINISHED && rpcCallResult(F)->Status==XRPC_CALL_CANCELLED && xrpcCallDestroy(C),"cancel poll");xrtFutureDestroy(F);
	testRequire(xrpcCallCreate(&Config,&C,&F),"token cancellation create");xcancel* Token=xrtFutureCancelToken(F);
	testRequire(Token && xrtCancelRequest(Token) && Context.Wakes==1 && xrpcCallPoll(C)==XRPC_CALL_FINISHED,"raw token poll without implicit producer wake");xrtCancelDestroy(Token);testRequire(xrpcCallDestroy(C),"token call destroy");xrtFutureDestroy(F);
	Config.Timeout=0;testRequire(xrpcCallCreate(&Config,&C,&F) && xrtFutureCancel(F),"immediate deadline call");
	xrpcresponses* Responses=rpcCallReplies();testRequire(xrpcCallFinish(C,XRPC_CALL_OK,Responses,0,NULL)==XRPC_CALL_FINISHED,"late response finish");
	const xrpccallresult* R=rpcCallResult(F);testRequire(R->Status==XRPC_CALL_TIMEOUT && R->Delivery==XRPC_REPLIED && !R->Responses,"deadline priority/late reply knowledge/payload discard");testRequire(xrpcCallDestroy(C),"late call destroy");xrtFutureDestroy(F);xrpcResponsesRelease(Responses);
	Config=rpcCallConfig();testRequire(xrpcCallCreate(&Config,&C,&F) && xrpcCallMarkSent(C),"cause call");
	xrtSetErrorInfo(XERR_IO,"xrt.test",123,"explicit");xerror* Cause=xrtErrorRef(xrtGetError());xrtClearError();
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",999,"unrelated ambient");const xerror* Ambient=xrtGetError();
	testRequire(xrpcCallFinish(C,XRPC_CALL_TRANSPORT,NULL,0,Cause)==XRPC_CALL_FINISHED && xrtGetError()==Ambient,"explicit failure completion changed ambient");xrtErrorFree(Cause);xrtClearError();
	R=rpcCallResult(F);testRequire(R->Status==XRPC_CALL_TRANSPORT && R->Delivery==XRPC_MAY_HAVE_EXECUTED && R->Cause && xrtErrorCode(R->Cause)==123,"explicit cause retained");testRequire(xrpcCallDestroy(C),"cause producer destroy");xrtFutureDestroy(F);
	testRequire(xrpcCallCreate(&Config,&C,&F),"no-cause call");xrtSetErrorInfo(XERR_VALUE,"xrt.test",999,"stale");
	testRequire(xrpcCallFinish(C,XRPC_CALL_PROTOCOL,NULL,0,NULL)==XRPC_CALL_FINISHED && !rpcCallResult(F)->Cause,"ambient diagnostic misattributed");xrtClearError();testRequire(xrpcCallDestroy(C),"no cause destroy");xrtFutureDestroy(F);
}
typedef struct rpccallreentry {xrpccall* Call;unsigned Called,Dropped;} rpccallreentry;
static void rpcCallReentryDrop(ptr User){++((rpccallreentry*)User)->Dropped;xrtSetErrorInfo(XERR_VALUE,"xrt.test",321,"drop diagnostic");}
static void rpcCallReentry(ptr User)
{
	rpccallreentry* R=User;++R->Called;testRequire(xrpcCallPoll(R->Call)==XRPC_CALL_ALREADY_DONE &&
		xrpcCallFinish(R->Call,XRPC_CALL_CLOSED,NULL,0,NULL)==XRPC_CALL_ALREADY_DONE && xrpcCallDestroy(R->Call),"completion notification reentry");R->Call=NULL;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",456,"notification diagnostic");
}
static void testRpcCallReentry(void)
{
	for(unsigned Destroy=0;Destroy<2;++Destroy){rpccallreentry Context={0};xrpccallconfig Config=rpcCallConfig();Config.User=&Context;Config.Drop=rpcCallReentryDrop;xfuture* F=NULL;
		testRequire(xrpcCallCreate(&Config,&Context.Call,&F),"reentrant call create");xfuturewatch Watch;
		testRequire(xrtFutureWatchInit(&Watch,rpcCallReentry,NULL,&Context) && xrtFutureWatchAdd(F,&Watch)==XFUTURE_WATCH_PENDING,"call completion watch");
		xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"caller ambient");const xerror* Ambient=xrtGetError();
		if(Destroy)testRequire(xrpcCallDestroy(Context.Call),"destroy/reentrant destroy");
		else testRequire(xrpcCallFinish(Context.Call,XRPC_CALL_CLOSED,NULL,0,NULL)==XRPC_CALL_FINISHED,"finish/reentrant destroy");
		testRequire(Context.Called==1 && Context.Dropped==1 && !Context.Call && xrtGetError()==Ambient,"notification/drop tail lifetime/error preservation");
		xrtFutureWatchRemove(F,&Watch);xrtClearError();testRequire(rpcCallResult(F)->Status==XRPC_CALL_CLOSED,"reentrant result");xrtFutureDestroy(F);
	}
}
static void testRpcCallFailures(void)
{
	xrpccallconfig Config=rpcCallConfig();xrpccall* C=NULL;xfuture* F=NULL;Config.Size=0;
	testRequire(!xrpcCallCreate(&Config,&C,&F) && !C && !F,"bad call ABI config");xrtClearError();Config=rpcCallConfig();Config.Timeout=-2;
	testRequire(!xrpcCallCreate(&Config,&C,&F) && !C && !F,"bad call timeout");xrtClearError();
	testRequire(!xrpcCallCreate(NULL,&C,(xfuture**)&C) && !xrpcCallCreate(&Config,(xrpccall**)&Config,&F),"call alias inputs");xrtClearError();
	Config=rpcCallConfig();testRequire(xrpcCallCreate(&Config,&C,&F),"invalid finish call");xrpcresponses* Responses=rpcCallReplies();
	testRequire(xrpcCallFinish(C,XRPC_CALL_OK,Responses,1,NULL)==XRPC_CALL_FINISH_ERROR && xrpcCallFinish(C,XRPC_CALL_OK,Responses,99,NULL)==XRPC_CALL_FINISH_ERROR &&
		xrpcCallFinish(C,XRPC_CALL_PROTOCOL,Responses,0,NULL)==XRPC_CALL_FINISH_ERROR && xrpcCallFinish(C,XRPC_CALL_OK,NULL,0,NULL)==XRPC_CALL_FINISH_ERROR &&
		xrtFutureState(F)==XFUTURE_PENDING,"bad response/status changed call");xrtClearError();xrpcResponsesRelease(Responses);testRequire(xrpcCallDestroy(C),"bad finish destroy");xrtFutureDestroy(F);
	testRequire(!xrpcCallConfigInit(NULL) && !xrpcCallCreate(NULL,NULL,&F) && !xrpcCallMarkSent(NULL) &&
		xrpcCallPoll(NULL)==XRPC_CALL_FINISH_ERROR && isnan(xrpcCallDeadline(NULL)) && xrpcCallDestroy(NULL),"call NULL API");xrtClearError();
	rpccallcontext Context={0};Config=rpcCallConfig();Config.User=&Context;Config.Drop=rpcCallDrop;
	unsigned Success=0,Failure=0;
	for(size_t Limit=1;Limit<=4096;++Limit){xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"call construction quota enter");C=NULL;F=NULL;unsigned Before=Context.Drops;
		if(xrpcCallCreate(&Config,&C,&F)){++Success;testRequire(C && F && xrpcCallDestroy(C) && Context.Drops==Before+1 && rpcCallResult(F)->Status==XRPC_CALL_CLOSED,"quota successful lifecycle");xrtFutureDestroy(F);}
		else {++Failure;testRequire(!C && !F && Context.Drops==Before,"failed create consumed context/published output");}
		xrtClearError();testRequire(xrtAllocationBudgetLeave(&S),"call quota leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"call constructor quota leak");xrtAllocationBudgetFree(B);
	}
	testRequire(Success && Failure,"call quota sweep missed boundary");
	Config=rpcCallConfig();testRequire(xrpcCallCreate(&Config,&C,&F),"allocation-free finish call");Responses=rpcCallReplies();xallocationbudget* B=NULL;xallocationscope S={0};
	testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&S),"call completion quota enter");
	testRequire(xrpcCallFinish(C,XRPC_CALL_OK,Responses,0,NULL)==XRPC_CALL_FINISHED && xrpcCallDestroy(C),"preallocated completion under one-byte quota");
	testRequire(xrtAllocationBudgetLeave(&S),"completion quota leave");xrtAllocationBudgetFree(B);xrpcResponsesRelease(Responses);
	const xrpccallresult* R=rpcCallResult(F);xrpcresponseinfo I={0};I.Size=sizeof(I);testRequire(R->Responses && xrpcResponsesInfo(R->Responses,&I) && I.Messages[R->Index].Result.Raw.Size==7,"retained result after producer/source release");xrtFutureDestroy(F);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct rpccallrace {xatomic32 Entered,Permit,Finished,Drops;xfuture* Future;xrpccall* Call;} rpccallrace;
static void rpcCallRaceWake(ptr User)
{rpccallrace* R=User;xrtAtomic32Store(&R->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&R->Permit,XMEMORY_ACQUIRE))xrtSleep(1);}
static void rpcCallRaceDrop(ptr User){rpccallrace* R=User;xrtAtomic32FetchAdd(&R->Drops,1,XMEMORY_RELAXED);}
static int32 rpcCallRaceCancel(ptr User){rpccallrace* R=User;return xrtFutureCancel(R->Future)?0:1;}
static int32 rpcCallRaceFinish(ptr User)
{rpccallrace* R=User;if(xrpcCallFinish(R->Call,XRPC_CALL_CLOSED,NULL,0,NULL)!=XRPC_CALL_FINISHED || !xrpcCallDestroy(R->Call))return 1;xrtAtomic32Store(&R->Finished,1,XMEMORY_RELEASE);return 0;}
static void testRpcCallRetirement(void)
{
	/* Controlled adversarial callback suspension models descheduling in Wake;
	 * production callbacks must only signal/post and never wait for their owner. */
	rpccallrace R={0};xrtAtomic32Init(&R.Entered,0);xrtAtomic32Init(&R.Permit,0);xrtAtomic32Init(&R.Finished,0);xrtAtomic32Init(&R.Drops,0);
	xrpccallconfig C=rpcCallConfig();C.User=&R;C.Wake=rpcCallRaceWake;C.Drop=rpcCallRaceDrop;
	testRequire(xrpcCallCreate(&C,&R.Call,&R.Future),"retirement create");xthread* Cancel=xrtThreadCreate(rpcCallRaceCancel,&R,0);testRequire(Cancel!=NULL,"cancel thread");
	double End=xrtTimer()+5;while(!xrtAtomic32Load(&R.Entered,XMEMORY_ACQUIRE) && xrtTimer()<End)xrtSleep(1);testRequire(xrtAtomic32Load(&R.Entered,XMEMORY_ACQUIRE),"cancel callback not entered");
	xthread* Finish=xrtThreadCreate(rpcCallRaceFinish,&R,0);testRequire(Finish!=NULL,"owner finish thread");End=xrtTimer()+5;
	while(!xrtAtomic32Load(&R.Finished,XMEMORY_ACQUIRE) && xrtTimer()<End)xrtSleep(1);
	testRequire(xrtAtomic32Load(&R.Finished,XMEMORY_ACQUIRE) && !xrtAtomic32Load(&R.Drops,XMEMORY_ACQUIRE) && xrtFutureState(R.Future)==XFUTURE_RESOLVED,
		"completion waited for cancel callback or dropped its captured context early");
	xrtAtomic32Store(&R.Permit,1,XMEMORY_RELEASE);testRequire(xrtThreadWait(Cancel)==XWAIT_OK && xrtThreadExitCode(Cancel)==0 && xrtThreadWait(Finish)==XWAIT_OK && xrtThreadExitCode(Finish)==0,"retirement threads");
	xrtThreadDestroy(Cancel);xrtThreadDestroy(Finish);testRequire(xrtAtomic32Load(&R.Drops,XMEMORY_ACQUIRE)==1 && rpcCallResult(R.Future)->Status==XRPC_CALL_CANCELLED,"retirement outcome/context drop");xrtFutureDestroy(R.Future);
}
#endif
#if defined(XRPC_FEATURE_XRPC_RECEIVE)
static void testRpcCallReceive(void)
{
	xrpcpending* P=NULL;testRequire(xrpcPendingCreate(NULL,&P),"call receive pending");xrpccall* Calls[4];xfuture* Futures[4];rpccallcontext Context={0};
	xrpccallconfig Config=rpcCallConfig();Config.Drop=rpcCallDrop;Config.User=&Context;
	for(unsigned K=0;K<4;++K){char Id[4];int N=snprintf(Id,sizeof(Id),"%u",K);xrpcpendingtoken Token;
		testRequire(xrpcCallCreate(&Config,&Calls[K],&Futures[K]) && xrpcPendingInsert(P,(xstrview){Id,(size_t)N},UINT64_MAX,Calls[K],&Token)==XRPC_PENDING_OK,"async batch admission");}
	xrpcmessage Scratch[6];xrpcpendingresolution Matches[6];xrpcreceiveinfo Received={0};Received.Size=sizeof(Received);xrpcresponses* Responses=NULL;
	xstrview Wire=XRT_STR_LITERAL("[{\"jsonrpc\":\"2.0\",\"id\":3,\"error\":{\"code\":-1,\"message\":\"m\"}},{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":11},{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":10},{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":12},{\"jsonrpc\":\"2.0\",\"id\":1.0,\"result\":99},{\"jsonrpc\":\"2.0\",\"id\":9,\"result\":99}]");
	testRequire(xrpcReceive(P,Wire,NULL,Scratch,6,Matches,6,&Responses,&Received)==XRPC_RESPONSE_DONE && Received.Matched==4 && Received.Repeated==1 && Received.Unknown==1,"out-of-order batch receive");
	xrpcresponseinfo Info={0};Info.Size=sizeof(Info);testRequire(xrpcResponsesInfo(Responses,&Info),"batch snapshot");
	for(size_t K=0;K<6;++K)if(Matches[K].Status==XRPC_PENDING_MATCHED)testRequire(xrpcCallFinish(Matches[K].Call.User,
		Info.Messages[K].Kind==XRPC_MESSAGE_ERROR?XRPC_CALL_REMOTE_ERROR:XRPC_CALL_OK,Responses,K,NULL)==XRPC_CALL_FINISHED,"batch Future publication");
	xrpcResponsesRelease(Responses);xrpcPendingDestroy(P);
	for(unsigned K=0;K<4;++K)testRequire(xrpcCallDestroy(Calls[K]),"batch producer release");
	testRequire(Context.Drops==4,"result kept producer contexts alive");
	for(unsigned K=0;K<4;++K){const xrpccallresult* R=rpcCallResult(Futures[K]);Info.Size=sizeof(Info);
		testRequire(R->Status==(K==3?XRPC_CALL_REMOTE_ERROR:XRPC_CALL_OK) && xrpcResponsesInfo(R->Responses,&Info) && Info.Messages[R->Index].Id.Raw.Size==1 &&
			Info.Messages[R->Index].Id.Raw.Data[0]==(char)('0'+K),"Future payload mismatched correlated ID");xrtFutureDestroy(Futures[K]);}
}
#endif
static void testRpcBatchCall(void)
{
    xrpcresponses* Replies=rpcCallReplies();xrpccallconfig K=rpcCallConfig();xrpccall* C=NULL;xfuture* F=NULL;
    testRequire(!xrpcBatchCallCreate(&K,0,&C,&F) && !C && !F,"empty batch constructor");xrtClearError();
    testRequire(!xrpcBatchCallCreate(&K,65537,&C,&F) && !C && !F,"oversized batch constructor");xrtClearError();
    for(unsigned Stop=0;Stop<3;++Stop){
        K=rpcCallConfig();if(Stop==1)K.Timeout=0;
        testRequire(xrpcBatchCallCreate(&K,3,&C,&F),"native batch create");
        size_t Duplicate[]={0,SIZE_MAX,0},Missing[]={0,SIZE_MAX,SIZE_MAX},Range[]={0,SIZE_MAX,2},Good[]={1,SIZE_MAX,0};
        testRequire(xrpcBatchCallFinish(C,Replies,Duplicate,3,NULL)==XRPC_CALL_FINISH_ERROR &&
            xrpcBatchCallFinish(C,Replies,Missing,3,NULL)==XRPC_CALL_FINISH_ERROR &&
            xrpcBatchCallFinish(C,Replies,Range,3,NULL)==XRPC_CALL_FINISH_ERROR &&
            xrpcBatchCallFinish(C,Replies,Good,2,NULL)==XRPC_CALL_FINISH_ERROR && xrtFutureState(F)==XFUTURE_PENDING,"bad batch maps published partial result");xrtClearError();
        if(Stop==2)testRequire(xrtFutureCancel(F),"batch cancel");
        xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"batch completion quota");
        testRequire(xrpcBatchCallFinish(C,Replies,Good,3,NULL)==XRPC_CALL_FINISHED,"preallocated batch completion");
        testRequire(xrtAllocationBudgetLeave(&Scope),"batch completion quota leave");xrtAllocationBudgetFree(Budget);
        testRequire(xrpcCallDestroy(C),"batch producer drop");const xrpcbatchcallresult* R=(const xrpcbatchcallresult*)rpcCallResult(F);
        testRequire(R->Count==3 && R->Call.Delivery==XRPC_REPLIED && R->Call.Index==SIZE_MAX && R->Call.Status==
            (Stop==1?XRPC_CALL_TIMEOUT:Stop==2?XRPC_CALL_CANCELLED:XRPC_CALL_BATCH_DONE),"batch terminal type/knowledge");
        for(size_t J=0;J<3;++J)testRequire(R->Indices[J]==(Stop?SIZE_MAX:Good[J]),"batch retained index map");
        testRequire(R->Call.Responses==(Stop?NULL:Replies),"batch stopped payload");xrtFutureDestroy(F);
    }
    K=rpcCallConfig();testRequire(xrpcBatchCallCreate(&K,2,&C,&F),"notification batch create");
    testRequire(xrpcCallFinish(C,XRPC_CALL_NOTIFY_DONE,NULL,0,NULL)==XRPC_CALL_FINISH_ERROR,"unsent notification completed");xrtClearError();
    testRequire(xrpcCallMarkSent(C) && xrpcCallFinish(C,XRPC_CALL_NOTIFY_DONE,NULL,0,NULL)==XRPC_CALL_FINISHED && xrpcCallDestroy(C),"notification batch completion");
    const xrpcbatchcallresult* R=(const xrpcbatchcallresult*)rpcCallResult(F);
    testRequire(R->Count==2 && R->Call.Status==XRPC_CALL_NOTIFY_DONE && R->Call.Delivery==XRPC_MAY_HAVE_EXECUTED && !R->Call.Responses && R->Indices[0]==SIZE_MAX && R->Indices[1]==SIZE_MAX,"notification delivery/map");xrtFutureDestroy(F);
    unsigned Passed=0,Failed=0;
    for(size_t Limit=1;Limit<=2048;++Limit){xallocationbudget* Budget=NULL;xallocationscope Scope={0};C=NULL;F=NULL;
        testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"batch allocation prefix enter");
        bool OK=xrpcBatchCallCreate(&K,3,&C,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"batch allocation prefix leave");
        if(OK){++Passed;testRequire(xrpcCallDestroy(C),"batch OOM producer cleanup");xrtFutureDestroy(F);}else{++Failed;testRequire(!C && !F,"batch OOM outputs");}
        xrtClearError();xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I) && !I.LiveBytes,"batch OOM leak");xrtAllocationBudgetFree(Budget);
    }
    testRequire(Passed && Failed,"batch constructor allocation boundaries");xrpcResponsesRelease(Replies);
}
int main(void)
{
	testRpcCallOutcomes();testRpcCallStopAndCause();testRpcCallReentry();testRpcCallFailures();testRpcBatchCall();
	#if defined(XRT_FEATURE_THREAD)
	testRpcCallRetirement();
	#endif
	#if defined(XRPC_FEATURE_XRPC_RECEIVE)
	testRpcCallReceive();
	#endif
	puts("[PASS] xrpc call: typed-Future/delivery/cancel-deadline/retained-payload/cause/observer-drop/reentry/nonblocking-cancel-tail/4096-OOM-prefixes/preallocated-completion");return 0;
}
