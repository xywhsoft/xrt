#include "../test.h"



/* 可开关故障分配器的状态具有整个测试进程的生命周期。 */
typedef struct testpoolallocator {
	bool Fail;
	size_t AllocCalls;
	size_t ReallocCalls;
	size_t FreeCalls;
} testpoolallocator;



static testpoolallocator __testPoolAllocator;



/* 正常模式转发到 C 堆，故障模式拒绝分配。 */
static ptr testPoolAlloc(ptr pContext, size_t iSize)
{
	testpoolallocator* pState = (testpoolallocator*)pContext;

	pState->AllocCalls++;
	if ( pState->Fail ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 正常模式转发到 C 堆，故障模式拒绝重分配。 */
static ptr testPoolRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testpoolallocator* pState = (testpoolallocator*)pContext;

	pState->ReallocCalls++;
	if ( pState->Fail ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 已有内存始终允许释放，避免故障注入制造测试泄漏。 */
static void testPoolFree(ptr pContext, ptr pMemory)
{
	testpoolallocator* pState = (testpoolallocator*)pContext;

	pState->FreeCalls++;
	free(pMemory);
}



/* 安装进程生命周期的可开关故障分配器。 */
static bool testPoolInstallAllocator(void)
{
	xallocator tAllocator;

	memset(&__testPoolAllocator, 0, sizeof(__testPoolAllocator));
	tAllocator.Context = &__testPoolAllocator;
	tAllocator.Alloc = testPoolAlloc;
	tAllocator.Realloc = testPoolRealloc;
	tAllocator.Free = testPoolFree;
	return xrtSetAllocator(&tAllocator);
}



/* 验证三层池在底层分配失败时保持可销毁和可继续使用状态。 */
int main(void)
{
	xpool tPool;
	xmempool tMemoryPool;
	xmempool tStablePool;
	xpoolpage* pPage;
	ptr arrLarge[48];
	size_t iCapacity;
	size_t iDeleted;

	testRequire(testPoolInstallAllocator(), "failed to install OOM allocator");
	__testPoolAllocator.Fail = true;
	pPage = xrtPoolPageCreate(32);
	testRequire(pPage == NULL, "pool page create should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "page OOM error kind mismatch");
	xrtClearError();

	testRequire(xrtPoolInit(&tPool, 32), "allocation-free fixed pool init failed");
	testRequire(xrtPoolAlloc(&tPool) == NULL, "fixed pool alloc should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "fixed pool OOM error kind mismatch");
	xrtPoolUnit(&tPool);
	xrtClearError();

	testRequire(!xrtMemPoolInit(&tMemoryPool, 128), "variable pool init should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "variable pool OOM error kind mismatch");
	xrtClearError();

	/* 表压缩失败必须保留旧登记表、统计和全部活动块。 */
	__testPoolAllocator.Fail = false;
	testRequire(xrtMemPoolInit(&tStablePool, 16), "stable variable pool init failed");
	for ( size_t i = 0; i < 47; i++ ) {
		arrLarge[i] = xrtMemPoolAlloc(&tStablePool, 17);
		testRequire(arrLarge[i] != NULL, "stable registry setup allocation failed");
	}
	for ( size_t i = 0; i < 32; i++ ) {
		testRequire(xrtMemPoolFree(&tStablePool, arrLarge[i]), "stable registry setup free failed");
	}
	iCapacity = tStablePool.LargeCapacity;
	iDeleted = tStablePool.LargeDeleted;
	testRequire(iCapacity == 64, "stable registry capacity setup mismatch");
	testRequire(iDeleted == 32, "stable registry tombstone setup mismatch");

	__testPoolAllocator.Fail = true;
	arrLarge[47] = xrtMemPoolAlloc(&tStablePool, 17);
	testRequire(arrLarge[47] == NULL, "registry compaction should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "registry OOM error kind mismatch");
	testRequire(tStablePool.LargeCapacity == iCapacity, "registry OOM changed capacity");
	testRequire(tStablePool.LargeCount == 15, "registry OOM changed live count");
	testRequire(tStablePool.LargeDeleted == iDeleted, "registry OOM changed tombstones");
	for ( size_t i = 32; i < 47; i++ ) {
		testRequire(
			xrtMemPoolOwns(&tStablePool, arrLarge[i]),
			"registry OOM lost an existing block"
		);
	}
	xrtClearError();

	__testPoolAllocator.Fail = false;
	arrLarge[47] = xrtMemPoolAlloc(&tStablePool, 17);
	testRequire(arrLarge[47] != NULL, "registry did not recover after OOM");
	testRequire(tStablePool.LargeCapacity == iCapacity, "registry recovery grew capacity");
	testRequire(tStablePool.LargeDeleted == 0, "registry recovery did not compact tombstones");
	xrtMemPoolUnit(&tStablePool);
	printf("[PASS] pool OOM\n");
	return 0;
}
