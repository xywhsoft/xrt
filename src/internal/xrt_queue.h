#ifndef XRT_INTERNAL_QUEUE_H
#define XRT_INTERNAL_QUEUE_H

#include "xrt_atomic.h"



#if defined(XRT_FEATURE_QUEUE)

/* 无副作用判断实际容量是否为队列支持的 2 次幂。 */
bool __xrtQueueCapacityValid(size_t iCapacity, size_t iMinimum);



/* 判断两段非空内存范围是否重叠，并拒绝地址计算回绕。 */
bool __xrtQueueMemoryRange(
	const void* pFirst,
	size_t iFirstSize,
	const void* pSecond,
	size_t iSecondSize,
	bool* pOverlaps
);



/* 检查指针批量数组的对齐、地址范围以及与队列和指针环的重叠。 */
bool __xrtQueuePointerBatchValid(
	const void* pQueue,
	size_t iQueueSize,
	const void* pStorage,
	size_t iStorageCount,
	const void* pItems,
	size_t iCount
);



/* 构造批量操作结果。 */
static inline xqueuebatchresult __xrtQueueBatchResult(
	xqueueresult iResult,
	size_t iCount
)
{
	xqueuebatchresult Result;

	Result.Result = iResult;
	Result.Count = iCount;
	return Result;
}



/* 快速检查单个 Pop 输出不会覆盖队列对象或内部存储。 */
static inline bool __xrtQueueOutputValid(
	const void* pQueue,
	size_t iQueueSize,
	const void* pStorage,
	size_t iStorageSize,
	const void* pOutput
)
{
	uintptr_t iOutput = (uintptr_t)pOutput;
	uintptr_t iOutputEnd;
	uintptr_t iQueue = (uintptr_t)pQueue;
	uintptr_t iStorage = (uintptr_t)pStorage;

	if (
		(pOutput == NULL) ||
		((iOutput & (sizeof(ptr) - 1u)) != 0) ||
		(iOutput > (UINTPTR_MAX - sizeof(ptr)))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iOutputEnd = iOutput + sizeof(ptr);
	if (
		((iOutput < (iQueue + iQueueSize)) &&
		 (iQueue < iOutputEnd)) ||
		((iOutput < (iStorage + iStorageSize)) &&
		 (iStorage < iOutputEnd))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



#if defined(XRT_FEATURE_QUEUE_MPSC) || defined(XRT_FEATURE_QUEUE_MPMC)

/* 初始化序列槽，使指定空环的每个逻辑位置都可由生产者预留。 */
void __xrtQueueSlotsSetup(xqueueslot* pSlots, size_t iCapacity);



/* 取得至少为二的序列槽环容量。 */
size_t __xrtQueueSequenceCapacity(size_t iCapacity);



/* 分配一个尚未初始化序列号的槽环。 */
xqueueslot* __xrtQueueSlotsAllocate(size_t iCapacity);



/* 判断序列槽存储与所有权字段是否自洽。 */
bool __xrtQueueSlotsValid(
	const void* pQueue,
	size_t iQueueSize,
	const xqueueslot* pSlots,
	ptr pAllocation,
	size_t iCapacity,
	size_t iMask
);



/* 检查指针批量数组的对齐、地址范围以及与队列和序列槽环的重叠。 */
bool __xrtQueueSlotBatchValid(
	const void* pQueue,
	size_t iQueueSize,
	const xqueueslot* pSlots,
	size_t iCapacity,
	const void* pItems,
	size_t iCount
);



/* 读取槽位序列号并计算它相对指定逻辑位置的有符号代次差。 */
int32 __xrtQueueSlotDiff(
	const xqueueslot* pSlot,
	uint32 iPosition,
	uint32 iOffset
);



/* 在已经验证的序列槽环上由任意生产者尝试压入一个元素。 */
xqueueresult __xrtQueueSequenceTryPush(
	xqueueslot* pSlots,
	size_t iMask,
	volatile uint32* pClosed,
	volatile uint32* pTail,
	ptr pItem
);



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
);



/* 返回头尾游标并发快照对应的有界近似数量。 */
size_t __xrtQueueSequenceCount(
	const volatile uint32* pHead,
	const volatile uint32* pTail,
	size_t iCapacity
);

#endif

#endif

#endif
