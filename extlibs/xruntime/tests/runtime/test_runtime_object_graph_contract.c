#include "../test.h"



typedef enum testgraphtracemode {
	TEST_GRAPH_TRACE_NORMAL,
	TEST_GRAPH_TRACE_SUCCESS_ERROR,
	TEST_GRAPH_TRACE_OVERREPORT,
	TEST_GRAPH_TRACE_ERROR,
	TEST_GRAPH_TRACE_SILENT,
	TEST_GRAPH_TRACE_NULL
} testgraphtracemode;



typedef struct testgraphcontractpayload {
	xrtobject* Self;
	int* DropCount;
} testgraphcontractpayload;



static testgraphtracemode testGraphTraceMode = TEST_GRAPH_TRACE_NORMAL;
static int* testGraphContractDropCount = NULL;



/* 安装一个由测试回调产生的临时错误。 */
static void testGraphContractTemporaryError(cstr sDomain, int32 iCode)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO, sDomain, iCode, "temporary callback error"
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化失败契约测试使用的自引用负载。 */
static bool testGraphContractInit(ptr pValue, const xrttype* pType)
{
	testgraphcontractpayload* pPayload = (testgraphcontractpayload*)pValue;
	(void)pType;

	pPayload->Self = NULL;
	pPayload->DropCount = testGraphContractDropCount;
	return true;
}



/* 释放自引用并记录对象只被析构一次。 */
static void testGraphContractDrop(ptr pValue, const xrttype* pType)
{
	testgraphcontractpayload* pPayload = (testgraphcontractpayload*)pValue;
	(void)pType;

	xrtObjectUnref(pPayload->Self);
	pPayload->Self = NULL;
	if ( pPayload->DropCount != NULL ) {
		(*pPayload->DropCount)++;
		pPayload->DropCount = NULL;
	}
}



/* 按当前模式模拟准确、多报或失败的类型追踪。 */
static bool testGraphContractTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testgraphcontractpayload* pPayload =
		(const testgraphcontractpayload*)pValue;
	(void)pType;

	if ( testGraphTraceMode == TEST_GRAPH_TRACE_SUCCESS_ERROR ) {
		testGraphContractTemporaryError("test.graph.trace-success", 29);
	}
	if ( testGraphTraceMode == TEST_GRAPH_TRACE_ERROR ) {
		xerror* pError = xrtErrorCreate(
			XERR_VALUE, "test.graph.trace", 31,
			"the test graph trace failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return false;
	}
	if ( testGraphTraceMode == TEST_GRAPH_TRACE_SILENT ) {
		return false;
	}
	if ( testGraphTraceMode == TEST_GRAPH_TRACE_NULL ) {
		return pVisit(NULL, pContext);
	}
	if ( (pPayload->Self != NULL) &&
		 !pVisit(pPayload->Self, pContext) ) {
		return false;
	}
	if ( (testGraphTraceMode == TEST_GRAPH_TRACE_OVERREPORT) &&
		 (pPayload->Self != NULL) ) {
		return pVisit(pPayload->Self, pContext);
	}
	return true;
}



/* 构造可切换追踪行为的引用类型。 */
static xrttype testGraphContractType(void)
{
	static const xrtinstanceops Ops = {
		.Init = testGraphContractInit,
		.Drop = testGraphContractDrop,
		.Trace = testGraphContractTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.GraphContract")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("GraphContract"),
		.AbiName = XRT_STR_INIT("tests.runtime.GraphContract"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testgraphcontractpayload),
		.InstanceAlign = TEST_ALIGNOF(testgraphcontractpayload),
		.InstanceOps = &Ops
	};

	return Type;
}



/* 创建一个只靠内部强引用存活的自环对象。 */
static xrtobject* testGraphContractObject(
	xrtobjectgraph* pGraph,
	const xrttype* pType,
	xrtweak* pWeak
)
{
	xrtobject* pObject = xrtObjectCreate(pType);
	testgraphcontractpayload* pPayload;

	testRequire(
		(pObject != NULL) &&
		xrtObjectGraphTrack(pGraph, pObject) &&
		xrtWeakInit(pWeak, pObject),
		"graph contract fixture failed"
	);
	pPayload = (testgraphcontractpayload*)xrtObjectData(pObject);
	pPayload->Self = xrtObjectRef(pObject);
	testRequire(pPayload->Self == pObject,
		"graph contract self reference retain failed");
	xrtObjectUnref(pObject);
	return pObject;
}



