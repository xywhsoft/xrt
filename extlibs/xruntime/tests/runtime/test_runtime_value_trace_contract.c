#include "../test.h"



#define TEST_RUNTIME_VALUE_TRACE_OBJECTS 48u



/* 记录追踪测试对象的最终释放次数。 */
static int gRuntimeValueTraceDrops;



/* 释放追踪测试对象时记录一次析构。 */
static void testRuntimeValueTraceDrop(ptr pValue, const xrttype* pType)
{
	(void)pValue;
	(void)pType;
	gRuntimeValueTraceDrops++;
}



/* 接受对象边并累计访问次数。 */
static bool testRuntimeValueTraceVisit(xrtobject* pObject, ptr pContext)
{
	size_t* pCount = (size_t*)pContext;

	testRequire(pObject != NULL, "runtime Value trace visited a null object");
	(*pCount)++;
	return true;
}



/* 不设置新错误并拒绝对象边。 */
static bool testRuntimeValueTraceReject(xrtobject* pObject, ptr pContext)
{
	(void)pObject;
	(void)pContext;
	return false;
}



/* 设置可复用的静态参数错误并拒绝对象边。 */
static bool testRuntimeValueTraceRejectWithError(
	xrtobject* pObject,
	ptr pContext
)
{
	int64 iValue;

	(void)pObject;
	(void)pContext;
	(void)xrtValueGetInt(NULL, &iValue);
	return false;
}



/* 设置临时错误但接受对象边，用于验证成功路径错误隔离。 */
static bool testRuntimeValueTraceAcceptWithError(
	xrtobject* pObject,
	ptr pContext
)
{
	int64 iValue;

	(void)pObject;
	(void)pContext;
	(void)xrtValueGetInt(NULL, &iValue);
	return true;
}



