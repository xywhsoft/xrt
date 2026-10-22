#include <xrt/detail/wait.h>
#include "../internal/xrt_task.h"
#include "../internal/xrt_net_engine.h"
#include <xrt/future_bridge.h>



#if defined(XRT_FEATURE_TASK_NET)

/* 网络任务数据补充当前 Worker，同时保存用户数据的受理后所有权。 */
typedef struct xrt_task_net_data {
	xnetworker* Worker;
	xtasknetproc Proc;
	ptr Data;
	xfuturefreeproc Destroy;
	ptr DestroyData;
} xrt_task_net_data;



/* 延迟任务桥接 Engine Timer、任务作业与协作取消监听。 */
typedef struct xrt_task_net_timer {
	xfuturebridge Bridge;
	volatile int32 References;
	xrt_task_job* Job;
	xnetengine* Engine;
	uint64 Timer;
} xrt_task_net_timer;



/* 在任务核心的隔离上下文内调用带 Worker 的用户过程。 */
static xtaskoutcome __xrtTaskNetRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_task_net_data* pTask = (xrt_task_net_data*)pData;

	return pTask->Proc(
		pTask->Worker,
		pCancel,
		pTask->Data,
		pResult
	);
}



/* 释放已经受理的用户数据和网络任务包装。 */
static void __xrtTaskNetDestroy(ptr pValue, ptr pData)
{
	xrt_task_net_data* pTask = (xrt_task_net_data*)pValue;

	(void)pData;
	if ( pTask->Destroy != NULL ) {
		pTask->Destroy(pTask->Data, pTask->DestroyData);
	}
	xrtFree(pTask);
}



/* 创建尚未受理的网络任务；失败时用户数据仍归调用方。 */
static xrt_task_job* __xrtTaskNetCreate(
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xfuture** ppFuture
)
{
	xrt_task_net_data* pTask;
	xtaskargs tArgs;
	xrt_task_job* pJob;

	pTask = (xrt_task_net_data*)xrtCalloc(1, sizeof(*pTask));
	if ( pTask == NULL ) {
		return NULL;
	}
	pTask->Proc = pProc;
	pTask->Data = pData;
	if ( pArgs != NULL ) {
		pTask->Destroy = pArgs->Destroy;
		pTask->DestroyData = pArgs->DestroyData;
	}
	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.Cancel = pArgs != NULL ? pArgs->Cancel : NULL;
	tArgs.Destroy = __xrtTaskNetDestroy;
	pJob = __xrtTaskCreate(
		__xrtTaskNetRun,
		pTask,
		&tArgs,
		ppFuture
	);
	if ( pJob == NULL ) {
		xrtFree(pTask);
	}
	return pJob;
}



/* 回滚一个未被 Engine 受理的任务，不释放调用方拥有的用户数据。 */
static void __xrtTaskNetReject(xrt_task_job* pJob)
{
	xrt_task_net_data* pTask = (xrt_task_net_data*)pJob->Data;

	__xrtTaskDestroy(pJob, false);
	xrtFree(pTask);
}



/* 为 Engine 停止或 Timer 内部失败创建稳定的任务错误。 */
static xerror* __xrtTaskNetError(xnetresult Result)
{
	if ( Result == XNET_RESULT_CLOSED ) {
		return xrtErrorCreate(
			XERR_CLOSED,
			"xrt.task.net",
			(int32)Result,
			"network engine stopped before the task deadline"
		);
	}
	return xrtErrorCreate(
		XERR_INTERNAL,
		"xrt.task.net",
		(int32)Result,
		"network engine timer failed"
	);
}



/* 在亲和 Worker 上执行立即任务并释放受理后的任务资源。 */
static void __xrtTaskNetPost(xnetworker* pWorker, ptr pData)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;
	xrt_task_net_data* pTask = (xrt_task_net_data*)pJob->Data;

	pTask->Worker = pWorker;
	__xrtTaskRun(pJob);
	__xrtTaskDestroy(pJob, true);
}



