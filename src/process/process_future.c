#include "../internal/xrt_process.h"



#if defined(XRT_FEATURE_PROCESS_FUTURE)

/* 释放 Future 成功值拥有的退出状态快照。 */
static void __xrtProcessFutureStatusFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtFree(pValue);
}

static bool __xrtProcessFutureStatusTrace(const void* pValue, const void* pData,
	xrtownershipvisitor Visit, ptr pContext)
{
	(void)pContext;
	return pValue != NULL && pData == NULL && Visit != NULL;
}
static const xfuturepayloadownershipv1 __xrtProcessFutureStatusPolicy = {
	sizeof(xfuturepayloadownershipv1), __xrtProcessFutureStatusFree, __xrtProcessFutureStatusTrace
};
XRT_API const xfuturepayloadownershipv1* xrtProcessFuturePayloadPolicyV1Get(void)
{ return &__xrtProcessFutureStatusPolicy; }



/* 把已发布的 Process 终态写入等待 Future。 */
void __xrtProcessFutureComplete(xprocess* pProcess)
{
	xpromise* pPromise = NULL;
	xprocessstatus* pStatus = NULL;
	xerror* pError = NULL;
	bool bCompleted;
	xrtownershipscope Mutation = {0};

	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
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
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( pPromise == NULL ) {
		return;
	}
	if ( pError != NULL ) {
		bCompleted = xrtPromiseReject(pPromise, pError);
		xrtErrorFree(pError);
		xrtFree(pStatus);
	} else {
		bCompleted = xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pStatus,
			&__xrtProcessFutureStatusPolicy
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
	xrtownershipscope Mutation = {0};

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
	if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;
	(void)xrtMutexLock(&pProcess->Lock);
	if (pProcess->OwnershipCleared) {
		(void)xrtMutexUnlock(&pProcess->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidState(); return NULL;
	}
	if ( pProcess->WaitFuture != NULL ) {
		pFuture = xrtFutureRef(pProcess->WaitFuture);
		(void)xrtMutexUnlock(&pProcess->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return pFuture;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	memset(&Mutation, 0, sizeof(Mutation));

	pStatus = (xprocessstatus*)xrtMalloc(sizeof(xprocessstatus));
	if ( pStatus == NULL ) {
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pCandidate, NULL);
	if ( pPromise == NULL ) {
		xrtFree(pStatus);
		return NULL;
	}

	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	(void)xrtMutexLock(&pProcess->Lock);
	if (pProcess->OwnershipCleared) {
		(void)xrtMutexUnlock(&pProcess->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		xrtPromiseDestroy(pPromise); xrtFutureDestroy(pCandidate); xrtFree(pStatus);
		__xrtErrorSetInvalidState(); return NULL;
	}
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
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();

	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pCandidate);
	xrtFree(pStatus);
	if ( bComplete ) {
		__xrtProcessFutureComplete(pProcess);
	}
	return pFuture;
}

#endif
