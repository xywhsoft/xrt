#include "../test.h"
#include "runtime_value_test.h"



typedef struct testcallableoom {
	bool Fail;
	int DropCount;
} testcallableoom;



/* 按开关允许或拒绝底层分配。 */
static ptr testCallableOomAlloc(ptr pContext, size_t iSize)
{
	testcallableoom* pState = (testcallableoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原块。 */
static ptr testCallableOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testcallableoom* pState = (testcallableoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器内存。 */
static void testCallableOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 空动态入口。 */
static bool testCallableOomEntry(
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



/* 记录 callable 最终析构。 */
static void testCallableOomDrop(ptr pEnvironment)
{
	testcallableoom* pState = (testcallableoom*)pEnvironment;

	pState->DropCount++;
}



/* 验证 callable 包装和 Take 的 OOM 保持来源与引用计数。 */
int main(void)
{
	testcallableoom State = { false, 0 };
	xallocator Allocator = {
		&State,
		testCallableOomAlloc,
		testCallableOomRealloc,
		testCallableOomFree
	};
	xrtcallable* pCallable;
	xvalue* pValue;
	xvalue* pProgressValue;
	xvalue* Held[TEST_RUNTIME_VALUE_EXHAUST_LIMIT];
	size_t iHeld;
	xrtprogresscall ProgressCall;
	xrtprogress Progress = {
		.iSize = sizeof(Progress),
		.iVersion = XRT_PROGRESS_VERSION,
		.iInputBytes = 1u,
		.iTotalInputBytes = 2u
	};

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime callable Value OOM allocator");
	pCallable = xrtCallableCreate(
		NULL, testCallableOomEntry, &State, testCallableOomDrop);
	testRequire(pCallable != NULL, "runtime callable Value OOM fixture failed");
	pProgressValue = xrtValueCallable(pCallable);
	testRequire(pProgressValue != NULL,
		"progress callable Value OOM fixture failed");
	xrtProgressCallInit(&ProgressCall, pProgressValue);
	State.Fail = true;
	iHeld = testRuntimeValueExhaust(Held, TEST_RUNTIME_VALUE_EXHAUST_LIMIT);
	testRequire(iHeld < TEST_RUNTIME_VALUE_EXHAUST_LIMIT,
		"runtime callable Value cache exhaustion did not reach OOM");
	xrtClearError();
	testRequire(xrtValueCallable(pCallable) == NULL,
		"callable retain wrapper survived OOM");
	testRequire(State.DropCount == 0,
		"callable retain wrapper OOM dropped source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"callable retain wrapper OOM error mismatch");
	xrtClearError();
	testRequire(xrtValueCallableTake(&pCallable) == NULL,
		"callable Value Take survived OOM");
	testRequire(pCallable != NULL,
		"callable Value Take OOM consumed source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"callable Value Take OOM error mismatch");
	xrtClearError();
	testRequire(!xrtProgressCallInvoke(&Progress, &ProgressCall) &&
		ProgressCall.InvokeFailed,
		"progress callable bridge survived argument OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"progress callable bridge OOM error mismatch");

	State.Fail = false;
	testRuntimeValueReleaseAll(Held, iHeld);
	xrtValueRelease(pProgressValue);
	xrtClearError();
	pValue = xrtValueCallableTake(&pCallable);
	testRequire((pValue != NULL) && (pCallable == NULL),
		"callable Value Take did not recover from OOM");
	xrtValueRelease(pValue);
	testRequire(State.DropCount == 1,
		"callable Value OOM path leaked or double-dropped callable");
	xrtClearError();
	printf("[PASS] runtime Value callable OOM\n");
	return 0;
}
