#include "../internal/xrt_future.h"



#if defined(XRT_FEATURE_FUTURE_COMBINE)

/* 组合模式只决定完成时机以及是否向败者传播取消请求。 */
typedef enum xrt_future_combine_mode {
	XRT_FUTURE_COMBINE_ANY = 1,
	XRT_FUTURE_COMBINE_ALL = 2,
	XRT_FUTURE_COMBINE_RACE = 3
} xrt_future_combine_mode;



typedef struct xrt_future_combine xrt_future_combine;



/* 每个输入槽位保存一个无需额外分配的 Future 完成监听。 */
typedef struct xrt_future_combine_item {
	xrt_future_waiter Waiter;
	xrt_future_combine* Group;
	size_t Index;
} xrt_future_combine_item;



/* 组合上下文、源引用和监听槽位由一次连续分配保存。 */
struct xrt_future_combine {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_future_combine_mode Mode;
	size_t Count;
	size_t Remaining;
	bool Completed;
	xpromise* Promise;
	xcancelwatch* Watch;
	xfuture** Sources;
	xrt_future_combine_item* Items;
	xfuturepick Pick;
	xfutureall All;
};



/* 增加组合上下文的内部引用。 */
static void __xrtFutureCombineRef(xrt_future_combine* pGroup)
{
	(void)xrtRefRetain(&pGroup->RefCount);
}



/* 回收已没有结果、创建者和监听持有者的组合上下文。 */
static void __xrtFutureCombineFree(xrt_future_combine* pGroup)
{
	for ( size_t i = 0; i < pGroup->Count; i++ ) {
		xrtFutureDestroy(pGroup->Sources[i]);
	}
	(void)xrtMutexUnit(&pGroup->Lock);
	xrtFree(pGroup);
}



/* 释放组合上下文的一个内部引用。 */
static void __xrtFutureCombineRelease(xrt_future_combine* pGroup)
{
	if ( xrtRefRelease(&pGroup->RefCount) == 0 ) {
		__xrtFutureCombineFree(pGroup);
	}
}



/* Future 监听离开源链表后释放它持有的组合上下文引用。 */
static void __xrtFutureCombineWaiterRelease(ptr pData)
{
	xrt_future_combine_item* pItem = (xrt_future_combine_item*)pData;

	__xrtFutureCombineRelease(pItem->Group);
}



/* 摘除全部仍挂接的源监听；当前回调槽位由 Future 完成路径自行收尾。 */
static void __xrtFutureCombineDetach(
	xrt_future_combine* pGroup,
	xrt_future_combine_item* pCurrent
)
{
	for ( size_t i = 0; i < pGroup->Count; i++ ) {
		xrt_future_combine_item* pItem = &pGroup->Items[i];

		if ( pItem != pCurrent ) {
			(void)__xrtFutureWaiterDetach(
				pGroup->Sources[i],
				&pItem->Waiter
			);
		}
	}
}



/* 向指定槽位以外的源发送协作取消请求，不伪造源 Future 终态。 */
static void __xrtFutureCombineCancelSources(
	xrt_future_combine* pGroup,
	size_t iExcept
)
{
	for ( size_t i = 0; i < pGroup->Count; i++ ) {
		if ( i != iExcept ) {
			(void)xrtFutureCancel(pGroup->Sources[i]);
		}
	}
}



/* 组合结果释放时注销取消监听，再交还结果持有的上下文引用。 */
static void __xrtFutureCombineDestroyValue(ptr pValue, ptr pData)
{
	xrt_future_combine* pGroup = (xrt_future_combine*)pData;
	xcancelwatch* pWatch;

	(void)pValue;
	(void)xrtMutexLock(&pGroup->Lock);
	pWatch = pGroup->Watch;
	pGroup->Watch = NULL;
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
	}
	__xrtFutureCombineRelease(pGroup);
}



/* 输出 Future 被请求取消时，结束组合监听并把请求传播给全部源。 */
static void __xrtFutureCombineCancelled(ptr pData)
{
	xrt_future_combine* pGroup = (xrt_future_combine*)pData;
	xpromise* pPromise;
	xcancelwatch* pWatch;

	(void)xrtMutexLock(&pGroup->Lock);
	if ( pGroup->Completed ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		return;
	}
	pGroup->Completed = true;
	pPromise = pGroup->Promise;
	pGroup->Promise = NULL;
	pWatch = pGroup->Watch;
	pGroup->Watch = NULL;
	(void)xrtMutexUnlock(&pGroup->Lock);

	__xrtFutureCombineDetach(pGroup, NULL);
	(void)xrtPromiseCancel(pPromise);
	xrtPromiseDestroy(pPromise);
	__xrtFutureCombineCancelSources(pGroup, SIZE_MAX);
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
	}
	__xrtFutureCombineRelease(pGroup);
}



