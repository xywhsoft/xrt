#include "../test.h"



/* 可切换失败分配器用于拒绝组合器申请新的堆 span。 */
typedef struct testfuturecombineoom {
	bool Fail;
	size_t Calls;
} testfuturecombineoom;



/* 正常阶段转发系统堆，失败阶段拒绝新分配。 */
static ptr testFutureCombineOomAlloc(ptr pData, size_t iSize)
{
	testfuturecombineoom* pState = (testfuturecombineoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : malloc(iSize);
}



/* 测试不依赖重分配，仍保持完整分配器契约。 */
static ptr testFutureCombineOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testfuturecombineoom* pState = (testfuturecombineoom*)pData;

	pState->Calls++;
	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段创建的底层块。 */
static void testFutureCombineOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 验证组合创建 OOM 不取得源引用，也不改变源状态。 */
int main(void)
{
	enum { TEST_FUTURE_COMBINE_OOM_LIMIT = 256 };
	testfuturecombineoom tState = { false, 0 };
	xallocator tAllocator = {
		&tState,
		testFutureCombineOomAlloc,
		testFutureCombineOomRealloc,
		testFutureCombineOomFree
	};
	xfuture* pSource;
	xfuture* arrFuture[1];
	xfuture* arrCombined[TEST_FUTURE_COMBINE_OOM_LIMIT];
	xfuture* pCombined = NULL;
	xpromise* pPromise;
	size_t iCombined = 0;
	int iValue = 7;

	testRequire(xrtSetAllocator(&tAllocator),
		"failed to install future combine OOM allocator");
	pPromise = xrtPromiseCreate(&pSource, NULL);
	testRequire(pPromise != NULL, "future combine OOM source create failed");
	arrFuture[0] = pSource;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(xrtMemDebugFailAfter(0),
			"future combine logical OOM setup failed");
	#else
		tState.Fail = true;
	#endif
	xrtClearError();
	while ( iCombined < TEST_FUTURE_COMBINE_OOM_LIMIT ) {
		pCombined = xrtFutureAll(arrFuture, 1);
		if ( pCombined == NULL ) {
			break;
		}
		arrCombined[iCombined++] = pCombined;
	}
	testRequire(pCombined == NULL,
		"future combine create succeeded under OOM");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(xrtMemDebugFailTriggered(),
			"future combine logical OOM was not consumed");
		xrtMemDebugFailClear();
	#else
		tState.Fail = false;
	#endif
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"future combine OOM error mismatch");
	testRequire(xrtFutureState(pSource) == XFUTURE_PENDING,
		"future combine OOM changed source state");

	for ( size_t i = 0; i < iCombined; i++ ) {
		xrtFutureDestroy(arrCombined[i]);
	}
	testRequire(xrtPromiseResolve(pPromise, &iValue),
		"future combine OOM source resolve failed");
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pSource);
	testRequire(tState.Calls != 0,
		"future combine OOM allocator observed no allocation");

	printf("[PASS] future combine OOM\n");
	return 0;
}
