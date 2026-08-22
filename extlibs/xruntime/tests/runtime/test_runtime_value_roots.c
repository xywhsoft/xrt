#include "../test.h"



/* 测试对象用一个 Value 图表达自身拥有的运行时对象引用。 */
typedef struct testvaluerootpayload {
	xvalue* Value;
	int* DropCount;
} testvaluerootpayload;



static int* gValueRootDropCount;



/* 初始化测试对象的空 Value 所有权槽。 */
static bool testValueRootInit(ptr pValue, const xrttype* pType)
{
	testvaluerootpayload* pPayload = (testvaluerootpayload*)pValue;
	(void)pType;

	pPayload->Value = NULL;
	pPayload->DropCount = gValueRootDropCount;
	return true;
}



/* 释放测试对象拥有的 Value 图并记录唯一终结。 */
static void testValueRootDrop(ptr pValue, const xrttype* pType)
{
	testvaluerootpayload* pPayload = (testvaluerootpayload*)pValue;
	(void)pType;

	xrtValueRelease(pPayload->Value);
	pPayload->Value = NULL;
	if ( pPayload->DropCount != NULL ) {
		(*pPayload->DropCount)++;
		pPayload->DropCount = NULL;
	}
}



/* 枚举测试对象 Value 图实际拥有的运行时对象强引用。 */
static bool testValueRootTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testvaluerootpayload* pPayload =
		(const testvaluerootpayload*)pValue;
	(void)pType;

	return (pPayload->Value == NULL) ||
		xrtValueTraceRuntimeObjects(pPayload->Value, pVisit, pContext);
}



/* 构造拥有 Value 图负载的测试引用类型。 */
static xrttype testValueRootType(void)
{
	static const xrtinstanceops Ops = {
		.Init = testValueRootInit,
		.Drop = testValueRootDrop,
		.Trace = testValueRootTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueRoots")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueRoots"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueRoots"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testvaluerootpayload),
		.InstanceAlign = TEST_ALIGNOF(testvaluerootpayload),
		.InstanceOps = &Ops
	};

	return Type;
}



/* 创建仅由自身 Value 图维持的对象环，并返回一个外部 Value 别名。 */
static xvalue* testValueRootCycle(
	xrtobjectgraph* pGraph,
	const xrttype* pType,
	xrtweak* pWeak,
	bool bContainer
)
{
	xrtobject* pObject = xrtObjectCreate(pType);
	testvaluerootpayload* pPayload;
	xvalue* pObjectValue;
	xvalue* pOwned;
	xvalue* pAlias;

	testRequire(
		(pObject != NULL) &&
		xrtObjectGraphTrack(pGraph, pObject) &&
		xrtWeakInit(pWeak, pObject),
		"Value root cycle object fixture failed"
	);
	pPayload = (testvaluerootpayload*)xrtObjectData(pObject);
	pObjectValue = xrtValueRuntimeObject(pObject);
	testRequire(pObjectValue != NULL,
		"Value root cycle handle fixture failed");
	if ( bContainer ) {
		pOwned = xrtValueArray();
		testRequire(
			(pOwned != NULL) &&
			xrtValueArrayAppendTake(pOwned, &pObjectValue),
			"Value root COW fixture failed"
		);
		pAlias = xrtValueClone(pOwned);
	} else {
		pOwned = pObjectValue;
		pObjectValue = NULL;
		pAlias = xrtValueRetain(pOwned);
	}
	testRequire(pAlias != NULL,
		"Value root external alias fixture failed");
	pPayload->Value = pOwned;
	xrtObjectUnref(pObject);
	return pAlias;
}



/* 安装一个用于验证成功路径错误隔离的宿主错误。 */
static void testValueRootHostError(void)
{
	xerror* pError = xrtErrorCreate(
		XERR_IO, "tests.value-roots.host", 71, "host error");

	testRequire(pError != NULL, "Value root host error fixture failed");
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 验证共享 Handle 外壳作为重复显式根时保持对象环存活。 */
static void testValueRootSharedShell(void)
{
	int iDropCount = 0;
	xrttype Type = testValueRootType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result;
	xrtweak Weak = { 0 };
	xvalue* pAlias;
	const xvalue* pRoots[2];
	const xerror* pError;

	gValueRootDropCount = &iDropCount;
	testRequire(pGraph != NULL, "Value root shared graph create failed");
	pAlias = testValueRootCycle(pGraph, &Type, &Weak, false);
	pRoots[0] = pAlias;
	pRoots[1] = pAlias;
	testValueRootHostError();
	testRequire(
		xrtObjectGraphCollectValueRoots(pGraph, pRoots, 2u, &Result) &&
		(Result.TrackedCount == 1u) &&
		(Result.EdgeCount == 1u) &&
		(Result.RootCount == 1u) &&
		(Result.CollectedCount == 0u) &&
		!xrtWeakExpired(&Weak) &&
		(iDropCount == 0),
		"shared Value root did not preserve the object cycle"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "tests.value-roots.host") == 0) &&
		(xrtErrorCode(pError) == 71),
		"successful Value root collection replaced the host error"
	);
	xrtClearError();
	xrtValueRelease(pAlias);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&Weak) &&
		(iDropCount == 1),
		"released shared Value root kept the object cycle alive"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证共享 COW backing 的外部 Value 根保持嵌套对象环存活。 */
