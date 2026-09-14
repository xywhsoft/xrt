#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_FUTURE)

/* Promise 嵌入 Future 对象，创建一对端点只产生一次堆分配。 */
struct xpromise {
	struct xfuture* Future;
};



/* Future 用一个锁保护终态、结果、条件变量和内部等待链。 */
struct xfuture {
	volatile int32 RefCount;
	volatile int32 PromiseRefs;
	xmutex Lock;
	xcond Ready;
	xfuturestate State;
	bool Completing;
	ptr Value;
	xfuturefreeproc Destroy;
	ptr DestroyData;
	xfutureownershiptrace OwnershipTrace;
	const xfuturepayloadownershipv1* OwnershipPolicy;
	xrtownershipref Producer;
	const xfutureproducerownershipv1* ProducerPolicy;
	const void* OwnershipClaim;
	bool OwnershipCleared;
	struct xfuture* Owner;
	xerror* Error;
	xcancel* Cancel;
	xrt_future_waiter* Waiters;
	xrt_future_waiter* WaitersTail;
	xpromise Promise;
};



static bool __xrtFutureOwnershipCount(const void* pData, size_t* pCount)
{
	const xfuture* pFuture = (const xfuture*)pData;
	int32 iCount;
	if (pFuture == NULL || pCount == NULL || pFuture->OwnershipCleared) return false;
	iCount = __xrtAtomicRefLoad(&pFuture->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}

static bool __xrtFutureOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xfuture* pFuture = (const xfuture*)pData;
	const xrt_future_waiter* pLast = NULL;
	if (pFuture == NULL || pVisit == NULL || pFuture->Completing || pFuture->OwnershipCleared) return false;
	/* Locks alone cannot stabilize the transitive graph. This entire traversal
	 * uses the caller's quiescent point, exactly like Value/callable views. */
	for (const xrt_future_waiter* pWaiter = pFuture->Waiters; pWaiter != NULL; pWaiter = pWaiter->Next) {
		if (!pWaiter->Linked || pWaiter->Calling) return false;
		if (pWaiter->Certified == 1) {
			const xfuturewatchownershipv1* pPolicy = pWaiter->OwnershipPolicy;
			if (pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) || pPolicy->Ops == NULL ||
				!pVisit((xrtownershipref){pWaiter->Data, pPolicy->Ops}, pContext)) return false;
		} else if (pWaiter->Certified == 2) {
			const xfuturewatchownershipv2* pPolicy = pWaiter->ProjectedOwnershipPolicy;
			xrtownershipref Owner;
			if (pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) || pPolicy->Reference == NULL) return false;
			Owner = pPolicy->Reference(pWaiter->Data);
			if (Owner.Data == NULL || Owner.Ops == NULL || !pVisit(Owner, pContext)) return false;
		} else if (pWaiter->Certified != 0 || pWaiter->OwnershipTrace == NULL ||
			!pWaiter->OwnershipTrace(pWaiter->Data, pVisit, pContext)) return false;
		pLast = pWaiter;
	}
	if (pLast != pFuture->WaitersTail) return false;
	if (pFuture->Producer.Data != NULL && !pVisit(pFuture->Producer, pContext)) return false;
	if (!pVisit(xrtCancelOwnership(pFuture->Cancel), pContext)) return false;
	if (pFuture->Error != NULL && !pVisit(xrtErrorOwnership(pFuture->Error), pContext)) return false;
	if (pFuture->Owner != NULL && !pVisit(xrtFutureOwnership(pFuture->Owner), pContext)) return false;
	if (pFuture->Destroy != NULL) {
		if (pFuture->OwnershipTrace == NULL) return false;
		return pFuture->OwnershipTrace(pFuture->Value, pFuture->DestroyData, pVisit, pContext);
	}
	return true;
}

static const xrtownershipops __xrtFutureOwnershipOps = {
	__xrtFutureOwnershipCount, __xrtFutureOwnershipTrace
};

XRT_API xrtownershipref xrtFutureOwnership(const xfuture* pFuture)
{
	xrtownershipref Result = {pFuture, pFuture != NULL ? &__xrtFutureOwnershipOps : NULL};
	return Result;
}

XRT_API xrtownershipref xrtPromiseOwnership(const xpromise* pPromise)
{
	return xrtFutureOwnership(pPromise != NULL ? pPromise->Future : NULL);
}

static void __xrtFutureProducerDrop(xrtownershipref Producer, const xfutureproducerownershipv1* pPolicy)
{
	xerror* pPrevious;
	if (Producer.Data == NULL) { if (pPolicy != NULL) abort(); return; }
	if (pPolicy == NULL) abort();
	pPrevious = xrtTakeError();
	pPolicy->Drop(Producer.Data);
	xrtClearError(); xrtSetErrorTake(pPrevious);
}

XRT_API bool xrtPromiseProducerBindTakeV1(xpromise* pPromise, xrtownershipref Producer,
	const xfutureproducerownershipv1* pPolicy)
{
	xrtownershipscope Mutation = {0}; xfuture* pFuture; bool bBound = false;
	if (pPromise == NULL || Producer.Data == NULL || Producer.Ops == NULL ||
		Producer.Ops->Count == NULL || Producer.Ops->Trace == NULL ||
		pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) || pPolicy->Drop == NULL) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	pFuture = pPromise->Future;
	if (!xrtMutexLock(&pFuture->Lock)) { if (!xrtOwnershipScopeEnd(&Mutation)) abort(); return false; }
	if (pFuture->State == XFUTURE_PENDING && !pFuture->Completing && !pFuture->OwnershipCleared &&
		pFuture->OwnershipClaim == NULL && pFuture->Producer.Data == NULL &&
		pFuture->Waiters == NULL && pFuture->WaitersTail == NULL &&
		__xrtAtomicRefLoad(&pFuture->RefCount) == 2 && __xrtAtomicRefLoad(&pFuture->PromiseRefs) == 1) {
		pFuture->Producer = Producer; pFuture->ProducerPolicy = pPolicy; bBound = true;
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bBound) __xrtErrorSetInvalidState();
	return bBound;
}

