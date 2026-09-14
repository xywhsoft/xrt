#include "../internal/xrt_cancel.h"



#if defined(XRT_FEATURE_CANCEL)

/* 一个父链节点只挂入对应令牌的监听链表。 */
typedef struct xcancelnode {
	struct xcancelnode* Next;
	struct xcancelwatch* Watch;
	struct xcancel* Cancel;
	bool Linked;
} xcancelnode;



/* 取消令牌保留父令牌，并用互斥锁保护监听链表。 */
struct xcancel {
	volatile int32 RefCount;
	volatile int32 Requested;
	xmutex Lock;
	struct xcancel* Parent;
	xcancelnode* WatchHead;
	size_t ActiveRequests;
	const void* OwnershipClaim;
	bool OwnershipCleared;
};



static bool __xrtCancelOwnershipCount(const void* pData, size_t* pCount)
{
	const xcancel* pCancel = (const xcancel*)pData;
	int32 iCount;
	if (pCancel == NULL || pCount == NULL || pCancel->ActiveRequests || pCancel->OwnershipCleared) return false;
	iCount = __xrtAtomicRefLoad(&pCancel->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}

static bool __xrtCancelOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xcancel* pCancel = (const xcancel*)pData;
	if (pCancel == NULL || pVisit == NULL || pCancel->ActiveRequests || pCancel->OwnershipCleared) return false;
	return pCancel->Parent == NULL || pVisit(xrtCancelOwnership(pCancel->Parent), pContext);
}

static const xrtownershipops __xrtCancelOwnershipOps = {
	__xrtCancelOwnershipCount, __xrtCancelOwnershipTrace
};

XRT_API xrtownershipref xrtCancelOwnership(const xcancel* pCancel)
{
	xrtownershipref Result = {pCancel, pCancel != NULL ? &__xrtCancelOwnershipOps : NULL};
	return Result;
}

static bool __xrtCancelAdapterHold(const void* pData)
{ return xrtCancelRef((xcancel*)pData) != NULL; }
static void __xrtCancelAdapterDrop(const void* pData)
{ xrtCancelDestroy((xcancel*)pData); }
static bool __xrtCancelAdapterClaim(const void* pData, const void* pToken)
{ return pData != NULL && pToken != NULL && ((const xcancel*)pData)->WatchHead == NULL; }
static void __xrtCancelAdapterKeep(const void* pData, const void* pToken)
{ (void)pData; (void)pToken; }
XRT_API const xrtownershipadapterv1* xrtCancelOwnershipAdapterV1(xrtownershipref Reference)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter),
		__xrtCancelAdapterHold, __xrtCancelAdapterDrop, __xrtCancelAdapterClaim,
		__xrtCancelAdapterKeep, NULL, __xrtCancelAdapterKeep, NULL};
	const xcancel* pCancel;
	if (Reference.Ops != &__xrtCancelOwnershipOps || Reference.Data == NULL) return NULL;
	pCancel = (const xcancel*)Reference.Data;
	return pCancel->WatchHead == NULL && !pCancel->ActiveRequests && !pCancel->OwnershipCleared &&
		__xrtAtomicRefLoad(&pCancel->RefCount) > 0 ? &Adapter : NULL;
}
void __xrtCancelOwnershipCloseUnobserved(xcancel* pCancel)
{
	if (pCancel == NULL || pCancel->WatchHead != NULL) abort();
	(void)__xrtAtomicRefCompareExchange(&pCancel->Requested, 1, 0);
}

/* 监听对象集中保存回调状态和全部父链节点，避免逐节点分配。 */
struct xcancelwatch {
	volatile int32 RefCount;
	volatile int32 Triggered;
	xmutex Lock;
	xcond Idle;
	xcancel* Cancel;
	xcancelproc Proc;
	ptr Data;
	uint32 NodeCount;
	bool Armed;
	bool CallbackStarted;
	bool CallbackActive;
	bool CallbackThreadValid;
	bool Destroying;
	bool DeferredRelease;
	size_t Dispatching;
	bool UnwatchDone;
	const xcancelwatchownershipv1* OwnershipPolicy;
	const void* OwnershipClaim;
	bool OwnershipCleared;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD CallbackThread;
	#else
		pthread_t CallbackThread;
	#endif
	xcancelnode Nodes[1];
};

