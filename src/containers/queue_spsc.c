#include "../internal/xrt_queue.h"



#if defined(XRT_FEATURE_QUEUE_SPSC)

/* 检查 SPSC 队列公开静态状态是否自洽。 */
static bool __xrtSPSCQueueValid(const xspscqueue* pQueue)
{
	uintptr_t iItems;
	uintptr_t iQueue;
	size_t iBytes;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pQueue->Items == NULL) ||
		!__xrtQueueCapacityValid(pQueue->Capacity, 1u) ||
		(pQueue->Mask != (pQueue->Capacity - 1u)) ||
		((pQueue->Allocation != NULL) && (pQueue->Allocation != pQueue->Items))
	) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pQueue->Capacity > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	iBytes = pQueue->Capacity * sizeof(ptr);
	iItems = (uintptr_t)pQueue->Items;
	iQueue = (uintptr_t)pQueue;
	if (
		((iItems & (sizeof(ptr) - 1u)) != 0) ||
		(iItems > (UINTPTR_MAX - iBytes)) ||
		(iQueue > (UINTPTR_MAX - sizeof(xspscqueue))) ||
		!(
			(iQueue >= (iItems + iBytes)) ||
			(iItems >= (iQueue + sizeof(xspscqueue)))
		)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 使用已经确定的实际容量初始化 SPSC 队列。 */
static void __xrtSPSCQueueSetup(
	xspscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity,
	ptr pAllocation
)
{
	memset(pQueue, 0, sizeof(xspscqueue));
	memset(pItems, 0, iCapacity * sizeof(ptr));
	pQueue->Items = pItems;
	pQueue->Allocation = pAllocation;
	pQueue->Capacity = iCapacity;
	pQueue->Mask = iCapacity - 1u;
	pQueue->Closed.Value = 0u;
	pQueue->Tail.Position.Value = 0u;
	pQueue->Head.Position.Value = 0u;
}



/* 分配并初始化已经解析容量的拥有型指针环。 */
static bool __xrtSPSCQueueAllocate(
	xspscqueue* pQueue,
	size_t iCapacity
)
{
	ptr* pItems;

	if ( iCapacity > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pItems = (ptr*)xrtMalloc(iCapacity * sizeof(ptr));
	if ( pItems == NULL ) {
		return false;
	}

	__xrtSPSCQueueSetup(pQueue, pItems, iCapacity, pItems);
	return true;
}



/* 返回不超过固定容量的并发数量快照。 */
static size_t __xrtSPSCQueueCount(const xspscqueue* pQueue)
{
	uint32 iHead = __xrtAtomic32LoadValue(
		&pQueue->Head.Position.Value,
		XMEMORY_ACQUIRE
	);
	uint32 iTail = __xrtAtomic32LoadValue(
		&pQueue->Tail.Position.Value,
		XMEMORY_ACQUIRE
	);
	size_t iCount = (uint32)(iTail - iHead);

	return iCount < pQueue->Capacity ? iCount : pQueue->Capacity;
}



/* 初始化拥有内部指针环的单生产者单消费者队列。 */
XRT_API bool xrtSPSCQueueInit(xspscqueue* pQueue, size_t iCapacity)
{
	size_t iActualCapacity;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pQueue, 0, sizeof(xspscqueue));
	iActualCapacity = xrtQueueCapacity(iCapacity);
	if ( iActualCapacity == 0 ) {
		return false;
	}
	return __xrtSPSCQueueAllocate(pQueue, iActualCapacity);
}



/* 在调用方提供的 2 次幂指针环上初始化 SPSC 队列。 */
XRT_API bool xrtSPSCQueueInitBuffer(
	xspscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
)
{
	bool bOverlaps;
	size_t iBytes;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pQueue, 0, sizeof(xspscqueue));
	if (
		(pItems == NULL) ||
		!__xrtQueueCapacityValid(iCapacity, 1u) ||
		(((uintptr_t)pItems & (sizeof(ptr) - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = iCapacity * sizeof(ptr);
	if (
		!__xrtQueueMemoryRange(
			pQueue,
			sizeof(xspscqueue),
			pItems,
			iBytes,
			&bOverlaps
		) ||
		bOverlaps
	) {
		if ( bOverlaps ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	__xrtSPSCQueueSetup(pQueue, pItems, iCapacity, NULL);
	return true;
}



/* 创建拥有结构和内部指针环的 SPSC 队列。 */
XRT_API xspscqueue* xrtSPSCQueueCreate(size_t iCapacity)
{
	xspscqueue* pQueue;
	size_t iActualCapacity;

	iActualCapacity = xrtQueueCapacity(iCapacity);
	if ( iActualCapacity == 0 ) {
		return NULL;
	}
	pQueue = (xspscqueue*)xrtMalloc(sizeof(xspscqueue));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !__xrtSPSCQueueAllocate(pQueue, iActualCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}

	return pQueue;
}



/* 释放拥有的指针环，但不释放队列结构或指针目标。 */
XRT_API void xrtSPSCQueueUnit(xspscqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}

	xrtFree(pQueue->Allocation);
	memset(pQueue, 0, sizeof(xspscqueue));
}



/* 释放 Create 返回的队列结构和内部指针环。 */
XRT_API void xrtSPSCQueueDestroy(xspscqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}

	xrtSPSCQueueUnit(pQueue);
	xrtFree(pQueue);
}



/* 尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtSPSCQueueTryPush(xspscqueue* pQueue, ptr pItem)
{
	uint32 iHead;
	uint32 iTail;
	uint32 iUsed;

	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return XQUEUE_ERROR;
	}
	if (
		__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u
	) {
		return XQUEUE_CLOSED;
	}

	iTail = __xrtAtomic32LoadValue(&pQueue->Tail.Position.Value, XMEMORY_RELAXED);
	iHead = __xrtAtomic32LoadValue(&pQueue->Head.Position.Value, XMEMORY_ACQUIRE);
	iUsed = (uint32)(iTail - iHead);
	if ( iUsed > pQueue->Capacity ) {
		__xrtErrorSetInvalidState();
		return XQUEUE_ERROR;
	}
	if ( iUsed == pQueue->Capacity ) {
		return XQUEUE_FULL;
	}
	pQueue->Items[iTail & pQueue->Mask] = pItem;
	__xrtAtomic32StoreValue(
		&pQueue->Tail.Position.Value,
		iTail + 1u,
		XMEMORY_RELEASE
	);
	return XQUEUE_OK;
}



/* 尝试批量压入连续指针值，允许部分成功。 */
XRT_API xqueuebatchresult xrtSPSCQueuePushBatch(
	xspscqueue* pQueue,
	ptr const* pItems,
	size_t iCount
)
{
	uint32 iHead;
	uint32 iTail;
	uint32 iUsed;
	size_t iAvailable;
	size_t iPushed;

	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iCount == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_OK, 0);
	}
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if (
		__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u
	) {
		return __xrtQueueBatchResult(XQUEUE_CLOSED, 0);
	}

	iTail = __xrtAtomic32LoadValue(&pQueue->Tail.Position.Value, XMEMORY_RELAXED);
	iHead = __xrtAtomic32LoadValue(&pQueue->Head.Position.Value, XMEMORY_ACQUIRE);
	iUsed = (uint32)(iTail - iHead);
	if ( iUsed > pQueue->Capacity ) {
		__xrtErrorSetInvalidState();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	iAvailable = pQueue->Capacity - iUsed;
	if ( iAvailable == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_FULL, 0);
	}
	iPushed = iCount < iAvailable ? iCount : iAvailable;
	if (
		!__xrtQueuePointerBatchValid(
			pQueue,
			sizeof(xspscqueue),
			pQueue->Items,
			pQueue->Capacity,
			pItems,
			iPushed
		)
	) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	for ( size_t i = 0; i < iPushed; i++ ) {
		pQueue->Items[(iTail + (uint32)i) & pQueue->Mask] = pItems[i];
	}
	__xrtAtomic32StoreValue(
		&pQueue->Tail.Position.Value,
		iTail + (uint32)iPushed,
		XMEMORY_RELEASE
	);
	return __xrtQueueBatchResult(XQUEUE_OK, iPushed);
}



/* 尝试弹出一个指针值，空值通过结果状态消除歧义。 */
XRT_API xqueueresult xrtSPSCQueueTryPop(xspscqueue* pQueue, ptr* pItem)
{
	uint32 iHead;
	uint32 iTail;
	uint32 iAvailable;

	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XQUEUE_ERROR;
	}
	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return XQUEUE_ERROR;
	}
	if (
		!__xrtQueueOutputValid(
			pQueue,
			sizeof(xspscqueue),
			pQueue->Items,
			pQueue->Capacity * sizeof(ptr),
			pItem
		)
	) {
		return XQUEUE_ERROR;
	}

	iHead = __xrtAtomic32LoadValue(&pQueue->Head.Position.Value, XMEMORY_RELAXED);
	iTail = __xrtAtomic32LoadValue(&pQueue->Tail.Position.Value, XMEMORY_ACQUIRE);
	iAvailable = (uint32)(iTail - iHead);
	if ( iAvailable > pQueue->Capacity ) {
		__xrtErrorSetInvalidState();
		return XQUEUE_ERROR;
	}
	if ( iAvailable == 0 ) {
		if (
			__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) == 0u
		) {
			*pItem = NULL;
			return XQUEUE_EMPTY;
		}

		/* 关闭由生产者在最后一次发布后执行，取得关闭状态后必须重读尾游标。 */
		iTail = __xrtAtomic32LoadValue(
			&pQueue->Tail.Position.Value,
			XMEMORY_ACQUIRE
		);
		iAvailable = (uint32)(iTail - iHead);
		if ( iAvailable > pQueue->Capacity ) {
			__xrtErrorSetInvalidState();
			return XQUEUE_ERROR;
		}
		if ( iAvailable == 0 ) {
			*pItem = NULL;
			return XQUEUE_CLOSED;
		}
	}
	*pItem = pQueue->Items[iHead & pQueue->Mask];
	pQueue->Items[iHead & pQueue->Mask] = NULL;
	__xrtAtomic32StoreValue(
		&pQueue->Head.Position.Value,
		iHead + 1u,
		XMEMORY_RELEASE
	);
	return XQUEUE_OK;
}



