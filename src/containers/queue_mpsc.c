#include "../internal/xrt_queue.h"



#if defined(XRT_FEATURE_QUEUE_MPSC)

/* 检查 MPSC 队列公开静态状态是否自洽。 */
static bool __xrtMPSCQueueValid(const xmpscqueue* pQueue)
{
	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		!__xrtQueueSlotsValid(
			pQueue,
			sizeof(xmpscqueue),
			pQueue->Slots,
			pQueue->Allocation,
			pQueue->Capacity,
			pQueue->Mask
		)
	) {
		__xrtErrorSetInvalidState();
		return false;
	}

	return true;
}



/* 使用已经确定的实际容量初始化 MPSC 队列。 */
static void __xrtMPSCQueueSetup(
	xmpscqueue* pQueue,
	xqueueslot* pSlots,
	size_t iCapacity,
	ptr pAllocation
)
{
	memset(pQueue, 0, sizeof(xmpscqueue));
	__xrtQueueSlotsSetup(pSlots, iCapacity);
	pQueue->Slots = pSlots;
	pQueue->Allocation = pAllocation;
	pQueue->Capacity = iCapacity;
	pQueue->Mask = iCapacity - 1u;
	pQueue->Closed.Value = 0u;
	pQueue->Tail.Position.Value = 0u;
	pQueue->Head.Position.Value = 0u;
}



/* 分配并初始化已经解析容量的拥有型序列槽环。 */
static bool __xrtMPSCQueueAllocate(
	xmpscqueue* pQueue,
	size_t iCapacity
)
{
	xqueueslot* pSlots;

	pSlots = __xrtQueueSlotsAllocate(iCapacity);
	if ( pSlots == NULL ) {
		return false;
	}

	__xrtMPSCQueueSetup(pQueue, pSlots, iCapacity, pSlots);
	return true;
}



/* 初始化拥有内部序列槽环的多生产者单消费者队列。 */
XRT_API bool xrtMPSCQueueInit(xmpscqueue* pQueue, size_t iCapacity)
{
	size_t iActualCapacity;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pQueue, 0, sizeof(xmpscqueue));
	iActualCapacity = __xrtQueueSequenceCapacity(iCapacity);
	if ( iActualCapacity == 0 ) {
		return false;
	}
	return __xrtMPSCQueueAllocate(pQueue, iActualCapacity);
}