static bool __xrtFutureAdapterHold(const void* pData)
{ return xrtFutureRef((xfuture*)pData) != NULL; }
static void __xrtFutureAdapterDrop(const void* pData)
{ xrtFutureDestroy((xfuture*)pData); }
static bool __xrtFutureAdapterClaim(const void* pData, const void* pToken)
{
	xfuture* pFuture = (xfuture*)pData;
	if (pToken == NULL || pFuture->OwnershipCleared ||
		(pFuture->OwnershipClaim != NULL && pFuture->OwnershipClaim != pToken)) return false;
	pFuture->OwnershipClaim = pToken; return true;
}
static void __xrtFutureAdapterRestore(const void* pData, const void* pToken)
{
	xfuture* pFuture = (xfuture*)pData;
	if (pToken == NULL || pFuture->OwnershipClaim != pToken || pFuture->OwnershipCleared) abort();
	pFuture->OwnershipClaim = NULL;
}
static void __xrtFutureAdapterClear(const void* pData, const void* pToken)
{
	xfuture* pFuture = (xfuture*)pData;
	if (pToken == NULL || pFuture->OwnershipClaim != pToken || pFuture->OwnershipCleared ||
		pFuture->Completing || pFuture->Waiters != NULL || pFuture->WaitersTail != NULL) abort();
	/* Revalidation and Clear share the exclusive freeze. No observer can be
	 * registered in between, so closing the last producer cannot notify code. */
	if (pFuture->State == XFUTURE_PENDING) __xrtCancelOwnershipCloseUnobserved(pFuture->Cancel);
	pFuture->State = XFUTURE_CLOSED;
	pFuture->OwnershipCleared = true;
	/* Actual slots remain owned and all targets stay pinned until Finish. */
}
static bool __xrtFutureAdapterFinish(const void* pData, const void* pToken)
{
	xfuture* pFuture = (xfuture*)pData;
	xrtownershipscope Mutation = {0};
	ptr pValue; xfuturefreeproc pDestroy; xfuture* pOwner; xerror* pError; xcancel* pCancel;
	xrtownershipref Producer; const xfutureproducerownershipv1* pProducerPolicy;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (pToken == NULL || pFuture->OwnershipClaim != pToken || !pFuture->OwnershipCleared) abort();
	pValue = pFuture->Value; pDestroy = pFuture->Destroy; pOwner = pFuture->Owner;
	pError = pFuture->Error; pCancel = pFuture->Cancel;
	Producer = pFuture->Producer; pProducerPolicy = pFuture->ProducerPolicy;
	pFuture->Value = NULL; pFuture->Destroy = NULL; pFuture->DestroyData = NULL;
	pFuture->OwnershipTrace = NULL; pFuture->OwnershipPolicy = NULL;
	pFuture->Owner = NULL; pFuture->Error = NULL; pFuture->Cancel = NULL;
	pFuture->Producer = (xrtownershipref){0}; pFuture->ProducerPolicy = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pDestroy != NULL) pDestroy(pValue, NULL);
	xrtFutureDestroy(pOwner); xrtErrorFree(pError); xrtCancelDestroy(pCancel);
	__xrtFutureProducerDrop(Producer, pProducerPolicy);
	return true;
}
static bool __xrtFuturePreparationReady(const void* pData)
{
	const xfuture* pFuture = pData;
	return pFuture->State != XFUTURE_PENDING ||
		(pFuture->Waiters == NULL && pFuture->Producer.Data == NULL);
}
static xrtownershipprepareresult __xrtFuturePrepare(const void* pData, const void* pToken)
{
	xfuture* pFuture = (xfuture*)pData; xrtownershipscope Mutation = {0}; bool bReady, bProduced;
	if (!xrtOwnershipMutationBegin(&Mutation)) return XRT_OWNERSHIP_PREPARE_FAILED;
	if (!xrtMutexLock(&pFuture->Lock)) { if (!xrtOwnershipScopeEnd(&Mutation)) abort(); return XRT_OWNERSHIP_PREPARE_FAILED; }
	if (pToken == NULL || pFuture->OwnershipClaim != pToken || pFuture->OwnershipCleared) abort();
	bReady = __xrtFuturePreparationReady(pFuture); bProduced = pFuture->Producer.Data != NULL;
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (bReady) return XRT_OWNERSHIP_PREPARE_READY;
	if (bProduced) return XRT_OWNERSHIP_PREPARE_BUSY;
	/* The plan's actual hold retains this endpoint and all callback code. Normal
	 * publication invokes every accepted callback; Clear must never do that. */
	if (xrtPromiseClose(&pFuture->Promise) || xrtFutureDone(pFuture)) return XRT_OWNERSHIP_PREPARE_READY;
	return XRT_OWNERSHIP_PREPARE_FAILED;
}
static const xrtownershipadapterv1 __xrtFutureAdapter = {sizeof(__xrtFutureAdapter),
	__xrtFutureAdapterHold, __xrtFutureAdapterDrop, __xrtFutureAdapterClaim,
	__xrtFutureAdapterRestore, NULL, __xrtFutureAdapterClear, __xrtFutureAdapterFinish};
static const xrtownershippreparationv1 __xrtFuturePreparation = {sizeof(__xrtFuturePreparation),
	&__xrtFutureAdapter, __xrtFuturePreparationReady, __xrtFuturePrepare};