/* 在独立错误上下文中转发 Future 取消，避免污染取消请求线程。 */
static void __xrtTaskNetTimerCancel(ptr pData)
{
	xrt_task_net_timer* pTimer = (xrt_task_net_timer*)pData;
	xrt_error_context tErrorContext;
	xrt_error_context* pPrevious;

	memset(&tErrorContext, 0, sizeof(tErrorContext));
	pPrevious = __xrtErrorContextSwap(&tErrorContext);
	(void)xrtNetEngineTimerCancel(pTimer->Engine, pTimer->Timer);
	xrtClearError();
	(void)__xrtErrorContextSwap(pPrevious);
}



/* 把 Engine Timer 的唯一终态映射为任务 Future 的唯一终态。 */
static void __xrtTaskNetTimerDone(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xrt_task_net_timer* pTimer = (xrt_task_net_timer*)pData;
	xrt_task_job* pJob = pTimer->Job;
	xrt_task_net_data* pTask = (xrt_task_net_data*)pJob->Data;
	xerror* pError = NULL;
	bool bReady;

	(void)Id;
	bReady = xrtFutureBridgeWait(&pTimer->Bridge);
	xrtFutureBridgeUnwatch(&pTimer->Bridge);
	if ( bReady ) {
		pTask->Worker = pWorker;
		if ( Result == XNET_RESULT_OK ) {
			__xrtTaskRun(pJob);
		} else if ( (Result == XNET_RESULT_CANCELLED) ||
			xrtCancelRequested(pJob->Cancel) ) {
			__xrtTaskCancel(pJob);
		} else {
			pError = __xrtTaskNetError(Result);
			__xrtTaskFail(pJob, pError);
		}
		__xrtTaskDestroy(pJob, true);
	} else {
		__xrtTaskNetReject(pJob);
	}
	xrtErrorFree(pError);
	if ( xrtRefRelease(&pTimer->References) == 0 ) {
		xrtFree(pTimer);
	}
}



/* 创建一个按截止时间执行的网络任务并完成取消桥装配。 */
static xfuture* __xrtTaskNetSchedule(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
)
{
	xrt_task_net_timer* pTimer;
	xrt_task_job* pJob;
	xfuture* pFuture;
	xerror* pError;

	if ( (pEngine == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pJob = __xrtTaskNetCreate(pProc, pData, pArgs, &pFuture);
	if ( pJob == NULL ) {
		return NULL;
	}
	pTimer = (xrt_task_net_timer*)xrtCalloc(1, sizeof(*pTimer));
	if ( pTimer == NULL ) {
		__xrtTaskNetReject(pJob);
		return NULL;
	}
	pTimer->Job = pJob;
	pTimer->Engine = pEngine;
	pTimer->References = 2;
	(void)xrtFutureBridgeInit(&pTimer->Bridge, pJob->Promise);
	pTimer->Timer = __xrtNetEngineSchedule(
		pEngine,
		iAffinity,
		iDeadline,
		__xrtTaskNetTimerDone,
		pTimer
	);
	if ( pTimer->Timer == 0 ) {
		__xrtTaskNetReject(pJob);
		xrtFree(pTimer);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pTimer->Bridge,
		__xrtTaskNetTimerCancel,
		pTimer
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pTimer->Bridge);
		__xrtTaskNetTimerCancel(pTimer);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtErrorSetInternal();
		}
		if ( xrtRefRelease(&pTimer->References) == 0 ) {
			xrtFree(pTimer);
		}
		return NULL;
	}
	(void)xrtFutureBridgeReady(&pTimer->Bridge);
	if ( xrtRefRelease(&pTimer->References) == 0 ) {
		xrtFree(pTimer);
	}
	return pFuture;
}



