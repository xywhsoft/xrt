#include "../internal/xrt_task.h"
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
	xdeadline iDeadline
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
	pTimer->Timer = xrtNetEngineSchedule(
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



/* 按相对微秒数调度网络任务。 */
XRT_API xfuture* xrtTaskNetAfter(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
)
{
	return __xrtTaskNetSchedule(
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		xrtDeadlineAfter(iTimeout)
	);
}



/* 按单调截止时间调度网络任务。 */
XRT_API xfuture* xrtTaskNetUntil(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
)
{
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