static bool __xrtCancelWatchStable(const xcancelwatch* pWatch)
{
	return pWatch != NULL && pWatch->Armed && !pWatch->CallbackActive &&
		!pWatch->Dispatching && !pWatch->DeferredRelease && !pWatch->OwnershipCleared &&
		(!pWatch->Destroying || (pWatch->OwnershipPolicy != NULL && pWatch->UnwatchDone));
}


static bool __xrtCancelWatchOwnershipCount(const void* pData, size_t* pCount)
{
	const xcancelwatch* pWatch = (const xcancelwatch*)pData;
	int32 iCount;
	if (pCount == NULL || !__xrtCancelWatchStable(pWatch)) return false;
	iCount = __xrtAtomicRefLoad(&pWatch->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}

static bool __xrtCancelWatchOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xcancelwatch* pWatch = (const xcancelwatch*)pData;
	if (pVisit == NULL || !__xrtCancelWatchStable(pWatch)) return false;
	return pVisit(xrtCancelOwnership(pWatch->Cancel), pContext) &&
		(pWatch->OwnershipPolicy == NULL || pVisit((xrtownershipref){pWatch->Data,
			pWatch->OwnershipPolicy->Ops}, pContext));
}

static const xrtownershipops __xrtCancelWatchOwnershipOps = {
	__xrtCancelWatchOwnershipCount, __xrtCancelWatchOwnershipTrace
};

XRT_API xrtownershipref xrtCancelWatchOwnership(const xcancelwatch* pWatch)
{
	xrtownershipref Result = {pWatch, pWatch != NULL ? &__xrtCancelWatchOwnershipOps : NULL};
	return Result;
}



/* 释放监听对象的一个内部引用。 */
static void __xrtCancelWatchRelease(xcancelwatch* pWatch)
{
	xcancel* pCancel; ptr pData; const xcancelwatchownershipv1* pPolicy;
	xrtownershipscope Mutation = {0};

	if (pWatch == NULL) return;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (xrtRefRelease(&pWatch->RefCount) != 0) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return;
	}
	pCancel = pWatch->Cancel;
	pData = pWatch->Data; pPolicy = pWatch->OwnershipPolicy;
	(void)xrtCondUnit(&pWatch->Idle);
	(void)xrtMutexUnit(&pWatch->Lock);
	xrtFree(pWatch);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pPolicy != NULL && pData != NULL) {
		xerror* pPrevious = xrtTakeError(); pPolicy->Drop(pData);
		xrtClearError(); xrtSetErrorTake(pPrevious);
	}
	xrtCancelDestroy(pCancel);
}



