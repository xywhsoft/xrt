#include "../test.h"



/* 拒绝全部底层分配，验证协程首个对象分配失败。 */
static ptr testCoroOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* OOM 分配器同样拒绝调整内存。 */
static ptr testCoroOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* OOM 分配器没有可释放对象。 */
static void testCoroOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* OOM 测试过程不会真正执行。 */
static ptr testCoroOomProc(ptr pData)
{
	return pData;
}



/* 验证协程创建在首个分配失败时保持结构化内存错误。 */
int main(void)
{
	xallocator tAllocator;

	memset(&tAllocator, 0, sizeof(tAllocator));
	tAllocator.Alloc = testCoroOomAlloc;
	tAllocator.Realloc = testCoroOomRealloc;
	tAllocator.Free = testCoroOomFree;
	testRequire(xrtSetAllocator(&tAllocator), "coroutine OOM allocator install failed");
	testRequire(xrtCoCreate(testCoroOomProc, NULL, NULL) == NULL, "coroutine OOM create succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "coroutine OOM error mismatch");

	printf("[PASS] coroutine_oom\n");
	return 0;
}