/* 验证失败收集没有修改统计输出、图成员或对象生命周期。 */
static void testGraphContractUnchanged(
	xrtobjectgraph* pGraph,
	const xrtweak* pWeak,
	const xrtobjectgraphresult* pResult,
	int iDropCount
)
{
	testRequire(
		(pResult->TrackedCount == 91u) &&
		(pResult->EdgeCount == 92u) &&
		(pResult->RootCount == 93u) &&
		(pResult->CollectedCount == 94u),
		"failed graph collection modified output"
	);
	testRequire(
		(iDropCount == 0) &&
		!xrtWeakExpired(pWeak) &&
		(xrtObjectGraphCount(pGraph) == 1u),
		"failed graph collection modified object state"
	);
}



/* 枚举根时模拟带有原因的宿主失败。 */
static bool testGraphRootsFail(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
)
{
	xerror* pError;
	(void)pVisit;
	(void)pVisitContext;
	(void)pContext;

	pError = xrtErrorCreate(XERR_IO, "test.graph.roots", 37,
		"the test root enumerator failed");
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 枚举一个根并留下临时错误，用于验证成功回调的错误隔离。 */
static bool testGraphRootsWithError(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
)
{
	testGraphContractTemporaryError("test.graph.roots-success", 43);
	return pVisit((xrtobject*)pContext, pVisitContext);
}



/* 验证错误追踪描述在任何析构前被拒绝且可恢复。 */
static void testGraphTraceFailures(void)
{
	const testgraphtracemode Modes[] = {
		TEST_GRAPH_TRACE_OVERREPORT,
		TEST_GRAPH_TRACE_ERROR,
		TEST_GRAPH_TRACE_SILENT,
		TEST_GRAPH_TRACE_NULL
	};

	for ( size_t i = 0; i < sizeof(Modes) / sizeof(Modes[0]); i++ ) {
		int iDropCount = 0;
		xrttype Type = testGraphContractType();
		xrtobjectgraph* pGraph = xrtObjectGraphCreate();
		xrtobjectgraphresult Result = { 91u, 92u, 93u, 94u };
		xrtweak Weak = { 0 };
		const xerror* pError;

		testGraphContractDropCount = &iDropCount;
		testRequire(pGraph != NULL, "graph contract create failed");
		(void)testGraphContractObject(pGraph, &Type, &Weak);
		testGraphTraceMode = Modes[i];
		xrtClearError();
		if ( Modes[i] == TEST_GRAPH_TRACE_SILENT ) {
			testGraphContractTemporaryError("test.graph.stale", 39);
		}
		testRequire(
			!xrtObjectGraphCollect(pGraph, &Result),
			"invalid graph trace collection succeeded"
		);
		testGraphContractUnchanged(pGraph, &Weak, &Result, iDropCount);
		pError = xrtGetError();
		testRequire(
			(pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "xrt.object-graph") == 0) &&
			(xrtErrorCode(pError) == XOBJECT_GRAPH_ERROR_TRACE),
			"invalid graph trace error mismatch"
		);
		if ( Modes[i] == TEST_GRAPH_TRACE_ERROR ) {
			const xerror* pTypeError = xrtErrorCause(pError);

			testRequire(
				(pTypeError != NULL) &&
				(strcmp(xrtErrorDomain(pTypeError), "xrt.type") == 0) &&
				(xrtErrorCause(pTypeError) != NULL) &&
				(strcmp(xrtErrorDomain(xrtErrorCause(pTypeError)),
					"test.graph.trace") == 0),
				"graph trace nested cause mismatch"
			);
		}
		if ( Modes[i] == TEST_GRAPH_TRACE_SILENT ) {
			const xerror* pTypeError = xrtErrorCause(pError);

			testRequire(
				(pTypeError != NULL) &&
				(strcmp(xrtErrorDomain(pTypeError), "xrt.type") == 0) &&
				(xrtErrorCause(pTypeError) == NULL),
				"graph trace reused a stale caller error"
			);
		}

		testGraphTraceMode = TEST_GRAPH_TRACE_NORMAL;
		xrtClearError();
		testRequire(
			xrtObjectGraphCollect(pGraph, &Result) &&
			(Result.CollectedCount == 1u) &&
			(iDropCount == 1) && xrtWeakExpired(&Weak),
			"graph did not recover after trace failure"
		);
		xrtWeakUnit(&Weak);
		xrtObjectGraphDestroy(pGraph);
	}
}



