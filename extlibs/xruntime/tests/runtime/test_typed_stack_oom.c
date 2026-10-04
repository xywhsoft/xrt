#include "../test.h"



/* 类型栈 OOM 测试状态控制后备分配器。 */
typedef struct testtypedstackoom {
	bool Fail;
	size_t Live;
	bool Probing;
	xtypedarray* ProbeSource;
	xtypedarray* ProbeTarget;
	size_t Blocked;
} testtypedstackoom;



/* 分配器也是用户代码：批次准备时不得修改来源或目标。 */
static void testTypedStackAllocatorProbe(testtypedstackoom* pState)
{
	if ( pState->Probing || !pState->ProbeSource ) return;
	pState->Probing = true;
	xerror* saved = xrtTakeError();
	(void)xrtTypedArrayCount(pState->ProbeSource);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"batch allocator could reenter source");
	xrtClearError();
	if ( pState->ProbeTarget ) {
		(void)xrtTypedArrayCount(pState->ProbeTarget);
		testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
			"batch allocator could reenter target");
		xrtClearError();
	}
	pState->Blocked++;
	xrtSetErrorTake(saved);
	pState->Probing = false;
}



static bool testTypedStackAllocationFails(testtypedstackoom* pState)
{
	testTypedStackAllocatorProbe(pState);
	return pState->Fail;
}



/* 按开关分配类型栈测试内存。 */
static ptr testTypedStackOomAlloc(ptr pContext, size_t iSize)
{
	testtypedstackoom* pState = (testtypedstackoom*)pContext;

	if ( testTypedStackAllocationFails(pState) ) return NULL;
	ptr memory = malloc(iSize);
	if ( memory ) pState->Live++;
	return memory;
}



/* 按开关重分配类型栈测试内存。 */
static ptr testTypedStackOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testtypedstackoom* pState = (testtypedstackoom*)pContext;

	if ( testTypedStackAllocationFails(pState) ) return NULL;
	bool hadMemory = pMemory != NULL;
	ptr memory = realloc(pMemory, iSize);
	if ( memory && !hadMemory ) pState->Live++;
	return memory;
}



/* 释放类型栈测试内存。 */
static void testTypedStackOomFree(ptr pContext, ptr pMemory)
{
	testtypedstackoom* pState = pContext;
	if ( pMemory ) {
		testRequire(pState->Live > 0u, "batch allocator double free");
		pState->Live--;
	}
	free(pMemory);
}