static const xrtownershipadapterv1* __xrtFutureAdapterQuery(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount,
	const xfutureproducerownershipv1* const* pProducerPolicies, size_t iProducerPolicyCount,
	const xfuturewatchownershipv1* const* pWatchPolicies, size_t iWatchPolicyCount,
	const xfutureownershipadmissionv1* pAdmission)
{
	const xfuture* pFuture;
	if (Reference.Ops != &__xrtFutureOwnershipOps || Reference.Data == NULL ||
		(iPolicyCount != 0 && pPolicies == NULL) ||
		(iProducerPolicyCount != 0 && pProducerPolicies == NULL) ||
		(iWatchPolicyCount != 0 && pWatchPolicies == NULL) || (pAdmission &&
		(pAdmission->size != sizeof(*pAdmission) ||
		(pAdmission->ProjectedWatchPolicyCount && !pAdmission->ProjectedWatchPolicies) ||
		(pAdmission->CancelWatchPolicyCount && !pAdmission->CancelWatchPolicies)))) return NULL;
	pFuture = (const xfuture*)Reference.Data;
	if (pFuture->Completing || pFuture->OwnershipCleared || __xrtAtomicRefLoad(&pFuture->RefCount) <= 0) return NULL;
	if (pAdmission) {
		const xrtownershippreparationv1* pPreparation = NULL;
		if (!xrtCancelOwnershipAdapterV2(xrtCancelOwnership(pFuture->Cancel), pAdmission->CancelWatchPolicies,
			pAdmission->CancelWatchPolicyCount, &pPreparation)) return NULL;
	} else if (!xrtCancelOwnershipAdapterV1(xrtCancelOwnership(pFuture->Cancel))) return NULL;
	const xrt_future_waiter* pLast = NULL;
	for (const xrt_future_waiter* pWaiter = pFuture->Waiters; pWaiter != NULL; pWaiter = pWaiter->Next) {
		bool bKnown = false;
		if (!pWaiter->Certified || !pWaiter->Phased || !pWaiter->Linked || pWaiter->Calling || pWaiter->Data == NULL) return NULL;
		if (pWaiter->Certified == 1) {
			for (size_t i = 0; i < iWatchPolicyCount; ++i)
				if (pWatchPolicies[i] != NULL && pWaiter->OwnershipPolicy == pWatchPolicies[i]) { bKnown = true; break; }
			if (!bKnown) return NULL;
			const xfuturewatchownershipv1* pPolicy = pWaiter->OwnershipPolicy;
			if (pPolicy->size != sizeof(*pPolicy) || pPolicy->Notify != pWaiter->Proc ||
				pPolicy->Release != pWaiter->Release || pPolicy->Ops == NULL ||
				pPolicy->Ops->Count == NULL || pPolicy->Ops->Trace == NULL) return NULL;
		} else if (pWaiter->Certified == 2 && pAdmission) {
			for (size_t i = 0; i < pAdmission->ProjectedWatchPolicyCount; ++i)
				if (pAdmission->ProjectedWatchPolicies[i] != NULL &&
					pWaiter->ProjectedOwnershipPolicy == pAdmission->ProjectedWatchPolicies[i]) { bKnown = true; break; }
			if (!bKnown) return NULL; /* Never project an unknown policy. */
			const xfuturewatchownershipv2* pPolicy = pWaiter->ProjectedOwnershipPolicy;
			if (pPolicy->size != sizeof(*pPolicy) || pPolicy->Notify != pWaiter->Proc ||
				pPolicy->Release != pWaiter->Release || pPolicy->Reference == NULL) return NULL;
		} else return NULL;
		pLast = pWaiter;
	}
	if (pLast != pFuture->WaitersTail) return NULL;
	if (pFuture->Producer.Data != NULL) {
		bool bKnown = false;
		for (size_t i = 0; i < iProducerPolicyCount; ++i)
			if (pProducerPolicies[i] != NULL && pFuture->ProducerPolicy == pProducerPolicies[i]) { bKnown = true; break; }
		if (!bKnown || pFuture->ProducerPolicy->size != sizeof(xfutureproducerownershipv1) ||
			pFuture->ProducerPolicy->Drop == NULL || pFuture->Producer.Ops == NULL ||
			pFuture->Producer.Ops->Count == NULL || pFuture->Producer.Ops->Trace == NULL) return NULL;
	} else if (pFuture->ProducerPolicy != NULL) return NULL;
	if (pFuture->Destroy != NULL) {
		bool bKnown = false;
		/* Match identity BEFORE dereferencing a producer's descriptor. */
		for (size_t i = 0; i < iPolicyCount; ++i)
			if (pPolicies[i] != NULL && pFuture->OwnershipPolicy == pPolicies[i]) { bKnown = true; break; }
		if (!bKnown || pFuture->OwnershipPolicy->size != sizeof(xfuturepayloadownershipv1) ||
			pFuture->OwnershipPolicy->Drop != pFuture->Destroy ||
			pFuture->OwnershipPolicy->Trace != pFuture->OwnershipTrace || pFuture->DestroyData != NULL) return NULL;
	}
	return &__xrtFutureAdapter;
}
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV2(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount,
	const xfutureproducerownershipv1* const* pProducerPolicies, size_t iProducerPolicyCount)
{ return __xrtFutureAdapterQuery(Reference, pPolicies, iPolicyCount, pProducerPolicies, iProducerPolicyCount, NULL, 0, NULL); }
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV3(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount,
	const xfutureproducerownershipv1* const* pProducerPolicies, size_t iProducerPolicyCount,
	const xfuturewatchownershipv1* const* pWatchPolicies, size_t iWatchPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	const xrtownershipadapterv1* pAdapter;
	if (ppPreparation == NULL) return NULL;
	pAdapter = __xrtFutureAdapterQuery(Reference, pPolicies, iPolicyCount,
		pProducerPolicies, iProducerPolicyCount, pWatchPolicies, iWatchPolicyCount, NULL);
	if (pAdapter != NULL) *ppPreparation = &__xrtFuturePreparation;
	return pAdapter;
}
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV4(xrtownershipref Reference,
	const xfutureownershipadmissionv1* pAdmission, const xrtownershippreparationv1** ppPreparation)
{
	const xrtownershipadapterv1* pAdapter;
	if (!pAdmission || pAdmission->size != sizeof(*pAdmission) || !ppPreparation) return NULL;
	pAdapter = __xrtFutureAdapterQuery(Reference, pAdmission->PayloadPolicies, pAdmission->PayloadPolicyCount,
		pAdmission->ProducerPolicies, pAdmission->ProducerPolicyCount,
		pAdmission->WatchPolicies, pAdmission->WatchPolicyCount, pAdmission);
	if (pAdapter) *ppPreparation = &__xrtFuturePreparation;
	return pAdapter;
}
XRT_API const xrtownershipadapterv1* xrtFutureOwnershipAdapterV1(xrtownershipref Reference,
	const xfuturepayloadownershipv1* const* pPolicies, size_t iPolicyCount)
{
	return xrtFutureOwnershipAdapterV2(Reference, pPolicies, iPolicyCount, NULL, 0);
}

/* 同一执行上下文中的完成通知使用迭代队列，避免 Future 链递归耗尽线程栈。 */
typedef struct xrt_future_notify_context {
	xrt_future_waiter* Head;
	xrt_future_waiter* Tail;
} xrt_future_notify_context;



#if defined(_WIN32) || defined(_WIN64)

static DWORD __xrtFutureNotifyKey = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtFutureNotifyKeyState;
static xrt_local_slot __xrtFutureNotifySlot;



/* 进程内只创建一次 Fiber 本地通知槽，使 Windows 协程切换不会混用派发队列。 */
static bool __xrtFutureNotifyKeyEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtFutureNotifyKeyState, 1, 0);

	if ( iState == 0 ) {
		__xrtFutureNotifyKey = __xrtLocalSlotAlloc(&__xrtFutureNotifySlot,
			NULL, XRT_LOCAL_BORROWED, true);
		InterlockedExchange(
			&__xrtFutureNotifyKeyState,
			__xrtFutureNotifyKey != FLS_OUT_OF_INDEXES ? 2 : 3
		);
		return __xrtFutureNotifyKey != FLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(
		&__xrtFutureNotifyKeyState, 0, 0
	)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 返回当前 Fiber 正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	return __xrtFutureNotifyKeyEnsure() ?
		(xrt_future_notify_context*)FlsGetValue(__xrtFutureNotifyKey) : NULL;
}



/* 切换当前 Fiber 的通知队列；失败时调用方退回直接派发。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	return __xrtFutureNotifyKeyEnsure() &&
		(FlsSetValue(__xrtFutureNotifyKey, pContext) != 0);
}

#elif defined(__TINYC__)

static pthread_key_t __xrtFutureNotifyKey;
static pthread_once_t __xrtFutureNotifyKeyOnce = PTHREAD_ONCE_INIT;
static bool __xrtFutureNotifyKeyReady;



/* 为 TinyCC POSIX 构建创建不带析构器的通知上下文槽。 */
static void __xrtFutureNotifyKeyInit(void)
{
	__xrtFutureNotifyKeyReady =
		pthread_key_create(&__xrtFutureNotifyKey, NULL) == 0;
}



/* 返回当前线程正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	(void)pthread_once(&__xrtFutureNotifyKeyOnce, __xrtFutureNotifyKeyInit);
	return __xrtFutureNotifyKeyReady ?
		(xrt_future_notify_context*)pthread_getspecific(__xrtFutureNotifyKey) :
		NULL;
}



/* 切换当前线程的通知队列；失败时调用方退回直接派发。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	(void)pthread_once(&__xrtFutureNotifyKeyOnce, __xrtFutureNotifyKeyInit);
	return __xrtFutureNotifyKeyReady &&
		(pthread_setspecific(__xrtFutureNotifyKey, pContext) == 0);
}

#else

static XRT_THREAD_LOCAL xrt_future_notify_context*
	__xrtFutureNotifyContext;



/* 返回当前线程正在使用的通知队列。 */
static xrt_future_notify_context* __xrtFutureNotifyContextGet(void)
{
	return __xrtFutureNotifyContext;
}



/* 切换当前线程的通知队列。 */
static bool __xrtFutureNotifyContextSet(xrt_future_notify_context* pContext)
{
	__xrtFutureNotifyContext = pContext;
	return true;
}

#endif



