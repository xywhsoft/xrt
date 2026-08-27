#ifndef XRT_INTERNAL_POOL_H
#define XRT_INTERNAL_POOL_H

#include "xrt_memory.h"



#define XRT_POOL_PAGE_FLAG_READY		0x0001u
#define XRT_POOL_PAGE_FLAG_VISITING	0x0002u
#define XRT_POOL_FLAG_READY			0x0001u
#define XRT_POOL_FLAG_VISITING		0x0002u
#define XRT_MEMPOOL_FLAG_READY		0x0001u
#define XRT_MEMPOOL_FLAG_VISITING	0x0002u
#define XRT_MEMPOOL_FLAG_PAGE_PENDING_SHIFT 26u
#define XRT_MEMPOOL_FLAG_PAGE_PENDING_MASK \
	(UINT32_C(0x3F) << XRT_MEMPOOL_FLAG_PAGE_PENDING_SHIFT)



/* 计算始终能把用户地址移出底层分配起点的对齐分配大小。 */
static inline bool __xrtPoolAlignedAllocationSize(
	size_t iPayloadSize,
	size_t iAlignment,
	size_t* pAllocationSize
)
{
	if ( iPayloadSize > (SIZE_MAX - iAlignment) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pAllocationSize = iPayloadSize + iAlignment;
	return true;
}



/* 返回底层分配起点之后的第一个对齐地址，避免池对象冒充全局堆对象。 */
static inline bool __xrtPoolAlignAfter(
	ptr pAllocation,
	size_t iAlignment,
	ptr* pMemory
)
{
	uintptr_t iAddress = (uintptr_t)pAllocation;

	if ( iAddress > (UINTPTR_MAX - iAlignment) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pMemory = (ptr)((iAddress + iAlignment) & ~((uintptr_t)iAlignment - 1u));
	return true;
}



#if defined(XRT_FEATURE_POOL_PAGE)
/* 设置带稳定域、操作和错误代码的内存池错误。 */
void __xrtPoolSetError(xerrkind Kind, int32 iCode, cstr sOperation, cstr sMessage);

#endif



#if defined(XRT_FEATURE_POOL)

/* pool.c 与 memory_pool.c 共用的页查找入口。 */
xpoolpage* __xrtPoolFindPage(const xpool* pPool, const void* pMemory);



/* 分配对象并返回所属页，供变长池同步全局页索引。 */
ptr __xrtPoolAllocObject(xpool* pPool, bool bZero, xpoolpage** ppPage);



/* 同步固定池和全部现有页的访问保护状态。 */
void __xrtPoolSetVisiting(xpool* pPool, bool bVisiting);

#endif

#endif
