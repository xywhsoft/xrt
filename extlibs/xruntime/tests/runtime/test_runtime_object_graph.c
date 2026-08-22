#include "../test.h"



typedef struct testgraphpayload {
	xrtobject* First;
	xrtobject* Second;
	int* DropCount;
} testgraphpayload;



static int* testGraphDropCount = NULL;



/* 初始化对象图测试负载并绑定析构计数。 */
static bool testGraphInit(ptr pValue, const xrttype* pType)
{
	testgraphpayload* pPayload = (testgraphpayload*)pValue;
	(void)pType;

	pPayload->First = NULL;
	pPayload->Second = NULL;
	pPayload->DropCount = testGraphDropCount;
	return true;
}



/* 释放负载拥有的全部强引用并记录一次析构。 */
static void testGraphDrop(ptr pValue, const xrttype* pType)
{
	testgraphpayload* pPayload = (testgraphpayload*)pValue;
	(void)pType;

	xrtObjectUnref(pPayload->First);
	xrtObjectUnref(pPayload->Second);
	pPayload->First = NULL;
	pPayload->Second = NULL;
	if ( pPayload->DropCount != NULL ) {
		(*pPayload->DropCount)++;
		pPayload->DropCount = NULL;
	}
}



/* 精确枚举两个声明字段实际持有的强引用。 */
static bool testGraphTrace(
	const void* pValue,
	const xrttype* pType,
	xrtobjectvisitor pVisit,
	ptr pContext
)
{
	const testgraphpayload* pPayload = (const testgraphpayload*)pValue;
	(void)pType;

	if ( (pPayload->First != NULL) &&
		 !pVisit(pPayload->First, pContext) ) {
		return false;
	}
	if ( (pPayload->Second != NULL) &&
		 !pVisit(pPayload->Second, pContext) ) {
		return false;
	}
	return true;
}



/* 构造对象图测试共享的引用类型描述。 */
static xrttype testGraphType(void)
{
	static const xrtinstanceops Ops = {
		.Init = testGraphInit,
		.Drop = testGraphDrop,
		.Trace = testGraphTrace
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.GraphNode")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("GraphNode"),
		.AbiName = XRT_STR_INIT("tests.runtime.GraphNode"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(testgraphpayload),
		.InstanceAlign = TEST_ALIGNOF(testgraphpayload),
		.InstanceOps = &Ops
	};

	return Type;
}



/* 用一个新强引用替换对象字段。 */
static void testGraphSet(xrtobject* pOwner, size_t iField, xrtobject* pTarget)
{
	testgraphpayload* pPayload = (testgraphpayload*)xrtObjectData(pOwner);
	xrtobject** pField = iField == 0u ? &pPayload->First : &pPayload->Second;
	xrtobject* pOld = *pField;

	if ( pTarget != NULL ) {
		testRequire(xrtObjectRef(pTarget) == pTarget,
			"graph field target retain failed");
	}
	*pField = pTarget;
	xrtObjectUnref(pOld);
}



/* 创建、跟踪并为两个对象建立一个强引用环。 */
static void testGraphPair(
	xrtobjectgraph* pGraph,
	const xrttype* pType,
	xrtobject** pFirst,
	xrtobject** pSecond
)
{
	*pFirst = xrtObjectCreate(pType);
	*pSecond = xrtObjectCreate(pType);
	testRequire(
		(*pFirst != NULL) && (*pSecond != NULL) &&
		xrtObjectGraphTrack(pGraph, *pFirst) &&
		xrtObjectGraphTrack(pGraph, *pSecond),
		"graph pair creation failed"
	);
	testGraphSet(*pFirst, 0u, *pSecond);
	testGraphSet(*pSecond, 0u, *pFirst);
}



/* 枚举测试上下文中的一个借用对象根。 */
static bool testGraphRoots(
	xrtobjectvisitor pVisit,
	ptr pVisitContext,
	ptr pContext
)
{
	return pVisit((xrtobject*)pContext, pVisitContext);
}



/* 验证不可达双节点环、自环和弱引用终结。 */
static void testGraphCycles(void)
{
	int iDropCount = 0;
	xrttype Type = testGraphType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result;
	xrtobject* pFirst;
	xrtobject* pSecond;
	xrtobject* pSelf;
	xrtweak FirstWeak = { 0 };
	xrtweak SecondWeak = { 0 };
	xrtweak SelfWeak = { 0 };

	testGraphDropCount = &iDropCount;
	testRequire(pGraph != NULL, "object graph create failed");
	testGraphPair(pGraph, &Type, &pFirst, &pSecond);
	testRequire(
		xrtWeakInit(&FirstWeak, pFirst) &&
		xrtWeakInit(&SecondWeak, pSecond),
		"graph pair weak reference creation failed"
	);
	xrtObjectUnref(pFirst);
	xrtObjectUnref(pSecond);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.TrackedCount == 2u) &&
		(Result.EdgeCount == 2u) &&
		(Result.RootCount == 0u) &&
		(Result.CollectedCount == 2u),
		"unreachable graph pair collection result mismatch"
	);
	testRequire(
		(iDropCount == 2) &&
		xrtWeakExpired(&FirstWeak) &&
		xrtWeakExpired(&SecondWeak) &&
		(xrtObjectGraphCount(pGraph) == 0u),
		"unreachable graph pair lifetime mismatch"
	);
	xrtWeakUnit(&FirstWeak);
	xrtWeakUnit(&SecondWeak);

	pSelf = xrtObjectCreate(&Type);
	testRequire(
		(pSelf != NULL) && xrtObjectGraphTrack(pGraph, pSelf) &&
		xrtWeakInit(&SelfWeak, pSelf),
		"self-cycle fixture failed"
	);
	testGraphSet(pSelf, 0u, pSelf);
	testGraphSet(pSelf, 1u, pSelf);
	xrtObjectUnref(pSelf);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.EdgeCount == 2u) &&
		(Result.CollectedCount == 1u) &&
		xrtWeakExpired(&SelfWeak) &&
		(iDropCount == 3),
		"self-cycle collection mismatch"
	);
	xrtWeakUnit(&SelfWeak);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证直接外部引用和显式宿主根都保留完整可达子图。 */
