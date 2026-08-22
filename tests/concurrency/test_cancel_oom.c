#include "../test.h"



/* 失败分配器拒绝取消令牌的首次堆分配。 */
static ptr testCancelOomAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	return NULL;
}



/* 失败分配器不支持重分配。 */
static ptr testCancelOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	(void)pMemory;
	(void)iSize;
	return NULL;
}



/* 失败分配器不会收到可释放内存。 */
static void testCancelOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	(void)pMemory;
}



/* 验证取消令牌创建完整传播 OOM。 */
int main(void)
{
	xallocator tAllocator = {
		NULL,
		testCancelOomAlloc,
		testCancelOomRealloc,
		testCancelOomFree
	};

	testRequire(xrtSetAllocator(&tAllocator), "failed to install cancel OOM allocator");
	testRequire(xrtCancelCreate() == NULL, "cancel create succeeded under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "cancel OOM error mismatch");

	printf("[PASS] cancel OOM\n");
	return 0;
}
