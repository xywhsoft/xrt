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
};



/* 增加任务组内部引用；达到计数上限时拒绝继续接纳活动项。 */
static bool __xrtTaskGroupRef(xtaskgroup* pGroup)
{
	return xrtRefRetain(&pGroup->RefCount) >= 0;
}



/* 释放任务组内部引用，并在全部活动项退出后回收公共状态。 */
static void __xrtTaskGroupRelease(xtaskgroup* pGroup)
{
	if ( xrtRefRelease(&pGroup->RefCount) == 0 ) {
		if ( pGroup->Watch != NULL ) {
			xrtCancelUnwatch(pGroup->Watch);
		}
		xrtPromiseDestroy(pGroup->Promise);
		xrtFutureDestroy(pGroup->Future);
		xrtCancelDestroy(pGroup->Cancel);
		xrtErrorFree(pGroup->FirstError);
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
	}
}



/* 增加活动项引用，供锁外关闭或取消传播使用。 */
static void __xrtTaskGroupItemRef(xrt_task_group_item* pItem)
{
	(void)xrtRefRetain(&pItem->RefCount);
}



/* 释放活动项及其源、子组和父组引用。 */
static void __xrtTaskGroupItemRelease(xrt_task_group_item* pItem)
{
	if ( xrtRefRelease(&pItem->RefCount) == 0 ) {
		xrtFutureDestroy(pItem->Future);
		if ( pItem->Child != NULL ) {
			__xrtTaskGroupRelease(pItem->Child);
		}
		__xrtTaskGroupRelease(pItem->Group);
		xrtFree(pItem);
	}
}



/* 从活动链中摘除指定项，调用方必须持有组锁。 */
static void __xrtTaskGroupItemRemoveLocked(
	xtaskgroup* pGroup,
	xrt_task_group_item* pItem
)
{
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
}



/* Future 完成批次释放监听所持有的活动项引用。 */
static void __xrtTaskGroupWaiterRelease(ptr pData)
{
	__xrtTaskGroupItemRelease((xrt_task_group_item*)pData);
}



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
	xrt_task_group_item* pItem = (xrt_task_group_item*)pData;
	xtaskgroup* pGroup = pItem->Group;
	xfuturestate State;
	xerror* pError = NULL;
	xpromise* pPromise;
	bool bCancel;

	if ( pItem->Child != NULL ) {
		State = __xrtTaskGroupChildState(pItem->Child, &pError);
	} else {
		State = xrtFutureState(pItem->Future);
		if ( State == XFUTURE_FAILED ) {
			const xerror* pSourceError = xrtFutureError(pItem->Future);

			pError = pSourceError != NULL ? xrtErrorRef(pSourceError) : NULL;
		}
	}

	(void)xrtMutexLock(&pGroup->Lock);
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
	(void)xrtMutexUnlock(&pGroup->Lock);

	xrtErrorFree(pError);
	if ( bCancel ) {
		(void)xrtCancelRequest(pGroup->Cancel);
	}
	__xrtTaskGroupResolve(pPromise);
}



/* 取消一个已经提交的活动项；预留项在提交时补发取消。 */
static void __xrtTaskGroupItemCancel(xrt_task_group_item* pItem)
{
	if ( pItem->Child != NULL ) {
		(void)xrtTaskGroupCancel(pItem->Child);
	} else {
		xfuture* pFuture;

		(void)xrtMutexLock(&pItem->Group->Lock);
		pFuture = pItem->Future != NULL ?
			xrtFutureRef(pItem->Future) : NULL;
		(void)xrtMutexUnlock(&pItem->Group->Lock);
		if ( pFuture != NULL ) {
			(void)xrtFutureCancel(pFuture);
			xrtFutureDestroy(pFuture);
		}
	}
}



/* 组取消令牌命中后关闭接纳，并无分配地快照全部活动项。 */
static void __xrtTaskGroupCancelled(ptr pData)
{
	xtaskgroup* pGroup = (xtaskgroup*)pData;
	xrt_task_group_item* pList = NULL;
	xpromise* pPromise;

	(void)xrtMutexLock(&pGroup->Lock);
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
	(void)xrtMutexUnlock(&pGroup->Lock);

	while ( pList != NULL ) {
		xrt_task_group_item* pItem = pList;

		pList = pItem->CancelNext;
		pItem->CancelNext = NULL;
		__xrtTaskGroupItemCancel(pItem);
		__xrtTaskGroupItemRelease(pItem);
	}
	__xrtTaskGroupResolve(pPromise);
}