/* 真正逐点注入直到首次未命中；失败保留来源布局/值/拥有者。 */
static void testTypedStackBatchOom(testtypedstackoom* pState)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	(void)pState;
	for ( unsigned int operation = 0u; operation < 5u; operation++ ) {
		bool done = false;
		for ( size_t failAt = 1u; failAt < 64u && !done; failAt++ ) {
			xtypedstack source, target;
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)
			xstrview item = XRT_STR_INIT("a\0b");
			const xrttype* type = xrtTypeStringView();
#else
			int64 item = 37;
			const xrttype* type = xrtTypeInt64();
#endif
			xmemdebugsnapshot baseline, after;
			xrtMemDebugSnapshot(&baseline);
			testRequire(xrtTypedStackInit(&source, type) &&
				xrtTypedStackInit(&target, type), "batch OOM init");
			for ( unsigned int i = 0u; i < 3u; i++ )
				testRequire(xrtTypedStackPush(&source, &item), "batch OOM fixture");
			bytes data = source.Storage.Data;
			size_t capacity = source.Storage.Capacity;
			xrtClearError();
			testRequire(xrtMemDebugFailAfter(failAt - 1u), "batch fault setup failed");
			xtypedarray* result = NULL;
			bool ok;
			if ( operation == 0u || operation == 3u ) {
				result = xrtTypedStackPopBatch(&source, operation == 0u ? SIZE_MAX : 0u);
				ok = result != NULL;
			} else if ( operation == 1u || operation == 4u ) {
				result = xrtTypedStackPeekBatch(&source, 0u, operation == 1u ? SIZE_MAX : 0u);
				ok = result != NULL;
			} else {
				ok = xrtTypedStackPushBatch(&target, &source);
			}
			bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
			if ( hit ) {
				testRequire(!ok && !result && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
					source.Storage.Data == data && source.Storage.Capacity == capacity &&
					xrtTypedStackCount(&source) == 3u && xrtTypedStackCount(&target) == 0u,
					"batch allocation failure changed source or published partial target");
				for ( size_t i = 0u; i < 3u; i++ ) {
#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)
					const xstrview* value = xrtTypedStackConstPeek(&source, i);
					testRequire(value->Size == 3u && !memcmp(value->Data, item.Data, 3u),
						"batch OOM string value changed");
#else
					testRequire(*(const int64*)xrtTypedStackConstPeek(&source, i) == item,
						"batch OOM scalar changed");
#endif
				}
			} else {
				testRequire(ok && !xrtGetError(), "batch first no-hit path failed");
				testRequire(xrtTypedStackCount(&source) == (operation == 0u ? 0u : 3u) &&
					xrtTypedStackCount(&target) == (operation == 2u ? 3u : 0u) &&
					(!result || xrtTypedArrayCount(result) == (operation >= 3u ? 0u : 3u)),
					"batch no-hit publication count mismatch");
				done = true;
				printf("[batch OOM] operation=%u first-no-hit=%zu\n", operation, failAt);
			}
			xrtTypedArrayDestroy(result);
			xrtTypedStackUnit(&source); xrtTypedStackUnit(&target); xrtClearError();
			xrtMemDebugSnapshot(&after);
			testRequire(after.LiveCount == baseline.LiveCount &&
				after.LiveBytes == baseline.LiveBytes &&
				after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount &&
				after.InvalidFreeCount == baseline.InvalidFreeCount &&
				after.DoubleFreeCount == baseline.DoubleFreeCount &&
				after.UseAfterFreeCount == baseline.UseAfterFreeCount,
				"batch OOM logical allocation imbalance");
		}
		testRequire(done, "batch OOM scan did not reach no-hit");
	}
#else
	(void)pState;
#endif
}



static void testTypedStackBatchAllocatorReentry(testtypedstackoom* pState)
{
	xtypedstack source, target;
	int64 value = 37;
	testRequire(xrtTypedStackInit(&source, xrtTypeInt64()) &&
		xrtTypedStackInit(&target, xrtTypeInt64()),
		"batch allocator probe fixture");
	for ( unsigned int i = 0u; i < 300u; i++ )
		testRequire(xrtTypedStackPush(&source, &value), "large batch allocator fixture");
	pState->ProbeSource = &source; pState->ProbeTarget = &target;
	pState->Blocked = 0u;
	testRequire(xrtTypedStackPushBatch(&target, &source) && pState->Blocked != 0u,
		"batch append reserve had no allocator probe");
	pState->ProbeTarget = NULL;
	pState->Blocked = 0u;
	xtypedarray* result = xrtTypedStackPeekBatch(&source, 0u, SIZE_MAX);
	testRequire(result && pState->Blocked != 0u, "batch peek preparation not guarded");
	pState->ProbeSource = NULL;
	xrtTypedArrayDestroy(result);
	pState->ProbeSource = &source; pState->Blocked = 0u;
	result = xrtTypedStackPopBatch(&source, SIZE_MAX);
	testRequire(result && pState->Blocked != 0u, "batch pop preparation not guarded");
	pState->ProbeSource = NULL;
	xrtTypedArrayDestroy(result);
	xrtTypedStackUnit(&source); xrtTypedStackUnit(&target); xrtClearError();
}



/* 验证扩容 OOM 不改变来源栈。 */
int main(void)
{
	/* The installed allocator is process-wide; Windows frees its cached
	 * backing metadata from FLS after main returns. Its context must outlive
	 * main's stack frame, including those legitimate deferred frees. */
	static testtypedstackoom State;
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
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugEnable(true), "batch memory debug unavailable");
#endif
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
	testTypedStackBatchOom(&State);
	testTypedStackBatchAllocatorReentry(&State);
	printf("[PASS] typed stack OOM\n");
	return 0;
}