/* 向指定亲和 Worker 提交立即任务。 */
XRT_API xfuture* xrtTaskNet(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	xrt_task_job* pJob;
	xfuture* pFuture;

	if ( (pEngine == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pJob = __xrtTaskNetCreate(pProc, pData, pArgs, &pFuture);
	if ( pJob == NULL ) {
		return NULL;
	}
	if ( !xrtNetEnginePost(
		pEngine,
		iAffinity,
		__xrtTaskNetPost,
		pJob
	) ) {
		__xrtTaskNetReject(pJob);
		return NULL;
	}
	return pFuture;
}



/* 按相对毫秒数调度网络任务。 */
XRT_API xfuture* xrtTaskNetAfter(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	int64 iTimeout
)
{
	return __xrtTaskNetSchedule(
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		__xrtWaitAfter(iTimeout)
	);
}



/* 按单调截止时间调度网络任务。 */
XRT_API xfuture* __xrtTaskNetUntil(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	double iDeadline
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return NULL; }

	return __xrtTaskNetSchedule(
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		iDeadline
	);
}

#endif

#if (defined(XRT_FEATURE_TASK_NET))
XRT_API xfuture* xrtTaskNetFor(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	int64 iTimeout
)
{
    return __xrtTaskNetUntil(pEngine, iAffinity, pProc, pData, pArgs, __xrtWaitAfter(iTimeout));
}
#endif

#if defined(XRT_FEATURE_TASK_NET)
/* ONE physical activation shared by Job Data, accepted Post/Timer and Watch.
 * Job owns a Frame reference and Frame owns Job's actual executor reference.
 * Bridge only borrows Job's Promise. The Engine shell reference is NOT a
 * LiveObjects pin, so accepted tasks cannot prevent their own shutdown drain. */
typedef struct xrt_task_net_owned {
    xfuturebridge Bridge;
    volatile int32 References;
    xmutex Lock;
    size_t Active;
    bool Publishing, Accepted, Completed, Cleared, CancelRequested;
    const void* Claim;
    xrt_task_job* Job;
    xnetengine* Engine;
    xnetworker* Worker;
    uint64 Timer;
    ptr Data;
    const xtasknetdataownershipv1* Policy;
} xrt_task_net_owned;

static void __xrtTaskNetOwnedBegin(xrt_task_net_owned* frame, xrtownershipscope* scope)
{
    if (!xrtOwnershipMutationBegin(scope) || !xrtMutexLock(&frame->Lock)) abort();
}
static void __xrtTaskNetOwnedEnd(xrt_task_net_owned* frame, xrtownershipscope* scope)
{
    if (!xrtMutexUnlock(&frame->Lock) || !xrtOwnershipScopeEnd(scope)) abort();
}
static bool __xrtTaskNetOwnedCount(const void* data, size_t* count)
{
    const xrt_task_net_owned* frame=data; xrtownershipref watch; int32 refs;
    if (!frame || !count || frame->Publishing || !frame->Accepted || frame->Active ||
        frame->Cleared || !xrtFutureBridgeWatchOwnershipV1(&frame->Bridge,&watch)) return false;
    refs=__xrtAtomicRefLoad(&frame->References); if (refs<=0) return false;
    *count=(size_t)refs; return true;
}
static bool __xrtTaskNetOwnedTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
    const xrt_task_net_owned* frame=data; xrtownershipref watch; size_t count;
    if (!visit || !__xrtTaskNetOwnedCount(data,&count) ||
        !xrtFutureBridgeWatchOwnershipV1(&frame->Bridge,&watch)) return false;
    if (frame->Job && !visit(__xrtTaskOwnership(frame->Job),context)) return false;
    if (frame->Engine && !visit(xrtNetEngineOwnership(frame->Engine),context)) return false;
    if (frame->Data && !visit((xrtownershipref){frame->Data,frame->Policy->Ops},context)) return false;
    return !watch.Data || visit(watch,context);
}
static const xrtownershipops __xrtTaskNetOwnedOps={
    __xrtTaskNetOwnedCount,__xrtTaskNetOwnedTrace
};
static bool __xrtTaskNetOwnedHold(const void* data)
{
    xrt_task_net_owned* frame=(xrt_task_net_owned*)data; xrtownershipscope scope={0}; bool held;
    __xrtTaskNetOwnedBegin(frame,&scope);
    held=!frame->Cleared && xrtRefRetain(&frame->References)>0;
    __xrtTaskNetOwnedEnd(frame,&scope); return held;
}
static void __xrtTaskNetOwnedDrop(const void* data)
{
    xrt_task_net_owned* frame=(xrt_task_net_owned*)data;
    xrtownershipscope scope={0}; int32 refs; xrtownershipref watch;
    __xrtTaskNetOwnedBegin(frame,&scope); refs=xrtRefRelease(&frame->References);
    if (refs<0 || (!refs && (frame->Publishing || !frame->Completed || frame->Active ||
        frame->Job || frame->Engine || frame->Data ||
        !xrtFutureBridgeWatchOwnershipV1(&frame->Bridge,&watch) || watch.Data))) abort();
    __xrtTaskNetOwnedEnd(frame,&scope);
    if (!refs) {
        if (!xrtOwnershipMutationBegin(&scope)) abort();
        if (!xrtMutexUnit(&frame->Lock)) abort();
        xrtFree(frame); if (!xrtOwnershipScopeEnd(&scope)) abort();
    }
}
static bool __xrtTaskNetOwnedBusy(const void* data)
{
    const xrt_task_net_owned* frame=data; xrtownershipref watch;
    return frame && !frame->Cleared && (frame->Publishing || frame->Active ||
        !xrtFutureBridgeWatchOwnershipV1(&frame->Bridge,&watch));
}
XRT_API const xrtownershipquiescencev1* xrtTaskNetOwnershipQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy={
        sizeof(policy),&__xrtTaskNetOwnedOps,__xrtTaskNetOwnedBusy};
    return &policy;
}

