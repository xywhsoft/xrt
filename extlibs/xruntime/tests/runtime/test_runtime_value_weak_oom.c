#include "../test.h"
#include "runtime_value_test.h"



typedef struct testweakoom {
	bool Fail;
} testweakoom;



/* 按开关允许或拒绝底层分配。 */
static ptr testWeakOomAlloc(ptr pContext, size_t iSize)
{
	testweakoom* pState = (testweakoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原块。 */
static ptr testWeakOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testweakoom* pState = (testweakoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器内存。 */
static void testWeakOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证弱引用包装和 Take 的 OOM 不消费控制块引用。 */
int main(void)
{
	testweakoom State = { false };
	xallocator Allocator = {
		&State,
		testWeakOomAlloc,
		testWeakOomRealloc,
		testWeakOomFree
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueWeakOom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueWeakOom"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueWeakOom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtobject* pObject;
	xrtweak Weak = { 0 };
	xvalue* pValue;
	xvalue* Held[TEST_RUNTIME_VALUE_EXHAUST_LIMIT];
	size_t iHeld;

	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime weak Value OOM allocator");
	pObject = xrtObjectCreate(&Type);
	testRequire(
		(pObject != NULL) && xrtWeakInit(&Weak, pObject),
		"runtime weak Value OOM fixture failed"
	);
	State.Fail = true;
	iHeld = testRuntimeValueExhaust(Held, TEST_RUNTIME_VALUE_EXHAUST_LIMIT);
	testRequire(iHeld < TEST_RUNTIME_VALUE_EXHAUST_LIMIT,
		"runtime weak Value cache exhaustion did not reach OOM");
	xrtClearError();
	testRequire(xrtValueWeak(&Weak) == NULL,
		"weak Value copy wrapper survived OOM");
	testRequire(!xrtWeakExpired(&Weak),
		"weak Value copy OOM changed source");
	xrtClearError();
	testRequire(xrtValueWeakTake(&Weak) == NULL,
		"weak Value Take survived OOM");
	testRequire(Weak.Control != NULL,
		"weak Value Take OOM consumed source");

	State.Fail = false;
	testRuntimeValueReleaseAll(Held, iHeld);
	xrtClearError();
	pValue = xrtValueWeakTake(&Weak);
	testRequire((pValue != NULL) && (Weak.Control == NULL),
		"weak Value Take did not recover from OOM");
	xrtObjectUnref(pObject);
	testRequire(xrtValueWeakExpired(pValue),
		"recovered weak Value did not expire");
	xrtValueRelease(pValue);
	xrtClearError();
	printf("[PASS] runtime Value weak OOM\n");
	return 0;
}