/* 在叶值外构造指定边深度的单分支数组图。 */
static xvalue* testRuntimeValueTraceChain(
	const xvalue* pLeaf,
	uint32 iDepth
)
{
	xvalue* pCurrent = xrtValueRetain(pLeaf);

	for ( uint32 i = 0u; (i < iDepth) && (pCurrent != NULL); i++ ) {
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



/* 验证大图去重表按实际 Handle 所有权槽计边。 */
static void testRuntimeValueTraceOverflow(xrtobject* pObject)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pShared = xrtValueRuntimeObject(pObject);
	size_t iCount = 0u;

	testRequire((pRoot != NULL) && (pShared != NULL),
		"runtime Value trace overflow fixture allocation failed");
	for ( size_t i = 0u; i < TEST_RUNTIME_VALUE_TRACE_OBJECTS; i++ ) {
		xvalue* pItem = xrtValueRuntimeObject(pObject);

		testRequire(
			(pItem != NULL) && xrtValueArrayAppendTake(pRoot, &pItem),
			"runtime Value trace overflow item failed"
		);
	}
	testRequire(
		xrtValueArrayAppend(pRoot, pShared) &&
		xrtValueArrayAppend(pRoot, pShared),
		"runtime Value trace shared item fixture failed"
	);
	testRequire(
		xrtValueTraceRuntimeObjects(
			pRoot, testRuntimeValueTraceVisit, &iCount
		) &&
		(iCount == (TEST_RUNTIME_VALUE_TRACE_OBJECTS + 1u)),
		"runtime Value trace did not preserve ownership edge identity"
	);
	xrtValueRelease(pShared);
	xrtValueRelease(pRoot);
}



/* 验证追踪深度与 Value 图层使用同一最大边界。 */
static void testRuntimeValueTraceDepth(xvalue* pLeaf)
{
	xvalue* pAllowed = testRuntimeValueTraceChain(
		pLeaf, XRT_VALUE_DEPTH_MAX - 1u
	);
	xvalue* pTooDeep = testRuntimeValueTraceChain(
		pLeaf, XRT_VALUE_DEPTH_MAX
	);
	size_t iCount = 0u;

	testRequire((pAllowed != NULL) && (pTooDeep != NULL),
		"runtime Value trace depth fixture failed");
	testRequire(
		xrtValueTraceRuntimeObjects(
			pAllowed, testRuntimeValueTraceVisit, &iCount
		) && (iCount == 1u),
		"runtime Value trace rejected the maximum allowed depth"
	);
	xrtClearError();
	iCount = 0u;
	testRequire(
		!xrtValueTraceRuntimeObjects(
			pTooDeep, testRuntimeValueTraceVisit, &iCount
		) &&
		(iCount == 0u) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.runtime-value") == 0) &&
		(xrtErrorCode(xrtGetError()) == XRUNTIME_VALUE_ERROR_TRACE),
		"runtime Value trace accepted a graph beyond the depth limit"
	);
	xrtValueRelease(pTooDeep);
	xrtValueRelease(pAllowed);
}



/* 验证访问器拒绝时只采用本次调用产生的错误。 */
static void testRuntimeValueTraceErrors(xvalue* pValue)
{
	xerror* pHostError;
	int64 iValue;

	xrtClearError();
	(void)xrtValueGetInt(NULL, &iValue);
	testRequire(
		!xrtValueTraceRuntimeObjects(
			pValue, testRuntimeValueTraceReject, NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.runtime-value") == 0) &&
		(xrtErrorCode(xrtGetError()) == XRUNTIME_VALUE_ERROR_TRACE),
		"runtime Value trace reused a stale visitor error"
	);

	/* 调用前与访问器设置同一静态错误，验证不能按指针地址误判。 */
	xrtClearError();
	(void)xrtValueGetInt(NULL, &iValue);
	testRequire(
		!xrtValueTraceRuntimeObjects(
			pValue, testRuntimeValueTraceRejectWithError, NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.core") == 0),
		"runtime Value trace replaced the visitor error"
	);

	/* 成功访问器留下的临时错误不能污染调用方原有错误。 */
	xrtClearError();
	pHostError = xrtErrorCreate(
		XERR_IO,
		"tests.runtime-value-trace",
		73,
		"host error"
	);
	testRequire(pHostError != NULL,
		"runtime Value trace host error fixture failed");
	xrtSetError(pHostError);
	xrtErrorFree(pHostError);
	testRequire(
		xrtValueTraceRuntimeObjects(
			pValue, testRuntimeValueTraceAcceptWithError, NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_IO) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"tests.runtime-value-trace"
		) == 0) &&
		(xrtErrorCode(xrtGetError()) == 73),
		"runtime Value trace leaked a successful visitor error"
	);
	xrtClearError();
	testRequire(
		!xrtValueTraceRuntimeObjects(NULL, testRuntimeValueTraceVisit, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"runtime Value trace accepted a null graph"
	);
	xrtClearError();
	testRequire(
		!xrtValueTraceRuntimeObjects(pValue, NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"runtime Value trace accepted a null visitor"
	);
}



/* 运行 Value 对象追踪的身份、深度和错误契约回归。 */
int main(void)
{
	static const xrtinstanceops tObjectOps = {
		.Drop = testRuntimeValueTraceDrop
	};
	xrttype ObjectType = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.runtime.ValueTrace")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueTrace"),
		.AbiName = XRT_STR_INIT("tests.runtime.ValueTrace"),
		.Size = sizeof(ptr),
		.Align = TEST_ALIGNOF(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = TEST_ALIGNOF(int64),
		.InstanceOps = &tObjectOps
	};
	xrtobject* pObject = xrtObjectCreate(&ObjectType);
	xvalue* pValue = xrtValueRuntimeObject(pObject);

	testRequire((pObject != NULL) && (pValue != NULL),
		"runtime Value trace object fixture failed");
	testRuntimeValueTraceOverflow(pObject);
	testRuntimeValueTraceDepth(pValue);
	testRuntimeValueTraceErrors(pValue);
	xrtValueRelease(pValue);
	xrtObjectUnref(pObject);
	testRequire(gRuntimeValueTraceDrops == 1,
		"runtime Value trace leaked or double-dropped its object");
	xrtClearError();
	printf("[PASS] runtime Value trace contract\n");
	return 0;
}