/* 在调用方提供的 2 次幂序列槽环上初始化 MPSC 队列。 */
XRT_API bool xrtMPSCQueueInitBuffer(
	xmpscqueue* pQueue,
	xqueueslot* pSlots,
	size_t iCapacity
)
{
	bool bOverlaps;
	size_t iBytes;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pQueue, 0, sizeof(xmpscqueue));
	if (
		(pSlots == NULL) ||
		!__xrtQueueCapacityValid(iCapacity, 2u) ||
		(((uintptr_t)pSlots & (sizeof(ptr) - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(xqueueslot)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iBytes = iCapacity * sizeof(xqueueslot);
	if (
		!__xrtQueueMemoryRange(
			pQueue,
			sizeof(xmpscqueue),
			pSlots,
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

	__xrtMPSCQueueSetup(pQueue, pSlots, iCapacity, NULL);
	return true;
}



/* 创建拥有结构和内部序列槽环的 MPSC 队列。 */
XRT_API xmpscqueue* xrtMPSCQueueCreate(size_t iCapacity)
{
	xmpscqueue* pQueue;
	size_t iActualCapacity;

	iActualCapacity = __xrtQueueSequenceCapacity(iCapacity);
	if ( iActualCapacity == 0 ) {
		return NULL;
	}
	pQueue = (xmpscqueue*)xrtMalloc(sizeof(xmpscqueue));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !__xrtMPSCQueueAllocate(pQueue, iActualCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}

	return pQueue;
}



/* 释放拥有的序列槽环，但不释放队列结构或指针目标。 */
XRT_API void xrtMPSCQueueUnit(xmpscqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}

	xrtFree(pQueue->Allocation);
	memset(pQueue, 0, sizeof(xmpscqueue));
}



/* 释放 Create 返回的队列结构和内部序列槽环。 */
XRT_API void xrtMPSCQueueDestroy(xmpscqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}

	xrtMPSCQueueUnit(pQueue);
	xrtFree(pQueue);
}



/* 由任意生产者尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtMPSCQueueTryPush(xmpscqueue* pQueue, ptr pItem)
{
	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return XQUEUE_ERROR;
	}
	return __xrtQueueSequenceTryPush(
		pQueue->Slots,
		pQueue->Mask,
		&pQueue->Closed.Value,
		&pQueue->Tail.Position.Value,
		pItem
	);
}



/* 由任意生产者尝试批量压入连续指针值，允许部分成功。 */
XRT_API xqueuebatchresult xrtMPSCQueuePushBatch(
	xmpscqueue* pQueue,
	ptr const* pItems,
	size_t iCount
)
{
	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iCount == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_OK, 0);
	}
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	return __xrtQueueSequencePushBatch(
		pQueue,
		sizeof(xmpscqueue),
		pQueue->Slots,
		pQueue->Capacity,
		pQueue->Mask,
		&pQueue->Closed.Value,
		&pQueue->Tail.Position.Value,
		pItems,
		iCount
	);
}



/* 由唯一消费者尝试弹出一个指针值。 */
XRT_API xqueueresult xrtMPSCQueueTryPop(xmpscqueue* pQueue, ptr* pItem)
{
	uint32 iHead;
	xqueueslot* pSlot;
	int32 iDiff;

	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XQUEUE_ERROR;
	}
	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return XQUEUE_ERROR;
	}
	if (
		!__xrtQueueOutputValid(
			pQueue,
			sizeof(xmpscqueue),
			pQueue->Slots,
			pQueue->Capacity * sizeof(xqueueslot),
			pItem
		)
	) {
		return XQUEUE_ERROR;
	}

	iHead = __xrtAtomic32LoadValue(
		&pQueue->Head.Position.Value,
		XMEMORY_RELAXED
	);
	pSlot = &pQueue->Slots[iHead & pQueue->Mask];
	iDiff = __xrtQueueSlotDiff(pSlot, iHead, 1u);
	if ( iDiff < 0 ) {
		if (
			__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) == 0u
		) {
			*pItem = NULL;
			return XQUEUE_EMPTY;
		}

		/* 关闭发生在生产者全部退出后，取得关闭状态后重新读取槽位发布。 */
		iDiff = __xrtQueueSlotDiff(pSlot, iHead, 1u);
		if ( iDiff < 0 ) {
			uint32 iTail = __xrtAtomic32LoadValue(
				&pQueue->Tail.Position.Value,
				XMEMORY_ACQUIRE
			);

			*pItem = NULL;
			return iTail == iHead ? XQUEUE_CLOSED : XQUEUE_EMPTY;
		}
	}
	if ( iDiff > 0 ) {
		__xrtErrorSetInvalidState();
		return XQUEUE_ERROR;
	}

	*pItem = pSlot->Item;
	pSlot->Item = NULL;
	__xrtAtomic32StoreValue(
		&pSlot->Sequence.Value,
		iHead + (uint32)pQueue->Capacity,
		XMEMORY_RELEASE
	);
	__xrtAtomic32StoreValue(
		&pQueue->Head.Position.Value,
		iHead + 1u,
		XMEMORY_RELEASE
	);
	return XQUEUE_OK;
}



