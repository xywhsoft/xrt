#include <xrt/detail/wait.h>
#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_TASK_GROUP)

typedef struct xrt_task_group_item xrt_task_group_item;



/* 活动项在 Future 完成批次、关闭传播和取消传播之间独立引用。 */
struct xrt_task_group_item {
	volatile int32 RefCount;
	struct xrt_task_group_item* Previous;
	struct xrt_task_group_item* Next;
	struct xrt_task_group_item* CloseNext;
	struct xrt_task_group_item* CancelNext;
	struct xtaskgroup* Group;
	struct xtaskgroup* Child;
	xfuture* Future;
	xrt_future_waiter Waiter;
	size_t Index;
	bool ClosePosted;
	bool CancelPosted;
	bool Listed;
};



/* 任务组用活动双向链表支持 O(1) 完成摘除和无分配传播快照。 */
struct xtaskgroup {
	volatile int32 RefCount;
	xmutex Lock;
	xcancel* Cancel;
	xcancelwatch* Watch;
	xfuture* Future;
	xpromise* Promise;
	xrt_task_group_item* Head;
	size_t Limit;
	size_t Active;
	size_t Reserved;
	uint32 CancelOn;
	uint64 Added;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 ClosedResults;
	uint64 Rejected;
	size_t FirstIndex;
	xfuturestate FirstState;
	xerror* FirstError;
	bool Closed;
	bool Cancelling;
	size_t Busy;
};

/* Every physical count/edge transition participates in whole-graph freeze.
 * Callback execution and endpoint release stay outside these short scopes. */
static void __xrtTaskGroupBegin(const xtaskgroup* pGroup, xrtownershipscope* pScope)
{
	if (!xrtOwnershipMutationBegin(pScope) || !xrtMutexLock((xmutex*)&pGroup->Lock)) abort();
}
static void __xrtTaskGroupEnd(const xtaskgroup* pGroup, xrtownershipscope* pScope)
{
	if (!xrtMutexUnlock((xmutex*)&pGroup->Lock) || !xrtOwnershipScopeEnd(pScope)) abort();
}
static bool __xrtTaskGroupOwnershipCount(const void* pData, size_t* pCount)
{
	const xtaskgroup* pGroup = pData; int32 iCount;
	if (!pGroup || !pCount || pGroup->Busy || pGroup->Reserved) return false;
	iCount = __xrtAtomicRefLoad(&pGroup->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}
static bool __xrtTaskGroupItemOwnershipCount(const void* pData, size_t* pCount)
{
	const xrt_task_group_item* pItem = pData; size_t iGroupCount; int32 iCount;
	if (!pItem || !pCount || !pItem->Listed || !pItem->Future || pItem->Waiter.Calling ||
		!__xrtTaskGroupOwnershipCount(pItem->Group, &iGroupCount)) return false;
	iCount = __xrtAtomicRefLoad(&pItem->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}
static bool __xrtTaskGroupOwnershipTrace(const void*, xrtownershipvisitor, ptr);
static bool __xrtTaskGroupItemOwnershipTrace(const void*, xrtownershipvisitor, ptr);
static const xrtownershipops __xrtTaskGroupOwnershipOps = {
	__xrtTaskGroupOwnershipCount, __xrtTaskGroupOwnershipTrace
};
static const xrtownershipops __xrtTaskGroupItemOwnershipOps = {
	__xrtTaskGroupItemOwnershipCount, __xrtTaskGroupItemOwnershipTrace
};
XRT_API xrtownershipref xrtTaskGroupOwnership(const xtaskgroup* pGroup)
{ return (xrtownershipref){pGroup, pGroup ? &__xrtTaskGroupOwnershipOps : NULL}; }
static bool __xrtTaskGroupItemOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xrt_task_group_item* pItem = pData; size_t iCount;
	if (!pVisit || !__xrtTaskGroupItemOwnershipCount(pData, &iCount)) return false;
	return pVisit(xrtTaskGroupOwnership(pItem->Group), pContext) &&
		(!pItem->Child || pVisit(xrtTaskGroupOwnership(pItem->Child), pContext)) &&
		pVisit(xrtFutureOwnership(pItem->Future), pContext);
}
static bool __xrtTaskGroupOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xtaskgroup* pGroup = pData; const xrt_task_group_item* pPrevious = NULL; size_t iCount, iItems = 0;
	if (!pVisit || !__xrtTaskGroupOwnershipCount(pData, &iCount)) return false;
	if (pGroup->Cancel && !pVisit(xrtCancelOwnership(pGroup->Cancel), pContext)) return false;
	if (pGroup->Watch && !pVisit(xrtCancelWatchOwnership(pGroup->Watch), pContext)) return false;
	if (pGroup->Future && !pVisit(xrtFutureOwnership(pGroup->Future), pContext)) return false;
	if (pGroup->Promise && !pVisit(xrtPromiseOwnership(pGroup->Promise), pContext)) return false;
	if (pGroup->FirstError && !pVisit(xrtErrorOwnership(pGroup->FirstError), pContext)) return false;
	for (const xrt_task_group_item* pItem = pGroup->Head; pItem; pItem = pItem->Next) {
		if (++iItems > pGroup->Active || !pItem->Listed || pItem->Group != pGroup ||
			pItem->Previous != pPrevious || !pVisit((xrtownershipref){pItem, &__xrtTaskGroupItemOwnershipOps}, pContext)) return false;
		pPrevious = pItem;
	}
	return iItems == pGroup->Active;
}



