#include "../test.h"



#define TEST_VALUE_ROOT_OBJECTS 512u
#define TEST_VALUE_ROOT_INLINE 32u
#define TEST_VALUE_ROOT_GRAPH_OBJECTS 160u



/* 记录组合收集路径的分配次数并精确拒绝一个分配点。 */
typedef struct testvaluerootoom {
	size_t Calls;
	size_t FailAt;
	size_t FailFrom;
} testvaluerootoom;



/* 在指定调用点模拟一次分配失败。 */
static ptr testValueRootOomAlloc(ptr pContext, size_t iSize)
{
	testvaluerootoom* pState = (testvaluerootoom*)pContext;

	pState->Calls++;
	return (
		(pState->FailAt == pState->Calls) ||
		((pState->FailFrom != 0u) && (pState->Calls >= pState->FailFrom))
	) ? NULL : malloc(iSize);
}



/* 在指定调用点模拟一次重分配失败并保留旧内存。 */
static ptr testValueRootOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvaluerootoom* pState = (testvaluerootoom*)pContext;

	pState->Calls++;
	return (
		(pState->FailAt == pState->Calls) ||
		((pState->FailFrom != 0u) && (pState->Calls >= pState->FailFrom))
	)
		? NULL
		: realloc(pMemory, iSize);
}



/* 释放组合 OOM 测试使用的内存。 */
static void testValueRootOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 测试对象拥有一个形成自环的 Value。 */
typedef struct testvaluerootoompayload {
	xvalue* Value;
	int* DropCount;
} testvaluerootoompayload;



static int* gValueRootOomDropCount;



/* 初始化组合 OOM 测试对象。 */
static bool testValueRootOomInit(ptr pValue, const xrttype* pType)
{
	testvaluerootoompayload* pPayload =
		(testvaluerootoompayload*)pValue;
	(void)pType;

	pPayload->Value = NULL;
	pPayload->DropCount = gValueRootOomDropCount;
	return true;
}



/* 释放组合 OOM 测试对象的自环 Value。 */
static void testValueRootOomDrop(ptr pValue, const xrttype* pType)
{
	testvaluerootoompayload* pPayload =
		(testvaluerootoompayload*)pValue;
	(void)pType;

	xrtValueRelease(pPayload->Value);
	pPayload->Value = NULL;
	if ( pPayload->DropCount != NULL ) {
		(*pPayload->DropCount)++;
		pPayload->DropCount = NULL;
	}
}



/* 追踪组合 OOM 测试对象的自环 Value。 */
static bool testValueRootOomTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testvaluerootoompayload* pPayload =
		(const testvaluerootoompayload*)pValue;
	(void)pType;

	return (pPayload->Value == NULL) ||
		xrtValueTraceRuntimeObjects(pPayload->Value, pVisit, pContext);
}



/* 构造组合 OOM 测试类型。 */
static xrttype testValueRootOomType(void)
{
	static const xrtinstanceops Ops = {
		.Init = testValueRootOomInit,
		.Drop = testValueRootOomDrop,
		.Trace = testValueRootOomTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueRootsOom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueRootsOom"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueRootsOom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testvaluerootoompayload),
		.InstanceAlign = TEST_ALIGNOF(testvaluerootoompayload),
		.InstanceOps = &Ops
	};

	return Type;
}



