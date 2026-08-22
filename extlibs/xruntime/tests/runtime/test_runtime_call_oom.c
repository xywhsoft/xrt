#include "../test.h"



typedef struct testcalloom {
	bool Fail;
} testcalloom;



#define TEST_CALL_OOM_HELD 4096u
#define TEST_CALL_OOM_OVERFLOW_BYTES \
	(XRT_CALL_RESULT_INLINE_COUNT * sizeof(xvalue*))



/* 按测试开关允许或拒绝底层分配。 */
static ptr testCallOomAlloc(ptr pContext, size_t iSize)
{
	testcalloom* pState = (testcalloom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留调用方原块，成功时使用系统重分配。 */
static ptr testCallOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testcalloom* pState = (testcalloom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器创建的底层内存。 */
static void testCallOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 空入口用于验证 callable 分配失败和恢复。 */
static bool testCallOomEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	(void)pResult;
	return true;
}



/* 耗尽结果溢出数组所在尺寸类，确保下一次增长进入底层 OOM。 */
static size_t testCallOomExhaust(
	testcalloom* pState,
	ptr* pHeld,
	size_t iCapacity
)
{
	size_t iCount = 0u;

	pState->Fail = true;
	while ( iCount < iCapacity ) {
		pHeld[iCount] = xrtMalloc(TEST_CALL_OOM_OVERFLOW_BYTES);
		if ( pHeld[iCount] == NULL ) {
			break;
		}
		iCount++;
	}
	testRequire(iCount < iCapacity,
		"runtime call OOM size class was not exhausted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"runtime call size class exhaustion error mismatch");
	xrtClearError();
	return iCount;
}



/* 释放尺寸类耗尽阶段暂时持有的块。 */
static void testCallOomRelease(
	testcalloom* pState,
	ptr* pHeld,
	size_t iCount
)
{
	pState->Fail = false;
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtFree(pHeld[i]);
	}
}



/* 验证创建和结果增长 OOM 均保持来源与目标状态并可以恢复。 */
int main(void)
{
	testcalloom State = { false };
	xallocator Allocator = {
		&State,
		testCallOomAlloc,
		testCallOomRealloc,
		testCallOomFree
	};
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	xrtcallable* pCallable;
	xvalue* pOverflow;
	ptr arrHeld[TEST_CALL_OOM_HELD];
	size_t iHeld;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime call OOM allocator");
	State.Fail = true;
	testRequire(
		xrtCallableCreate(NULL, testCallOomEntry, NULL, NULL) == NULL,
		"callable creation survived OOM"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"callable creation OOM error mismatch");

	State.Fail = false;
	xrtClearError();
	pCallable = xrtCallableCreate(NULL, testCallOomEntry, NULL, NULL);
	testRequire(pCallable != NULL, "callable creation did not recover from OOM");
	for ( int64 i = 0; i < 4; i++ ) {
		xvalue* pValue = xrtValueInt(i);

		testRequire(
			(pValue != NULL) && xrtCallResultPushTake(&Result, &pValue),
			"inline result setup failed"
		);
	}
	pOverflow = xrtValueInt(4);
	testRequire(pOverflow != NULL, "overflow value creation failed");
	iHeld = testCallOomExhaust(&State, arrHeld, TEST_CALL_OOM_HELD);
	testRequire(!xrtCallResultPushTake(&Result, &pOverflow),
		"result overflow growth survived OOM");
	testRequire(Result.Count == 4u,
		"result overflow OOM changed result count");
	testRequire(pOverflow != NULL,
		"result overflow OOM consumed source value");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"result overflow OOM error mismatch");

	testCallOomRelease(&State, arrHeld, iHeld);
	xrtClearError();
	testRequire(xrtCallResultPushTake(&Result, &pOverflow),
		"result overflow did not recover from OOM");
	testRequire((Result.Count == 5u) && (pOverflow == NULL),
		"result overflow recovery ownership mismatch");
	xrtCallResultUnit(&Result);
	xrtCallableUnref(pCallable);
	xrtClearError();
	printf("[PASS] runtime call OOM\n");
	return 0;
}