/* 释放当前 Future，并返回需要继续释放的透传结果所有者。 */
static xfuture* __xrtFutureFree(xfuture* pFuture, xrtownershipscope* pMutation)
{
	ptr pValue = pFuture->Value;
	xfuturefreeproc pDestroy = pFuture->Destroy;
	ptr pDestroyData = pFuture->DestroyData;
	xfuture* pOwner = pFuture->Owner;
	xerror* pError = pFuture->Error;
	xcancel* pCancel = pFuture->Cancel;
	bool bPhased = pFuture->OwnershipPolicy != NULL;
	/* Last PromiseDestroy closes before returning its physical reference;
	 * terminal publication or graph Finish has already returned this owner. */
	if (pFuture->Producer.Data != NULL || pFuture->ProducerPolicy != NULL) abort();

	(void)xrtCondUnit(&pFuture->Ready);
	(void)xrtMutexUnit(&pFuture->Lock);
	xrtFree(pFuture);
	/* Only explicit payload policies certify cooperative destruction. Legacy
	 * trace-only callbacks retain their conservative mutation exclusion. */
	if (bPhased && !xrtOwnershipScopeEnd(pMutation)) abort();
	if ( pDestroy != NULL ) {
		pDestroy(pValue, pDestroyData);
	}
	xrtErrorFree(pError);
	xrtCancelDestroy(pCancel);
	if (!bPhased && !xrtOwnershipScopeEnd(pMutation)) abort();
	return pOwner;
}



/* 迭代释放 Future 及透传所有者链，避免深延续链递归耗尽线程栈。 */
static void __xrtFutureRelease(xfuture* pFuture)
{
	while (pFuture != NULL) {
		xrtownershipscope Mutation = {0};
		if (!xrtOwnershipMutationBegin(&Mutation)) return;
		if (xrtRefRelease(&pFuture->RefCount) != 0) {
			if (!xrtOwnershipScopeEnd(&Mutation)) abort();
			return;
		}
		pFuture = __xrtFutureFree(pFuture, &Mutation);
	}
}



static void __xrtFutureInvoke(void (*pProc)(ptr), ptr pData, bool bPhased)
{
	xrtownershipscope Mutation = {0};
	if (pProc == NULL) return;
	if (!bPhased && !xrtOwnershipMutationBegin(&Mutation)) abort();
	pProc(pData);
	if (!bPhased && !xrtOwnershipScopeEnd(&Mutation)) abort();
}

/* 执行一个完成通知，并在回调返回后发布节点可移除状态。 */
static void __xrtFutureNotifyOne(xrt_future_waiter* pWaiter)
{
	xrtownershipscope Mutation = {0};
	xfuture* pFuture = pWaiter->NotifyFuture;
	void (*pRelease)(ptr pData) = pWaiter->Release;
	ptr pData = pWaiter->Data;
	bool bReleaseFuture = pWaiter->NotifyRelease;
	bool bPhased = pWaiter->Phased;

	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	pWaiter->Next = NULL;
	pWaiter->NotifyFuture = NULL;
	pWaiter->NotifyRelease = false;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	__xrtFutureInvoke(pWaiter->Proc, pData, bPhased);
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	(void)xrtMutexLock(&pFuture->Lock);
	pWaiter->Calling = false;
	(void)xrtCondBroadcast(&pFuture->Ready);
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( pRelease != NULL ) {
		__xrtFutureInvoke(pRelease, pData, bPhased);
	}
	if ( bReleaseFuture ) {
		xrtFutureDestroy(pFuture);
	}
}



/* 在 Future 锁外把完成批次加入当前执行上下文，并由最外层调用迭代排空。 */
static void __xrtFutureNotify(
	xrt_future_waiter* pHead,
	xrt_future_waiter* pTail
)
{
	xrt_future_notify_context tContext;
	xrt_future_notify_context* pContext = __xrtFutureNotifyContextGet();

	if ( pHead == NULL ) {
		return;
	}
	if ( pContext != NULL ) {
		if ( pContext->Tail != NULL ) {
			pContext->Tail->Next = pHead;
		} else {
			pContext->Head = pHead;
		}
		pContext->Tail = pTail;
		return;
	}
	memset(&tContext, 0, sizeof(tContext));
	if ( !__xrtFutureNotifyContextSet(&tContext) ) {
		while ( pHead != NULL ) {
			xrt_future_waiter* pNext = pHead->Next;

			__xrtFutureNotifyOne(pHead);
			pHead = pNext;
		}
		return;
	}
	tContext.Head = pHead;
	tContext.Tail = pTail;
	while ( tContext.Head != NULL ) {
		xrt_future_waiter* pWaiter = tContext.Head;

		tContext.Head = pWaiter->Next;
		if ( tContext.Head == NULL ) {
			tContext.Tail = NULL;
		}
		__xrtFutureNotifyOne(pWaiter);
	}
	(void)__xrtFutureNotifyContextSet(NULL);
}



/* 在持锁状态下发布唯一终态，并摘取全部等待节点。 */
static void __xrtFuturePublishLocked(
	xfuture* pFuture,
	xfuturestate State,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData,
	xfuture* pOwner,
	xerror* pError,
	xrt_future_waiter** ppWaiter,
	xrt_future_waiter** ppWaiterTail,
	xrtownershipref* pProducer,
	const xfutureproducerownershipv1** ppProducerPolicy,
	xfutureownershiptrace pTrace,
	const xfuturepayloadownershipv1* pPolicy
)
{
	xrt_future_waiter* pWaiterTail = NULL;

	pFuture->State = State;
	pFuture->Completing = false;
	pFuture->Value = pValue;
	pFuture->Destroy = pDestroy;
	pFuture->DestroyData = pDestroyData;
	pFuture->OwnershipTrace = pTrace;
	pFuture->OwnershipPolicy = pPolicy;
	pFuture->Owner = pOwner;
	pFuture->Error = pError;
	*pProducer = pFuture->Producer; *ppProducerPolicy = pFuture->ProducerPolicy;
	pFuture->Producer = (xrtownershipref){0}; pFuture->ProducerPolicy = NULL;
	*ppWaiter = pFuture->Waiters;
	pFuture->Waiters = NULL;
	pFuture->WaitersTail = NULL;
	for ( xrt_future_waiter* pCurrent = *ppWaiter;
		pCurrent != NULL; pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
		pCurrent->Calling = true;
		pCurrent->NotifyFuture = pFuture;
		pCurrent->NotifyRelease = false;
		pWaiterTail = pCurrent;
	}
	if ( pWaiterTail != NULL ) {
		pWaiterTail->NotifyRelease = true;
		(void)xrtFutureRef(pFuture);
	}
	*ppWaiterTail = pWaiterTail;
	(void)xrtCondBroadcast(&pFuture->Ready);
}