/* 尝试批量弹出连续指针值，允许部分成功。 */
XRT_API xqueuebatchresult xrtSPSCQueuePopBatch(
	xspscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
)
{
	uint32 iHead;
	uint32 iTail;
	size_t iAvailable;
	size_t iPopped;

	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iCapacity == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_OK, 0);
	}
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}

	iHead = __xrtAtomic32LoadValue(&pQueue->Head.Position.Value, XMEMORY_RELAXED);
	iTail = __xrtAtomic32LoadValue(&pQueue->Tail.Position.Value, XMEMORY_ACQUIRE);
	iAvailable = (uint32)(iTail - iHead);
	if ( iAvailable > pQueue->Capacity ) {
		__xrtErrorSetInvalidState();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iAvailable == 0 ) {
		if (
			__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) == 0u
		) {
			return __xrtQueueBatchResult(XQUEUE_EMPTY, 0);
		}

		/* 与单元素弹出相同，关闭发布后重新确认最后一批元素。 */
		iTail = __xrtAtomic32LoadValue(
			&pQueue->Tail.Position.Value,
			XMEMORY_ACQUIRE
		);
		iAvailable = (uint32)(iTail - iHead);
		if ( iAvailable > pQueue->Capacity ) {
			__xrtErrorSetInvalidState();
			return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
		}
		if ( iAvailable == 0 ) {
			return __xrtQueueBatchResult(XQUEUE_CLOSED, 0);
		}
	}
	iPopped = iCapacity < iAvailable ? iCapacity : iAvailable;
	if (
		!__xrtQueuePointerBatchValid(
			pQueue,
			sizeof(xspscqueue),
			pQueue->Items,
			pQueue->Capacity,
			pItems,
			iPopped
		)
	) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	for ( size_t i = 0; i < iPopped; i++ ) {
		size_t iSlot = (iHead + (uint32)i) & pQueue->Mask;

		pItems[i] = pQueue->Items[iSlot];
		pQueue->Items[iSlot] = NULL;
	}
	__xrtAtomic32StoreValue(
		&pQueue->Head.Position.Value,
		iHead + (uint32)iPopped,
		XMEMORY_RELEASE
	);
	return __xrtQueueBatchResult(XQUEUE_OK, iPopped);
}