/* 一个源进入终态后更新组合计数，并由唯一胜出者完成输出 Future。 */
static void __xrtFutureCombineSourceDone(ptr pData)
{
	xrt_future_combine_item* pItem = (xrt_future_combine_item*)pData;
	xrt_future_combine* pGroup = pItem->Group;
	xpromise* pPromise = NULL;
	ptr pResult = NULL;
	bool bRace = false;

	(void)xrtMutexLock(&pGroup->Lock);
	if ( !pGroup->Completed ) {
		if ( pGroup->Mode == XRT_FUTURE_COMBINE_ALL ) {
			pGroup->Remaining--;
			if ( pGroup->Remaining == 0 ) {
				pGroup->Completed = true;
				pResult = &pGroup->All;
			}
		} else {
			pGroup->Completed = true;
			pGroup->Pick.Index = pItem->Index;
			pGroup->Pick.Future = pGroup->Sources[pItem->Index];
			pResult = &pGroup->Pick;
			bRace = pGroup->Mode == XRT_FUTURE_COMBINE_RACE;
		}
		if ( pGroup->Completed ) {
			pPromise = pGroup->Promise;
			pGroup->Promise = NULL;
		}
	}
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( pPromise == NULL ) {
		return;
	}

	__xrtFutureCombineDetach(pGroup, pItem);
	if ( !xrtPromiseResolveOwned(
		pPromise,
		pResult,
		__xrtFutureCombineDestroyValue,
		pGroup
	) ) {
		__xrtFutureCombineRelease(pGroup);
	}
	xrtPromiseDestroy(pPromise);
	if ( bRace ) {
		__xrtFutureCombineCancelSources(pGroup, pItem->Index);
	}
}



/* 判断组合是否已由更早完成的源占据终态。 */
static bool __xrtFutureCombineDone(xrt_future_combine* pGroup)
{
	bool bDone;

	(void)xrtMutexLock(&pGroup->Lock);
	bDone = pGroup->Completed;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return bDone;
}



/* 为一个输入槽位注册完成监听，并补偿注册前已经完成的竞争窗口。 */
static void __xrtFutureCombineAttach(
	xrt_future_combine* pGroup,
	size_t iIndex
)
{
	xrt_future_combine_item* pItem = &pGroup->Items[iIndex];

	__xrtFutureCombineRef(pGroup);
	if ( !__xrtFutureWaiterAdd(pGroup->Sources[iIndex], &pItem->Waiter) ) {
		__xrtFutureCombineSourceDone(pItem);
		__xrtFutureCombineRelease(pGroup);
	}
}



/* 回收尚未暴露给调用方的创建失败上下文。 */
static void __xrtFutureCombineCreateFailed(
	xrt_future_combine* pGroup,
	xfuture* pFuture
)
{
	if ( pGroup->Watch != NULL ) {
		xrtCancelUnwatch(pGroup->Watch);
		pGroup->Watch = NULL;
	}
	if ( pGroup->Promise != NULL ) {
		xrtPromiseDestroy(pGroup->Promise);
		pGroup->Promise = NULL;
	}
	xrtFutureDestroy(pFuture);
	__xrtFutureCombineRelease(pGroup);
	__xrtFutureCombineRelease(pGroup);
}



