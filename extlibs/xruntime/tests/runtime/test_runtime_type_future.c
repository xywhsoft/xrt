#include "../test.h"



/* 验证 Future 类型描述、槽生命周期、身份比较和引用保活语义。 */
int main(void)
{
	const xrttype* pType = xrtTypeFuture();
	xfuture* pFuture = NULL;
	xfuture* pCopy;
	xfuture* pClone;
	xfuture* pMoved;
	xpromise* pPromise = xrtPromiseCreate(&pFuture, NULL);
	uint64 iHash;
	int iCompare;
	int iAnswer = 42;

	testRequire((pPromise != NULL) && (pFuture != NULL),
		"runtime Future type fixture failed");
	testRequire(
		xrtTypeValidate(pType) &&
		(pType->Id == xrtTypeId(XRT_STR_LITERAL("xrt.future"))) &&
		(pType->Kind == XRT_TYPE_FUTURE) &&
		(pType->Size == sizeof(xfuture*)) &&
		xrtTypeIsCopyable(pType) &&
		xrtTypeIsRelocatable(pType) &&
		xrtTypeIsComparable(pType) &&
		xrtTypeIsHashable(pType),
		"runtime Future type descriptor is invalid"
	);
	testRequire(
		xrtTypeInitValue(pType, &pCopy) &&
		xrtTypeInitValue(pType, &pClone) &&
		xrtTypeInitValue(pType, &pMoved) &&
		(pCopy == NULL) && (pClone == NULL) && (pMoved == NULL),
		"runtime Future type init mismatch"
	);
	testRequire(
		xrtTypeCopyValue(pType, &pCopy, &pFuture) &&
		(pCopy == pFuture) &&
		xrtTypeCopyValue(pType, &pCopy, &pCopy) &&
		(pCopy == pFuture),
		"runtime Future type copy mismatch"
	);
	testRequire(
		xrtTypeCloneValue(pType, &pClone, &pFuture) &&
		(pClone == pFuture),
		"runtime Future type clone mismatch"
	);
	testRequire(
		xrtTypeCompareValue(pType, &pFuture, &pCopy, &iCompare) &&
		(iCompare == 0) &&
		xrtTypeHashValue(pType, &pFuture, &iHash) &&
		(iHash == (uint64)(uintptr_t)pFuture),
		"runtime Future type identity mismatch"
	);
	testRequire(
		xrtTypeMoveValue(pType, &pMoved, &pCopy) &&
		(pMoved == pFuture) && (pCopy == NULL) &&
		xrtTypeMoveValue(pType, &pMoved, &pMoved) &&
		(pMoved == pFuture),
		"runtime Future type move mismatch"
	);
	xrtTypeDropValue(pType, &pClone);
	testRequire(pClone == NULL,
		"runtime Future type drop did not clear cloned slot");

	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	testRequire(
		xrtPromiseResolve(pPromise, &iAnswer) &&
		(xrtFutureState(pMoved) == XFUTURE_RESOLVED) &&
		(xrtFutureValue(pMoved) == &iAnswer),
		"runtime Future type slot did not preserve consumer lifetime"
	);

	xrtTypeDropValue(pType, &pCopy);
	xrtTypeDropValue(pType, &pMoved);
	testRequire((pCopy == NULL) && (pMoved == NULL),
		"runtime Future type final drop mismatch");
	xrtPromiseDestroy(pPromise);
	xrtClearError();
	printf("[PASS] runtime Future type\n");
	return 0;
}
