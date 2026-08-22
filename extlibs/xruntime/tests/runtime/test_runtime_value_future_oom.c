#include "../test.h"
#include "runtime_value_test.h"



typedef struct testfutureoom {
	bool Fail;
} testfutureoom;



/* 按开关允许或拒绝底层分配。 */
static ptr testFutureOomAlloc(ptr pContext, size_t iSize)
{
	testfutureoom* pState = (testfutureoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testFutureOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testfutureoom* pState = (testfutureoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器内存。 */
static void testFutureOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证 Future 普通装箱和 Take 在 OOM 后保持来源所有权。 */
int main(void)
{
	testfutureoom State = { false };
	xallocator Allocator = {
		&State,
		testFutureOomAlloc,
		testFutureOomRealloc,
		testFutureOomFree
	};
	xfuture* pFuture = NULL;
	xpromise* pPromise;
	xvalue* pValue;
	xvalue* Held[TEST_RUNTIME_VALUE_EXHAUST_LIMIT];
	size_t iHeld;
	int iAnswer = 42;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime Future Value OOM allocator");
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	testRequire((pPromise != NULL) && (pFuture != NULL),
		"runtime Future Value OOM fixture failed");

	State.Fail = true;
	iHeld = testRuntimeValueExhaust(Held, TEST_RUNTIME_VALUE_EXHAUST_LIMIT);
	testRequire(iHeld < TEST_RUNTIME_VALUE_EXHAUST_LIMIT,
		"runtime Future Value cache exhaustion did not reach OOM");
	xrtClearError();
	testRequire(xrtValueFuture(pFuture) == NULL,
		"Future retain wrapper survived OOM");
	testRequire(xrtFutureState(pFuture) == XFUTURE_PENDING,
		"Future retain wrapper OOM damaged source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Future retain wrapper OOM error mismatch");
	xrtClearError();
	testRequire(xrtValueFutureTake(&pFuture) == NULL,
		"Future Value Take survived OOM");
	testRequire((pFuture != NULL) &&
		(xrtFutureState(pFuture) == XFUTURE_PENDING),
		"Future Value Take OOM consumed or damaged source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Future Value Take OOM error mismatch");

	State.Fail = false;
	testRuntimeValueReleaseAll(Held, iHeld);
	xrtClearError();
	pValue = xrtValueFutureTake(&pFuture);
	testRequire((pValue != NULL) && (pFuture == NULL),
		"Future Value Take did not recover from OOM");
	testRequire(
		xrtPromiseResolve(pPromise, &iAnswer) &&
		(xrtFutureValue(xrtValueGetFuture(pValue)) == &iAnswer),
		"runtime Future Value OOM recovery result mismatch"
	);
	xrtValueRelease(pValue);
	xrtPromiseDestroy(pPromise);
	xrtClearError();
	printf("[PASS] runtime Value Future OOM\n");
	return 0;
}
