#include "../test.h"



/* 失败分配器拒绝拥有式同步对象的堆分配。 */
static ptr testSyncOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器拒绝重分配。 */
static ptr testSyncOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 失败分配器不应收到可释放内存。 */
static void testSyncOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 验证启用的拥有式同步对象都正确传播 OOM。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testSyncOomAlloc,
		testSyncOomRealloc,
		testSyncOomFree
	};

	testRequire(xrtSetAllocator(&tAllocator), "failed to install sync OOM allocator");
	#if defined(XRT_FEATURE_MUTEX)
		testRequire(xrtMutexCreate() == NULL, "mutex create succeeded under OOM");
	#endif
	#if defined(XRT_FEATURE_COND)
		testRequire(xrtCondCreate() == NULL, "condition create succeeded under OOM");
	#endif
	#if defined(XRT_FEATURE_SEM)
		testRequire(xrtSemCreate(0, 1) == NULL, "semaphore create succeeded under OOM");
	#endif
	#if defined(XRT_FEATURE_RWLOCK)
		testRequire(xrtRWLockCreate() == NULL, "rwlock create succeeded under OOM");
	#endif
	#if defined(XRT_FEATURE_EVENT)
		testRequire(xrtEventCreate(false, false) == NULL, "event create succeeded under OOM");
	#endif
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "sync OOM error mismatch");
	printf("[PASS] sync OOM\n");
	return 0;
}
