#include "../test.h"



/* 验证最近一次错误来自运行时 Future Value 桥接层。 */
static void testRuntimeValueFutureError(
	xruntimevalueerror Code,
	cstr sOperation
)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime Future Value error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.runtime-value") == 0,
		"runtime Future Value error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code,
		"runtime Future Value error code mismatch");
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime Future Value error operation mismatch"
	);
}



/* 验证 Future Value 装箱、身份语义和所有权转移。 */
int main(void)
{
	xfuture* pFuture = NULL;
	xfuture* pTakeFuture = NULL;
	xfuture* pEmpty = NULL;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	xpromise* pTakePromise;
	xvalue* pValue;
	xvalue* pSecond;
	xvalue* pTakenValue;
	uint64 iFirstHash;
	uint64 iSecondHash;
	int iAnswer = 42;
	int iTakeAnswer = 84;

	testRequire((pPromise != NULL) && (pFuture != NULL),
		"runtime Future Value fixture failed");

	pValue = xrtValueFuture(pFuture);
	pSecond = xrtValueFuture(pFuture);
	testRequire((pValue != NULL) && (pSecond != NULL),
		"runtime Future Value creation failed");
	testRequire(
		xrtValueIsFuture(pValue) &&
		!xrtValueIsFuture(xrtValueBool(true)) &&
		(xrtValueGetFuture(pValue) == pFuture),
		"runtime Future Value identity mismatch"
	);
	testRequire(
		xrtValueHash(pValue, &iFirstHash) &&
		xrtValueHash(pSecond, &iSecondHash) &&
		(iFirstHash == iSecondHash) &&
		xrtValueScalarEqual(pValue, pSecond),
		"runtime Future Value hash/equality mismatch"
	);

	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	testRequire(
		xrtPromiseResolve(pPromise, &iAnswer) &&
		(xrtFutureState(xrtValueGetFuture(pValue)) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(xrtValueGetFuture(pValue)) == &iAnswer),
		"runtime Future Value did not preserve consumer lifetime"
	);
	{
		xrtownershipref roots[3] = {xrtValueOwnership(pValue), xrtValueOwnership(pSecond), xrtPromiseOwnership(pPromise)};
		xrtownershipresult graph = {0}; bool reachable = true;
		testRequire(xrtOwnershipInspectReachable(roots, 1, roots, 3, &reachable, &graph, NULL, NULL), "Future Value graph adapter");
		testRequire(graph.NodeCount == 4 && graph.EdgeCount == 6 && graph.ExternalRootCount == 0 && !reachable,
			"two Value wrappers share one Future and cancellation node");
		testRequire(xrtValueRetain(pValue) == pValue, "native Value alias");
		testRequire(xrtOwnershipInspectReachable(roots, 1, roots, 3, &reachable, &graph, NULL, NULL) && reachable && graph.ExternalRootCount == 1,
			"native Value alias remains an external graph root");
		xrtValueRelease(pValue);
	}
	xrtValueRelease(pSecond);
	xrtValueRelease(pValue);
	xrtPromiseDestroy(pPromise);

	pTakePromise = xrtPromiseCreate(&pTakeFuture, NULL);
	testRequire((pTakePromise != NULL) && (pTakeFuture != NULL),
		"runtime Future Value Take fixture failed");
	pTakenValue = xrtValueFutureTake(&pTakeFuture);
	testRequire((pTakenValue != NULL) && (pTakeFuture == NULL),
		"runtime Future Value Take did not consume source");
	testRequire(
		xrtPromiseResolve(pTakePromise, &iTakeAnswer) &&
		(xrtFutureValue(xrtValueGetFuture(pTakenValue)) == &iTakeAnswer),
		"taken runtime Future Value result mismatch"
	);
	{
		xrtownershipref roots[2] = {xrtValueOwnership(pTakenValue), xrtPromiseOwnership(pTakePromise)};
		xrtownershipresult graph = {0}; bool reachable = true;
		testRequire(xrtOwnershipInspectReachable(roots, 1, roots, 2, &reachable, &graph, NULL, NULL) &&
			graph.NodeCount == 3 && graph.EdgeCount == 4 && !reachable, "Future Take graph owns exactly one consumer reference");
	}
	xrtValueRelease(pTakenValue);
	xrtPromiseDestroy(pTakePromise);

	xrtClearError();
	testRequire(xrtValueFuture(NULL) == NULL,
		"null Future produced a Value");
	testRuntimeValueFutureError(XRUNTIME_VALUE_ERROR_FUTURE, "future");
	xrtClearError();
	testRequire(xrtValueFutureTake(NULL) == NULL,
		"null Future source was accepted");
	testRuntimeValueFutureError(XRUNTIME_VALUE_ERROR_OWNERSHIP, "future-take");
	xrtClearError();
	testRequire(xrtValueFutureTake(&pEmpty) == NULL,
		"empty Future source was accepted");
	testRuntimeValueFutureError(XRUNTIME_VALUE_ERROR_OWNERSHIP, "future-take");
	xrtClearError();
	testRequire(xrtValueGetFuture(xrtValueBool(true)) == NULL,
		"non-Future Value produced a Future");
	testRuntimeValueFutureError(XRUNTIME_VALUE_ERROR_TYPE, "future-get");

	xrtClearError();
	printf("[PASS] runtime Value Future\n");
	return 0;
}