/* 增加任务组内部引用；达到计数上限时拒绝继续接纳活动项。 */
static bool __xrtTaskGroupRef(xtaskgroup* pGroup)
{
	xrtownershipscope Scope = {0}; bool bHeld;
	__xrtTaskGroupBegin(pGroup, &Scope);
	bHeld = xrtRefRetain(&pGroup->RefCount) >= 0;
	__xrtTaskGroupEnd(pGroup, &Scope); return bHeld;
}



/* 释放任务组内部引用，并在全部活动项退出后回收公共状态。 */
static void __xrtTaskGroupRelease(xtaskgroup* pGroup)
{
	xrtownershipscope Scope = {0}; int32 iLeft;
	xpromise* pPromise = NULL; xfuture* pFuture = NULL; xcancel* pCancel = NULL; xerror* pError = NULL;
	__xrtTaskGroupBegin(pGroup, &Scope); iLeft = xrtRefRelease(&pGroup->RefCount);
	if (iLeft == 0) {
		if (pGroup->Watch || pGroup->Head || pGroup->Active || pGroup->Reserved || pGroup->Busy) abort();
		pPromise = pGroup->Promise; pGroup->Promise = NULL;
		pFuture = pGroup->Future; pGroup->Future = NULL;
		pCancel = pGroup->Cancel; pGroup->Cancel = NULL;
		pError = pGroup->FirstError; pGroup->FirstError = NULL;
	}
	__xrtTaskGroupEnd(pGroup, &Scope);
	if (iLeft < 0) abort();
	if (iLeft != 0) return;
	xrtPromiseDestroy(pPromise); xrtFutureDestroy(pFuture); xrtCancelDestroy(pCancel); xrtErrorFree(pError);
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	(void)xrtMutexUnit(&pGroup->Lock); xrtFree(pGroup);
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
}



/* 增加活动项引用，供锁外关闭或取消传播使用。 */
static void __xrtTaskGroupItemRef(xrt_task_group_item* pItem)
{
	if (xrtRefRetain(&pItem->RefCount) < 0) abort();
}



/* 释放活动项及其源、子组和父组引用。 */
static void __xrtTaskGroupItemRelease(xrt_task_group_item* pItem)
{
	xrtownershipscope Scope = {0}; int32 iLeft; xtaskgroup *pGroup = NULL, *pChild = NULL; xfuture* pFuture = NULL;
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	iLeft = xrtRefRelease(&pItem->RefCount);
	if (iLeft == 0) {
		if (pItem->Listed || pItem->Waiter.Linked || pItem->Waiter.Calling) abort();
		pGroup = pItem->Group; pChild = pItem->Child; pFuture = pItem->Future;
		pItem->Group = NULL; pItem->Child = NULL; pItem->Future = NULL; xrtFree(pItem);
	}
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	if (iLeft < 0) abort();
	if (iLeft != 0) return;
	xrtFutureDestroy(pFuture);
	if (pChild) __xrtTaskGroupRelease(pChild);
	__xrtTaskGroupRelease(pGroup);
}