/* Job's semantic Data release, NOT Frame's physical final release. Collector
 * pins must never postpone user/code retirement beyond Future publication. */
static void __xrtTaskNetOwnedDataDrop(ptr data, ptr unused)
{
    xrt_task_net_owned* frame=data; xrtownershipscope scope={0}; ptr user;
    (void)unused;
    __xrtTaskNetOwnedBegin(frame,&scope); ++frame->Active;
    user=frame->Data; frame->Data=NULL;
    __xrtTaskNetOwnedEnd(frame,&scope);
    if (user) frame->Policy->Drop(user,NULL);
    __xrtTaskNetOwnedBegin(frame,&scope); --frame->Active;
    __xrtTaskNetOwnedEnd(frame,&scope); __xrtTaskNetOwnedDrop(frame);
}
static xtaskoutcome __xrtTaskNetOwnedRun(xcancel* cancel, ptr data, xtaskvalue* result)
{
    xrt_task_net_owned* frame=data;
    return frame->Policy->Proc(frame->Worker,cancel,frame->Data,result);
}
static const xtaskdataownershipv1 __xrtTaskNetOwnedJobPolicy={
    sizeof(xtaskdataownershipv1),__xrtTaskNetOwnedRun,__xrtTaskNetOwnedDataDrop,&__xrtTaskNetOwnedOps
};
XRT_API const xtaskdataownershipv1* xrtTaskNetJobDataPolicyV1Get(void)
{ return &__xrtTaskNetOwnedJobPolicy; }

/* Watch can notify synchronously before acceptance. Remember cancellation
 * until the real Timer ID is published. Take an independent shell reference
 * under the Frame lock; submit cancellation outside the lock/mutation. */