/* 返回并发快照下的近似元素数量。 */
XRT_API size_t xrtSPSCQueueCount(const xspscqueue* pQueue)
{
	return __xrtSPSCQueueValid(pQueue) ? __xrtSPSCQueueCount(pQueue) : 0;
}



/* 幂等关闭写入端，并允许消费者继续排空已有元素。 */
XRT_API void xrtSPSCQueueClose(xspscqueue* pQueue)
{
	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return;
	}

	__xrtAtomic32StoreValue(&pQueue->Closed.Value, 1u, XMEMORY_RELEASE);
}



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtSPSCQueueIsClosed(const xspscqueue* pQueue)
{
	return
		__xrtSPSCQueueValid(pQueue) &&
		(__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u);
}



/* 判断队列是否已经关闭且排空。 */
XRT_API bool xrtSPSCQueueIsDrained(const xspscqueue* pQueue)
{
	return xrtSPSCQueueIsClosed(pQueue) && (__xrtSPSCQueueCount(pQueue) == 0);
}



/* 排空当前可见元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtSPSCQueueDrain(
	xspscqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
)
{
	size_t iCount = 0;
	ptr pItem;

	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return 0;
	}
	while ( xrtSPSCQueueTryPop(pQueue, &pItem) == XQUEUE_OK ) {
		if ( pDrain != NULL ) {
			pDrain(pItem, pContext);
		}
		iCount++;
	}
	return iCount;
}



/* 在调用方独占且队列为空时重置游标并重新开放。 */
XRT_API bool xrtSPSCQueueReset(xspscqueue* pQueue)
{
	uint32 iHead;
	uint32 iTail;
	uint32 iCount;

	if ( !__xrtSPSCQueueValid(pQueue) ) {
		return false;
	}
	iHead = __xrtAtomic32LoadValue(
		&pQueue->Head.Position.Value,
		XMEMORY_ACQUIRE
	);
	iTail = __xrtAtomic32LoadValue(
		&pQueue->Tail.Position.Value,
		XMEMORY_ACQUIRE
	);
	iCount = (uint32)(iTail - iHead);
	if ( iCount > pQueue->Capacity ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( iCount != 0 ) {
		__xrtErrorSetAgain();
		return false;
	}

	memset(pQueue->Items, 0, pQueue->Capacity * sizeof(ptr));
	__xrtAtomic32StoreValue(&pQueue->Head.Position.Value, 0u, XMEMORY_RELAXED);
	__xrtAtomic32StoreValue(&pQueue->Tail.Position.Value, 0u, XMEMORY_RELAXED);
	__xrtAtomic32StoreValue(&pQueue->Closed.Value, 0u, XMEMORY_RELAXED);
	return true;
}

#endif
