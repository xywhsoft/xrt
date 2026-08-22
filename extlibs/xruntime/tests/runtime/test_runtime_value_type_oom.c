#include "../test.h"
#include "runtime_value_test.h"



typedef struct testruntimevaluetypeoom {
	bool Fail;
} testruntimevaluetypeoom;



/* 按开关拒绝 Value 图复制需要的堆分配。 */
static ptr testRuntimeValueTypeOomAlloc(ptr pContext, size_t iSize)
{
	testruntimevaluetypeoom* pState = (testruntimevaluetypeoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testRuntimeValueTypeOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testruntimevaluetypeoom* pState = (testruntimevaluetypeoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放 Value 类型 OOM 测试内存。 */
static void testRuntimeValueTypeOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证 COW 复制和深克隆 OOM 都不替换目标 Value 槽。 */
int main(void)
{
	testruntimevaluetypeoom State = { false };
	xallocator Allocator = {
		&State,
		testRuntimeValueTypeOomAlloc,
		testRuntimeValueTypeOomRealloc,
		testRuntimeValueTypeOomFree
	};
	const xrttype* pType = xrtTypeValue();
	xvalue* pSource;
	xvalue* pItem;
	xvalue* pTarget;
	xvalue* pOriginal;
	xvalue* Held[TEST_RUNTIME_VALUE_EXHAUST_LIMIT];
	size_t iHeld;

	testRequire(xrtSetAllocator(&Allocator),
		"runtime Value type OOM allocator install failed");
	pSource = xrtValueArray();
	pItem = xrtValueInt(7);
	pTarget = xrtValueInt(9);
	pOriginal = pTarget;
	testRequire(
		(pSource != NULL) && (pItem != NULL) && (pTarget != NULL) &&
		xrtValueArrayAppend(pSource, pItem),
		"runtime Value type OOM fixture failed"
	);
	State.Fail = true;
	iHeld = testRuntimeValueExhaust(Held, TEST_RUNTIME_VALUE_EXHAUST_LIMIT);
	testRequire(iHeld < TEST_RUNTIME_VALUE_EXHAUST_LIMIT,
		"runtime Value type cache exhaustion did not reach OOM");
	xrtClearError();
	testRequire(
		!xrtTypeCopyValue(pType, &pTarget, &pSource) &&
		(pTarget == pOriginal) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime Value type OOM changed its initialized target"
	);
	xrtClearError();
	testRequire(
		!xrtTypeCloneValue(pType, &pTarget, &pSource) &&
		(pTarget == pOriginal) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime Value type clone OOM changed its initialized target"
	);
	State.Fail = false;
	testRuntimeValueReleaseAll(Held, iHeld);
	xrtValueRelease(pTarget);
	xrtValueRelease(pItem);
	xrtValueRelease(pSource);
	xrtClearError();
	printf("[PASS] runtime Value type OOM\n");
	return 0;
}