static void testValueRootCowBacking(void)
{
	int iDropCount = 0;
	xrttype Type = testValueRootType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result;
	xrtweak Weak = { 0 };
	xvalue* pAlias;

	gValueRootDropCount = &iDropCount;
	testRequire(pGraph != NULL, "Value root COW graph create failed");
	pAlias = testValueRootCycle(pGraph, &Type, &Weak, true);
	testRequire(
		xrtObjectGraphCollectValueRoot(pGraph, pAlias, &Result) &&
		(Result.RootCount == 1u) &&
		(Result.CollectedCount == 0u) &&
		!xrtWeakExpired(&Weak),
		"COW Value root did not preserve the nested object cycle"
	);
	xrtValueRelease(pAlias);
	testRequire(
		xrtObjectGraphCollectValueRoots(pGraph, NULL, 0u, &Result) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&Weak) &&
		(iDropCount == 1),
		"released COW Value root kept the nested cycle alive"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 构造超过统一 Value 图深度限制的宿主根。 */
static xvalue* testValueRootDeepGraph(void)
{
	xvalue* pCurrent = xrtValueNull();

	for ( uint32 i = 0u; i < XRT_VALUE_DEPTH_MAX; i++ ) {
		xvalue* pParent = xrtValueArray();

		if ( (pParent == NULL) ||
			 !xrtValueArrayAppendTake(pParent, &pCurrent) ) {
			xrtValueRelease(pParent);
			xrtValueRelease(pCurrent);
			return NULL;
		}
		pCurrent = pParent;
	}
	return pCurrent;
}



/* 验证 Value 追踪失败保留完整组合错误链和对象图失败原子性。 */
static void testValueRootTraceFailure(void)
{
	int iDropCount = 0;
	xrttype Type = testValueRootType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result = { 91u, 92u, 93u, 94u };
	xrtweak Weak = { 0 };
	xvalue* pAlias;
	xvalue* pDeep;
	const xerror* pGraphError;
	const xerror* pRootError;
	const xerror* pTraceError;

	gValueRootDropCount = &iDropCount;
	testRequire(pGraph != NULL, "Value root failure graph create failed");
	pAlias = testValueRootCycle(pGraph, &Type, &Weak, false);
	xrtValueRelease(pAlias);
	pDeep = testValueRootDeepGraph();
	testRequire(pDeep != NULL, "Value root deep graph fixture failed");
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectValueRoot(pGraph, pDeep, &Result) &&
		(Result.TrackedCount == 91u) &&
		(Result.CollectedCount == 94u) &&
		!xrtWeakExpired(&Weak) &&
		(iDropCount == 0),
		"failed Value root trace changed object graph state"
	);
	pGraphError = xrtGetError();
	pRootError = pGraphError != NULL ? xrtErrorCause(pGraphError) : NULL;
	pTraceError = pRootError != NULL ? xrtErrorCause(pRootError) : NULL;
	testRequire(
		(pGraphError != NULL) &&
		(strcmp(xrtErrorDomain(pGraphError), "xrt.object-graph") == 0) &&
		(xrtErrorCode(pGraphError) == XOBJECT_GRAPH_ERROR_ROOTS) &&
		(pRootError != NULL) &&
		(strcmp(xrtErrorDomain(pRootError), "xrt.runtime-value") == 0) &&
		(xrtErrorCode(pRootError) == XRUNTIME_VALUE_ERROR_ROOTS) &&
		(pTraceError != NULL) &&
		(strcmp(xrtErrorDomain(pTraceError), "xrt.runtime-value") == 0) &&
		(xrtErrorCode(pTraceError) == XRUNTIME_VALUE_ERROR_TRACE),
		"Value root trace cause chain mismatch"
	);
	xrtValueRelease(pDeep);
	xrtClearError();
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&Weak) &&
		(iDropCount == 1),
		"Value root graph did not recover after trace failure"
	);
	xrtWeakUnit(&Weak);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证空根、空数组和空元素在对象图快照前报告稳定错误。 */
static void testValueRootArguments(void)
{
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result = { 91u, 92u, 93u, 94u };
	const xvalue* pRoots[1] = { NULL };
	const xerror* pError;

	testRequire(pGraph != NULL, "Value root argument graph create failed");
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectValueRoot(pGraph, NULL, &Result) &&
		(Result.TrackedCount == 91u),
		"single Value root accepted null"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.runtime-value") == 0) &&
		(xrtErrorCode(pError) == XRUNTIME_VALUE_ERROR_ROOTS),
		"single Value root null error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectValueRoots(pGraph, NULL, 1u, &Result) &&
		(Result.TrackedCount == 91u) &&
		(xrtErrorCode(xrtGetError()) == XRUNTIME_VALUE_ERROR_ROOTS),
		"Value root array accepted a missing view"
	);
	xrtClearError();
	testRequire(
		!xrtObjectGraphCollectValueRoots(pGraph, pRoots, 1u, &Result) &&
		(Result.TrackedCount == 91u) &&
		(xrtErrorCode(xrtGetError()) == XRUNTIME_VALUE_ERROR_ROOTS),
		"Value root array accepted a null element"
	);
	xrtClearError();
	testRequire(
		xrtObjectGraphCollectValueRoots(pGraph, NULL, 0u, &Result) &&
		(Result.TrackedCount == 0u) &&
		(Result.CollectedCount == 0u),
		"empty Value root array did not delegate to normal collection"
	);
	xrtObjectGraphDestroy(pGraph);
}



/* 运行 Value 根适配器的共享所有权、COW 和参数契约测试。 */
int main(void)
{
	testValueRootSharedShell();
	testValueRootCowBacking();
	testValueRootTraceFailure();
	testValueRootArguments();
	xrtClearError();
	printf("[PASS] runtime Value roots\n");
	return 0;
}
