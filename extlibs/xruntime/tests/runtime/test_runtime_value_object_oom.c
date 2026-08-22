#include "../test.h"
#include "runtime_value_test.h"



typedef struct testobjectoom {
	bool Fail;
	int DropCount;
} testobjectoom;



static testobjectoom* testObjectOomState = NULL;



/* 按开关允许或拒绝底层分配。 */
static ptr testObjectOomAlloc(ptr pContext, size_t iSize)
{
	testobjectoom* pState = (testobjectoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原块。 */
static ptr testObjectOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testobjectoom* pState = (testobjectoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器内存。 */
static void testObjectOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 记录对象最终析构。 */
static void testObjectOomDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	testObjectOomState->DropCount++;
}



/* 验证包装和 Take 的 OOM 不消费来源、也不泄漏临时强引用。 */
int main(void)
{
	testobjectoom State = { false, 0 };
	xallocator Allocator = {
		&State,
		testObjectOomAlloc,
		testObjectOomRealloc,
		testObjectOomFree
	};
	xrtinstanceops Ops = { .Drop = testObjectOomDrop };
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueObjectOom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueObjectOom"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueObjectOom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &Ops
	};
	xrtobject* pObject;
	xvalue* pValue;
	xvalue* Held[TEST_RUNTIME_VALUE_EXHAUST_LIMIT];
	size_t iHeld;

	testObjectOomState = &State;
	testRequire(xrtSetAllocator(&Allocator),
		"failed to install runtime Value object OOM allocator");
	pObject = xrtObjectCreate(&Type);
	testRequire(pObject != NULL, "runtime Value object OOM fixture failed");
	State.Fail = true;
	iHeld = testRuntimeValueExhaust(Held, TEST_RUNTIME_VALUE_EXHAUST_LIMIT);
	testRequire(iHeld < TEST_RUNTIME_VALUE_EXHAUST_LIMIT,
		"runtime object Value cache exhaustion did not reach OOM");
	xrtClearError();
	testRequire(xrtValueRuntimeObject(pObject) == NULL,
		"runtime object retain wrapper survived OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"runtime object wrapper OOM error mismatch");
	xrtClearError();
	testRequire(xrtValueRuntimeObjectTake(&pObject) == NULL,
		"runtime object Take survived OOM");
	testRequire(pObject != NULL,
		"runtime object Take OOM consumed source");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"runtime object Take OOM error mismatch");

	State.Fail = false;
	testRuntimeValueReleaseAll(Held, iHeld);
	xrtClearError();
	pValue = xrtValueRuntimeObjectTake(&pObject);
	testRequire((pValue != NULL) && (pObject == NULL),
		"runtime object Take did not recover from OOM");
	xrtValueRelease(pValue);
	testRequire(State.DropCount == 1,
		"runtime object OOM path leaked or double-dropped object");
	xrtClearError();
	printf("[PASS] runtime Value object OOM\n");
	return 0;
}
