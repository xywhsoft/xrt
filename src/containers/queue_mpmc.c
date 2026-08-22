#include "../internal/xrt_queue.h"



#if defined(XRT_FEATURE_QUEUE_MPMC)

/* 检查 MPMC 队列公开静态状态是否自洽。 */
static bool __xrtMPMCQueueValid(const xmpmcqueue* pQueue)
{
	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		!__xrtQueueSlotsValid(
			pQueue,
			sizeof(xmpmcqueue),
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



/* 使用已经确定的实际容量初始化 MPMC 队列。 */
static void __xrtMPMCQueueSetup(
	xmpmcqueue* pQueue,
	xqueueslot* pSlots,
	size_t iCapacity,
	ptr pAllocation
)
{
	memset(pQueue, 0, sizeof(xmpmcqueue));
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
static bool __xrtMPMCQueueAllocate(
	xmpmcqueue* pQueue,
	size_t iCapacity
)
{
	xqueueslot* pSlots = __xrtQueueSlotsAllocate(iCapacity);

	if ( pSlots == NULL ) {
		return false;
	}
	__xrtMPMCQueueSetup(pQueue, pSlots, iCapacity, pSlots);
	return true;
}



/* 初始化拥有内部序列槽环的多生产者多消费者队列。 */
XRT_API bool xrtMPMCQueueInit(xmpmcqueue* pQueue, size_t iCapacity)
{
	size_t iActualCapacity;

	if ( pQueue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pQueue, 0, sizeof(xmpmcqueue));
	iActualCapacity = __xrtQueueSequenceCapacity(iCapacity);
	if ( iActualCapacity == 0 ) {
		return false;
	}
	return __xrtMPMCQueueAllocate(pQueue, iActualCapacity);
}



/* 在调用方提供的 2 次幂序列槽环上初始化 MPMC 队列。 */
XRT_API bool xrtMPMCQueueInitBuffer(
	xmpmcqueue* pQueue,
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
	memset(pQueue, 0, sizeof(xmpmcqueue));
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
			sizeof(xmpmcqueue),
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

	__xrtMPMCQueueSetup(pQueue, pSlots, iCapacity, NULL);
	return true;
}



/* 创建拥有结构和内部序列槽环的 MPMC 队列。 */
XRT_API xmpmcqueue* xrtMPMCQueueCreate(size_t iCapacity)
{
	xmpmcqueue* pQueue;
	size_t iActualCapacity = __xrtQueueSequenceCapacity(iCapacity);

	if ( iActualCapacity == 0 ) {
		return NULL;
	}
	pQueue = (xmpmcqueue*)xrtMalloc(sizeof(xmpmcqueue));
	if ( pQueue == NULL ) {
		return NULL;
	}
	if ( !__xrtMPMCQueueAllocate(pQueue, iActualCapacity) ) {
		xrtFree(pQueue);
		return NULL;
	}
	return pQueue;
}



/* 释放拥有的序列槽环，但不释放队列结构或指针目标。 */
XRT_API void xrtMPMCQueueUnit(xmpmcqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtFree(pQueue->Allocation);
	memset(pQueue, 0, sizeof(xmpmcqueue));
}



/* 释放 Create 返回的队列结构和内部序列槽环。 */
XRT_API void xrtMPMCQueueDestroy(xmpmcqueue* pQueue)
{
	if ( pQueue == NULL ) {
		return;
	}
	xrtMPMCQueueUnit(pQueue);
	xrtFree(pQueue);
}



/* 由任意生产者尝试压入一个可为空的指针值。 */
XRT_API xqueueresult xrtMPMCQueueTryPush(xmpmcqueue* pQueue, ptr pItem)
{
	if ( !__xrtMPMCQueueValid(pQueue) ) {
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
XRT_API xqueuebatchresult xrtMPMCQueuePushBatch(
	xmpmcqueue* pQueue,
	ptr const* pItems,
	size_t iCount
)
{
	if ( !__xrtMPMCQueueValid(pQueue) ) {
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
		sizeof(xmpmcqueue),
		pQueue->Slots,
		pQueue->Capacity,
		pQueue->Mask,
		&pQueue->Closed.Value,
		&pQueue->Tail.Position.Value,
		pItems,
		iCount
	);
}



/* 由任意消费者尝试弹出一个指针值。 */
XRT_API xqueueresult xrtMPMCQueueTryPop(xmpmcqueue* pQueue, ptr* pItem)
{
	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XQUEUE_ERROR;
	}
	if ( !__xrtMPMCQueueValid(pQueue) ) {
		return XQUEUE_ERROR;
	}
	if (
		!__xrtQueueOutputValid(
			pQueue,
			sizeof(xmpmcqueue),
			pQueue->Slots,
			pQueue->Capacity * sizeof(xqueueslot),
			pItem
		)
	) {
		return XQUEUE_ERROR;
	}

	for ( ;; ) {
		uint32 iHead = __xrtAtomic32LoadValue(
			&pQueue->Head.Position.Value,
			XMEMORY_RELAXED
		);
		xqueueslot* pSlot = &pQueue->Slots[iHead & pQueue->Mask];
		int32 iDiff = __xrtQueueSlotDiff(pSlot, iHead, 1u);

		if ( iDiff == 0 ) {
			if (
				__xrtAtomic32CompareValue(
					&pQueue->Head.Position.Value,
					iHead,
					iHead + 1u,
					XMEMORY_RELAXED,
					XMEMORY_RELAXED
				) == iHead
			) {
				ptr pValue = pSlot->Item;

				pSlot->Item = NULL;
				__xrtAtomic32StoreValue(
					&pSlot->Sequence.Value,
					iHead + (uint32)pQueue->Capacity,
					XMEMORY_RELEASE
				);
				*pItem = pValue;
				return XQUEUE_OK;
			}
			__xrtAtomicPause();
			continue;
		}
		if ( iDiff > 0 ) {
			__xrtAtomicPause();
			continue;
		}
		if (
			__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) == 0u
		) {
			*pItem = NULL;
			return XQUEUE_EMPTY;
		}

		/* 关闭后重新确认槽位和头游标，避免把并发领取误判为终态。 */
		iDiff = __xrtQueueSlotDiff(pSlot, iHead, 1u);
		if ( iDiff >= 0 ) {
			__xrtAtomicPause();
			continue;
		}
		if (
			__xrtAtomic32LoadValue(
				&pQueue->Head.Position.Value,
				XMEMORY_ACQUIRE
			) != iHead
		) {
			__xrtAtomicPause();
			continue;
		}
		*pItem = NULL;
		return
			__xrtAtomic32LoadValue(
				&pQueue->Tail.Position.Value,
				XMEMORY_ACQUIRE
			) == iHead ? XQUEUE_CLOSED : XQUEUE_EMPTY;
	}
}



/* 由任意消费者尝试批量弹出连续指针值，允许部分成功。 */
XRT_API xqueuebatchresult xrtMPMCQueuePopBatch(
	xmpmcqueue* pQueue,
	ptr* pItems,
	size_t iCapacity
)
{
	if ( !__xrtMPMCQueueValid(pQueue) ) {
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}
	if ( iCapacity == 0 ) {
		return __xrtQueueBatchResult(XQUEUE_OK, 0);
	}
	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
	}

	for ( ;; ) {
		uint32 iHead = __xrtAtomic32LoadValue(
			&pQueue->Head.Position.Value,
			XMEMORY_RELAXED
		);
		size_t iReady = 0;
		int32 iDiff = 0;

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
				__xrtAtomicPause();
				continue;
			}
			if (
				__xrtAtomic32LoadValue(
					&pQueue->Closed.Value,
					XMEMORY_ACQUIRE
				) == 0u
			) {
				return __xrtQueueBatchResult(XQUEUE_EMPTY, 0);
			}

			/* 关闭后重新读取槽位，处理发布可见性和消费者竞争。 */
			iDiff = __xrtQueueSlotDiff(
				&pQueue->Slots[iHead & pQueue->Mask],
				iHead,
				1u
			);
			if ( iDiff >= 0 ) {
				__xrtAtomicPause();
				continue;
			}
			if (
				__xrtAtomic32LoadValue(
					&pQueue->Head.Position.Value,
					XMEMORY_ACQUIRE
				) != iHead
			) {
				__xrtAtomicPause();
				continue;
			}
			return __xrtQueueBatchResult(
				__xrtAtomic32LoadValue(
					&pQueue->Tail.Position.Value,
					XMEMORY_ACQUIRE
				) == iHead ? XQUEUE_CLOSED : XQUEUE_EMPTY,
				0
			);
		}
		if (
			!__xrtQueueSlotBatchValid(
				pQueue,
				sizeof(xmpmcqueue),
				pQueue->Slots,
				pQueue->Capacity,
				pItems,
				iReady
			)
		) {
			return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
		}
		if (
			__xrtAtomic32CompareValue(
				&pQueue->Head.Position.Value,
				iHead,
				iHead + (uint32)iReady,
				XMEMORY_RELAXED,
				XMEMORY_RELAXED
			) == iHead
		) {
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
			return __xrtQueueBatchResult(XQUEUE_OK, iReady);
		}
		__xrtAtomicPause();
	}
}