/* 判断当前线程是否正在执行指定监听的回调。 */
static bool __xrtCancelWatchIsCallbackThread(const xcancelwatch* pWatch)
{
	if ( !pWatch->CallbackThreadValid ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return pWatch->CallbackThread == GetCurrentThreadId();
	#else
		return pthread_equal(pWatch->CallbackThread, pthread_self()) != 0;
	#endif
}



/* 在持有监听锁时记录回调启动，并返回待执行过程。 */
static xcancelproc __xrtCancelWatchStart(xcancelwatch* pWatch, ptr* ppData)
{
	if (
		pWatch->Destroying || !pWatch->Armed ||
		(__xrtAtomicRefLoad(&pWatch->Triggered) == 0) ||
		pWatch->CallbackStarted
	) {
		return NULL;
	}
	pWatch->CallbackStarted = true;
	pWatch->CallbackActive = true;
	pWatch->CallbackThreadValid = true;
	#if defined(_WIN32) || defined(_WIN64)
		pWatch->CallbackThread = GetCurrentThreadId();
	#else
		pWatch->CallbackThread = pthread_self();
	#endif
	*ppData = pWatch->Data;
	return pWatch->Proc;
}



/* 执行回调并在返回后唤醒注销方或完成回调内延迟回收。 */
static void __xrtCancelWatchRun(
	xcancelwatch* pWatch,
	xcancelproc pProc,
	ptr pData
)
{
	bool bRelease;
	xrtownershipscope Mutation = {0}, Callback = {0};

	if (pWatch->OwnershipPolicy == NULL && !xrtOwnershipMutationBegin(&Callback)) abort();
	pProc(pData);
	if (pWatch->OwnershipPolicy == NULL && !xrtOwnershipScopeEnd(&Callback)) abort();
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	(void)xrtMutexLock(&pWatch->Lock);
	pWatch->CallbackActive = false;
	pWatch->CallbackThreadValid = false;
	bRelease = pWatch->DeferredRelease;
	pWatch->DeferredRelease = false;
	(void)xrtCondBroadcast(&pWatch->Idle);
	(void)xrtMutexUnlock(&pWatch->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( bRelease ) {
		__xrtCancelWatchRelease(pWatch);
	}
}



/* 标记监听已触发，并在监听完成装配后同步执行一次回调。 */
static void __xrtCancelWatchNotify(xcancelwatch* pWatch)
{
	xcancelproc pProc = NULL;
	ptr pData = NULL;
	xrtownershipscope Mutation = {0};

	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	(void)xrtMutexLock(&pWatch->Lock);
	if ( !pWatch->Destroying && (__xrtAtomicRefLoad(&pWatch->Triggered) == 0) ) {
		(void)__xrtAtomicRefCompareExchange(&pWatch->Triggered, 1, 0);
		pProc = __xrtCancelWatchStart(pWatch, &pData);
	}
	(void)xrtMutexUnlock(&pWatch->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( pProc != NULL ) {
		__xrtCancelWatchRun(pWatch, pProc, pData);
	}
}



/* 创建一个独立的取消令牌。 */
static xcancel* __xrtOwnershipBody_CancelCreate(void)
{
	xcancel* pCancel = (xcancel*)xrtMalloc(sizeof(xcancel));

	if ( pCancel == NULL ) {
		return NULL;
	}
	memset(pCancel, 0, sizeof(xcancel));
	pCancel->RefCount = 1;
	if ( !xrtMutexInit(&pCancel->Lock) ) {
		xrtFree(pCancel);
		return NULL;
	}
	return pCancel;
}

XRT_API xcancel* xrtCancelCreate(void)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xcancel*, NULL, __xrtOwnershipBody_CancelCreate());
}



/* 创建一个继承不可变父链的子取消令牌。 */
static xcancel* __xrtOwnershipBody_CancelChild(xcancel* pParent)
{
	xcancel* pCancel = xrtCancelCreate();

	if ( pCancel == NULL ) {
		return NULL;
	}
	if ( pParent != NULL ) {
		pCancel->Parent = xrtCancelRef(pParent);
		if ( pCancel->Parent == NULL ) {
			xrtCancelDestroy(pCancel);
			return NULL;
		}
	}
	return pCancel;
}

XRT_API xcancel* xrtCancelChild(xcancel* pParent)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xcancel*, NULL, __xrtOwnershipBody_CancelChild(pParent));
}