/* 把 Pending 原子转换为唯一终态，并按成功与失败保存结果。 */
static bool __xrtFutureCompleteTraced(
	xfuture* pFuture,
	xfuturestate State,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData,
	xfuture* pOwner,
	xerror* pError,
	bool bRequestCancel,
	bool bReportDuplicate,
	xfutureownershiptrace pTrace,
	const xfuturepayloadownershipv1* pPolicy
)
{
	bool bCompleted = false;
	bool bReserved = false;
	xrtownershipscope Mutation = {0};
	xrt_future_waiter* pWaiter = NULL;
	xrt_future_waiter* pWaiterTail = NULL;
	xrtownershipref Producer = {0};
	const xfutureproducerownershipv1* pProducerPolicy = NULL;

	if ( pFuture == NULL ) {
		if ( bReportDuplicate ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return false;
	}
	if ( (pFuture->State == XFUTURE_PENDING) && !pFuture->Completing && !pFuture->OwnershipCleared ) {
		if ( bRequestCancel ) {
			pFuture->Completing = true;
			bReserved = true;
		} else {
			__xrtFuturePublishLocked(
				pFuture,
				State,
				pValue,
				pDestroy,
				pDestroyData,
				pOwner,
				pError,
				&pWaiter,
				&pWaiterTail,
				&Producer, &pProducerPolicy,
				pTrace, pPolicy
			);
			bCompleted = true;
		}
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();

	if ( bReserved ) {
		(void)xrtCancelRequest(pFuture->Cancel);
		if (!xrtOwnershipMutationBegin(&Mutation)) abort();
		if ( !xrtMutexLock(&pFuture->Lock) ) {
			if (!xrtOwnershipScopeEnd(&Mutation)) abort();
			return false;
		}
		__xrtFuturePublishLocked(
			pFuture,
			State,
			pValue,
			pDestroy,
			pDestroyData,
			pOwner,
			pError,
			&pWaiter,
			&pWaiterTail,
			&Producer, &pProducerPolicy,
			pTrace, pPolicy
		);
		bCompleted = true;
		(void)xrtMutexUnlock(&pFuture->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	}
	if ( bCompleted ) {
		__xrtFutureProducerDrop(Producer, pProducerPolicy);
		__xrtFutureNotify(pWaiter, pWaiterTail);
	} else if ( bReportDuplicate ) {
		__xrtErrorSetInvalidState();
	}
	return bCompleted;
}



static bool __xrtFutureComplete(xfuture* pFuture, xfuturestate State, ptr pValue,
	xfuturefreeproc pDestroy, ptr pDestroyData, xfuture* pOwner, xerror* pError,
	bool bRequestCancel, bool bReportDuplicate)
{
	return __xrtFutureCompleteTraced(pFuture, State, pValue, pDestroy, pDestroyData,
		pOwner, pError, bRequestCancel, bReportDuplicate, NULL, NULL);
}

static bool __xrtOwnershipBody_PromiseResolveOwnedTraced(xpromise* pPromise, ptr pValue,
	xfuturefreeproc pDestroy, ptr pDestroyData, xfutureownershiptrace pTrace)
{
	if (pPromise == NULL || pDestroy == NULL || pTrace == NULL) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	return __xrtFutureCompleteTraced(pPromise->Future, XFUTURE_RESOLVED, pValue,
		pDestroy, pDestroyData, NULL, NULL, false, true, pTrace, NULL);
}

XRT_API bool xrtPromiseResolveOwnedPolicyV1(xpromise* pPromise, ptr pValue,
	const xfuturepayloadownershipv1* pPolicy)
{
	if (pPromise == NULL || pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) ||
		pPolicy->Drop == NULL || pPolicy->Trace == NULL) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	return __xrtFutureCompleteTraced(pPromise->Future, XFUTURE_RESOLVED, pValue,
		pPolicy->Drop, NULL, NULL, NULL, false, true, pPolicy->Trace, pPolicy);
}

XRT_API bool xrtPromiseResolveOwnedTraced(xpromise* pPromise, ptr pValue,
	xfuturefreeproc pDestroy, ptr pDestroyData, xfutureownershiptrace pTrace)
{
	return __xrtOwnershipBody_PromiseResolveOwnedTraced(pPromise, pValue, pDestroy, pDestroyData, pTrace);
}

/* 可取消等待在 Future 锁下记录终态竞争结果。 */
typedef struct xrt_future_cancel_wait {
	xfuture* Future;
	bool Cancelled;
} xrt_future_cancel_wait;



/* 取消与 Future 完成共用一把锁，先取得锁的一方确定等待结果。 */
static void __xrtFutureWaitCancelled(ptr pData)
{
	xrt_future_cancel_wait* pWait = (xrt_future_cancel_wait*)pData;
	xfuture* pFuture = pWait->Future;

	if ( xrtMutexLock(&pFuture->Lock) ) {
		if ( pFuture->State == XFUTURE_PENDING ) {
			pWait->Cancelled = true;
			(void)xrtCondBroadcast(&pFuture->Ready);
		}
		(void)xrtMutexUnlock(&pFuture->Lock);
	}
}



/* Future 尚未完成时挂入一个不分配内存的内部等待节点。 */
static bool __xrtOwnershipBody_FutureWaiterAdd(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	bool bLinked = false;

	if ( (pFuture == NULL) || (pWaiter == NULL) || (pWaiter->Proc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		return false;
	}
	if ( pWaiter->Linked || pWaiter->Calling ) {
		(void)xrtMutexUnlock(&pFuture->Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pFuture->State == XFUTURE_PENDING ) {
		pWaiter->Next = NULL;
		pWaiter->Linked = true;
		if ( pFuture->WaitersTail != NULL ) {
			pFuture->WaitersTail->Next = pWaiter;
		} else {
			pFuture->Waiters = pWaiter;
		}
		pFuture->WaitersTail = pWaiter;
		bLinked = true;
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	return bLinked;
}

bool __xrtFutureWaiterAdd(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtOwnershipBody_FutureWaiterAdd(pFuture, pWaiter));
}



/* 摘除尚未进入完成批次的等待节点，但不等待已经开始的回调。 */
static bool __xrtOwnershipBody_FutureWaiterDetach(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	xrtownershipscope Mutation = {0};
	xrt_future_waiter** ppWaiter;
	xrt_future_waiter* pPrevious = NULL;
	void (*pRelease)(ptr pData) = NULL;
	ptr pData = NULL;
	bool bDetached = false;
	bool bPhased = false;

	if ( (pFuture == NULL) || (pWaiter == NULL) ) {
		return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return false;
	}
	if ( pWaiter->Linked ) {
		ppWaiter = &pFuture->Waiters;
		while ( (*ppWaiter != NULL) && (*ppWaiter != pWaiter) ) {
			pPrevious = *ppWaiter;
			ppWaiter = &(*ppWaiter)->Next;
		}
		if ( *ppWaiter == pWaiter ) {
			*ppWaiter = pWaiter->Next;
			if ( pFuture->WaitersTail == pWaiter ) {
				pFuture->WaitersTail = pPrevious;
			}
			pWaiter->Next = NULL;
			pWaiter->Linked = false;
			pRelease = pWaiter->Release;
			pData = pWaiter->Data;
			bPhased = pWaiter->Phased;
			bDetached = true;
		}
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( pRelease != NULL ) {
		__xrtFutureInvoke(pRelease, pData, bPhased);
	}
	return bDetached;
}

bool __xrtFutureWaiterDetach(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	return __xrtOwnershipBody_FutureWaiterDetach(pFuture, pWaiter);
}



/* 从等待链移除仍然挂接的内部节点。 */
static void __xrtOwnershipBody_FutureWaiterRemove(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	xrtownershipscope Mutation = {0};
	xrt_future_waiter** ppWaiter;
	xrt_future_waiter* pPrevious = NULL;
	void (*pRelease)(ptr pData) = NULL;
	ptr pData = NULL;
	bool bPhased = false;

	if ( (pFuture == NULL) || (pWaiter == NULL) ) {
		return;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return;
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return;
	}
	if ( pWaiter->Linked ) {
		ppWaiter = &pFuture->Waiters;
		while ( (*ppWaiter != NULL) && (*ppWaiter != pWaiter) ) {
			pPrevious = *ppWaiter;
			ppWaiter = &(*ppWaiter)->Next;
		}
		if ( *ppWaiter == pWaiter ) {
			*ppWaiter = pWaiter->Next;
			if ( pFuture->WaitersTail == pWaiter ) {
				pFuture->WaitersTail = pPrevious;
			}
		}
		pWaiter->Next = NULL;
		pWaiter->Linked = false;
		pRelease = pWaiter->Release;
		pData = pWaiter->Data;
		bPhased = pWaiter->Phased;
	}
	/* No graph mutation is needed to wait for the already detached callback.
	 * End admission before reacquiring the condition lock, so callback writes
	 * can enter their own mutation even while a graph inspector is active. */
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!xrtMutexLock(&pFuture->Lock)) abort();
	while ( pWaiter->Calling ) {
		(void)xrtCondWait(&pFuture->Ready, &pFuture->Lock);
	}
	(void)xrtMutexUnlock(&pFuture->Lock);
	if ( pRelease != NULL ) {
		__xrtFutureInvoke(pRelease, pData, bPhased);
	}
}

void __xrtFutureWaiterRemove(xfuture* pFuture, xrt_future_waiter* pWaiter)
{
	__xrtOwnershipBody_FutureWaiterRemove(pFuture, pWaiter);
}



/* 初始化调用方持有的无分配 Future Watch。 */
static bool __xrtOwnershipBody_FutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
)
{
	xrt_future_watch_impl* pImpl;

	if ( !__xrtRangeValid(pWatch, sizeof(*pWatch)) ||
		(pNotify == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pWatch, 0, sizeof(*pWatch));
	pImpl = __xrtFutureWatchImpl(pWatch);
	pImpl->Waiter.Proc = pNotify;
	pImpl->Waiter.Release = pRelease;
	pImpl->Waiter.Data = pData;
	pImpl->Magic = XRT_FUTURE_WATCH_MAGIC;
	return true;
}

XRT_API bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
)
{
	XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtOwnershipBody_FutureWatchInit(pWatch, pNotify, pRelease, pData));
}



static bool __xrtOwnershipBody_FutureWatchInitTraced(xfuturewatch* pWatch,
	xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
	xrtownershiptrace pTrace)
{
	if (pRelease == NULL || pTrace == NULL) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (!xrtFutureWatchInit(pWatch, pNotify, pRelease, pData)) return false;
	__xrtFutureWatchImpl(pWatch)->Waiter.OwnershipTrace = pTrace;
	return true;
}

XRT_API bool xrtFutureWatchInitTraced(xfuturewatch* pWatch,
	xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
	xrtownershiptrace pTrace)
{
	XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtOwnershipBody_FutureWatchInitTraced(pWatch, pNotify, pRelease, pData, pTrace));
}

static bool __xrtFutureWatchInitPhased(xfuturewatch* pWatch,
	xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
	xrtownershiptrace pTrace)
{
	if (!xrtFutureWatchInitTraced(pWatch, pNotify, pRelease, pData, pTrace)) return false;
	__xrtFutureWatchImpl(pWatch)->Waiter.Phased = true;
	return true;
}
XRT_API bool xrtFutureWatchInitPhased(xfuturewatch* pWatch,
	xfuturewatchproc pNotify, xfuturewatchreleaseproc pRelease, ptr pData,
	xrtownershiptrace pTrace)
{
	XRT_OWNERSHIP_MUTATION_RETURN(bool, false, __xrtFutureWatchInitPhased(pWatch, pNotify, pRelease, pData, pTrace));
}
XRT_API bool xrtFutureWatchInitOwnershipV1(xfuturewatch* pWatch, ptr pData,
	const xfuturewatchownershipv1* pPolicy)
{
	xrtownershipscope Mutation = {0};
	if (pData == NULL || pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) ||
		pPolicy->Notify == NULL || pPolicy->Release == NULL || pPolicy->Ops == NULL ||
		pPolicy->Ops->Count == NULL || pPolicy->Ops->Trace == NULL) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bOk = __xrtOwnershipBody_FutureWatchInit(pWatch, pPolicy->Notify, pPolicy->Release, pData);
	if (bOk) {
		xrt_future_waiter* pWaiter = &__xrtFutureWatchImpl(pWatch)->Waiter;
		pWaiter->Phased = true; pWaiter->Certified = true; pWaiter->OwnershipPolicy = pPolicy;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bOk;
}

XRT_API bool xrtFutureWatchInitOwnershipV2(xfuturewatch* pWatch, ptr pData,
	const xfuturewatchownershipv2* pPolicy)
{
	xrtownershipscope Mutation = {0};
	if (!pData || !pPolicy || pPolicy->size != sizeof(*pPolicy) ||
		!pPolicy->Notify || !pPolicy->Release || !pPolicy->Reference) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bOk = __xrtOwnershipBody_FutureWatchInit(pWatch, pPolicy->Notify, pPolicy->Release, pData);
	if (bOk) {
		xrt_future_waiter* pWaiter = &__xrtFutureWatchImpl(pWatch)->Waiter;
		pWaiter->Phased = true; pWaiter->Certified = 2; pWaiter->ProjectedOwnershipPolicy = pPolicy;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bOk;
}


/* 把 Watch 挂入仍为 Pending 的 Future。 */
static xfuturewatchresult __xrtOwnershipBody_FutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;
	xerror* pPrevious;
	xerror* pCurrent;
	bool bAdded;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return XFUTURE_WATCH_ERROR;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( (pImpl->Magic != XRT_FUTURE_WATCH_MAGIC) ||
		(pImpl->Waiter.Proc == NULL) ) {
		__xrtErrorSetInvalidState();
		return XFUTURE_WATCH_ERROR;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	bAdded = __xrtFutureWaiterAdd(pFuture, &pImpl->Waiter);
	pCurrent = __xrtErrorSwapOwned(pPrevious);
	if ( bAdded ) {
		return XFUTURE_WATCH_PENDING;
	}
	if ( pCurrent != NULL ) {
		xrtSetError(pCurrent);
		xrtErrorFree(pCurrent);
		return XFUTURE_WATCH_ERROR;
	}
	return XFUTURE_WATCH_READY;
}

XRT_API xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xfuturewatchresult, XFUTURE_WATCH_ERROR, __xrtOwnershipBody_FutureWatchAdd(pFuture, pWatch));
}



