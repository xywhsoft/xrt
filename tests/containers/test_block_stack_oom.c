#include "../test.h"



/* 可指定成功分配次数的分块栈失败注入器。 */
typedef struct testblockoom {
	size_t Calls;
	size_t Succeed;
	size_t Frees;
} testblockoom;



/* 在成功额度耗尽前转发分配，之后稳定失败。 */
static ptr testBlockAlloc(ptr pContext, size_t iSize)
{
	testblockoom* pState = (testblockoom*)pContext;

	if ( pState->Calls++ >= pState->Succeed ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 分块栈测试不依赖重分配，但保留完整分配器接口。 */
static ptr testBlockRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testblockoom* pState = (testblockoom*)pContext;

	if ( pState->Calls++ >= pState->Succeed ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 统计所有已取得临时块和索引内存的回收。 */
static void testBlockFree(ptr pContext, ptr pMemory)
{
	testblockoom* pState = (testblockoom*)pContext;

	pState->Frees++;
	free(pMemory);
}



/* 验证块分配和块索引分配失败都不改变原栈合同。 */
int main(void)
{
	testblockoom tState = { 0, 2, 0 };
	xallocator tAllocator = {
		&tState,
		testBlockAlloc,
		testBlockRealloc,
		testBlockFree
	};
	xblockstack tStack;
	unsigned char pValue[2048] = { 7 };
	unsigned char* pStable;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot tBefore;
		xmemdebugsnapshot tAfter;
	#else
	size_t iFrees;
	#endif

	testRequire(xrtSetAllocator(&tAllocator), "failed to install block stack OOM allocator");
	testRequire(
		xrtBlockStackInitLayout(&tStack, sizeof(pValue), 1, 1),
		"block stack OOM init failed"
	);

	/*
	 * 大于小对象阈值的块逐个走底层分配：
	 * 两个临时块成功、块索引 span 分配失败时必须完整回滚。
	 */
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tBefore);
	#endif
	testRequire(!xrtBlockStackReserve(&tStack, 2), "block stack descriptor OOM should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "block stack descriptor OOM mismatch");
	testRequire(
		(tStack.Blocks.Data == NULL) &&
		(tStack.Blocks.Count == 0) &&
		(tStack.Capacity == 0),
		"block stack descriptor OOM changed state"
	);
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tAfter);
		testRequire(
			(tAfter.LiveCount == tBefore.LiveCount) &&
			(tAfter.LiveBytes == tBefore.LiveBytes),
			"block stack descriptor OOM retained temporary blocks"
		);
	#else
		testRequire(tState.Frees == 2, "block stack descriptor OOM leaked temporary blocks");
	#endif

	/* 建立一个有效块，再在后续多块预留中间失败。 */
	tState.Calls = 0;
	tState.Succeed = SIZE_MAX;
	testRequire(xrtBlockStackPush(&tStack, pValue), "block stack OOM recovery push failed");
	pStable = (unsigned char*)xrtBlockStackTop(&tStack);
	testRequire((pStable != NULL) && (pStable[0] == 7), "block stack recovery value mismatch");

	tState.Calls = 0;
	tState.Succeed = 1;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tBefore);
	#else
		iFrees = tState.Frees;
	#endif
	xrtClearError();
	testRequire(!xrtBlockStackReserve(&tStack, 4), "block stack partial block OOM should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "block stack partial OOM mismatch");
	testRequire(
		(tStack.Count == 1) &&
		(tStack.Capacity == 1) &&
		(tStack.Blocks.Count == 1) &&
		(xrtBlockStackTop(&tStack) == pStable),
		"block stack partial OOM changed live state"
	);
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xrtMemDebugSnapshot(&tAfter);
		testRequire(
			(tAfter.LiveCount == tBefore.LiveCount) &&
			(tAfter.LiveBytes == tBefore.LiveBytes),
			"block stack partial OOM retained temporary blocks"
		);
	#else
		testRequire(tState.Frees == (iFrees + 1u), "block stack partial OOM rollback mismatch");
	#endif

	tState.Succeed = SIZE_MAX;
	xrtBlockStackUnit(&tStack);
	printf("[PASS] block_stack OOM\n");
	return 0;
}