/* 从活动链中摘除指定项，调用方必须持有组锁。 */
static void __xrtTaskGroupItemRemoveLocked(
	xtaskgroup* pGroup,
	xrt_task_group_item* pItem
)
{
	if (!pItem->Listed) abort();
	if ( pItem->Previous != NULL ) {
		pItem->Previous->Next = pItem->Next;
	} else if ( pGroup->Head == pItem ) {
		pGroup->Head = pItem->Next;
	}
	if ( pItem->Next != NULL ) {
		pItem->Next->Previous = pItem->Previous;
	}
	pItem->Previous = NULL;
	pItem->Next = NULL;
	pItem->Listed = false;
}



/* Future 完成批次释放监听所持有的活动项引用。 */
static void __xrtTaskGroupWaiterRelease(ptr pData)
{
	__xrtTaskGroupItemRelease((xrt_task_group_item*)pData);
}

static void __xrtTaskGroupSourceDone(ptr pData);
static void __xrtTaskGroupCancelled(ptr pData);
static void __xrtTaskGroupCancelDrop(const void* pData)
{ __xrtTaskGroupRelease((xtaskgroup*)pData); }
static const xfuturewatchownershipv1 __xrtTaskGroupWatchPolicy = {
	sizeof(xfuturewatchownershipv1), __xrtTaskGroupSourceDone,
	__xrtTaskGroupWaiterRelease, &__xrtTaskGroupItemOwnershipOps
};
static const xcancelwatchownershipv1 __xrtTaskGroupCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtTaskGroupCancelled,
	__xrtTaskGroupCancelDrop, &__xrtTaskGroupOwnershipOps
};
XRT_API const xfuturewatchownershipv1* xrtTaskGroupWatchPolicyV1Get(void)
{ return &__xrtTaskGroupWatchPolicy; }
XRT_API const xcancelwatchownershipv1* xrtTaskGroupCancelPolicyV1Get(void)
{ return &__xrtTaskGroupCancelPolicy; }



/* 在持锁状态下取出已经满足关闭与归零条件的唯一 Promise。 */
static xpromise* __xrtTaskGroupTakePromise(xtaskgroup* pGroup)
{
	xpromise* pPromise = NULL;

	if ( pGroup->Closed && (pGroup->Active == 0) ) {
		pPromise = pGroup->Promise;
		pGroup->Promise = NULL;
	}
	return pPromise;
}



/* 在组锁外完成并释放 Done Promise。 */
static void __xrtTaskGroupResolve(xpromise* pPromise)
{
	if ( pPromise != NULL ) {
		(void)xrtPromiseResolve(pPromise, NULL);
		xrtPromiseDestroy(pPromise);
	}
}



/* 把嵌套子组的累计结果压缩成父组中的一个有效终态。 */
static xfuturestate __xrtTaskGroupChildState(
	xtaskgroup* pChild,
	xerror** ppError
)
{
	xtaskgroupstats tStats;
	const xerror* pError;

	memset(&tStats, 0, sizeof(tStats));
	(void)xrtTaskGroupGet(pChild, &tStats);
	if ( tStats.Failed != 0 ) {
		pError = xrtTaskGroupError(pChild);
		*ppError = pError != NULL ? xrtErrorRef(pError) : NULL;
		return XFUTURE_FAILED;
	}
	if ( tStats.Closed != 0 ) {
		return XFUTURE_CLOSED;
	}
	if ( tStats.Cancelled != 0 ) {
		return XFUTURE_CANCELLED;
	}
	return XFUTURE_RESOLVED;
}



/* 判断一个终态是否命中配置的兄弟项取消策略。 */
static bool __xrtTaskGroupShouldCancel(
	const xtaskgroup* pGroup,
	xfuturestate State
)
{
	if ( State == XFUTURE_FAILED ) {
		return (pGroup->CancelOn & XRT_TASK_GROUP_CANCEL_ON_FAILED) != 0;
	}
	if ( State == XFUTURE_CANCELLED ) {
		return (pGroup->CancelOn & XRT_TASK_GROUP_CANCEL_ON_CANCELLED) != 0;
	}
	if ( State == XFUTURE_CLOSED ) {
		return (pGroup->CancelOn & XRT_TASK_GROUP_CANCEL_ON_CLOSED) != 0;
	}
	return false;
}



