#include "../test.h"



/* 可切换分配器用于验证动态栈首次增长失败原子性。 */
typedef struct testoomstate {
	bool Fail;
} testoomstate;



/* 大元素强制增长请求绕过小块缓存，精确测试重分配失败。 */
typedef struct testlargeitem {
	unsigned char Data[4096];
} testlargeitem;



/* 在允许状态下转发到底层分配器。 */
static ptr testOomAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 在允许状态下转发到底层重分配器。 */
static ptr testOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器取得的内存。 */
static void testOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证失败压栈不改变深度、容量或数据地址。 */
int main(void)
{
	testoomstate tState = { false };
	xallocator tAllocator = { &tState, testOomAlloc, testOomRealloc, testOomFree };
	xstack tStack;
	xstack tLarge;
	int iValue = 7;
	testlargeitem tValue;
	const testlargeitem* pSource;
	ptr pData;
	size_t iCapacity;

	testRequire(xrtSetAllocator(&tAllocator), "failed to install stack OOM allocator");
	testRequire(xrtStackInit(&tStack, sizeof(int)), "OOM stack init failed");
	tState.Fail = true;
	testRequire(!xrtStackPush(&tStack, &iValue), "stack push should fail under OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "stack OOM error mismatch");
	testRequire(
		(tStack.Data == NULL) &&
		(tStack.Count == 0) &&
		(tStack.Capacity == 0),
		"stack OOM changed visible state"
	);

	tState.Fail = false;
	testRequire(xrtStackPush(&tStack, &iValue), "stack recovery push failed");
	testRequire((tStack.Count == 1) && (*(int*)xrtStackTop(&tStack) == 7), "stack recovery state mismatch");
	xrtStackUnit(&tStack);

	/* 已有数据的自引用扩容失败必须保留地址、容量、深度和内容。 */
	memset(&tValue, 0x5A, sizeof(tValue));
	testRequire(
		xrtStackInit(&tLarge, sizeof(testlargeitem)),
		"large OOM stack init failed"
	);
	testRequire(
		xrtStackPush(&tLarge, &tValue) &&
		xrtStackTrim(&tLarge),
		"large OOM stack fixture failed"
	);
	pSource = (const testlargeitem*)xrtStackConstTop(&tLarge);
	pData = tLarge.Data;
	iCapacity = tLarge.Capacity;
	tState.Fail = true;
	xrtClearError();
	testRequire(
		!xrtStackPush(&tLarge, pSource),
		"large self push should fail under OOM"
	);
	testRequire(
		(tLarge.Data == pData) &&
		(tLarge.Count == 1) &&
		(tLarge.Capacity == iCapacity) &&
		(memcmp(xrtStackConstTop(&tLarge), &tValue, sizeof(tValue)) == 0),
		"large self push OOM changed visible state"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"large self push OOM error mismatch"
	);

	tState.Fail = false;
	testRequire(
		xrtStackPush(&tLarge, pSource),
		"large self push recovery failed"
	);
	testRequire(
		(tLarge.Count == 2) &&
		(memcmp(xrtStackConstTop(&tLarge), &tValue, sizeof(tValue)) == 0),
		"large self push recovery value mismatch"
	);
	xrtStackUnit(&tLarge);
	printf("[PASS] stack OOM\n");
	return 0;
}