/* 由唯一消费者尝试批量弹出连续指针值，允许部分成功。 */
XRT_API xqueuebatchresult xrtMPSCQueuePopBatch(
	xmpscqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
)
{
	uint32 iHead;
	size_t iReady = 0;
	int32 iDiff = 0;

	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iCapacity == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_OK, 0);
	}
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}

	iHead = __xrtAtomic32LoadValue(
		&pQueue->Head.Position.Value,
		XMEMORY_RELAXED
	);
	while ( (iReady < iCapacity) && (iReady < pQueue->Capacity) ) {
		uint32 iPosition = iHead + (uint32)iReady;
		xqueueslot* pSlot = &pQueue->Slots[iPosition & pQueue->Mask];

		iDiff = __xrtQueueSlotDiff(pSlot, iPosition, 1u);
		if ( iDiff != 0 ) {
			break;
		}
		iReady++;
	}
	if ( iReady == 0 ) {
		if ( iDiff > 0 ) {
			__xrtErrorSetInvalidState();
			return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
		}
		if (
			__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u
		) {
			xqueueslot* pSlot = &pQueue->Slots[iHead & pQueue->Mask];

			iDiff = __xrtQueueSlotDiff(pSlot, iHead, 1u);
			if ( iDiff == 0 ) {
				iReady = 1;
			} else if ( iDiff > 0 ) {
				__xrtErrorSetInvalidState();
				return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
			} else {
				uint32 iTail = __xrtAtomic32LoadValue(
					&pQueue->Tail.Position.Value,
					XMEMORY_ACQUIRE
				);

				return __xrtQueueBatchResult(
					iTail == iHead ? XQUEUE_CLOSED : XQUEUE_EMPTY,
					0
				);
			}
		} else {
			return __xrtQueueBatchResult(XQUEUE_EMPTY, 0);
		}
	}
	if (
		!__xrtQueueSlotBatchValid(
			pQueue,
			sizeof(xmpscqueue),
			pQueue->Slots,
			pQueue->Capacity,
			pItems,
			iReady
		)
	) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}

	for ( size_t i = 0; i < iReady; i++ ) {
		uint32 iPosition = iHead + (uint32)i;
		xqueueslot* pSlot = &pQueue->Slots[iPosition & pQueue->Mask];

		pItems[i] = pSlot->Item;
		pSlot->Item = NULL;
		__xrtAtomic32StoreValue(
			&pSlot->Sequence.Value,
			iPosition + (uint32)pQueue->Capacity,
			XMEMORY_RELEASE
		);
	}
	__xrtAtomic32StoreValue(
		&pQueue->Head.Position.Value,
		iHead + (uint32)iReady,
		XMEMORY_RELEASE
	);
	return __xrtQueueBatchResult(XQUEUE_OK, iReady);
}



/* 返回包含已预留槽位的并发近似元素数量。 */
XRT_API size_t xrtMPSCQueueCount(const xmpscqueue* pQueue)
{
	return
		__xrtMPSCQueueValid(pQueue) ?
		__xrtQueueSequenceCount(
			&pQueue->Head.Position.Value,
			&pQueue->Tail.Position.Value,
			pQueue->Capacity
		) : 0;
}



/* 在全部生产者停止后幂等关闭写入端。 */
XRT_API void xrtMPSCQueueClose(xmpscqueue* pQueue)
{
	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return;
	}

	__xrtAtomic32StoreValue(&pQueue->Closed.Value, 1u, XMEMORY_RELEASE);
}



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtMPSCQueueIsClosed(const xmpscqueue* pQueue)
{
	return
		__xrtMPSCQueueValid(pQueue) &&
		(__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u);
}



/* 判断队列是否已经关闭且排空。 */
XRT_API bool xrtMPSCQueueIsDrained(const xmpscqueue* pQueue)
{
	return xrtMPSCQueueIsClosed(pQueue) && (xrtMPSCQueueCount(pQueue) == 0);
}



/* 由唯一消费者排空元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtMPSCQueueDrain(
	xmpscqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
)
{
	size_t iCount = 0;
	ptr pItem;

	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return 0;
	}
	while ( xrtMPSCQueueTryPop(pQueue, &pItem) == XQUEUE_OK ) {
		if ( pDrain != NULL ) {
			pDrain(pItem, pContext);
		}
		iCount++;
	}
	return iCount;
}



/* 在调用方独占且队列为空时重置全部序列槽并重新开放。 */
XRT_API bool xrtMPSCQueueReset(xmpscqueue* pQueue)
{
	if ( !__xrtMPSCQueueValid(pQueue) ) {
		return false;
	}
	if ( xrtMPSCQueueCount(pQueue) != 0 ) {
		__xrtErrorSetAgain();
		return false;
	}

	__xrtQueueSlotsSetup(pQueue->Slots, pQueue->Capacity);
	__xrtAtomic32StoreValue(&pQueue->Head.Position.Value, 0u, XMEMORY_RELAXED);
	__xrtAtomic32StoreValue(&pQueue->Tail.Position.Value, 0u, XMEMORY_RELAXED);
	__xrtAtomic32StoreValue(&pQueue->Closed.Value, 0u, XMEMORY_RELAXED);
	return true;
}

#endif