/* 一个源完成后从活动链摘除，并更新累计结果和组完成条件。 */
static void __xrtTaskGroupSourceDone(ptr pData)
{
	xrtownershipscope Scope = {0};
	xrt_task_group_item* pItem = (xrt_task_group_item*)pData;
	xtaskgroup* pGroup = pItem->Group;
	xfuturestate State;
	xerror* pError = NULL;
	xpromise* pPromise;
	bool bCancel;

	__xrtTaskGroupBegin(pGroup, &Scope); ++pGroup->Busy; __xrtTaskGroupEnd(pGroup, &Scope);
	if ( pItem->Child != NULL ) {
		State = __xrtTaskGroupChildState(pItem->Child, &pError);
	} else {
		State = xrtFutureState(pItem->Future);
		if ( State == XFUTURE_FAILED ) {
			const xerror* pSourceError = xrtFutureError(pItem->Future);

			pError = pSourceError != NULL ? xrtErrorRef(pSourceError) : NULL;
		}
	}

	__xrtTaskGroupBegin(pGroup, &Scope);
	__xrtTaskGroupItemRemoveLocked(pGroup, pItem);
	pGroup->Active--;
	pGroup->Completed++;
	if ( State == XFUTURE_RESOLVED ) {
		pGroup->Succeeded++;
	} else if ( State == XFUTURE_FAILED ) {
		pGroup->Failed++;
	} else if ( State == XFUTURE_CANCELLED ) {
		pGroup->Cancelled++;
	} else {
		pGroup->ClosedResults++;
	}
	if ( (State != XFUTURE_RESOLVED) && (pGroup->FirstIndex == SIZE_MAX) ) {
		pGroup->FirstIndex = pItem->Index;
		pGroup->FirstState = State;
	}
	if ( (State == XFUTURE_FAILED) && (pGroup->FirstError == NULL) ) {
		pGroup->FirstError = pError;
		pError = NULL;
	}
	bCancel = !pGroup->Cancelling &&
		__xrtTaskGroupShouldCancel(pGroup, State);
	pPromise = __xrtTaskGroupTakePromise(pGroup);
	__xrtTaskGroupEnd(pGroup, &Scope);

	xrtErrorFree(pError);
	if ( bCancel ) {
		(void)xrtCancelRequest(pGroup->Cancel);
	}
	__xrtTaskGroupResolve(pPromise);
	/* Discharge the list's right separately from the completion callback. */
	__xrtTaskGroupItemRelease(pItem);
	__xrtTaskGroupBegin(pGroup, &Scope); --pGroup->Busy; __xrtTaskGroupEnd(pGroup, &Scope);
}



/* 取消一个已经提交的活动项；预留项在提交时补发取消。 */
static void __xrtTaskGroupItemCancel(xrt_task_group_item* pItem)
{
	xrtownershipscope Scope = {0};
	if ( pItem->Child != NULL ) {
		(void)xrtTaskGroupCancel(pItem->Child);
	} else {
		xfuture* pFuture;

		__xrtTaskGroupBegin(pItem->Group, &Scope);
		pFuture = pItem->Future != NULL ?
			xrtFutureRef(pItem->Future) : NULL;
		__xrtTaskGroupEnd(pItem->Group, &Scope);
		if ( pFuture != NULL ) {
			(void)xrtFutureCancel(pFuture);
			xrtFutureDestroy(pFuture);
		}
	}
}



/* 组取消令牌命中后关闭接纳，并无分配地快照全部活动项。 */
static void __xrtTaskGroupCancelled(ptr pData)
{
	xrtownershipscope Scope = {0};
	xtaskgroup* pGroup = (xtaskgroup*)pData;
	xrt_task_group_item* pList = NULL;
	xpromise* pPromise;

	__xrtTaskGroupBegin(pGroup, &Scope);
	++pGroup->Busy;
	if ( !pGroup->Cancelling ) {
		pGroup->Cancelling = true;
		pGroup->Closed = true;
		for ( xrt_task_group_item* pItem = pGroup->Head;
			pItem != NULL; pItem = pItem->Next ) {
			if ( !pItem->CancelPosted ) {
				pItem->CancelPosted = true;
				__xrtTaskGroupItemRef(pItem);
				pItem->CancelNext = pList;
				pList = pItem;
			}
		}
	}
	pPromise = __xrtTaskGroupTakePromise(pGroup);
	__xrtTaskGroupEnd(pGroup, &Scope);

	while ( pList != NULL ) {
		xrt_task_group_item* pItem = pList;

		pList = pItem->CancelNext;
		pItem->CancelNext = NULL;
		__xrtTaskGroupItemCancel(pItem);
		__xrtTaskGroupItemRelease(pItem);
	}
	__xrtTaskGroupResolve(pPromise);
	__xrtTaskGroupBegin(pGroup, &Scope); --pGroup->Busy; __xrtTaskGroupEnd(pGroup, &Scope);
}