static void testGraphRootsAndReachability(void)
{
	int iDropCount = 0;
	xrttype Type = testGraphType();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	xrtobjectgraphresult Result;
	xrtobject* pFirst;
	xrtobject* pSecond;

	testGraphDropCount = &iDropCount;
	testRequire(pGraph != NULL, "root graph create failed");
	testGraphPair(pGraph, &Type, &pFirst, &pSecond);
	xrtObjectUnref(pSecond);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.RootCount == 1u) &&
		(Result.CollectedCount == 0u) &&
		(xrtObjectGraphCount(pGraph) == 2u) &&
		(iDropCount == 0),
		"direct external graph root was not preserved"
	);
	xrtObjectUnref(pFirst);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.RootCount == 0u) &&
		(Result.CollectedCount == 2u) &&
		(iDropCount == 2),
		"released external graph root remained reachable"
	);

	testGraphPair(pGraph, &Type, &pFirst, &pSecond);
	xrtObjectUnref(pFirst);
	xrtObjectUnref(pSecond);
	testRequire(
		xrtObjectGraphCollectRoots(
			pGraph, testGraphRoots, pFirst, &Result
		) &&
		(Result.RootCount == 1u) &&
		(Result.CollectedCount == 0u),
		"explicit host graph root was not preserved"
	);
	testRequire(
		xrtObjectGraphCollect(pGraph, &Result) &&
		(Result.CollectedCount == 2u) &&
		(iDropCount == 4),
		"removed host graph root remained reachable"
	);
	xrtObjectGraphDestroy(pGraph);
}



/* 验证跟踪归属、幂等性、手动摘除和普通终结自动摘除。 */
static void testGraphMembership(void)
{
	int iDropCount = 0;
	xrttype Type = testGraphType();
	xrtobjectgraph* pFirstGraph = xrtObjectGraphCreate();
	xrtobjectgraph* pSecondGraph = xrtObjectGraphCreate();
	xrtobject* pObject;

	testGraphDropCount = &iDropCount;
	pObject = xrtObjectCreate(&Type);
	testRequire(
		(pFirstGraph != NULL) && (pSecondGraph != NULL) &&
		(pObject != NULL),
		"graph membership fixture failed"
	);
	testRequire(
		xrtObjectGraphTrack(pFirstGraph, pObject) &&
		xrtObjectGraphTrack(pFirstGraph, pObject) &&
		xrtObjectGraphContains(pFirstGraph, pObject) &&
		(xrtObjectGraphCount(pFirstGraph) == 1u),
		"graph idempotent track mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtObjectGraphTrack(pSecondGraph, pObject) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(xrtErrorCode(xrtGetError()) == XOBJECT_GRAPH_ERROR_TRACK),
		"object was accepted by two graphs"
	);
	testRequire(
		xrtObjectGraphUntrack(pFirstGraph, pObject) &&
		!xrtObjectGraphContains(pFirstGraph, pObject),
		"graph manual untrack failed"
	);
	xrtClearError();
	testRequire(
		!xrtObjectGraphUntrack(pFirstGraph, pObject) &&
		(xrtGetError() == NULL),
		"missing graph untrack reported an error"
	);
	testRequire(xrtObjectGraphTrack(pSecondGraph, pObject),
		"object could not move to another graph");
	xrtObjectUnref(pObject);
	testRequire(
		(iDropCount == 1) &&
		(xrtObjectGraphCount(pSecondGraph) == 0u),
		"normal object finalization did not detach graph membership"
	);
	pObject = xrtObjectCreate(&Type);
	testRequire(
		(pObject != NULL) && xrtObjectGraphTrack(pSecondGraph, pObject),
		"graph destroy fixture failed"
	);
	xrtObjectGraphDestroy(pSecondGraph);
	testRequire(
		(xrtObjectRefCount(pObject) == 1u) && xrtObjectUnique(pObject),
		"graph destroy changed object ownership"
	);
	xrtObjectUnref(pObject);
	testRequire(iDropCount == 2, "graph destroy object did not finalize normally");
	xrtObjectGraphDestroy(pFirstGraph);
}



/* 运行对象图常规生命周期与可达性测试。 */
int main(void)
{
	testGraphCycles();
	testGraphRootsAndReachability();
	testGraphMembership();
	xrtClearError();
	printf("[PASS] runtime object graph\n");
	return 0;
}
