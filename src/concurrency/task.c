#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK)

/* The executor serializes job state; short mutation scopes coordinate these
 * transitions with graph inspection, never enclosing user code or a wait.
 * Physical references: creator/accepted executor, output Future producer, and
 * explicit collector holds. Queue/worker transfers do not invent new refs. */
static void __xrtTaskMutationBegin(xrtownershipscope* pScope)
{ if (!xrtOwnershipMutationBegin(pScope)) abort(); }
static void __xrtTaskMutationEnd(xrtownershipscope* pScope)
{ if (!xrtOwnershipScopeEnd(pScope)) abort(); }
static bool __xrtTaskOwnershipCount(const void* pData, size_t* pCount)
{
	const xrt_task_job* pJob = pData; int32 iRefs;
	if (pJob == NULL || pCount == NULL || pJob->DataPolicy == NULL || !pJob->Accepted ||
		pJob->Active || pJob->OwnershipCleared) return false;
	iRefs = __xrtAtomicRefLoad(&pJob->RefCount);
	if (iRefs <= 0) return false;
	*pCount = (size_t)iRefs; return true;
}
static bool __xrtTaskOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xrt_task_job* pJob = pData; size_t iRefs;
	if (pVisit == NULL || !__xrtTaskOwnershipCount(pData, &iRefs)) return false;
	/* Future is borrowed under Promise after acceptance. Do not trace a second
	 * owning endpoint or an executor queue linkage which is only a transfer. */
	return pVisit(xrtPromiseOwnership(pJob->Promise), pContext) &&
		pVisit(xrtCancelOwnership(pJob->Cancel), pContext) &&
		(pJob->Data == NULL || pVisit((xrtownershipref){pJob->Data, pJob->DataPolicy->Ops}, pContext));
}
static const xrtownershipops __xrtTaskOwnershipOps = {__xrtTaskOwnershipCount, __xrtTaskOwnershipTrace};
xrtownershipref __xrtTaskOwnership(const xrt_task_job* pJob)
{ return (xrtownershipref){pJob, pJob != NULL ? &__xrtTaskOwnershipOps : NULL}; }
static bool __xrtTaskHold(const void* pData)
{
	xrt_task_job* pJob = (xrt_task_job*)pData; xrtownershipscope Mutation = {0}; bool bHeld;
	__xrtTaskMutationBegin(&Mutation);
	bHeld = !pJob->OwnershipCleared && xrtRefRetain(&pJob->RefCount) >= 0;
	__xrtTaskMutationEnd(&Mutation); return bHeld;
}
static void __xrtTaskRelease(const void* pData)
{
	xrt_task_job* pJob = (xrt_task_job*)pData; xrtownershipscope Mutation = {0}; int32 iRefs;
	__xrtTaskMutationBegin(&Mutation); iRefs = xrtRefRelease(&pJob->RefCount);
	if (iRefs < 0) abort();
	if (iRefs == 0) {
		if (!pJob->Destroyed || pJob->Active || pJob->Data != NULL || pJob->Promise != NULL || pJob->Cancel != NULL) abort();
		xrtFree(pJob);
	}
	__xrtTaskMutationEnd(&Mutation);
}
static const xfutureproducerownershipv1 __xrtTaskProducerPolicy = {sizeof(__xrtTaskProducerPolicy), __xrtTaskRelease};
XRT_API const xfutureproducerownershipv1* xrtTaskProducerPolicyV1Get(void) { return &__xrtTaskProducerPolicy; }
static bool __xrtTaskClaim(const void* pData, const void* pToken)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;
	if (pToken == NULL || pJob->OwnershipCleared || (pJob->OwnershipClaim != NULL && pJob->OwnershipClaim != pToken)) return false;
	pJob->OwnershipClaim = pToken; return true;
}
static void __xrtTaskRestore(const void* pData, const void* pToken)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;
	if (pToken == NULL || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared) abort();
	pJob->OwnershipClaim = NULL;
}
static bool __xrtTaskPreparationReady(const void* pData)
{
	const xrt_task_job* pJob = pData;
	return pJob->Destroyed && pJob->Finished && !pJob->Active;
}
static xrtownershipprepareresult __xrtTaskPrepare(const void* pData, const void* pToken)
{
	const xrt_task_job* pJob = pData; xrtownershipscope Freeze = {0}; bool bReady;
	/* A shared mutation is not a writer mutex. Sample executor-owned flags
	 * under a short nonblocking freeze, with no callback or wait inside it. */
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (pToken == NULL || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared) abort();
	bReady = __xrtTaskPreparationReady(pData); __xrtTaskMutationEnd(&Freeze);
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTaskClear(const void* pData, const void* pToken)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;
	if (pToken == NULL || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared ||
		!__xrtTaskPreparationReady(pData) || pJob->Data != NULL || pJob->Promise != NULL || pJob->Cancel != NULL) abort();
	pJob->OwnershipCleared = true;
}
static bool __xrtTaskFinishOwnership(const void* pData, const void* pToken)
{
	const xrt_task_job* pJob = pData;
	if (pToken == NULL || pJob->OwnershipClaim != pToken || !pJob->OwnershipCleared) abort();
	return true;
}
static const xrtownershipadapterv1 __xrtTaskAdapter = {sizeof(__xrtTaskAdapter),
	__xrtTaskHold, __xrtTaskRelease, __xrtTaskClaim, __xrtTaskRestore, NULL, __xrtTaskClear, __xrtTaskFinishOwnership};
