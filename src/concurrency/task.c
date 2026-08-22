#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK)

/* 释放尚未转移到 Future 的任务成功值。 */
static void __xrtTaskValueDestroy(xtaskvalue* pValue)
{
	if ( pValue->Destroy != NULL ) {
		pValue->Destroy(pValue->Value, pValue->DestroyData);
		pValue->Value = NULL;
		pValue->Destroy = NULL;
		pValue->DestroyData = NULL;
	}
}



/* 把显式任务结果映射到 Promise 的唯一终态。 */
static void __xrtTaskComplete(
	xrt_task_job* pJob,
	xtaskoutcome Outcome,
	xtaskvalue* pValue,
	xerror* pError
)
{
	bool bCompleted = false;

	if ( Outcome == XTASK_SUCCESS ) {
		if ( pValue->Destroy != NULL ) {
			bCompleted = xrtPromiseResolveOwned(
				pJob->Promise,
				pValue->Value,
				pValue->Destroy,
				pValue->DestroyData
			);
			if ( bCompleted ) {
				pValue->Destroy = NULL;
			}
		} else {
			bCompleted = xrtPromiseResolve(pJob->Promise, pValue->Value);
		}
	} else if ( Outcome == XTASK_CANCELLED ) {
		bCompleted = xrtPromiseCancel(pJob->Promise);
	} else {
		bCompleted = xrtPromiseReject(pJob->Promise, pError);
	}
	if ( !bCompleted ) {
		__xrtTaskValueDestroy(pValue);
	}
}



/* 在隔离的错误与临时内存上下文中完成一次任务生命周期。 */
static void __xrtTaskFinish(
	xrt_task_job* pJob,
	bool bRun,
	bool bFail,
	const xerror* pFailure
)
{
	xrt_error_context tErrorContext;
	xrt_error_context* pPreviousError;
	xtemparena tTemp;
	xtemparena* pPreviousTemp = NULL;
	xtaskvalue tValue;
	xtaskoutcome Outcome = XTASK_CANCELLED;
	xerror* pError = NULL;
	bool bTempReady = false;

	memset(&tErrorContext, 0, sizeof(tErrorContext));
	memset(&tTemp, 0, sizeof(tTemp));
	memset(&tValue, 0, sizeof(tValue));
	pPreviousError = __xrtErrorContextSwap(&tErrorContext);

	/* 每个任务拥有独立 arena，避免工作线程在相邻任务间泄漏临时状态。 */
	if ( xrtTempInit(&tTemp, NULL) ) {
		pPreviousTemp = __xrtTempContextSwap(&tTemp);
		bTempReady = true;
		if ( bRun && !xrtCancelRequested(pJob->Cancel) ) {
			Outcome = pJob->Proc(pJob->Cancel, pJob->Data, &tValue);
			if (
				(Outcome != XTASK_SUCCESS) &&
				(Outcome != XTASK_FAILED) &&
				(Outcome != XTASK_CANCELLED)
			) {
				Outcome = XTASK_FAILED;
				__xrtErrorSetInternal();
			}
		} else if ( bFail ) {
			Outcome = XTASK_FAILED;
			if ( pFailure != NULL ) {
				xrtSetError(pFailure);
			} else {
				__xrtErrorSetInternal();
			}
		}
	} else {
		Outcome = XTASK_FAILED;
	}

	/* 失败错误在离开任务上下文前取走，Promise 会保存自己的引用。 */
	if ( Outcome == XTASK_FAILED ) {
		if ( xrtGetError() == NULL ) {
			__xrtErrorSetInternal();
		}
		pError = xrtTakeError();
	}
	if ( Outcome != XTASK_SUCCESS ) {
		__xrtTaskValueDestroy(&tValue);
	}

	/* 先释放任务数据，使 Future 终态成为内部任务上下文的回收屏障。 */
	if ( pJob->Destroy != NULL ) {
		pJob->Destroy(pJob->Data, pJob->DestroyData);
		pJob->Destroy = NULL;
		pJob->Data = NULL;
	}
	__xrtTaskComplete(pJob, Outcome, &tValue, pError);
	xrtErrorFree(pError);
	xrtClearError();
	if ( bTempReady ) {
		(void)__xrtTempContextSwap(pPreviousTemp);
		xrtTempUnit(&tTemp);
	}
	(void)__xrtErrorContextSwap(pPreviousError);
}



/* 创建尚未被执行器受理的任务作业和 Future/Promise 对。 */
xrt_task_job* __xrtTaskCreate(
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xfuture** ppFuture
)
{
	xrt_task_job* pJob;
	xcancel* pParent = pArgs != NULL ? pArgs->Cancel : NULL;

	if ( (pProc == NULL) || (ppFuture == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	*ppFuture = NULL;
	pJob = (xrt_task_job*)xrtCalloc(1, sizeof(xrt_task_job));
	if ( pJob == NULL ) {
		return NULL;
	}
	pJob->Proc = pProc;
	pJob->Data = pData;
	if ( pArgs != NULL ) {
		pJob->Destroy = pArgs->Destroy;
		pJob->DestroyData = pArgs->DestroyData;
	}
	pJob->Promise = xrtPromiseCreate(&pJob->Future, pParent);
	if ( pJob->Promise == NULL ) {
		xrtFree(pJob);
		return NULL;
	}
	pJob->Cancel = xrtPromiseCancelToken(pJob->Promise);
	if ( pJob->Cancel == NULL ) {
		xrtPromiseDestroy(pJob->Promise);
		xrtFutureDestroy(pJob->Future);
		xrtFree(pJob);
		return NULL;
	}
	*ppFuture = pJob->Future;
	return pJob;
}



/* 执行任务或在执行前取消时跳过用户过程。 */
void __xrtTaskRun(xrt_task_job* pJob)
{
	if ( pJob != NULL ) {
		__xrtTaskFinish(pJob, true, false, NULL);
	}
}



/* 不执行用户过程，直接完成取消终态。 */
void __xrtTaskCancel(xrt_task_job* pJob)
{
	if ( pJob != NULL ) {
		(void)xrtCancelRequest(pJob->Cancel);
		__xrtTaskFinish(pJob, false, false, NULL);
	}
}



/* 不执行用户过程，以外部错误完成任务失败终态。 */
void __xrtTaskFail(xrt_task_job* pJob, const xerror* pError)
{
	if ( pJob != NULL ) {
		__xrtTaskFinish(pJob, false, true, pError);
	}
}



/* 返回任务 Future 当前状态。 */
xfuturestate __xrtTaskState(const xrt_task_job* pJob)
{
	return pJob != NULL ? xrtFutureState(pJob->Future) : XFUTURE_CLOSED;
}



/* 释放任务端点和作业存储，并按受理结果决定是否释放数据。 */
void __xrtTaskDestroy(xrt_task_job* pJob, bool bDestroyData)
{
	if ( pJob == NULL ) {
		return;
	}
	if ( bDestroyData && (pJob->Destroy != NULL) ) {
		pJob->Destroy(pJob->Data, pJob->DestroyData);
	}
	xrtCancelDestroy(pJob->Cancel);
	xrtPromiseDestroy(pJob->Promise);
	if ( !bDestroyData ) {
		xrtFutureDestroy(pJob->Future);
	}
	xrtFree(pJob);
}

#endif