/* 摘除尚未开始回调的 Future Watch。 */
static bool __xrtOwnershipBody_FutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( pImpl->Magic != XRT_FUTURE_WATCH_MAGIC ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return __xrtFutureWaiterDetach(pFuture, &pImpl->Waiter);
}

XRT_API bool xrtFutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	return __xrtOwnershipBody_FutureWatchDetach(pFuture, pWatch);
}



/* 移除 Future Watch，并与并发中的通知回调汇合。 */
static void __xrtOwnershipBody_FutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	xrt_future_watch_impl* pImpl;

	if ( (pFuture == NULL) ||
		!__xrtRangeValid(pWatch, sizeof(*pWatch)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	pImpl = __xrtFutureWatchImpl(pWatch);
	if ( pImpl->Magic != XRT_FUTURE_WATCH_MAGIC ) {
		__xrtErrorSetInvalidState();
		return;
	}
	__xrtFutureWaiterRemove(pFuture, &pImpl->Waiter);
}

XRT_API void xrtFutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
)
{
	__xrtOwnershipBody_FutureWatchRemove(pFuture, pWatch);
}



/* 创建共享 Future 与嵌入式 Promise 端点。 */
static xpromise* __xrtOwnershipBody_PromiseCreate(xfuture** ppFuture, xcancel* pParentCancel)
{
	xfuture* pFuture;

	if ( ppFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	*ppFuture = NULL;
	pFuture = (xfuture*)xrtCalloc(1, sizeof(xfuture));
	if ( pFuture == NULL ) {
		return NULL;
	}
	pFuture->RefCount = 2;
	pFuture->PromiseRefs = 1;
	pFuture->State = XFUTURE_PENDING;
	pFuture->Promise.Future = pFuture;
	if ( !xrtMutexInit(&pFuture->Lock) ) {
		xrtFree(pFuture);
		return NULL;
	}
	if ( !xrtCondInit(&pFuture->Ready) ) {
		(void)xrtMutexUnit(&pFuture->Lock);
		xrtFree(pFuture);
		return NULL;
	}
	pFuture->Cancel = xrtCancelChild(pParentCancel);
	if ( pFuture->Cancel == NULL ) {
		(void)xrtCondUnit(&pFuture->Ready);
		(void)xrtMutexUnit(&pFuture->Lock);
		xrtFree(pFuture);
		return NULL;
	}
	*ppFuture = pFuture;
	return &pFuture->Promise;
}

XRT_API xpromise* xrtPromiseCreate(xfuture** ppFuture, xcancel* pParentCancel)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xpromise*, NULL, __xrtOwnershipBody_PromiseCreate(ppFuture, pParentCancel));
}



