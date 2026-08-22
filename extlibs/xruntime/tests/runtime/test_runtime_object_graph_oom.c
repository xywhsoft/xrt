#include "../test.h"



#define TEST_GRAPH_OOM_OBJECTS 128u



typedef struct testgraphoomstate {
	bool Fail;
	int DropCount;
} testgraphoomstate;



typedef struct testgraphoompayload {
	xrtobject* Next;
} testgraphoompayload;



static testgraphoomstate* testGraphOomState = NULL;



/* 按失败开关分配对象图测试底层内存。 */
static ptr testGraphOomAlloc(ptr pContext, size_t iSize)
{
	testgraphoomstate* pState = (testgraphoomstate*)pContext;

	return pState->Fail ? NULL : malloc(iSize);
}



/* 失败时保留原内存块。 */
static ptr testGraphOomRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testgraphoomstate* pState = (testgraphoomstate*)pContext;

	return pState->Fail ? NULL : realloc(pMemory, iSize);
}



/* 释放对象图测试底层内存。 */
static void testGraphOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 初始化环节点的空后继。 */
static bool testGraphOomInit(ptr pValue, const xrttype* pType)
{
	testgraphoompayload* pPayload = (testgraphoompayload*)pValue;
	(void)pType;

	pPayload->Next = NULL;
	return true;
}



/* 释放环后继并记录一次析构。 */
static void testGraphOomDrop(ptr pValue, const xrttype* pType)
{
	testgraphoompayload* pPayload = (testgraphoompayload*)pValue;
	(void)pType;

	xrtObjectUnref(pPayload->Next);
	pPayload->Next = NULL;
	testGraphOomState->DropCount++;
}



/* 枚举环节点唯一的强引用后继。 */
static bool testGraphOomTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testgraphoompayload* pPayload =
		(const testgraphoompayload*)pValue;
	(void)pType;

	return (pPayload->Next == NULL) || pVisit(pPayload->Next, pContext);
}



/* 验证快照 OOM 不销毁对象、不摘除成员且恢复后可完整收集。 */
int main(void)
{
	testgraphoomstate State = { false, 0 };
	xallocator Allocator = {
		&State,
		testGraphOomAlloc,
		testGraphOomRealloc,
		testGraphOomFree
	};
	static const xrtinstanceops Ops = {
		.Init = testGraphOomInit,
		.Drop = testGraphOomDrop,
		.Trace = testGraphOomTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.GraphOom")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("GraphOom"),
		.AbiName = XRT_STR_INIT("tests.runtime.GraphOom"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testgraphoompayload),
		.InstanceAlign = TEST_ALIGNOF(testgraphoompayload),
		.InstanceOps = &Ops
	};
	xrtobject* arrObject[TEST_GRAPH_OOM_OBJECTS];
	xrtobjectgraphresult Result = { 81u, 82u, 83u, 84u };
	xrtobjectgraph* pGraph;
	xrtweak Weak = { 0 };

	testGraphOomState = &State;
	testRequire(xrtSetAllocator(&Allocator),
		"failed to install object graph OOM allocator");
	pGraph = xrtObjectGraphCreate();
	testRequire(pGraph != NULL, "object graph OOM fixture graph failed");
	for ( size_t i = 0; i < TEST_GRAPH_OOM_OBJECTS; i++ ) {
		arrObject[i] = xrtObjectCreate(&Type);
		testRequire(
			(arrObject[i] != NULL) &&
			xrtObjectGraphTrack(pGraph, arrObject[i]),
			"object graph OOM fixture object failed"
		);
	}
	for ( size_t i = 0; i < TEST_GRAPH_OOM_OBJECTS; i++ ) {
		testgraphoompayload* pPayload =
			(testgraphoompayload*)xrtObjectData(arrObject[i]);

		pPayload->Next = xrtObjectRef(
			arrObject[(i + 1u) % TEST_GRAPH_OOM_OBJECTS]
		);
		testRequire(pPayload->Next != NULL,
			"object graph OOM ring retain failed");
	}
	testRequire(xrtWeakInit(&Weak, arrObject[0]),
		"object graph OOM weak fixture failed");
	for ( size_t i = 0; i < TEST_GRAPH_OOM_OBJECTS; i++ ) {
		xrtObjectUnref(arrObject[i]);
	}

	State.Fail = true;
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollect(pGraph, &Result),
		"object graph collection survived forced OOM"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Result.TrackedCount == 81u) &&
		(Result.EdgeCount == 82u) &&
		(Result.RootCount == 83u) &&
		(Result.CollectedCount == 84u) &&
		(State.DropCount == 0) &&
		!xrtWeakExpired(&Weak) &&
		(xrtObjectGraphCount(pGraph) == TEST_GRAPH_OOM_OBJECTS),
		"object graph OOM changed graph state"
	);

	State.Fail = false;
	xrtClearError();
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.TrackedCount == TEST_GRAPH_OOM_OBJECTS) &&
		(Result.EdgeCount == TEST_GRAPH_OOM_OBJECTS) &&
		(Result.CollectedCount == TEST_GRAPH_OOM_OBJECTS) &&
		(State.DropCount == (int)TEST_GRAPH_OOM_OBJECTS) &&
		xrtWeakExpired(&Weak),
		"object graph did not recover after OOM"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
	xrtClearError();
	printf("[PASS] runtime object graph OOM\n");
	return 0;
}