static void __xrtTaskNetOwnedCancel(ptr data)
{
    xrt_task_net_owned* frame=data; xrtownershipscope scope={0};
    xnetengine* engine=NULL; uint64 timer=0; xerror* previous=xrtTakeError();
    __xrtTaskNetOwnedBegin(frame,&scope); ++frame->Active; frame->CancelRequested=true;
    if (frame->Engine && frame->Timer && !frame->Completed) {
        if (!__xrtNetEngineStorageRetain(frame->Engine)) abort();
        engine=frame->Engine; timer=frame->Timer;
    }
    __xrtTaskNetOwnedEnd(frame,&scope);
    if (engine) { (void)xrtNetEngineTimerCancel(engine,timer); __xrtNetEngineStorageRelease(engine); }
    __xrtTaskNetOwnedBegin(frame,&scope); --frame->Active;
    __xrtTaskNetOwnedEnd(frame,&scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static const xcancelwatchownershipv1 __xrtTaskNetOwnedCancelPolicy={
    sizeof(xcancelwatchownershipv1),__xrtTaskNetOwnedCancel,__xrtTaskNetOwnedDrop,&__xrtTaskNetOwnedOps
};
XRT_API const xcancelwatchownershipv1* xrtTaskNetCancelPolicyV1Get(void)
{ return &__xrtTaskNetOwnedCancelPolicy; }

static void __xrtTaskNetOwnedComplete(xrt_task_net_owned* frame,
    xnetworker* worker, xnetresult result)
{
    xrtownershipscope scope={0}; xrt_task_job* job; xnetengine* engine;
    xerror* previous=xrtTakeError(); xerror* failure=NULL;
    if (!xrtFutureBridgeWait(&frame->Bridge)) abort(); /* no accepted hidden failure */
    __xrtTaskNetOwnedBegin(frame,&scope); ++frame->Active;
    frame->Worker=worker; job=frame->Job;
    __xrtTaskNetOwnedEnd(frame,&scope);
    xrtFutureBridgeUnwatch(&frame->Bridge);
    if (result==XNET_RESULT_OK) __xrtTaskRun(job);
    else if (result==XNET_RESULT_CANCELLED || xrtCancelRequested(job->Cancel)) __xrtTaskCancel(job);
    else {
        failure=__xrtTaskNetError(result); __xrtTaskFail(job,failure);
    }
    __xrtTaskNetOwnedBegin(frame,&scope);
    frame->Job=NULL; engine=frame->Engine; frame->Engine=NULL;
    frame->Timer=0; frame->Worker=NULL; frame->Completed=true;
    __xrtTaskNetOwnedEnd(frame,&scope);
    __xrtTaskDestroy(job,true); __xrtNetEngineStorageRelease(engine); xrtErrorFree(failure);
    __xrtTaskNetOwnedBegin(frame,&scope); --frame->Active;
    __xrtTaskNetOwnedEnd(frame,&scope);
    xrtClearError(); xrtSetErrorTake(previous);
}
static void __xrtTaskNetOwnedPost(xnetworker* worker, ptr data)
{ __xrtTaskNetOwnedComplete(data,worker,XNET_RESULT_OK); }
static void __xrtTaskNetOwnedTimer(xnetworker* worker, uint64 id, xnetresult result, ptr data)
{ (void)id; __xrtTaskNetOwnedComplete(data,worker,result); }
static void __xrtTaskNetOwnedEngineDrop(ptr data)
{ __xrtTaskNetOwnedDrop(data); }
static const xnettaskownershipv1 __xrtTaskNetOwnedPostPolicy={
    sizeof(xnettaskownershipv1),__xrtTaskNetOwnedPost,__xrtTaskNetOwnedEngineDrop,&__xrtTaskNetOwnedOps
};
static const xnettimerownershipv1 __xrtTaskNetOwnedTimerPolicy={
    sizeof(xnettimerownershipv1),__xrtTaskNetOwnedTimer,__xrtTaskNetOwnedEngineDrop,&__xrtTaskNetOwnedOps
};
XRT_API const xnettaskownershipv1* xrtTaskNetPostPolicyV1Get(void)
{ return &__xrtTaskNetOwnedPostPolicy; }
XRT_API const xnettimerownershipv1* xrtTaskNetTimerPolicyV1Get(void)
{ return &__xrtTaskNetOwnedTimerPolicy; }

static bool __xrtTaskNetOwnedClaim(const void* data, const void* token)
{
    xrt_task_net_owned* frame=(xrt_task_net_owned*)data; size_t count;
    if (!token || !__xrtTaskNetOwnedCount(data,&count) ||
        (frame->Claim && frame->Claim!=token)) return false;
    frame->Claim=token; return true;
}
static void __xrtTaskNetOwnedRestore(const void* data, const void* token)
{
    xrt_task_net_owned* frame=(xrt_task_net_owned*)data;
    if (!token || frame->Claim!=token || frame->Cleared) abort();
    frame->Claim=NULL;
}
static bool __xrtTaskNetOwnedReady(const void* data)
{
    const xrt_task_net_owned* frame=data; size_t count; xrtownershipref watch;
    return __xrtTaskNetOwnedCount(data,&count) && frame->Completed && !frame->Job &&
        !frame->Engine && !frame->Data &&
        xrtFutureBridgeWatchOwnershipV1(&frame->Bridge,&watch) && !watch.Data;
}
static xrtownershipprepareresult __xrtTaskNetOwnedPrepare(const void* data, const void* token)
{
    const xrt_task_net_owned* frame=data; xrtownershipscope scope={0}; bool ready;
    if (!xrtOwnershipFreezeTryBegin(&scope)) return XRT_OWNERSHIP_PREPARE_BUSY;
    if (!token || frame->Claim!=token || frame->Cleared) abort();
    ready=__xrtTaskNetOwnedReady(data);
    if (!xrtOwnershipScopeEnd(&scope)) abort();
    return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTaskNetOwnedClear(const void* data, const void* token)
{
    xrt_task_net_owned* frame=(xrt_task_net_owned*)data;
    if (!token || frame->Claim!=token || frame->Cleared || !__xrtTaskNetOwnedReady(data)) abort();
    frame->Cleared=true;
}
XRT_API const xrtownershipadapterv1* xrtTaskNetOwnershipAdapterV1(
    xrtownershipref ref, const xtasknetdataownershipv1* const* policies, size_t count,
    const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),__xrtTaskNetOwnedHold,
        __xrtTaskNetOwnedDrop,__xrtTaskNetOwnedClaim,__xrtTaskNetOwnedRestore,NULL,
        __xrtTaskNetOwnedClear,NULL};
    static const xrtownershippreparationv1 prepare={sizeof(prepare),&adapter,
        __xrtTaskNetOwnedReady,__xrtTaskNetOwnedPrepare};
    const xrt_task_net_owned* frame; bool known=false; size_t refs;
    if (!ref.Data || ref.Ops!=&__xrtTaskNetOwnedOps || !preparation || (count&&!policies)) return NULL;
    frame=ref.Data;
    for (size_t i=0;i<count;++i) if (policies[i] && policies[i]==frame->Policy) { known=true; break; }
    if (!known || !__xrtTaskNetOwnedCount(frame,&refs)) return NULL;
    *preparation=&prepare; return &adapter;
}

static xfuture* __xrtTaskNetOwnedStart(xnetengine* engine, uint64 affinity,
    ptr data, xcancel* cancel, const xtasknetdataownershipv1* policy,
    const xfuturepayloadownershipv1* resultPolicy, bool delayed, double deadline)
{
    xrt_task_net_owned* frame; xrt_task_job* job=NULL; xfuture* future=NULL;
    xrtownershipscope scope={0}; xnetengine* held; xerror* error; uint64 timer=0; bool replay;
    if (!engine || !data || !policy || policy->size!=sizeof(*policy) || !policy->Proc ||
        !policy->Drop || !policy->Ops || !policy->Ops->Count || !policy->Ops->Trace ||
        !resultPolicy || resultPolicy->size!=sizeof(*resultPolicy) ||
        !resultPolicy->Drop || !resultPolicy->Trace) { __xrtErrorSetInvalidArgument(); return NULL; }
    frame=xrtCalloc(1,sizeof(*frame)); if (!frame) return NULL;
    frame->References=1; frame->Publishing=true; frame->Policy=policy; frame->Data=data;
    if (!xrtMutexInit(&frame->Lock)) { xrtFree(frame); return NULL; }
    if (!__xrtNetEngineStorageRetain(engine)) {
        (void)xrtMutexUnit(&frame->Lock); xrtFree(frame); return NULL;
    }
    frame->Engine=engine;
    xtaskargs args={cancel,__xrtTaskNetOwnedDataDrop,NULL};
    if (!__xrtTaskNetOwnedHold(frame)) abort();
    job=__xrtTaskCreateOwned(__xrtTaskNetOwnedRun,frame,&args,&__xrtTaskNetOwnedJobPolicy,&future);
    if (!job) { __xrtTaskNetOwnedDrop(frame); goto failed; }
    frame->Job=job; job->ResultPolicy=resultPolicy; job->ResultTrace=resultPolicy->Trace;
    if (!xrtFutureBridgeInit(&frame->Bridge,job->Promise)) abort();
    if (delayed) {
        if (!__xrtTaskNetOwnedHold(frame)) abort();
        if (!xrtFutureBridgeWatchOwnedV1(&frame->Bridge,frame,&__xrtTaskNetOwnedCancelPolicy)) {
            __xrtTaskNetOwnedDrop(frame); goto failed;
        }
    }
    if (!__xrtTaskNetOwnedHold(frame)) abort();
    __xrtTaskAccept(job); /* before a worker can observe the Job */
    if (delayed) {
        timer=__xrtNetEngineScheduleOwnedV1(engine,affinity,deadline,frame,&__xrtTaskNetOwnedTimerPolicy);
        if (!timer) { __xrtTaskNetOwnedDrop(frame); goto failed; }
    } else if (!xrtNetEnginePostOwnedV1(engine,affinity,frame,&__xrtTaskNetOwnedPostPolicy)) {
        __xrtTaskNetOwnedDrop(frame); goto failed;
    }
    __xrtTaskNetOwnedBegin(frame,&scope);
    frame->Timer=timer; frame->Accepted=true; frame->Publishing=false;
    replay=delayed && frame->CancelRequested;
    __xrtTaskNetOwnedEnd(frame,&scope);
    if (!xrtFutureBridgeReady(&frame->Bridge)) abort();
    if (replay) __xrtTaskNetOwnedCancel(frame);
    __xrtTaskNetOwnedDrop(frame); return future;
failed:
    error=xrtTakeError();
    if (job) {
        if (!xrtFutureBridgeFail(&frame->Bridge)) abort();
        xrtFutureBridgeUnwatch(&frame->Bridge);
    }
    __xrtTaskNetOwnedBegin(frame,&scope);
    frame->Job=NULL; held=frame->Engine; frame->Engine=NULL;
    frame->Data=NULL; frame->Publishing=false; frame->Completed=true;
    __xrtTaskNetOwnedEnd(frame,&scope);
    if (job) { __xrtTaskDestroy(job,false); __xrtTaskNetOwnedDrop(frame); }
    __xrtNetEngineStorageRelease(held);
    /* No successful bridge init exists if Job creation failed. Its last Drop
     * must not inspect uninitialized bridge storage. */
    if (!job) {
        (void)xrtMutexUnit(&frame->Lock); xrtFree(frame);
    } else __xrtTaskNetOwnedDrop(frame);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}
XRT_API xfuture* xrtTaskNetDataPolicyV1(xnetengine* engine, uint64 affinity,
    ptr data, xcancel* cancel, const xtasknetdataownershipv1* policy,
    const xfuturepayloadownershipv1* resultPolicy)
{ return __xrtTaskNetOwnedStart(engine,affinity,data,cancel,policy,resultPolicy,false,0); }
XRT_API xfuture* __xrtTaskNetUntilDataPolicyV1(xnetengine* engine, uint64 affinity,
    ptr data, xcancel* cancel, const xtasknetdataownershipv1* policy,
    const xfuturepayloadownershipv1* resultPolicy, double deadline)
{
    if (!__xrtWaitValid(deadline)) return NULL;
    return __xrtTaskNetOwnedStart(engine,affinity,data,cancel,policy,resultPolicy,true,deadline);
}
XRT_API xfuture* xrtTaskNetAfterDataPolicyV1(xnetengine* engine, uint64 affinity,
    ptr data, xcancel* cancel, const xtasknetdataownershipv1* policy,
    const xfuturepayloadownershipv1* resultPolicy, int64 timeout)
{ return __xrtTaskNetUntilDataPolicyV1(engine,affinity,data,cancel,policy,resultPolicy,__xrtWaitAfter(timeout)); }
XRT_API xfuture* xrtTaskNetForDataPolicyV1(xnetengine* engine, uint64 affinity,
    ptr data, xcancel* cancel, const xtasknetdataownershipv1* policy,
    const xfuturepayloadownershipv1* resultPolicy, int64 timeout)
{ return xrtTaskNetAfterDataPolicyV1(engine,affinity,data,cancel,policy,resultPolicy,timeout); }
#endif