/* 创建一个尚未发布 Future 的活动项，并持有组生命周期。 */
static xrt_task_group_item* __xrtTaskGroupItemCreate(
	xtaskgroup* pGroup,
	xtaskgroup* pChild
)
{
	xrt_task_group_item* pItem;

	pItem = (xrt_task_group_item*)xrtCalloc(1, sizeof(xrt_task_group_item));
	if ( pItem == NULL ) {
		(void)xrtMutexLock(&pGroup->Lock);
		pGroup->Rejected++;
		(void)xrtMutexUnlock(&pGroup->Lock);
		return NULL;
	}
	pItem->RefCount = 1;
	pItem->Group = pGroup;
	pItem->Child = pChild;
	pItem->Waiter.Proc = __xrtTaskGroupSourceDone;
	pItem->Waiter.Release = __xrtTaskGroupWaiterRelease;
	pItem->Waiter.Data = pItem;
	if ( !__xrtTaskGroupRef(pGroup) ) {
		xrtFree(pItem);
		(void)xrtMutexLock(&pGroup->Lock);
		pGroup->Rejected++;
		(void)xrtMutexUnlock(&pGroup->Lock);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( (pChild != NULL) && !__xrtTaskGroupRef(pChild) ) {
		__xrtTaskGroupRelease(pGroup);
		xrtFree(pItem);
		(void)xrtMutexLock(&pGroup->Lock);
		pGroup->Rejected++;
		(void)xrtMutexUnlock(&pGroup->Lock);
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

	(void)xrtMutexLock(&pGroup->Lock);
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
		pGroup->Active++;
		pGroup->Reserved++;
	}
	(void)xrtMutexUnlock(&pGroup->Lock);
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
	xtaskgroup* pGroup = pItem->Group;
	xpromise* pPromise;

	(void)xrtMutexLock(&pGroup->Lock);
	__xrtTaskGroupItemRemoveLocked(pGroup, pItem);
	pGroup->Active--;
	pGroup->Reserved--;
	pGroup->Rejected++;
	pPromise = __xrtTaskGroupTakePromise(pGroup);
	(void)xrtMutexUnlock(&pGroup->Lock);
	__xrtTaskGroupResolve(pPromise);
	__xrtTaskGroupItemRelease(pItem);
}



/* 提交预留项的 Future，并在已经取消时补发协作取消请求。 */
static bool __xrtTaskGroupCommit(
	xrt_task_group_item* pItem,
	xfuture* pFuture
)
{
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

	(void)xrtMutexLock(&pGroup->Lock);
	pItem->Future = pTracked;
	pItem->Index = (size_t)pGroup->Added;
	pGroup->Added++;
	pGroup->Reserved--;
	bCancel = pItem->CancelPosted;
	(void)xrtMutexUnlock(&pGroup->Lock);

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
	pGroup->Watch = xrtCancelWatch(
		pGroup->Cancel,
		__xrtTaskGroupCancelled,
		pGroup
	);
	if ( pGroup->Watch == NULL ) {
		xrtPromiseDestroy(pGroup->Promise);
		xrtFutureDestroy(pGroup->Future);
		xrtCancelDestroy(pGroup->Cancel);
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
		return NULL;
	}
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
	xrt_task_group_item* pList = NULL;
	xpromise* pPromise;
	bool bClosed;

	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pGroup->Lock);
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
	(void)xrtMutexUnlock(&pGroup->Lock);

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
	return xrtTaskGroupWaitUntilCancel(
		pGroup,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 关闭并在相对微秒数内等待组完成。 */
XRT_API xwaitresult xrtTaskGroupWaitFor(xtaskgroup* pGroup, uint64 iTimeout)
{
	return xrtTaskGroupWaitUntilCancel(
		pGroup,
		xrtDeadlineAfter(iTimeout),
		NULL
	);
}



/* 关闭并等待到指定截止时间。 */
XRT_API xwaitresult xrtTaskGroupWaitUntil(
	xtaskgroup* pGroup,
	xdeadline iDeadline
)
{
	return xrtTaskGroupWaitUntilCancel(pGroup, iDeadline, NULL);
}



/* 关闭组，再复用 Future 的统一截止时间与调用方取消等待语义。 */
XRT_API xwaitresult xrtTaskGroupWaitUntilCancel(
	xtaskgroup* pGroup,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTaskGroupFuture(pGroup);
	xwaitresult Result;

	if ( pFuture == NULL ) {
		return XWAIT_ERROR;
	}
	(void)xrtTaskGroupClose(pGroup);
	Result = xrtFutureWaitUntilCancel(pFuture, iDeadline, pCancel);
	xrtFutureDestroy(pFuture);
	return Result;
}



/* 复制任务组统计快照。 */
XRT_API bool xrtTaskGroupGet(
	const xtaskgroup* pGroup,
	xtaskgroupstats* pStats
)
{
	if ( (pGroup == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock((xmutex*)&pGroup->Lock);
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
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return true;
}



/* 返回任务组保留的首个失败错误。 */
XRT_API const xerror* xrtTaskGroupError(const xtaskgroup* pGroup)
{
	const xerror* pError;

	if ( pGroup == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	(void)xrtMutexLock((xmutex*)&pGroup->Lock);
	pError = pGroup->FirstError;
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
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
	bool bActive;

	if ( pGroup == NULL ) {
		return;
	}
	(void)xrtTaskGroupClose(pGroup);
	(void)xrtMutexLock(&pGroup->Lock);
	bActive = pGroup->Active != 0;
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( bActive ) {
		(void)xrtTaskGroupCancel(pGroup);
	}
	__xrtTaskGroupRelease(pGroup);
}

#endif
