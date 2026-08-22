#include "../test.h"



/* 失败分配器拒绝线程管理对象的唯一堆分配。 */
static ptr testThreadOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器拒绝重分配。 */
static ptr testThreadOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 失败分配器不应收到可释放内存。 */
static void testThreadOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 不会被执行的线程入口。 */
static int32 testThreadOomEntry(ptr pData)
{
	(void)pData;
	return 0;
}



/* 验证线程对象分配失败不会创建原生线程。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testThreadOomAlloc,
		testThreadOomRealloc,
		testThreadOomFree
	};

	testRequire(xrtSetAllocator(&tAllocator), "failed to install thread OOM allocator");
	testRequire(
		xrtThreadCreate(testThreadOomEntry, NULL, 0) == NULL,
		"thread create succeeded under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"thread OOM error mismatch"
	);
	printf("[PASS] thread OOM\n");
	return 0;
}
