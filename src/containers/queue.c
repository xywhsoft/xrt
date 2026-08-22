#include "../internal/xrt_queue.h"



#if defined(XRT_FEATURE_QUEUE)

/* 把最小容量向上取整为队列可使用的 2 次幂容量。 */
XRT_API size_t xrtQueueCapacity(size_t iMinimum)
{
	size_t iCapacity;

	if ( iMinimum == 0 ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( iMinimum > XRT_QUEUE_MAX_CAPACITY ) {
		__xrtErrorSetSizeOverflow();
		return 0;
	}

	iCapacity = 1u;
	while ( iCapacity < iMinimum ) {
		iCapacity <<= 1u;
	}
	return iCapacity;
}



/* 无副作用判断实际容量是否为队列支持的 2 次幂。 */
bool __xrtQueueCapacityValid(size_t iCapacity, size_t iMinimum)
{
	return
		(iCapacity >= iMinimum) &&
		(iCapacity <= XRT_QUEUE_MAX_CAPACITY) &&
		((iCapacity & (iCapacity - 1u)) == 0);
}



/* 判断两段非空内存范围是否重叠，并拒绝地址计算回绕。 */
bool __xrtQueueMemoryRange(
	const void* pFirst,
	size_t iFirstSize,
	const void* pSecond,
	size_t iSecondSize,
	bool* pOverlaps
)
{
	uintptr_t iFirst;
	uintptr_t iSecond;

	if ( pOverlaps == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pOverlaps = false;
	if ( (iFirstSize == 0) || (iSecondSize == 0) ) {
		return true;
	}
	if ( (pFirst == NULL) || (pSecond == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	iFirst = (uintptr_t)pFirst;
	iSecond = (uintptr_t)pSecond;
	if (
		(iFirst > (UINTPTR_MAX - iFirstSize)) ||
		(iSecond > (UINTPTR_MAX - iSecondSize))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pOverlaps =
		(iFirst < (iSecond + iSecondSize)) &&
		(iSecond < (iFirst + iFirstSize));
	return true;
}



/* 检查指针数组可安全访问且不覆盖队列对象或内部存储。 */
static bool __xrtQueueBatchMemoryValid(
	const void* pQueue,
	size_t iQueueSize,
	const void* pStorage,
	size_t iStorageSize,
	const void* pItems,
	size_t iItemCount
)
{
	size_t iItemBytes;
	bool bOverlaps;

	if (
		(iItemCount == 0) ||
		(pItems == NULL) ||
		(((uintptr_t)pItems & (sizeof(ptr) - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iItemCount > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iItemBytes = iItemCount * sizeof(ptr);
	if (
		!__xrtQueueMemoryRange(
			pQueue,
			iQueueSize,
			pItems,
			iItemBytes,
			&bOverlaps
		) ||
		bOverlaps
	) {
		if ( bOverlaps ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if (
		!__xrtQueueMemoryRange(
			pStorage,
			iStorageSize,
			pItems,
			iItemBytes,
			&bOverlaps
		) ||
		bOverlaps
	) {
		if ( bOverlaps ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	return true;
}



/* 检查实际访问的指针数组没有与队列对象或指针环重叠。 */
bool __xrtQueuePointerBatchValid(
	const void* pQueue,
	size_t iQueueSize,
	const void* pStorage,
	size_t iStorageCount,
	const void* pItems,
	size_t iCount
)
{
	if ( iStorageCount > (SIZE_MAX / sizeof(ptr)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return __xrtQueueBatchMemoryValid(
		pQueue,
		iQueueSize,
		pStorage,
		iStorageCount * sizeof(ptr),
		pItems,
		iCount
	);
}



#if defined(XRT_FEATURE_QUEUE_MPSC) || defined(XRT_FEATURE_QUEUE_MPMC)

/* 初始化序列槽，使指定空环的每个逻辑位置都可由生产者预留。 */
void __xrtQueueSlotsSetup(xqueueslot* pSlots, size_t iCapacity)
{
	for ( size_t i = 0; i < iCapacity; i++ ) {
		pSlots[i].Sequence.Value = (uint32)i;
		pSlots[i].Item = NULL;
	}
}



/* 取得至少为二的序列槽环容量。 */
size_t __xrtQueueSequenceCapacity(size_t iCapacity)
{
	if ( iCapacity == 0 ) {
		return xrtQueueCapacity(0);
	}
	return xrtQueueCapacity(iCapacity < 2u ? 2u : iCapacity);
}



/* 分配一个尚未初始化序列号的槽环。 */
xqueueslot* __xrtQueueSlotsAllocate(size_t iCapacity)
{
	if ( iCapacity > (SIZE_MAX / sizeof(xqueueslot)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	return (xqueueslot*)xrtMalloc(iCapacity * sizeof(xqueueslot));
}



/* 判断序列槽存储与所有权字段是否自洽。 */
bool __xrtQueueSlotsValid(
	const void* pQueue,
	size_t iQueueSize,
	const xqueueslot* pSlots,
	ptr pAllocation,
	size_t iCapacity,
	size_t iMask
)
{
	uintptr_t iQueue;
	uintptr_t iSlots;
	size_t iBytes;

	if (
		(pQueue == NULL) ||
		(iQueueSize == 0) ||
		(pSlots == NULL) ||
		!__xrtQueueCapacityValid(iCapacity, 2u) ||
		(iCapacity > (SIZE_MAX / sizeof(xqueueslot))) ||
		(iMask != (iCapacity - 1u)) ||
		(((uintptr_t)pSlots & (sizeof(ptr) - 1u)) != 0) ||
		((pAllocation != NULL) && (pAllocation != pSlots))
	) {
		return false;
	}

	iQueue = (uintptr_t)pQueue;
	iSlots = (uintptr_t)pSlots;
	iBytes = iCapacity * sizeof(xqueueslot);
	if (
		(iQueue > (UINTPTR_MAX - iQueueSize)) ||
		(iSlots > (UINTPTR_MAX - iBytes))
	) {
		return false;
	}
	return
		(iQueue >= (iSlots + iBytes)) ||
		(iSlots >= (iQueue + iQueueSize));
}



/* 检查实际访问的指针数组没有与队列对象或序列槽环重叠。 */
bool __xrtQueueSlotBatchValid(
	const void* pQueue,
	size_t iQueueSize,
	const xqueueslot* pSlots,
	size_t iCapacity,
	const void* pItems,
	size_t iCount
)
{
	if ( iCapacity > (SIZE_MAX / sizeof(xqueueslot)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return __xrtQueueBatchMemoryValid(
		pQueue,
		iQueueSize,
		pSlots,
		iCapacity * sizeof(xqueueslot),
		pItems,
		iCount
	);
}



/* 读取槽位序列号并计算它相对指定逻辑位置的有符号代次差。 */
int32 __xrtQueueSlotDiff(
	const xqueueslot* pSlot,
	uint32 iPosition,
	uint32 iOffset
)
{
	uint32 iSequence = __xrtAtomic32LoadValue(
		&pSlot->Sequence.Value,
		XMEMORY_ACQUIRE
	);

	return (int32)(iSequence - (iPosition + iOffset));
}



/* 在已经验证的序列槽环上由任意生产者尝试压入一个元素。 */
xqueueresult __xrtQueueSequenceTryPush(
	xqueueslot* pSlots,
	size_t iMask,
	volatile uint32* pClosed,
	volatile uint32* pTail,
	ptr pItem
)
{
	for ( ;; ) {
		uint32 iTail;
		xqueueslot* pSlot;
		int32 iDiff;

		if ( __xrtAtomic32LoadValue(pClosed, XMEMORY_ACQUIRE) != 0u ) {
			return XQUEUE_CLOSED;
		}
		iTail = __xrtAtomic32LoadValue(pTail, XMEMORY_RELAXED);
		pSlot = &pSlots[iTail & iMask];
		iDiff = __xrtQueueSlotDiff(pSlot, iTail, 0u);
		if ( iDiff == 0 ) {
			if (
				__xrtAtomic32CompareValue(
					pTail,
					iTail,
					iTail + 1u,
					XMEMORY_RELAXED,
					XMEMORY_RELAXED
				) == iTail
			) {
				pSlot->Item = pItem;
				__xrtAtomic32StoreValue(
					&pSlot->Sequence.Value,
					iTail + 1u,
					XMEMORY_RELEASE
				);
				return XQUEUE_OK;
			}
			__xrtAtomicPause();
			continue;
		}
		if ( iDiff < 0 ) {
			return XQUEUE_FULL;
		}
		__xrtAtomicPause();
	}
}



/* 在已经验证的序列槽环上由任意生产者尝试批量预留和发布。 */
xqueuebatchresult __xrtQueueSequencePushBatch(
	const void* pQueue,
	size_t iQueueSize,
	xqueueslot* pSlots,
	size_t iCapacity,
	size_t iMask,
	volatile uint32* pClosed,
	volatile uint32* pTail,
	ptr const* pItems,
	size_t iCount
)
{
	for ( ;; ) {
		uint32 iTail;
		size_t iReady = 0;
		int32 iDiff = 0;

		if ( __xrtAtomic32LoadValue(pClosed, XMEMORY_ACQUIRE) != 0u ) {
			return __xrtQueueBatchResult(XQUEUE_CLOSED, 0);
		}
		iTail = __xrtAtomic32LoadValue(pTail, XMEMORY_RELAXED);
		while ( (iReady < iCount) && (iReady < iCapacity) ) {
			uint32 iPosition = iTail + (uint32)iReady;
			xqueueslot* pSlot = &pSlots[iPosition & iMask];

			iDiff = __xrtQueueSlotDiff(pSlot, iPosition, 0u);
			if ( iDiff != 0 ) {
				break;
			}
			iReady++;
		}
		if ( iReady == 0 ) {
			if ( iDiff < 0 ) {
				return __xrtQueueBatchResult(XQUEUE_FULL, 0);
			}
			__xrtAtomicPause();
			continue;
		}
		if (
			!__xrtQueueSlotBatchValid(
				pQueue,
				iQueueSize,
				pSlots,
				iCapacity,
				pItems,
				iReady
			)
		) {
			return __xrtQueueBatchResult(XQUEUE_ERROR, 0);
		}
		if (
			__xrtAtomic32CompareValue(
				pTail,
				iTail,
				iTail + (uint32)iReady,
				XMEMORY_RELAXED,
				XMEMORY_RELAXED
			) == iTail
		) {
			for ( size_t i = 0; i < iReady; i++ ) {
				uint32 iPosition = iTail + (uint32)i;
				xqueueslot* pSlot = &pSlots[iPosition & iMask];

				pSlot->Item = pItems[i];
				__xrtAtomic32StoreValue(
					&pSlot->Sequence.Value,
					iPosition + 1u,
					XMEMORY_RELEASE
				);
			}
			return __xrtQueueBatchResult(XQUEUE_OK, iReady);
		}
		__xrtAtomicPause();
	}
}



/* 返回头尾游标并发快照对应的有界近似数量。 */
size_t __xrtQueueSequenceCount(
	const volatile uint32* pHead,
	const volatile uint32* pTail,
	size_t iCapacity
)
{
	uint32 iHead = __xrtAtomic32LoadValue(pHead, XMEMORY_ACQUIRE);
	uint32 iTail = __xrtAtomic32LoadValue(pTail, XMEMORY_ACQUIRE);
	size_t iCount = (uint32)(iTail - iHead);

	return iCount < iCapacity ? iCount : iCapacity;
}

#endif

#endif
