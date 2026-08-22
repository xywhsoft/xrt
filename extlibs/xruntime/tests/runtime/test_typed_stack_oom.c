#include "../test.h"



/* 类型栈 OOM 测试状态控制后备分配器。 */
typedef struct testtypedstackoom {
	bool Fail;
} testtypedstackoom;



/* 按开关分配类型栈测试内存。 */
static ptr testTypedStackOomAlloc(ptr pContext, size_t iSize)
{
	testtypedstackoom* pState = (testtypedstackoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 按开关重分配类型栈测试内存。 */
static ptr testTypedStackOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedstackoom* pState = (testtypedstackoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放类型栈测试内存。 */
static void testTypedStackOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证扩容 OOM 不改变来源栈。 */
int main(void)
{
	testtypedstackoom State = { false };
	xallocator Allocator = {
		&State,
		testTypedStackOomAlloc,
		testTypedStackOomRealloc,
		testTypedStackOomFree
	};
	xtypedstack Stack;
	bytes pData;
	size_t iCapacity;
	int64 iValue = 37;

	testRequire(
		xrtSetAllocator(&Allocator),
		"typed stack OOM allocator install failed"
	);
	testRequire(
		xrtTypedStackInit(&Stack, xrtTypeInt64()) &&
		xrtTypedStackPush(&Stack, &iValue),
		"typed stack OOM fixture failed"
	);
	pData = Stack.Storage.Data;
	iCapacity = Stack.Storage.Capacity;
	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtTypedStackReserve(&Stack, 1024u * 1024u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Stack.Storage.Data == pData) &&
		(Stack.Storage.Capacity == iCapacity) &&
		(xrtTypedStackCount(&Stack) == 1u) &&
		(*(const int64*)xrtTypedStackConstTop(&Stack) == iValue),
		"typed stack reserve OOM changed visible state"
	);
	State.Fail = false;
	xrtTypedStackUnit(&Stack);
	xrtClearError();
	printf("[PASS] typed stack OOM\n");
	return 0;
}