/* 增加 Promise 生产端引用。 */
static xpromise* __xrtOwnershipBody_PromiseRef(xpromise* pPromise)
{
	xfuture* pFuture;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pFuture = pPromise->Future;
	if (pFuture->OwnershipCleared) { __xrtErrorSetInvalidState(); return NULL; }
	if ( xrtRefRetain(&pFuture->PromiseRefs) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( xrtRefRetain(&pFuture->RefCount) < 0 ) {
		(void)xrtRefRelease(&pFuture->PromiseRefs);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pPromise;
}

XRT_API xpromise* xrtPromiseRef(xpromise* pPromise)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xpromise*, NULL, __xrtOwnershipBody_PromiseRef(pPromise));
}



/* 释放 Promise，并在最后一个生产端离开时关闭未完成结果。 */
static void __xrtOwnershipBody_PromiseDestroy(xpromise* pPromise)
{
	xrtownershipscope Mutation = {0};
	xfuture* pFuture;
	int32 iRefs;

	if ( pPromise == NULL ) {
		return;
	}
	pFuture = pPromise->Future;
	if (!xrtOwnershipMutationBegin(&Mutation)) return;
	iRefs = xrtRefRelease(&pFuture->PromiseRefs);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( iRefs < 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iRefs == 0 ) {
		(void)__xrtFutureComplete(
			pFuture,
			XFUTURE_CLOSED,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			true,
			false
		);
	}
	__xrtFutureRelease(pFuture);
}

XRT_API void xrtPromiseDestroy(xpromise* pPromise)
{
	__xrtOwnershipBody_PromiseDestroy(pPromise);
}



/* 增加 Future 消费端引用。 */
static xfuture* __xrtOwnershipBody_FutureRef(xfuture* pFuture)
{
	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if (pFuture->OwnershipCleared || xrtRefRetain(&pFuture->RefCount) < 0) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pFuture;
}

XRT_API xfuture* xrtFutureRef(xfuture* pFuture)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xfuture*, NULL, __xrtOwnershipBody_FutureRef(pFuture));
}



/* 释放 Future 消费端引用。 */
static void __xrtOwnershipBody_FutureDestroy(xfuture* pFuture)
{
	__xrtFutureRelease(pFuture);
}

XRT_API void xrtFutureDestroy(xfuture* pFuture)
{
	__xrtOwnershipBody_FutureDestroy(pFuture);
}



/* 返回 Future 状态快照。 */
XRT_API xfuturestate xrtFutureState(const xfuture* pFuture)
{
	xfuturestate State;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XFUTURE_CLOSED;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return XFUTURE_CLOSED;
	}
	State = pFuture->State;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return State;
}



/* 判断 Future 是否已经进入终态。 */
XRT_API bool xrtFutureDone(const xfuture* pFuture)
{
	return xrtFutureState(pFuture) != XFUTURE_PENDING;
}



/* 复制借用的 Future 结果。 */
XRT_API bool xrtFutureResult(const xfuture* pFuture, xfutureresult* pResult)
{
	if ( (pFuture == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return false;
	}
	if (pFuture->OwnershipCleared) {
		(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
		__xrtErrorSetClosed(); return false;
	}
	if ( pFuture->State == XFUTURE_PENDING ) {
		(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
		__xrtErrorSetAgain();
		return false;
	}
	pResult->State = pFuture->State;
	pResult->Value = pFuture->Value;
	pResult->Error = pFuture->Error;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return true;
}



/* 返回成功值，并把非成功状态映射到当前错误上下文。 */
static ptr __xrtOwnershipBody_FutureValue(const xfuture* pFuture)
{
	xfuturestate State;
	ptr pValue;
	xerror* pError = NULL;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return NULL;
	}
	State = pFuture->State;
	pValue = pFuture->Value;
	if ( State == XFUTURE_FAILED ) {
		pError = xrtErrorRef(pFuture->Error);
	}
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	if ( State == XFUTURE_RESOLVED ) {
		return pValue;
	}
	if ( State == XFUTURE_FAILED ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtErrorSetCancelled();
	} else if ( State == XFUTURE_CLOSED ) {
		__xrtErrorSetClosed();
	} else {
		__xrtErrorSetAgain();
	}
	return NULL;
}

XRT_API ptr xrtFutureValue(const xfuture* pFuture)
{
	XRT_OWNERSHIP_MUTATION_RETURN(ptr, NULL, __xrtOwnershipBody_FutureValue(pFuture));
}



/* 返回失败终态借用的结构化错误。 */
XRT_API const xerror* xrtFutureError(const xfuture* pFuture)
{
	const xerror* pError;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pFuture->Lock) ) {
		return NULL;
	}
	pError = pFuture->State == XFUTURE_FAILED ? pFuture->Error : NULL;
	(void)xrtMutexUnlock((xmutex*)&pFuture->Lock);
	return pError;
}



/* 请求 Future 的生产过程协作取消。 */
XRT_API bool xrtFutureCancel(xfuture* pFuture)
{
	xrtownershipscope Mutation = {0}; xcancel* pCancel; bool bRequested;

	if ( pFuture == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return false;
	}
	/* The real local Cancel reference protects callback/release tails after
	 * leaving the Future transition. Certified observers must not inherit an
	 * API-owned scope; a caller-owned outer scope remains untouched. */
	pCancel = pFuture->State == XFUTURE_PENDING && !pFuture->OwnershipCleared ?
		xrtCancelRef(pFuture->Cancel) : NULL;
	(void)xrtMutexUnlock(&pFuture->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	bRequested = pCancel != NULL && xrtCancelRequest(pCancel);
	xrtCancelDestroy(pCancel); return bRequested;
}



/* 返回 Future 取消令牌的新增引用。 */
static xcancel* __xrtOwnershipBody_FutureCancelToken(const xfuture* pFuture)
{
	if ( pFuture == NULL || pFuture->OwnershipCleared ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pFuture->Cancel);
}

XRT_API xcancel* xrtFutureCancelToken(const xfuture* pFuture)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xcancel*, NULL, __xrtOwnershipBody_FutureCancelToken(pFuture));
}