/* 验证成功的 Trace 和根回调不会覆盖调用方原有错误。 */
static void testGraphSuccessErrorIsolation(void)
{
	int iDropCount = 0;
	xrttype Type = testGraphContractType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result;
	xrtweak Weak = { 0 };
	xrtobject* pObject;
	xerror* pHostError;
	const xerror* pCurrent;

	testGraphContractDropCount = &iDropCount;
	testRequire(pGraph != NULL, "success isolation graph create failed");
	pObject = testGraphContractObject(pGraph, &Type, &Weak);
	testGraphTraceMode = TEST_GRAPH_TRACE_SUCCESS_ERROR;
	pHostError = xrtErrorCreate(
		XERR_IO, "test.graph.host", 47, "host error"
	);
	testRequire(pHostError != NULL,
		"success isolation host error fixture failed");
	xrtSetError(pHostError);
	xrtErrorFree(pHostError);
	testRequire(
		xrtObjectGraphCollectRoots(
			pGraph, testGraphRootsWithError, pObject, &Result
		) &&
		(Result.RootCount == 1u) &&
		(Result.CollectedCount == 0u),
		"successful graph callbacks changed collection semantics"
	);
	pCurrent = xrtGetError();
	testRequire(
		(pCurrent != NULL) &&
		(strcmp(xrtErrorDomain(pCurrent), "test.graph.host") == 0) &&
		(xrtErrorCode(pCurrent) == 47),
		"successful graph callback leaked its temporary error"
	);

	testGraphTraceMode = TEST_GRAPH_TRACE_NORMAL;
	xrtClearError();
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) &&
		(iDropCount == 1) && xrtWeakExpired(&Weak),
		"graph did not recover after successful callback isolation"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证宿主根枚举失败同样保持图和结果不变。 */
static void testGraphRootFailure(void)
{
	int iDropCount = 0;
	xrttype Type = testGraphContractType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result = { 91u, 92u, 93u, 94u };
	xrtweak Weak = { 0 };
	const xerror* pError;

	testGraphContractDropCount = &iDropCount;
	testGraphTraceMode = TEST_GRAPH_TRACE_NORMAL;
	testRequire(pGraph != NULL, "root failure graph create failed");
	(void)testGraphContractObject(pGraph, &Type, &Weak);
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectRoots(
			pGraph, testGraphRootsFail, NULL, &Result
		),
		"failed root enumeration collection succeeded"
	);
	testGraphContractUnchanged(pGraph, &Weak, &Result, iDropCount);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.object-graph") == 0) &&
		(xrtErrorCode(pError) == XOBJECT_GRAPH_ERROR_ROOTS) &&
		(xrtErrorCause(pError) != NULL) &&
		(strcmp(xrtErrorDomain(xrtErrorCause(pError)),
			"test.graph.roots") == 0),
		"root enumeration cause mismatch"
	);
	testGraphTraceMode = TEST_GRAPH_TRACE_NORMAL;
	testRequire(
		xrtObjectGraphCollect(pGraph, NULL) &&
		(iDropCount == 1) && xrtWeakExpired(&Weak),
		"graph did not recover after root enumeration failure"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 运行对象图失败原子性和错误链测试。 */
int main(void)
{
	testGraphTraceFailures();
	testGraphSuccessErrorIsolation();
	testGraphRootFailure();
	xrtClearError();
	printf("[PASS] runtime object graph contract\n");
	return 0;
}
