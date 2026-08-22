#include "../test.h"



typedef struct testoomstate {
	bool Fail;
} testoomstate;



/* 按测试开关允许或拒绝底层分配。 */
static ptr testOomAlloc(ptr pContext, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 拒绝重分配时保留调用方原有内存。 */
static ptr testOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testoomstate* pState = (testoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放测试分配器创建的底层内存。 */
static void testOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 验证对象分配失败、长度溢出和失败后的恢复能力。 */
int main(void)
{
	testoomstate State = { false };
	xallocator Allocator = {
		&State,
		testOomAlloc,
		testOomRealloc,
		testOomFree
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.Oom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Oom"),
		.AbiName = XRT_STR_INIT("tests.runtime.Oom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtobject* pObject;

	testRequire(xrtSetAllocator(&Allocator), "failed to install OOM allocator");
	State.Fail = true;
	xrtClearError();
	testRequire(xrtObjectCreate(&Type) == NULL, "object allocation survived OOM");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"object OOM error mismatch"
	);

	State.Fail = false;
	xrtClearError();
	testRequire(
		xrtObjectCreateSized(&Type, SIZE_MAX) == NULL,
		"object allocation size overflow succeeded"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) == XOBJECT_ERROR_SIZE),
		"object allocation overflow error mismatch"
	);

	xrtClearError();
	pObject = xrtObjectCreate(&Type);
	testRequire(pObject != NULL, "object allocation did not recover after OOM");
	xrtObjectUnref(pObject);
	xrtClearError();
	printf("[PASS] runtime object OOM\n");
	return 0;
}