/* 统计 Value 根追踪实际报告的运行时对象边。 */
static bool testValueRootOomVisit(xrtobject* pObject, ptr pContext)
{
	size_t* pCount = (size_t*)pContext;

	if ( pObject == NULL ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 构造需要溢出栈内追踪身份表的宽对象根。 */
static xvalue* testValueRootOomWideRoot(xrtobject* pObject)
{
	xvalue* pRoot = xrtValueArray();

	for ( size_t i = 0u; i < TEST_VALUE_ROOT_OBJECTS; i++ ) {
		xvalue* pItem = xrtValueRuntimeObject(pObject);

		testRequire(
			(pItem != NULL) &&
			xrtValueArrayAppendTake(pRoot, &pItem),
			"Value root OOM wide object fixture failed"
		);
	}
	return pRoot;
}



/* 扫描对象图快照和 Value 根追踪的全部组合分配边界。 */
int main(void)
{
	testvaluerootoom State = { 0u, 0u, 0u };
	xallocator Allocator = {
		&State,
		testValueRootOomAlloc,
		testValueRootOomRealloc,
		testValueRootOomFree
	};
	int iDropCount = 0;
	xrttype Type = testValueRootOomType();
	xrtobjectgraph* pGraph;
	xrtobjectgraphresult Result;
	xrtweak Weak = { 0 };
	xrtobject* pObject;
	xrtobject* pGraphObjects[TEST_VALUE_ROOT_GRAPH_OBJECTS];
	testvaluerootoompayload* pPayload;
	xvalue* pRoot;
	size_t iVisitCount = 0u;

	testRequire(xrtSetAllocator(&Allocator),
		"Value root OOM allocator install failed");
	gValueRootOomDropCount = &iDropCount;
	pGraph = xrtObjectGraphCreate();
	pObject = xrtObjectCreate(&Type);
	pGraphObjects[0] = pObject;
	testRequire(
		(pGraph != NULL) && (pObject != NULL) &&
		xrtObjectGraphTrack(pGraph, pObject) &&
		xrtWeakInit(&Weak, pObject),
		"Value root OOM graph fixture failed"
	);
	for ( size_t i = 1u; i < TEST_VALUE_ROOT_GRAPH_OBJECTS; i++ ) {
		pGraphObjects[i] = xrtObjectCreate(&Type);
		testRequire(
			(pGraphObjects[i] != NULL) &&
			xrtObjectGraphTrack(pGraph, pGraphObjects[i]),
			"Value root OOM wide graph fixture failed"
		);
	}
	pPayload = (testvaluerootoompayload*)xrtObjectData(pObject);
	pPayload->Value = xrtValueRuntimeObject(pObject);
	pRoot = testValueRootOomWideRoot(pObject);
	testRequire((pPayload->Value != NULL) && (pRoot != NULL),
		"Value root OOM ownership fixture failed");
	xrtObjectUnref(pObject);

	/* 宽 Value 图必须在栈内身份表溢出后报告 OOM，并可在下一次调用中恢复。 */
	State.Calls = 0u;
	State.FailAt = 1u;
	State.FailFrom = 0u;
	xrtClearError();
	testRequire(
		!xrtValueTraceRuntimeObjects(
			pRoot, testValueRootOomVisit, &iVisitCount
		) &&
		(State.Calls >= 1u) &&
		(iVisitCount == (TEST_VALUE_ROOT_INLINE - 1u)) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Value root identity overflow OOM contract failed"
	);
	State.Calls = 0u;
	State.FailAt = 0u;
	iVisitCount = 0u;
	xrtClearError();
	testRequire(
		xrtValueTraceRuntimeObjects(
			pRoot, testValueRootOomVisit, &iVisitCount
		) &&
		(iVisitCount == TEST_VALUE_ROOT_OBJECTS),
		"Value root identity overflow did not recover"
	);

	/* 前三项分别拒绝对象图快照的节点、索引和工作数组。 */
	for ( size_t i = 1u; i <= 3u; i++ ) {
		State.Calls = 0u;
		State.FailAt = i;
		State.FailFrom = 0u;
		Result.TrackedCount = 91u;
		Result.EdgeCount = 92u;
		Result.RootCount = 93u;
		Result.CollectedCount = 94u;
		xrtClearError();
		if ( xrtObjectGraphCollectValueRoot(pGraph, pRoot, &Result) ) {
			fprintf(stderr,
				"[FAIL] Value root OOM allocation %zu unexpectedly succeeded "
				"after %zu calls\n", i, State.Calls);
			return 1;
		}
		testRequire(
			(Result.TrackedCount == 91u) &&
			(Result.CollectedCount == 94u) &&
			!xrtWeakExpired(&Weak) &&
			(xrtObjectGraphCount(pGraph) == TEST_VALUE_ROOT_GRAPH_OBJECTS) &&
			(iDropCount == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
			"Value root OOM failure changed graph state"
		);
	}
	/* 快照完成后持续拒绝分配，证明适配层能够原子处理 Value 根追踪 OOM。 */
	State.Calls = 0u;
	State.FailAt = 0u;
	State.FailFrom = 4u;
	Result.TrackedCount = 91u;
	Result.EdgeCount = 92u;
	Result.RootCount = 93u;
	Result.CollectedCount = 94u;
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectValueRoot(pGraph, pRoot, &Result) &&
		(State.Calls >= 4u) &&
		(Result.TrackedCount == 91u) &&
		(Result.CollectedCount == 94u) &&
		!xrtWeakExpired(&Weak) &&
		(xrtObjectGraphCount(pGraph) == TEST_VALUE_ROOT_GRAPH_OBJECTS) &&
		(iDropCount == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Value root adapter OOM failure changed graph state"
	);
	State.Calls = 0u;
	State.FailAt = 0u;
	State.FailFrom = 0u;
	xrtClearError();
	testRequire(
		xrtObjectGraphCollectValueRoot(pGraph, pRoot, &Result) &&
		(Result.CollectedCount == 0u),
		"Value root collection did not recover after OOM"
	);
	xrtValueRelease(pRoot);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&Weak) &&
		(iDropCount == 1),
		"Value root OOM fixture did not collect after root release"
	);
	for ( size_t i = 1u; i < TEST_VALUE_ROOT_GRAPH_OBJECTS; i++ ) {
		xrtObjectUnref(pGraphObjects[i]);
	}
	testRequire(xrtObjectGraphCount(pGraph) == 0u,
		"Value root OOM fixture retained wide graph objects");
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
	xrtClearError();
	printf("[PASS] runtime Value roots OOM\n");
	return 0;
}
