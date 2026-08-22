#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS_FUTURE)

/* 释放 Future 成功值拥有的退出状态快照。 */
static void __xrtProcessFutureStatusFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}



/* 把已发布的 Process 终态写入等待 Future。 */
void __xrtProcessFutureComplete(xprocess* pProcess)
{
	xpromise* pPromise = NULL;
	xprocessstatus* pStatus = NULL;
	xerror* pError = NULL;
	bool bCompleted;

	(void)xrtMutexLock(&pProcess->Lock);
	if ( (pProcess->State == XPROCESS_EXITED) &&
		(pProcess->WaitPromise != NULL) ) {
		pPromise = pProcess->WaitPromise;
		pStatus = pProcess->WaitStatus;
		pProcess->WaitPromise = NULL;
		pProcess->WaitStatus = NULL;
		*pStatus = pProcess->Status;
		if ( pProcess->Status.Kind == XPROCESS_EXIT_LOST ) {
			pError = xrtErrorRef(pProcess->Error);
		}
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	if ( pPromise == NULL ) {
		return;
	}
	if ( pError != NULL ) {
		bCompleted = xrtPromiseReject(pPromise, pError);
		xrtErrorFree(pError);
		xrtFree(pStatus);
	} else {
		bCompleted = xrtPromiseResolveOwned(
			pPromise,
			pStatus,
			__xrtProcessFutureStatusFree,
			NULL
		);
		if ( !bCompleted ) {
			xrtFree(pStatus);
		}
	}
	xrtPromiseDestroy(pPromise);
}



/* 返回每个 Process 唯一的共享终态 Future。 */
XRT_API xfuture* xrtProcessWaitAsync(xprocess* pProcess)
{
	xfuture* pCandidate = NULL;
	xfuture* pFuture;
	xpromise* pPromise = NULL;
	xprocessstatus* pStatus = NULL;
	bool bComplete = false;

	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"wait.async",
			"process is null",
			0
		);
		return NULL;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->WaitFuture != NULL ) {
		pFuture = xrtFutureRef(pProcess->WaitFuture);
		(void)xrtMutexUnlock(&pProcess->Lock);
		return pFuture;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);

	pStatus = (xprocessstatus*)xrtMalloc(sizeof(xprocessstatus));
	if ( pStatus == NULL ) {
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pCandidate, NULL);
	if ( pPromise == NULL ) {
		xrtFree(pStatus);
		return NULL;
	}

	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->WaitFuture == NULL ) {
		pProcess->WaitFuture = pCandidate;
		pProcess->WaitPromise = pPromise;
		pProcess->WaitStatus = pStatus;
		pCandidate = NULL;
		pPromise = NULL;
		pStatus = NULL;
		bComplete = pProcess->State == XPROCESS_EXITED;
	}
	pFuture = xrtFutureRef(pProcess->WaitFuture);
	(void)xrtMutexUnlock(&pProcess->Lock);

	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pCandidate);
	xrtFree(pStatus);
	if ( bComplete ) {
		__xrtProcessFutureComplete(pProcess);
	}
	return pFuture;
}

#endif