/* 创建一个尚未发布 Future 的活动项，并持有组生命周期。 */
static xrt_task_group_item* __xrtTaskGroupItemCreate(
	xtaskgroup* pGroup,
	xtaskgroup* pChild
)
{
	xrtownershipscope Scope = {0};
	xrt_task_group_item* pItem;

	pItem = (xrt_task_group_item*)xrtCalloc(1, sizeof(xrt_task_group_item));
	if ( pItem == NULL ) {
		__xrtTaskGroupBegin(pGroup, &Scope);
		pGroup->Rejected++;
		__xrtTaskGroupEnd(pGroup, &Scope);
		return NULL;
	}
	pItem->RefCount = 1;
	pItem->Group = pGroup;
	pItem->Child = pChild;
	pItem->Waiter.Proc = __xrtTaskGroupSourceDone;
	pItem->Waiter.PublicWatch = true;
	pItem->Waiter.Release = __xrtTaskGroupWaiterRelease;
	pItem->Waiter.Data = pItem;
	pItem->Waiter.Phased = true;
	pItem->Waiter.Certified = 1;
	pItem->Waiter.OwnershipPolicy = &__xrtTaskGroupWatchPolicy;
	if ( !__xrtTaskGroupRef(pGroup) ) {
		xrtFree(pItem);
		__xrtTaskGroupBegin(pGroup, &Scope);
		pGroup->Rejected++;
		__xrtTaskGroupEnd(pGroup, &Scope);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( (pChild != NULL) && !__xrtTaskGroupRef(pChild) ) {
		__xrtTaskGroupRelease(pGroup);
		xrtFree(pItem);
		__xrtTaskGroupBegin(pGroup, &Scope);
		pGroup->Rejected++;
		__xrtTaskGroupEnd(pGroup, &Scope);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	return pItem;
}



/* 在组内预留一个活动槽位，使关闭和取消覆盖 Future 启动窗口。 */
static xrt_task_group_item* __xrtTaskGroupReserve(
	xtaskgroup* pGroup,
	xtaskgroup* pChild
)
{
	xrtownershipscope Scope = {0};
	xrt_task_group_item* pItem;
	bool bClosed;
	bool bLimited;
	bool bOverflow;

	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pItem = __xrtTaskGroupItemCreate(pGroup, pChild);
	if ( pItem == NULL ) {
		return NULL;
	}

	__xrtTaskGroupBegin(pGroup, &Scope);
	bClosed = pGroup->Closed;
	bLimited = (pGroup->Limit != 0) && (pGroup->Active >= pGroup->Limit);
	bOverflow = (pGroup->Added >= (uint64)SIZE_MAX) ||
		(pGroup->Reserved >= (size_t)((uint64)SIZE_MAX - pGroup->Added));
	if ( bClosed || bLimited || bOverflow ) {
		pGroup->Rejected++;
	} else {
		pItem->Next = pGroup->Head;
		if ( pGroup->Head != NULL ) {
			pGroup->Head->Previous = pItem;
		}
		pGroup->Head = pItem;
		pItem->Listed = true;
		__xrtTaskGroupItemRef(pItem);
		pGroup->Active++;
		pGroup->Reserved++;
	}
	__xrtTaskGroupEnd(pGroup, &Scope);
	if ( bClosed || bLimited || bOverflow ) {
		__xrtTaskGroupItemRelease(pItem);
		if ( bClosed ) {
			__xrtErrorSetClosed();
		} else if ( bLimited ) {
			__xrtErrorSetAgain();
		} else {
			__xrtErrorSetSizeOverflow();
		}
		return NULL;
	}
	return pItem;
}



/* 回滚未能启动 Future 的预留项，并重新检查组完成条件。 */
static void __xrtTaskGroupAbort(xrt_task_group_item* pItem)
{
	xrtownershipscope Scope = {0};
	xtaskgroup* pGroup = pItem->Group;
	xpromise* pPromise;

	__xrtTaskGroupBegin(pGroup, &Scope);
	__xrtTaskGroupItemRemoveLocked(pGroup, pItem);
	pGroup->Active--;
	pGroup->Reserved--;
	pGroup->Rejected++;
	pPromise = __xrtTaskGroupTakePromise(pGroup);
	__xrtTaskGroupEnd(pGroup, &Scope);
	__xrtTaskGroupResolve(pPromise);
	__xrtTaskGroupItemRelease(pItem);
	__xrtTaskGroupItemRelease(pItem); /* List and untransferred creator. */
}



/* 提交预留项的 Future，并在已经取消时补发协作取消请求。 */
static bool __xrtTaskGroupCommit(
	xrt_task_group_item* pItem,
	xfuture* pFuture
)
{
	xrtownershipscope Scope = {0};
	xtaskgroup* pGroup = pItem->Group;
	xfuture* pTracked;
	bool bCancel;

	if ( pFuture == pGroup->Future ) {
		__xrtTaskGroupAbort(pItem);
		__xrtErrorSetInvalidState();
		return false;
	}
	pTracked = xrtFutureRef(pFuture);
	if ( pTracked == NULL ) {
		__xrtTaskGroupAbort(pItem);
		return false;
	}

	__xrtTaskGroupBegin(pGroup, &Scope);
	pItem->Future = pTracked;
	pItem->Index = (size_t)pGroup->Added;
	pGroup->Added++;
	pGroup->Reserved--;
	bCancel = pItem->CancelPosted;
	__xrtTaskGroupEnd(pGroup, &Scope);

	if ( bCancel ) {
		__xrtTaskGroupItemCancel(pItem);
	}
	if ( !__xrtFutureWaiterAdd(pItem->Future, &pItem->Waiter) ) {
		__xrtTaskGroupSourceDone(pItem);
		__xrtTaskGroupItemRelease(pItem);
	}
	return true;
}



/* 内部添加路径同时支持普通 Future 和由父组管理的子组。 */
static bool __xrtTaskGroupAdd(
	xtaskgroup* pGroup,
	xfuture* pFuture,
	xtaskgroup* pChild
)
{
	xrt_task_group_item* pItem;

	if ( (pGroup == NULL) || (pFuture == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pFuture == pGroup->Future ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pItem = __xrtTaskGroupReserve(pGroup, pChild);
	if ( pItem == NULL ) {
		return false;
	}
	return __xrtTaskGroupCommit(pItem, pFuture);
}



/* 创建结构化任务组及其取消源和稳定 Done Future。 */
XRT_API xtaskgroup* xrtTaskGroupCreate(const xtaskgroupconfig* pConfig)
{
	xrtownershipscope Scope = {0};
	xtaskgroup* pGroup;
	xcancel* pParent = pConfig != NULL ? pConfig->Cancel : NULL;
	uint32 iCancelOn = pConfig != NULL ? pConfig->CancelOn : 0;

	if ( (iCancelOn & ~XRT_TASK_GROUP_CANCEL_ON_STOPPED) != 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pGroup = (xtaskgroup*)xrtCalloc(1, sizeof(xtaskgroup));
	if ( pGroup == NULL ) {
		return NULL;
	}
	pGroup->RefCount = 1;
	pGroup->Busy = 1; /* Owned observer can synchronously notify during construction. */
	pGroup->Limit = pConfig != NULL ? pConfig->Limit : 0;
	pGroup->CancelOn = iCancelOn;
	pGroup->FirstIndex = SIZE_MAX;
	pGroup->FirstState = XFUTURE_PENDING;
	if ( !xrtMutexInit(&pGroup->Lock) ) {
		xrtFree(pGroup);
		return NULL;
	}
	pGroup->Cancel = xrtCancelChild(pParent);
	if ( pGroup->Cancel == NULL ) {
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
		return NULL;
	}
	pGroup->Promise = xrtPromiseCreate(&pGroup->Future, NULL);
	if ( pGroup->Promise == NULL ) {
		xrtCancelDestroy(pGroup->Cancel);
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
		return NULL;
	}
	if (!__xrtTaskGroupRef(pGroup)) {
		pGroup->Busy = 0; __xrtTaskGroupRelease(pGroup); __xrtErrorSetSizeOverflow(); return NULL;
	}
	pGroup->Watch = xrtCancelWatchOwnedV1(pGroup->Cancel, pGroup, &__xrtTaskGroupCancelPolicy);
	if ( pGroup->Watch == NULL ) {
		pGroup->Busy = 0;
		__xrtTaskGroupRelease(pGroup); /* Constructor failure did not consume Data. */
		__xrtTaskGroupRelease(pGroup);
		return NULL;
	}
	__xrtTaskGroupBegin(pGroup, &Scope); --pGroup->Busy; __xrtTaskGroupEnd(pGroup, &Scope);
	return pGroup;
}



/* 创建受父组关闭与取消语义管理的嵌套子组。 */
XRT_API xtaskgroup* xrtTaskGroupChild(
	xtaskgroup* pParent,
	const xtaskgroupconfig* pConfig
)
{
	xtaskgroup* pChild;

	if ( pParent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pChild = xrtTaskGroupCreate(pConfig);
	if ( pChild == NULL ) {
		return NULL;
	}
	if ( !__xrtTaskGroupAdd(pParent, pChild->Future, pChild) ) {
		xrtTaskGroupDestroy(pChild);
		return NULL;
	}
	return pChild;
}



/* 跟踪一个普通 Future。 */
XRT_API bool xrtTaskGroupAdd(xtaskgroup* pGroup, xfuture* pFuture)
{
	return __xrtTaskGroupAdd(pGroup, pFuture, NULL);
}



/* 在组槽位保护下同步启动 Future，失败时完整回滚预留。 */
XRT_API xfuture* xrtTaskGroupStart(
	xtaskgroup* pGroup,
	xtaskgroupstartproc pProc,
	ptr pData
)
{
	xrt_task_group_item* pItem;
	xfuture* pFuture;
	xerror* pError;

	if ( (pGroup == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pItem = __xrtTaskGroupReserve(pGroup, NULL);
	if ( pItem == NULL ) {
		return NULL;
	}
	pFuture = pProc(pData);
	if ( pFuture == NULL ) {
		pError = xrtTakeError();
		__xrtTaskGroupAbort(pItem);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtErrorSetInternal();
		}
		return NULL;
	}
	if ( !__xrtTaskGroupCommit(pItem, pFuture) ) {
		pError = xrtTakeError();
		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtErrorSetInternal();
		}
		return NULL;
	}
	return pFuture;
}



/* 停止接纳，并把关闭动作传播到当前嵌套子组。 */
XRT_API bool xrtTaskGroupClose(xtaskgroup* pGroup)
{
	xrtownershipscope Scope = {0};
	xrt_task_group_item* pList = NULL;
	xpromise* pPromise;
	bool bClosed;

	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtTaskGroupBegin(pGroup, &Scope);
	bClosed = pGroup->Closed;
	if ( !bClosed ) {
		pGroup->Closed = true;
		for ( xrt_task_group_item* pItem = pGroup->Head;
			pItem != NULL; pItem = pItem->Next ) {
			if ( (pItem->Child != NULL) && !pItem->ClosePosted ) {
				pItem->ClosePosted = true;
				__xrtTaskGroupItemRef(pItem);
				pItem->CloseNext = pList;
				pList = pItem;
			}
		}
	}
	pPromise = __xrtTaskGroupTakePromise(pGroup);
	__xrtTaskGroupEnd(pGroup, &Scope);

	while ( pList != NULL ) {
		xrt_task_group_item* pItem = pList;

		pList = pItem->CloseNext;
		pItem->CloseNext = NULL;
		(void)xrtTaskGroupClose(pItem->Child);
		__xrtTaskGroupItemRelease(pItem);
	}
	__xrtTaskGroupResolve(pPromise);
	return !bClosed;
}



/* 请求组取消；取消监听负责统一关闭和锁外传播。 */
XRT_API bool xrtTaskGroupCancel(xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return xrtCancelRequest(pGroup->Cancel);
}



/* 返回稳定 Done Future 的新增引用。 */
XRT_API xfuture* xrtTaskGroupFuture(const xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtFutureRef(pGroup->Future);
}



/* 关闭并永久等待组完成。 */
XRT_API xwaitresult xrtTaskGroupWait(xtaskgroup* pGroup)
{
	return __xrtTaskGroupWaitUntilCancel(
		pGroup,
		INFINITY,
		NULL
	);
}



/* 关闭并在相对毫秒数内等待组完成。 */
XRT_API xwaitresult xrtTaskGroupWaitFor(xtaskgroup* pGroup, int64 iTimeout)
{
	return __xrtTaskGroupWaitUntilCancel(
		pGroup,
		__xrtWaitAfter(iTimeout),
		NULL
	);
}



/* 关闭并等待到指定截止时间。 */
XRT_API xwaitresult __xrtTaskGroupWaitUntil(
	xtaskgroup* pGroup,
	double iDeadline
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

	return __xrtTaskGroupWaitUntilCancel(pGroup, iDeadline, NULL);
}



/* 关闭组，再复用 Future 的统一截止时间与调用方取消等待语义。 */
XRT_API xwaitresult __xrtTaskGroupWaitUntilCancel(
	xtaskgroup* pGroup,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return XWAIT_ERROR; }

	xfuture* pFuture = xrtTaskGroupFuture(pGroup);
	xwaitresult Result;

	if ( pFuture == NULL ) {
		return XWAIT_ERROR;
	}
	(void)xrtTaskGroupClose(pGroup);
	Result = __xrtFutureWaitUntilCancel(pFuture, iDeadline, pCancel);
	xrtFutureDestroy(pFuture);
	return Result;
}



/* 复制任务组统计快照。 */
XRT_API bool xrtTaskGroupGet(
	const xtaskgroup* pGroup,
	xtaskgroupstats* pStats
)
{
	xrtownershipscope Scope = {0};
	if ( (pGroup == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtTaskGroupBegin(pGroup, &Scope);
	pStats->Active = pGroup->Active;
	pStats->Added = pGroup->Added;
	pStats->Completed = pGroup->Completed;
	pStats->Succeeded = pGroup->Succeeded;
	pStats->Failed = pGroup->Failed;
	pStats->Cancelled = pGroup->Cancelled;
	pStats->Closed = pGroup->ClosedResults;
	pStats->Rejected = pGroup->Rejected;
	pStats->FirstIndex = pGroup->FirstIndex;
	pStats->FirstState = pGroup->FirstState;
	pStats->Accepting = !pGroup->Closed;
	pStats->Cancelling = pGroup->Cancelling;
	__xrtTaskGroupEnd(pGroup, &Scope);
	return true;
}



/* 返回任务组保留的首个失败错误。 */
XRT_API const xerror* xrtTaskGroupError(const xtaskgroup* pGroup)
{
	xrtownershipscope Scope = {0};
	const xerror* pError;

	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	__xrtTaskGroupBegin(pGroup, &Scope);
	pError = pGroup->FirstError;
	__xrtTaskGroupEnd(pGroup, &Scope);
	return pError;
}



/* 返回组取消令牌的新增引用。 */
XRT_API xcancel* xrtTaskGroupCancelToken(const xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtCancelRef(pGroup->Cancel);
}



/* 关闭并取消仍活动的项，最终回收由活动监听引用计数驱动。 */
XRT_API void xrtTaskGroupDestroy(xtaskgroup* pGroup)
{
	xrtownershipscope Scope = {0};
	bool bActive;
	xcancelwatch* pWatch;

	if ( pGroup == NULL ) {
		return;
	}
	(void)xrtTaskGroupClose(pGroup);
	__xrtTaskGroupBegin(pGroup, &Scope);
	bActive = pGroup->Active != 0;
	__xrtTaskGroupEnd(pGroup, &Scope);
	if ( bActive ) {
		(void)xrtTaskGroupCancel(pGroup);
	}
	/* Consume only the public owner's registration right. Watch's Data ref is
	 * independent and may return later when a real callback/dispatch retires. */
	__xrtTaskGroupBegin(pGroup, &Scope); pWatch = pGroup->Watch; pGroup->Watch = NULL;
	__xrtTaskGroupEnd(pGroup, &Scope); xrtCancelUnwatch(pWatch);
	__xrtTaskGroupRelease(pGroup);
}

#endif

#if (defined(XRT_FEATURE_TASK_GROUP))
XRT_API xwaitresult xrtTaskGroupWaitForCancel(
	xtaskgroup* pGroup,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtTaskGroupWaitUntilCancel(pGroup, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