/* 返回包含已预留生产和消费区间的并发近似元素数量。 */
XRT_API size_t xrtMPMCQueueCount(const xmpmcqueue* pQueue)
{
	return
		__xrtMPMCQueueValid(pQueue) ?
		__xrtQueueSequenceCount(
			&pQueue->Head.Position.Value,
			&pQueue->Tail.Position.Value,
			pQueue->Capacity
		) : 0;
}



/* 在全部生产者停止后幂等关闭写入端。 */
XRT_API void xrtMPMCQueueClose(xmpmcqueue* pQueue)
{
	if ( !__xrtMPMCQueueValid(pQueue) ) {
		return;
	}
	__xrtAtomic32StoreValue(&pQueue->Closed.Value, 1u, XMEMORY_RELEASE);
}



/* 判断队列写入端是否已经关闭。 */
XRT_API bool xrtMPMCQueueIsClosed(const xmpmcqueue* pQueue)
{
	return
		__xrtMPMCQueueValid(pQueue) &&
		(__xrtAtomic32LoadValue(&pQueue->Closed.Value, XMEMORY_ACQUIRE) != 0u);
}



/* 判断队列是否已经关闭且没有尚未领取的元素。 */
XRT_API bool xrtMPMCQueueIsDrained(const xmpmcqueue* pQueue)
{
	return xrtMPMCQueueIsClosed(pQueue) && (xrtMPMCQueueCount(pQueue) == 0);
}



/* 由任意消费者排空当前可领取元素；回调为空时直接丢弃指针值。 */
XRT_API size_t xrtMPMCQueueDrain(
	xmpmcqueue* pQueue,
	xqueuedrainfn pDrain,
	ptr pContext
)
{
	size_t iCount = 0;
	ptr pItem;

	if ( !__xrtMPMCQueueValid(pQueue) ) {
		return 0;
	}
	while ( xrtMPMCQueueTryPop(pQueue, &pItem) == XQUEUE_OK ) {
		if ( pDrain != NULL ) {
			pDrain(pItem, pContext);
		}
		iCount++;
	}
	return iCount;
}



/* 在调用方独占且队列为空时重置全部序列槽并重新开放。 */
XRT_API bool xrtMPMCQueueReset(xmpmcqueue* pQueue)
{
	if ( !__xrtMPMCQueueValid(pQueue) ) {
		return false;
	}
	if ( xrtMPMCQueueCount(pQueue) != 0 ) {
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
