#include "../test.h"



/* 可切换分配器用于验证指针栈增长失败原子性。 */
typedef struct testptrstackoom {
	bool Fail;
} testptrstackoom;



/* 在允许状态下转发到底层分配器。 */
static ptr testPtrStackAlloc(ptr pContext, size_t iSize)
{
	testptrstackoom* pState = (testptrstackoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层重分配器。 */
static ptr testPtrStackRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testptrstackoom* pState = (testptrstackoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testPtrStackFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证首次 Push 和已有数据后的大容量 Reserve 均可原子失败并恢复。 */
int main(void)
{
	testptrstackoom tState = { true };
	xallocator tAllocator = {
		&tState,
		testPtrStackAlloc,
		testPtrStackRealloc,
		testPtrStackFree
	};
	xptrstack tStack;
	int iValue = 17;
	ptr pData;
	size_t iCapacity;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install pointer stack OOM allocator"
	);
	testRequire(xrtPtrStackInit(&tStack), "pointer stack OOM init failed");

	testRequire(
		!xrtPtrStackPush(&tStack, &iValue),
		"pointer stack first push should fail under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"pointer stack first push OOM error mismatch"
	);
	testRequire(
		(tStack.Data == NULL) &&
		(tStack.Count == 0) &&
		(tStack.Capacity == 0),
		"pointer stack first push OOM changed state"
	);

	tState.Fail = false;
	testRequire(
		xrtPtrStackPush(&tStack, &iValue),
		"pointer stack OOM recovery push failed"
	);
	pData = tStack.Data;
	iCapacity = tStack.Capacity;

	/* 大容量请求绕过小块缓存，稳定命中失败分配器。 */
	tState.Fail = true;
	xrtClearError();
	testRequire(
		!xrtPtrStackReserve(&tStack, 4096),
		"pointer stack large reserve should fail under OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"pointer stack large reserve OOM error mismatch"
	);
	testRequire(
		(tStack.Data == pData) &&
		(tStack.Count == 1) &&
		(tStack.Capacity == iCapacity) &&
		(xrtPtrStackTop(&tStack) == &iValue),
		"pointer stack large reserve OOM changed live state"
	);

	tState.Fail = false;
	testRequire(
		xrtPtrStackReserve(&tStack, 4096),
		"pointer stack large reserve recovery failed"
	);
	testRequire(
		(tStack.Count == 1) &&
		(tStack.Capacity >= 4096) &&
		(xrtPtrStackTop(&tStack) == &iValue),
		"pointer stack large reserve recovery state mismatch"
	);

	xrtPtrStackUnit(&tStack);
	printf("[PASS] ptr_stack OOM\n");
	return 0;
}