/* 返回 Promise 取消令牌的新增引用。 */
static xcancel* __xrtOwnershipBody_PromiseCancelToken(const xpromise* pPromise)
{
	if ( pPromise == NULL || pPromise->Future->OwnershipCleared ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pPromise->Future->Cancel);
}

XRT_API xcancel* xrtPromiseCancelToken(const xpromise* pPromise)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xcancel*, NULL, __xrtOwnershipBody_PromiseCancelToken(pPromise));
}



/* 永久等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWait(xfuture* pFuture)
{
	return xrtFutureWaitUntilCancel(pFuture, XRT_DEADLINE_NEVER, NULL);
}



/* 在相对微秒数内等待 Future。 */
XRT_API xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout)
{
	return xrtFutureWaitUntilCancel(pFuture, xrtDeadlineAfter(iTimeout), NULL);
}



/* 等待 Future 到指定截止时间。 */
XRT_API xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline)
{
	return xrtFutureWaitUntilCancel(pFuture, iDeadline, NULL);
}



/* 等待 Future、截止时间或外部取消令牌中的首个事件。 */
XRT_API xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xcancelwatch* pWatch = NULL;
	xrt_future_cancel_wait CancelWait;
	xwaitresult Result = XWAIT_OK;

	pFuture = xrtFutureRef(pFuture);
	if ( pFuture == NULL ) {
		return XWAIT_ERROR;
	}
	memset(&CancelWait, 0, sizeof(CancelWait));
	CancelWait.Future = pFuture;
	if ( pCancel != NULL ) {
		pWatch = xrtCancelWatch(
			pCancel,
			__xrtFutureWaitCancelled,
			&CancelWait
		);
		if ( pWatch == NULL ) {
			xrtFutureDestroy(pFuture);
			return XWAIT_ERROR;
		}
	}
	if ( !xrtMutexLock(&pFuture->Lock) ) {
		Result = XWAIT_ERROR;
	} else {
		while ( (pFuture->State == XFUTURE_PENDING) &&
				 !CancelWait.Cancelled ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				Result = XWAIT_TIMEOUT;
				break;
			}
			Result = xrtCondWaitUntil(&pFuture->Ready, &pFuture->Lock, iDeadline);
			if ( Result == XWAIT_ERROR ) {
				break;
			}
			if ( Result == XWAIT_TIMEOUT ) {
				if ( CancelWait.Cancelled ) {
					Result = XWAIT_CANCELLED;
				} else if ( pFuture->State != XFUTURE_PENDING ) {
					Result = XWAIT_OK;
				}
				break;
			}
		}
		if ( CancelWait.Cancelled ) {
			Result = XWAIT_CANCELLED;
		} else if ( pFuture->State != XFUTURE_PENDING ) {
			Result = XWAIT_OK;
		}
		(void)xrtMutexUnlock(&pFuture->Lock);
	}
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
	}
	xrtFutureDestroy(pFuture);
	return Result;
}



/* 以借用值完成 Promise。 */
static bool __xrtOwnershipBody_PromiseResolve(xpromise* pPromise, ptr pValue)
{
	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_RESOLVED,
		pValue,
		NULL,
		NULL,
		NULL,
		NULL,
		false,
		true
	);
}

XRT_API bool xrtPromiseResolve(xpromise* pPromise, ptr pValue)
{
	return __xrtOwnershipBody_PromiseResolve(pPromise, pValue);
}



/* 以转移所有权的值完成 Promise。 */
static bool __xrtOwnershipBody_PromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	if ( (pPromise == NULL) || (pDestroy == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_RESOLVED,
		pValue,
		pDestroy,
		pDestroyData,
		NULL,
		NULL,
		false,
		true
	);
}

XRT_API bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
)
{
	return __xrtOwnershipBody_PromiseResolveOwned(pPromise, pValue, pDestroy, pDestroyData);
}



/* 以增加引用的结构化错误完成 Promise。 */
static bool __xrtOwnershipBody_PromiseReject(xpromise* pPromise, const xerror* pError)
{
	xerror* pHeldError;
	bool bCompleted;

	if ( (pPromise == NULL) || (pError == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pHeldError = xrtErrorRef(pError);
	if ( pHeldError == NULL ) {
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_FAILED,
		NULL,
		NULL,
		NULL,
		NULL,
		pHeldError,
		false,
		true
	);
	if ( !bCompleted ) {
		xrtErrorFree(pHeldError);
	}
	return bCompleted;
}

XRT_API bool xrtPromiseReject(xpromise* pPromise, const xerror* pError)
{
	return __xrtOwnershipBody_PromiseReject(pPromise, pError);
}



/* 把源终态透传到 Promise，并在成功值借用期间保留源 Future。 */
static bool __xrtOwnershipBody_PromiseForward(xpromise* pPromise, xfuture* pSource)
{
	xfutureresult tResult;
	bool bCompleted;

	if ( (pPromise == NULL) || (pSource == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pPromise->Future == pSource ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pSource = xrtFutureRef(pSource);
	if ( pSource == NULL ) {
		return false;
	}
	if ( !xrtFutureResult(pSource, &tResult) ) {
		xrtFutureDestroy(pSource);
		return false;
	}
	if ( tResult.State == XFUTURE_RESOLVED ) {
		bCompleted = __xrtFutureComplete(
			pPromise->Future,
			XFUTURE_RESOLVED,
			tResult.Value,
			NULL,
			NULL,
			pSource,
			NULL,
			false,
			true
		);
		if ( !bCompleted ) {
			xrtFutureDestroy(pSource);
		}
		return bCompleted;
	}
	if ( tResult.State == XFUTURE_FAILED ) {
		bCompleted = xrtPromiseReject(pPromise, tResult.Error);
	} else if ( tResult.State == XFUTURE_CANCELLED ) {
		bCompleted = xrtPromiseCancel(pPromise);
	} else {
		bCompleted = xrtPromiseClose(pPromise);
	}
	xrtFutureDestroy(pSource);
	return bCompleted;
}

XRT_API bool xrtPromiseForward(xpromise* pPromise, xfuture* pSource)
{
	return __xrtOwnershipBody_PromiseForward(pPromise, pSource);
}



/* 完成取消终态并同步发出协作取消请求。 */
static bool __xrtOwnershipBody_PromiseCancel(xpromise* pPromise)
{
	bool bCompleted;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_CANCELLED,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		true,
		true
	);
	return bCompleted;
}

XRT_API bool xrtPromiseCancel(xpromise* pPromise)
{
	return __xrtOwnershipBody_PromiseCancel(pPromise);
}



/* 完成关闭终态并同步发出协作取消请求。 */
static bool __xrtOwnershipBody_PromiseClose(xpromise* pPromise)
{
	bool bCompleted;

	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bCompleted = __xrtFutureComplete(
		pPromise->Future,
		XFUTURE_CLOSED,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		true,
		true
	);
	return bCompleted;
}

XRT_API bool xrtPromiseClose(xpromise* pPromise)
{
	return __xrtOwnershipBody_PromiseClose(pPromise);
}



/* 判断 Promise 对应的 Future 是否已经完成。 */
XRT_API bool xrtPromiseDone(const xpromise* pPromise)
{
	if ( pPromise == NULL ) {
		__xrtErrorSetInvalidArgument();
		return true;
	}
	return xrtFutureDone(pPromise->Future);
}

#endif