/* 创建并装配一种 Future 组合器。 */
static xfuture* __xrtFutureCombineCreate(
	xfuture* const* pFutures,
	size_t iCount,
	xrt_future_combine_mode Mode
)
{
	xrt_future_combine* pGroup;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	size_t iBytes;

	if (
		((Mode != XRT_FUTURE_COMBINE_ALL) && (iCount == 0)) ||
		((iCount != 0) && (pFutures == NULL))
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if (
		(iCount > (size_t)(INT32_MAX - 2)) ||
		iCount >
		((SIZE_MAX - sizeof(xrt_future_combine)) /
		(sizeof(xfuture*) + sizeof(xrt_future_combine_item)))
	) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = sizeof(xrt_future_combine) +
		(iCount * (sizeof(xfuture*) + sizeof(xrt_future_combine_item)));
	pGroup = (xrt_future_combine*)xrtCalloc(1, iBytes);
	if ( pGroup == NULL ) {
		return NULL;
	}
	pGroup->RefCount = 2;
	pGroup->Mode = Mode;
	pGroup->Count = iCount;
	pGroup->Remaining = iCount;
	pGroup->Sources = iCount == 0 ? NULL : (xfuture**)(pGroup + 1);
	pGroup->Items = iCount == 0 ? NULL :
		(xrt_future_combine_item*)(pGroup->Sources + iCount);
	pGroup->All.Count = iCount;
	pGroup->All.Futures = pGroup->Sources;
	if ( !xrtMutexInit(&pGroup->Lock) ) {
		xrtFree(pGroup);
		return NULL;
	}

	for ( size_t i = 0; i < iCount; i++ ) {
		pGroup->Sources[i] = xrtFutureRef(pFutures[i]);
		if ( pGroup->Sources[i] == NULL ) {
			__xrtFutureCombineRelease(pGroup);
			__xrtFutureCombineRelease(pGroup);
			return NULL;
		}
		pGroup->Items[i].Group = pGroup;
		pGroup->Items[i].Index = i;
		pGroup->Items[i].Waiter.Proc = __xrtFutureCombineSourceDone;
		pGroup->Items[i].Waiter.Release = __xrtFutureCombineWaiterRelease;
		pGroup->Items[i].Waiter.Data = &pGroup->Items[i];
	}

	pGroup->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pGroup->Promise == NULL ) {
		__xrtFutureCombineRelease(pGroup);
		__xrtFutureCombineRelease(pGroup);
		return NULL;
	}
	if ( iCount == 0 ) {
		pGroup->Completed = true;
		if ( !xrtPromiseResolveOwned(
			pGroup->Promise,
			&pGroup->All,
			__xrtFutureCombineDestroyValue,
			pGroup
		) ) {
			__xrtFutureCombineCreateFailed(pGroup, pFuture);
			return NULL;
		}
		xrtPromiseDestroy(pGroup->Promise);
		pGroup->Promise = NULL;
		__xrtFutureCombineRelease(pGroup);
		return pFuture;
	}

	pCancel = xrtPromiseCancelToken(pGroup->Promise);
	if ( pCancel == NULL ) {
		__xrtFutureCombineCreateFailed(pGroup, pFuture);
		return NULL;
	}
	pGroup->Watch = xrtCancelWatch(pCancel, __xrtFutureCombineCancelled, pGroup);
	xrtCancelDestroy(pCancel);
	if ( pGroup->Watch == NULL ) {
		__xrtFutureCombineCreateFailed(pGroup, pFuture);
		return NULL;
	}

	if ( Mode == XRT_FUTURE_COMBINE_ALL ) {
		for ( size_t i = 0; i < iCount; i++ ) {
			__xrtFutureCombineAttach(pGroup, i);
		}
	} else {
		for ( size_t i = 0; i < iCount; i++ ) {
			if ( xrtFutureDone(pGroup->Sources[i]) ) {
				__xrtFutureCombineAttach(pGroup, i);
				break;
			}
		}
		if ( !__xrtFutureCombineDone(pGroup) ) {
			for ( size_t i = 0; i < iCount; i++ ) {
				if ( __xrtFutureCombineDone(pGroup) ) {
					break;
				}
				__xrtFutureCombineAttach(pGroup, i);
			}
		}
	}
	__xrtFutureCombineRelease(pGroup);
	return pFuture;
}



/* 创建第一个源终态选择器。 */
XRT_API xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount)
{
	return __xrtFutureCombineCreate(
		pFutures,
		iCount,
		XRT_FUTURE_COMBINE_ANY
	);
}



/* 创建等待全部源终态的保序组合器。 */
XRT_API xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount)
{
	return __xrtFutureCombineCreate(
		pFutures,
		iCount,
		XRT_FUTURE_COMBINE_ALL
	);
}



/* 创建首个终态胜出并取消其余源的竞争组合器。 */
XRT_API xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount)
{
	return __xrtFutureCombineCreate(
		pFutures,
		iCount,
		XRT_FUTURE_COMBINE_RACE
	);
}

#endif
