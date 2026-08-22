#include "../test.h"



/* 失败分配器拒绝 Future/Promise 对的首次堆分配。 */
static ptr testFutureOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器不支持重分配。 */
static ptr testFutureOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 首次分配失败时不会产生可释放内存。 */
static void testFutureOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 验证 Future 创建完整传播 OOM 且清空输出端点。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testFutureOomAlloc,
		testFutureOomRealloc,
		testFutureOomFree
	};
	xfuture* pFuture = (xfuture*)(uintptr_t)1;

	testRequire(xrtSetAllocator(&tAllocator), "failed to install future OOM allocator");
	testRequire(xrtPromiseCreate(&pFuture, NULL) == NULL,
		"future create succeeded under OOM");
	testRequire(pFuture == NULL, "future OOM did not clear output endpoint");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "future OOM error mismatch");

	printf("[PASS] future OOM\n");
	return 0;
}
