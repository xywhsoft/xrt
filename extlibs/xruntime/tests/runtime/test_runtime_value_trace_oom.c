#include "../test.h"



#define TEST_RUNTIME_VALUE_TRACE_OOM_OBJECTS 256u



/* 控制追踪身份表的底层分配失败。 */
typedef struct testruntimevaluetraceoom {
	bool Fail;
} testruntimevaluetraceoom;



/* 按开关拒绝追踪测试分配。 */
static ptr testRuntimeValueTraceOomAlloc(ptr pContext, size_t iSize)
{
	testruntimevaluetraceoom* pState =
		(testruntimevaluetraceoom*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testRuntimeValueTraceOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testruntimevaluetraceoom* pState =
		(testruntimevaluetraceoom*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放追踪 OOM 测试内存。 */
static void testRuntimeValueTraceOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 接受追踪 OOM 测试已经访问的对象边。 */
static bool testRuntimeValueTraceOomVisit(
	xrtobject* pObject,
	ptr pContext
)
{
	size_t* pCount = (size_t*)pContext;

	if ( pObject == NULL ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 验证大图身份表 OOM 保留内存错误且不破坏来源图。 */
int main(void)
{
	testruntimevaluetraceoom State = { false };
	xallocator Allocator = {
		&State,
		testRuntimeValueTraceOomAlloc,
		testRuntimeValueTraceOomRealloc,
		testRuntimeValueTraceOomFree
	};
	xrttype ObjectType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueTraceOom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueTraceOom"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueTraceOom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64)
	};
	xrtobject* pObject;
	xvalue* pRoot;
	size_t iVisited = 0u;

	testRequire(xrtSetAllocator(&Allocator),
		"runtime Value trace OOM allocator install failed");
	pObject = xrtObjectCreate(&ObjectType);
	pRoot = xrtValueArray();
	testRequire((pObject != NULL) && (pRoot != NULL),
		"runtime Value trace OOM fixture allocation failed");
	for ( size_t i = 0u; i < TEST_RUNTIME_VALUE_TRACE_OOM_OBJECTS; i++ ) {
		xvalue* pItem = xrtValueRuntimeObject(pObject);

		testRequire(
			(pItem != NULL) && xrtValueArrayAppendTake(pRoot, &pItem),
			"runtime Value trace OOM fixture item failed"
		);
	}

	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtValueTraceRuntimeObjects(
			pRoot, testRuntimeValueTraceOomVisit, &iVisited
		) &&
		(iVisited < TEST_RUNTIME_VALUE_TRACE_OOM_OBJECTS) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"runtime Value trace identity OOM was not failure atomic"
	);
	State.Fail = false;
	xrtClearError();
	iVisited = 0u;
	testRequire(
		xrtValueTraceRuntimeObjects(
			pRoot, testRuntimeValueTraceOomVisit, &iVisited
		) &&
		(iVisited == TEST_RUNTIME_VALUE_TRACE_OOM_OBJECTS),
		"runtime Value trace did not recover after OOM"
	);
	xrtValueRelease(pRoot);
	xrtObjectUnref(pObject);
	xrtClearError();
	printf("[PASS] runtime Value trace OOM\n");
	return 0;
}
