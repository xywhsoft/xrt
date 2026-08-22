#include "../internal/xrt_memory.h"



/* 使用 C 运行库分配内存。 */
static ptr __xrtSystemAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	return malloc(iSize);
}



/* 使用 C 运行库调整内存。 */
static ptr __xrtSystemRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	return realloc(pMemory, iSize);
}



/* 使用 C 运行库释放内存。 */
static void __xrtSystemFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 默认分配器在第一次原始分配后不可替换。 */
static xallocator __xrtAllocator = {
	NULL,
	__xrtSystemAlloc,
	__xrtSystemRealloc,
	__xrtSystemFree
};
static bool __xrtAllocatorLocked = false;
#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static xrt_spinlock __xrtAllocatorLock = { PTHREAD_MUTEX_INITIALIZER };
#else
static xrt_spinlock __xrtAllocatorLock = { 0 };
#endif



/* 在首次分配前替换进程级底层分配器。 */
XRT_API bool xrtSetAllocator(const xallocator* pAllocator)
{
	if ( (pAllocator == NULL) ||
		 (pAllocator->Alloc == NULL) ||
		 (pAllocator->Realloc == NULL) ||
		 (pAllocator->Free == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtSpinLock(&__xrtAllocatorLock);
	if ( __xrtAllocatorLocked ) {
		__xrtSpinUnlock(&__xrtAllocatorLock);
		__xrtErrorSetInvalidState();
		return false;
	}

	__xrtAllocator = *pAllocator;
	__xrtSpinUnlock(&__xrtAllocatorLock);
	return true;
}



/* 复制当前进程级底层分配器。 */
XRT_API void xrtGetAllocator(xallocator* pAllocator)
{
	if ( pAllocator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}

	__xrtSpinLock(&__xrtAllocatorLock);
	*pAllocator = __xrtAllocator;
	__xrtSpinUnlock(&__xrtAllocatorLock);
}



/* 锁定底层分配器并申请原始内存。 */
ptr __xrtBackingAlloc(size_t iSize)
{
	xallocator Allocator;
	ptr pMemory;

	__xrtSpinLock(&__xrtAllocatorLock);
	__xrtAllocatorLocked = true;
	Allocator = __xrtAllocator;
	__xrtSpinUnlock(&__xrtAllocatorLock);
	pMemory = Allocator.Alloc(Allocator.Context, iSize);
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_BACKING_ALLOC, iSize);
	return pMemory;
}



/* 使用底层分配器调整原始内存。 */
ptr __xrtBackingRealloc(ptr pMemory, size_t iSize)
{
	xallocator Allocator;
	ptr pResult;

	__xrtSpinLock(&__xrtAllocatorLock);
	__xrtAllocatorLocked = true;
	Allocator = __xrtAllocator;
	__xrtSpinUnlock(&__xrtAllocatorLock);
	pResult = Allocator.Realloc(Allocator.Context, pMemory, iSize);
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_BACKING_REALLOC, iSize);
	return pResult;
}



/* 使用底层分配器释放原始内存。 */
void __xrtBackingFree(ptr pMemory)
{
	xallocator Allocator;

	if ( pMemory != NULL ) {
		__xrtSpinLock(&__xrtAllocatorLock);
		Allocator = __xrtAllocator;
		__xrtSpinUnlock(&__xrtAllocatorLock);
		Allocator.Free(Allocator.Context, pMemory);
		__xrtMemStatsRecord(XRT_MEM_STATS_OP_BACKING_FREE, 0);
	}
}