/* 增加取消令牌引用。 */
static xcancel* __xrtOwnershipBody_CancelRef(xcancel* pCancel)
{
	if ( (pCancel == NULL) || pCancel->OwnershipCleared || (xrtRefRetain(&pCancel->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCancel;
}

XRT_API xcancel* xrtCancelRef(xcancel* pCancel)
{
	XRT_OWNERSHIP_MUTATION_RETURN(xcancel*, NULL, __xrtOwnershipBody_CancelRef(pCancel));
}



/* 释放取消令牌引用，并顺着唯一父引用迭代回收。 */
static void __xrtOwnershipBody_CancelDestroy(xcancel* pCancel)
{
	while ( (pCancel != NULL) && (xrtRefRelease(&pCancel->RefCount) == 0) ) {
		xcancel* pParent = pCancel->Parent;

		(void)xrtMutexUnit(&pCancel->Lock);
		xrtFree(pCancel);
		pCancel = pParent;
	}
}

XRT_API void xrtCancelDestroy(xcancel* pCancel)
{
	XRT_OWNERSHIP_MUTATION_RETURN_VOID(__xrtOwnershipBody_CancelDestroy(pCancel));
}



/* 首次请求取消并在令牌锁外通知全部监听。 */
XRT_API bool xrtCancelRequest(xcancel* pCancel)
{
	xcancelnode* pList;
	xcancelnode* pNode;
	xrtownershipscope Mutation = {0};

	if ( pCancel == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if (pCancel->OwnershipCleared) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidState(); return false;
	}
	(void)xrtMutexLock(&pCancel->Lock);
	if ( __xrtAtomicRefLoad(&pCancel->Requested) != 0 ) {
		(void)xrtMutexUnlock(&pCancel->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return false;
	}
	if (xrtRefRetain(&pCancel->RefCount) < 0) abort();
	++pCancel->ActiveRequests;
	(void)__xrtAtomicRefCompareExchange(&pCancel->Requested, 1, 0);
	pList = pCancel->WatchHead;
	pCancel->WatchHead = NULL;
	for ( pNode = pList; pNode != NULL; pNode = pNode->Next ) {
		pNode->Linked = false;
		(void)xrtRefRetain(&pNode->Watch->RefCount);
		(void)xrtMutexLock(&pNode->Watch->Lock);
		++pNode->Watch->Dispatching;
		(void)xrtMutexUnlock(&pNode->Watch->Lock);
	}
	(void)xrtMutexUnlock(&pCancel->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();

	while ( pList != NULL ) {
		if (!xrtOwnershipMutationBegin(&Mutation)) abort();
		pNode = pList;
		pList = pNode->Next;
		pNode->Next = NULL;
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtCancelWatchNotify(pNode->Watch);
		if (!xrtOwnershipMutationBegin(&Mutation)) abort();
		(void)xrtMutexLock(&pNode->Watch->Lock);
		if (!pNode->Watch->Dispatching) abort();
		--pNode->Watch->Dispatching;
		(void)xrtMutexUnlock(&pNode->Watch->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtCancelWatchRelease(pNode->Watch);
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	(void)xrtMutexLock(&pCancel->Lock);
	--pCancel->ActiveRequests;
	(void)xrtMutexUnlock(&pCancel->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	xrtCancelDestroy(pCancel);
	return true;
}



/* 查询令牌及其不可变父链是否已取消。 */
XRT_API bool xrtCancelRequested(const xcancel* pCancel)
{
	while ( pCancel != NULL ) {
		if ( __xrtAtomicRefLoad(&pCancel->Requested) != 0 ) {
			return true;
		}
		pCancel = pCancel->Parent;
	}
	return false;
}



/* 为令牌及其全部祖先一次性装配监听节点。 */
static xcancelwatch* __xrtOwnershipBody_CancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData, const xcancelwatchownershipv1* pPolicy
)
{
	xcancelwatch* pWatch;
	xcancel* pCurrent;
	uint32 iCount = 0;
	size_t iBytes;

	if ( (pCancel == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pCancel = xrtCancelRef(pCancel);
	if ( pCancel == NULL ) {
		return NULL;
	}
	for ( pCurrent = pCancel; pCurrent != NULL; pCurrent = pCurrent->Parent ) {
		if ( iCount == UINT32_MAX ) {
			xrtCancelDestroy(pCancel);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCount++;
	}
	#if SIZE_MAX <= UINT32_MAX
		if (
			(size_t)iCount >
			((SIZE_MAX - offsetof(xcancelwatch, Nodes)) / sizeof(xcancelnode))
		) {
			xrtCancelDestroy(pCancel);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
	#endif
	iBytes = offsetof(xcancelwatch, Nodes) + ((size_t)iCount * sizeof(xcancelnode));
	pWatch = (xcancelwatch*)xrtMalloc(iBytes);
	if ( pWatch == NULL ) {
		xrtCancelDestroy(pCancel);
		return NULL;
	}
	memset(pWatch, 0, iBytes);
	pWatch->RefCount = 1;
	pWatch->Cancel = pCancel;
	pWatch->Proc = pProc;
	pWatch->Data = pData;
	pWatch->OwnershipPolicy = pPolicy;
	pWatch->NodeCount = iCount;
	if ( !xrtMutexInit(&pWatch->Lock) ) {
		xrtFree(pWatch);
		xrtCancelDestroy(pCancel);
		return NULL;
	}
	if ( !xrtCondInit(&pWatch->Idle) ) {
		(void)xrtMutexUnit(&pWatch->Lock);
		xrtFree(pWatch);
		xrtCancelDestroy(pCancel);
		return NULL;
	}

	pCurrent = pCancel;
	for ( uint32 i = 0; i < iCount; i++, pCurrent = pCurrent->Parent ) {
		xcancelnode* pNode = &pWatch->Nodes[i];

		pNode->Watch = pWatch;
		pNode->Cancel = pCurrent;
		(void)xrtMutexLock(&pCurrent->Lock);
		if ( __xrtAtomicRefLoad(&pCurrent->Requested) != 0 ) {
			(void)__xrtAtomicRefCompareExchange(&pWatch->Triggered, 1, 0);
		} else {
			pNode->Next = pCurrent->WatchHead;
			pCurrent->WatchHead = pNode;
			pNode->Linked = true;
		}
		(void)xrtMutexUnlock(&pCurrent->Lock);
	}

	return pWatch;
}

static xcancelwatch* __xrtCancelWatchCreate(xcancel* pCancel, xcancelproc pProc,
	ptr pData, const xcancelwatchownershipv1* pPolicy)
{
	xrtownershipscope Mutation = {0}; xcancelwatch* pWatch;
	xcancelproc pStart = NULL; ptr pStartData = NULL;
	if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;
	pWatch = __xrtOwnershipBody_CancelWatch(pCancel, pProc, pData, pPolicy);
	if (pWatch != NULL) {
		(void)xrtMutexLock(&pWatch->Lock);
		pWatch->Armed = true; pStart = __xrtCancelWatchStart(pWatch, &pStartData);
		(void)xrtMutexUnlock(&pWatch->Lock);
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pStart != NULL) __xrtCancelWatchRun(pWatch, pStart, pStartData);
	return pWatch;
}

XRT_API xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
)
{
	return __xrtCancelWatchCreate(pCancel, pProc, pData, NULL);
}

XRT_API xcancelwatch* xrtCancelWatchOwnedV1(xcancel* pCancel, ptr pData,
	const xcancelwatchownershipv1* pPolicy)
{
	if (pData == NULL || pPolicy == NULL || pPolicy->size != sizeof(*pPolicy) ||
		pPolicy->Notify == NULL || pPolicy->Drop == NULL || pPolicy->Ops == NULL ||
		pPolicy->Ops->Count == NULL || pPolicy->Ops->Trace == NULL) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	return __xrtCancelWatchCreate(pCancel, pPolicy->Notify, pData, pPolicy);
}



/* 查询监听是否已经命中取消。 */
XRT_API bool xrtCancelTriggered(const xcancelwatch* pWatch)
{
	if ( pWatch == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtAtomicRefLoad(&pWatch->Triggered) != 0;
}



/* 从一个令牌链表中移除仍然挂接的监听节点。 */
static void __xrtCancelUnlinkNode(xcancelnode* pNode)
{
	xcancelnode** ppNode;
	xcancel* pCancel = pNode->Cancel;

	(void)xrtMutexLock(&pCancel->Lock);
	if ( pNode->Linked ) {
		ppNode = &pCancel->WatchHead;
		while ( (*ppNode != NULL) && (*ppNode != pNode) ) {
			ppNode = &(*ppNode)->Next;
		}
		if ( *ppNode == pNode ) {
			*ppNode = pNode->Next;
		}
		pNode->Next = NULL;
		pNode->Linked = false;
	}
	(void)xrtMutexUnlock(&pCancel->Lock);
}



/* 注销监听，并针对回调自身注销采用返回后延迟回收。 */
XRT_API void xrtCancelUnwatch(xcancelwatch* pWatch)
{
	bool bSelf, bFirst;
	xrtownershipscope Mutation = {0};

	if ( pWatch == NULL ) {
		return;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) return;
	if ( xrtRefRetain(&pWatch->RefCount) < 0 ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidArgument();
		return;
	}
	(void)xrtMutexLock(&pWatch->Lock);
	bFirst = !pWatch->Destroying;
	pWatch->Destroying = true;
	(void)xrtMutexUnlock(&pWatch->Lock);

	for ( uint32 i = 0; bFirst && i < pWatch->NodeCount; i++ ) {
		__xrtCancelUnlinkNode(&pWatch->Nodes[i]);
	}

	(void)xrtMutexLock(&pWatch->Lock);
	if (bFirst) pWatch->UnwatchDone = true;
	bSelf = pWatch->CallbackActive && __xrtCancelWatchIsCallbackThread(pWatch);
	if ( bSelf && bFirst ) {
		pWatch->DeferredRelease = true;
		(void)xrtMutexUnlock(&pWatch->Lock);
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtCancelWatchRelease(pWatch);
		return;
	}
	(void)xrtMutexUnlock(&pWatch->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* No scope owned by this API is held while another callback is awaited. */
	(void)xrtMutexLock(&pWatch->Lock);
	while ( pWatch->CallbackActive && !bSelf ) {
		(void)xrtCondWait(&pWatch->Idle, &pWatch->Lock);
	}
	(void)xrtMutexUnlock(&pWatch->Lock);
	__xrtCancelWatchRelease(pWatch);
	if (bFirst) __xrtCancelWatchRelease(pWatch);
}

/* Policy admission is deliberately separate from trace. Observers are not
 * strong token slots: only their actual owners may make the Watch reachable. */
static bool __xrtCancelWatchPolicyKnown(const xcancelwatch* pWatch,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount)
{
	const xcancelwatchownershipv1* pPolicy = pWatch->OwnershipPolicy;
	bool bKnown = false;
	for (size_t i = 0; i < iPolicyCount; ++i)
		if (pPolicies[i] != NULL && pPolicies[i] == pPolicy) { bKnown = true; break; }
	return bKnown && pPolicy->size == sizeof(*pPolicy) && pPolicy->Notify == pWatch->Proc &&
		pPolicy->Drop != NULL && pPolicy->Ops != NULL && pPolicy->Ops->Count != NULL &&
		pPolicy->Ops->Trace != NULL && pWatch->Data != NULL;
}
static bool __xrtCancelWatchAdapterHold(const void* pData)
{
	xcancelwatch* pWatch = (xcancelwatch*)pData; xrtownershipscope Mutation = {0}; bool bHeld;
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bHeld = !pWatch->OwnershipCleared && xrtRefRetain(&pWatch->RefCount) > 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static void __xrtCancelWatchAdapterDrop(const void* pData)
{ __xrtCancelWatchRelease((xcancelwatch*)pData); }
static bool __xrtCancelWatchAdapterClaim(const void* pData, const void* pToken)
{
	xcancelwatch* pWatch = (xcancelwatch*)pData;
	if (!pToken || !__xrtCancelWatchStable(pWatch) ||
		(pWatch->OwnershipClaim && pWatch->OwnershipClaim != pToken)) return false;
	pWatch->OwnershipClaim = pToken; return true;
}
static void __xrtCancelWatchAdapterRestore(const void* pData, const void* pToken)
{
	xcancelwatch* pWatch = (xcancelwatch*)pData;
	if (!pToken || pWatch->OwnershipClaim != pToken || pWatch->OwnershipCleared) abort();
	pWatch->OwnershipClaim = NULL;
}
static bool __xrtCancelWatchPrepared(const void* pData)
{
	const xcancelwatch* pWatch = pData;
	return pWatch->Destroying && pWatch->UnwatchDone && !pWatch->CallbackActive &&
		!pWatch->Dispatching && !pWatch->DeferredRelease;
}
static xrtownershipprepareresult __xrtCancelWatchPrepare(const void* pData, const void* pToken)
{
	const xcancelwatch* pWatch = pData; bool bReady; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pWatch->OwnershipClaim != pToken || pWatch->OwnershipCleared) abort();
	(void)xrtMutexLock((xmutex*)&pWatch->Lock); bReady = __xrtCancelWatchPrepared(pData);
	(void)xrtMutexUnlock((xmutex*)&pWatch->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtCancelWatchAdapterClear(const void* pData, const void* pToken)
{
	xcancelwatch* pWatch = (xcancelwatch*)pData;
	if (!pToken || pWatch->OwnershipClaim != pToken || pWatch->OwnershipCleared || !__xrtCancelWatchPrepared(pData)) abort();
	pWatch->OwnershipCleared = true;
}
static bool __xrtCancelWatchAdapterFinish(const void* pData, const void* pToken)
{
	xcancelwatch* pWatch = (xcancelwatch*)pData; ptr pOwned; xcancel* pCancel;
	const xcancelwatchownershipv1* pPolicy; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!pToken || pWatch->OwnershipClaim != pToken || !pWatch->OwnershipCleared || !__xrtCancelWatchPrepared(pData)) abort();
	pOwned = pWatch->Data; pPolicy = pWatch->OwnershipPolicy; pCancel = pWatch->Cancel;
	pWatch->Data = NULL; pWatch->Cancel = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pOwned != NULL) {
		xerror* pPrevious = xrtTakeError(); pPolicy->Drop(pOwned);
		xrtClearError(); xrtSetErrorTake(pPrevious);
	}
	xrtCancelDestroy(pCancel); return true;
}
XRT_API const xrtownershipadapterv1* xrtCancelWatchOwnershipAdapterV1(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtCancelWatchAdapterHold,
		__xrtCancelWatchAdapterDrop, __xrtCancelWatchAdapterClaim, __xrtCancelWatchAdapterRestore,
		NULL, __xrtCancelWatchAdapterClear, __xrtCancelWatchAdapterFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtCancelWatchPrepared, __xrtCancelWatchPrepare};
	const xcancelwatch* pWatch;
	if (Reference.Ops != &__xrtCancelWatchOwnershipOps || !Reference.Data || !ppPreparation ||
		(iPolicyCount && !pPolicies)) return NULL;
	pWatch = Reference.Data;
	if (!__xrtCancelWatchStable(pWatch) || __xrtAtomicRefLoad(&pWatch->RefCount) <= 0 ||
		!__xrtCancelWatchPolicyKnown(pWatch, pPolicies, iPolicyCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

static bool __xrtCancelAdapterClaimV2(const void* pData, const void* pToken)
{
	xcancel* pCancel = (xcancel*)pData;
	if (!pToken || pCancel->OwnershipCleared || pCancel->ActiveRequests ||
		(pCancel->OwnershipClaim && pCancel->OwnershipClaim != pToken)) return false;
	pCancel->OwnershipClaim = pToken; return true;
}
static void __xrtCancelAdapterRestoreV2(const void* pData, const void* pToken)
{
	xcancel* pCancel = (xcancel*)pData;
	if (!pToken || pCancel->OwnershipClaim != pToken || pCancel->OwnershipCleared) abort();
	pCancel->OwnershipClaim = NULL;
}
static bool __xrtCancelPreparedV2(const void* pData)
{
	const xcancel* pCancel = pData;
	return pCancel->WatchHead == NULL && !pCancel->ActiveRequests;
}
static xrtownershipprepareresult __xrtCancelPrepareV2(const void* pData, const void* pToken)
{
	const xcancel* pCancel = pData; bool bReady; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pCancel->OwnershipClaim != pToken || pCancel->OwnershipCleared) abort();
	(void)xrtMutexLock((xmutex*)&pCancel->Lock); bReady = __xrtCancelPreparedV2(pData);
	(void)xrtMutexUnlock((xmutex*)&pCancel->Lock);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtCancelAdapterClearV2(const void* pData, const void* pToken)
{
	xcancel* pCancel = (xcancel*)pData;
	if (!pToken || pCancel->OwnershipClaim != pToken || pCancel->OwnershipCleared || !__xrtCancelPreparedV2(pData)) abort();
	pCancel->OwnershipCleared = true;
}
static bool __xrtCancelAdapterFinishV2(const void* pData, const void* pToken)
{
	xcancel* pCancel = (xcancel*)pData; xcancel* pParent; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!pToken || pCancel->OwnershipClaim != pToken || !pCancel->OwnershipCleared) abort();
	pParent = pCancel->Parent; pCancel->Parent = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	xrtCancelDestroy(pParent); return true;
}
XRT_API const xrtownershipadapterv1* xrtCancelOwnershipAdapterV2(xrtownershipref Reference,
	const xcancelwatchownershipv1* const* pPolicies, size_t iPolicyCount,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtCancelAdapterHold,
		__xrtCancelAdapterDrop, __xrtCancelAdapterClaimV2, __xrtCancelAdapterRestoreV2,
		NULL, __xrtCancelAdapterClearV2, __xrtCancelAdapterFinishV2};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtCancelPreparedV2, __xrtCancelPrepareV2};
	const xcancel* pCancel;
	if (Reference.Ops != &__xrtCancelOwnershipOps || !Reference.Data || !ppPreparation ||
		(iPolicyCount && !pPolicies)) return NULL;
	pCancel = Reference.Data;
	if (pCancel->OwnershipCleared || pCancel->ActiveRequests || __xrtAtomicRefLoad(&pCancel->RefCount) <= 0) return NULL;
	for (const xcancelnode* pNode = pCancel->WatchHead; pNode; pNode = pNode->Next) {
		if (!pNode->Linked || pNode->Cancel != pCancel || !__xrtCancelWatchStable(pNode->Watch) ||
			pNode->Watch->Destroying || !__xrtCancelWatchPolicyKnown(pNode->Watch, pPolicies, iPolicyCount)) return NULL;
	}
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