static const xrtownershippreparationv1 __xrtTaskPreparation = {sizeof(__xrtTaskPreparation),
	&__xrtTaskAdapter, __xrtTaskPreparationReady, __xrtTaskPrepare};
XRT_API const xrtownershipadapterv1* xrtTaskOwnershipAdapterV1(xrtownershipref Reference,
	const xtaskdataownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	const xrt_task_job* pJob; const xtaskdataownershipv1* pPolicy; bool bKnown = false;
	if (Reference.Data == NULL || Reference.Ops != &__xrtTaskOwnershipOps || ppPreparation == NULL ||
		(iPolicyCount != 0 && pPolicies == NULL)) return NULL;
	pJob = Reference.Data; pPolicy = pJob->DataPolicy;
	for (size_t i = 0; i < iPolicyCount; ++i) if (pPolicies[i] != NULL && pPolicies[i] == pPolicy) { bKnown = true; break; }
	if (!bKnown || pPolicy->size != sizeof(*pPolicy) || pPolicy->Proc == NULL || pPolicy->Drop == NULL ||
		pPolicy->Ops == NULL || pPolicy->Ops->Count == NULL || pPolicy->Ops->Trace == NULL ||
		pJob->Proc != pPolicy->Proc || !pJob->Accepted || pJob->Active || pJob->OwnershipCleared ||
		__xrtAtomicRefLoad(&pJob->RefCount) <= 0) return NULL;
	if (pJob->Finished ? (pJob->Data != NULL || pJob->Destroy != NULL || pJob->DestroyData != NULL) :
		(pJob->Destroy != pPolicy->Drop || pJob->DestroyData != NULL || pJob->Data == NULL)) return NULL;
	*ppPreparation = &__xrtTaskPreparation; return &__xrtTaskAdapter;
}
void __xrtTaskAccept(xrt_task_job* pJob)
{
	xrtownershipscope Mutation = {0}; __xrtTaskMutationBegin(&Mutation);
	if (pJob->Accepted || pJob->Active || pJob->Finished || pJob->Destroyed) abort();
	pJob->Accepted = true; __xrtTaskMutationEnd(&Mutation);
}

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
			bCompleted = pJob->ResultPolicy != NULL ? xrtPromiseResolveOwnedPolicyV1(
				pJob->Promise, pValue->Value, pJob->ResultPolicy
			) : pJob->ResultTrace != NULL ? xrtPromiseResolveOwnedTraced(
				pJob->Promise, pValue->Value, pValue->Destroy,
				pValue->DestroyData, pJob->ResultTrace
			) : xrtPromiseResolveOwned(
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
	xrtownershipscope Mutation = {0};
	__xrtTaskMutationBegin(&Mutation);
	if (pJob->Active || pJob->Finished || pJob->Destroyed) abort();
	pJob->Active = true; __xrtTaskMutationEnd(&Mutation);

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

	/* Validate the closed result contract before retiring task data/code, not
	 * after publishing an incorrectly certified box to another thread. */
	if (Outcome == XTASK_SUCCESS && pJob->ResultPolicy != NULL &&
		(tValue.Destroy != NULL || tValue.Value != NULL || tValue.DestroyData != NULL) &&
		(tValue.Destroy != pJob->ResultPolicy->Drop || tValue.DestroyData != NULL)) {
		Outcome = XTASK_FAILED;
		__xrtErrorSetInvalidState();
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
	__xrtTaskMutationBegin(&Mutation);
	ptr pData = pJob->Data; ptr pDestroyData = pJob->DestroyData; xfuturefreeproc pDestroy = pJob->Destroy;
	pJob->Data = NULL; pJob->Destroy = NULL; pJob->DestroyData = NULL;
	__xrtTaskMutationEnd(&Mutation);
	if (pDestroy != NULL) pDestroy(pData, pDestroyData);
	__xrtTaskComplete(pJob, Outcome, &tValue, pError);
	xrtErrorFree(pError);
	xrtClearError();
	if ( bTempReady ) {
		(void)__xrtTempContextSwap(pPreviousTemp);
		xrtTempUnit(&tTemp);
	}
	(void)__xrtErrorContextSwap(pPreviousError);
	__xrtTaskMutationBegin(&Mutation); pJob->Finished = true; pJob->Active = false; __xrtTaskMutationEnd(&Mutation);
}



/* 创建尚未被执行器受理的任务作业和 Future/Promise 对。 */
xrt_task_job* __xrtTaskCreateOwned(
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	const xtaskdataownershipv1* pDataPolicy,
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
	pJob->RefCount = 1; pJob->DataPolicy = pDataPolicy;
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
	/* A real second holder is transferred into the private Future. Even opaque
	 * jobs must not masquerade as producerless pending sources to a collector. */
	pJob->RefCount = 2;
	if (!xrtPromiseProducerBindTakeV1(pJob->Promise,
		(xrtownershipref){pJob, &__xrtTaskOwnershipOps}, &__xrtTaskProducerPolicy)) {
		pJob->RefCount = 1; __xrtTaskDestroy(pJob, false); return NULL;
	}
	*ppFuture = pJob->Future;
	return pJob;
}

xrt_task_job* __xrtTaskCreate(xtaskproc pProc, ptr pData, const xtaskargs* pArgs, xfuture** ppFuture)
{ return __xrtTaskCreateOwned(pProc, pData, pArgs, NULL, ppFuture); }



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
	xrtownershipscope Mutation = {0}; __xrtTaskMutationBegin(&Mutation);
	if (pJob->Active || pJob->Destroyed) abort();
	pJob->Active = true;
	ptr pData = pJob->Data; ptr pDestroyData = pJob->DestroyData; xfuturefreeproc pDestroy = pJob->Destroy;
	xcancel* pCancel = pJob->Cancel; xpromise* pPromise = pJob->Promise; xfuture* pFuture = pJob->Future;
	pJob->Data = NULL; pJob->Destroy = NULL; pJob->DestroyData = NULL;
	pJob->Cancel = NULL; pJob->Promise = NULL; pJob->Future = NULL;
	__xrtTaskMutationEnd(&Mutation);
	if (bDestroyData && pDestroy != NULL) pDestroy(pData, pDestroyData);
	xrtCancelDestroy(pCancel);
	xrtPromiseDestroy(pPromise);
	if ( !bDestroyData ) {
		xrtFutureDestroy(pFuture);
	}
	__xrtTaskMutationBegin(&Mutation);
	pJob->Destroyed = true; pJob->Finished = true; pJob->Active = false;
	__xrtTaskMutationEnd(&Mutation);
	__xrtTaskRelease(pJob);
}

#endif
