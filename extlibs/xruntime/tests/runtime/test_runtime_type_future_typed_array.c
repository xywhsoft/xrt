#include "../test.h"



/* 验证类型数组通过 Future 描述独立维护消费端引用。 */
int main(void)
{
	const xrttype* pType = xrtTypeFuture();
	xtypedarray Array;
	xtypedarray* pClone;
	xfuture* pFuture = NULL;
	xfuture* pOutput;
	xfuture* pStored;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	int iAnswer = 42;

	testRequire((pPromise != NULL) && (pFuture != NULL),
		"runtime Future typed array fixture failed");
	testRequire(xrtTypedArrayInit(&Array, pType),
		"runtime Future typed array init failed");
	testRequire(xrtTypedArrayPush(&Array, &pFuture),
		"runtime Future typed array push failed");
	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	pStored = *(xfuture* const*)xrtTypedArrayConstGet(&Array, 0u);
	testRequire(
		(pStored != NULL) &&
		xrtPromiseResolve(pPromise, &iAnswer) &&
		(xrtFutureValue(pStored) == &iAnswer),
		"runtime Future typed array did not preserve consumer lifetime"
	);

	pClone = xrtTypedArrayClone(&Array);
	testRequire(
		(pClone != NULL) &&
		(xrtTypedArrayItemType(pClone) == pType) &&
		(xrtTypedArrayCount(pClone) == 1u),
		"runtime Future typed array clone failed"
	);
	xrtTypedArrayClear(&Array);
	testRequire(
		(xrtFutureValue(
			*(xfuture* const*)xrtTypedArrayConstGet(pClone, 0u)
		) == &iAnswer),
		"runtime Future typed array clone lost result identity"
	);

	testRequire(
		xrtTypeInitValue(pType, &pOutput) &&
		xrtTypedArrayPop(pClone, &pOutput) &&
		(xrtTypedArrayCount(pClone) == 0u) &&
		(xrtFutureValue(pOutput) == &iAnswer),
		"runtime Future typed array Take path failed"
	);
	xrtTypedArrayDestroy(pClone);
	xrtTypedArrayUnit(&Array);
	xrtPromiseDestroy(pPromise);
	xrtTypeDropValue(pType, &pOutput);
	xrtClearError();
	printf("[PASS] runtime Future type typed array\n");
	return 0;
}
