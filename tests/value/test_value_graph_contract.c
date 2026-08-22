#include "../test.h"



/* 图层策略测试记录资源、活动 Value 和回调保护结果。 */
typedef struct testvaluegraphcontract {
	int CloneCount;
	int DropCount;
	int HashCount;
	int EqualCount;
	xvalue* LeftRoot;
	xvalue* RightRoot;
	xvalue* LeftValue;
	xvalue* RightValue;
	ptr LeftData;
	ptr RightData;
	bool RootsBlocked;
	bool ValuesBlocked;
	bool SelfEqualBlocked;
} testvaluegraphcontract;



/* 验证回调期间指定 Value 不能被再次克隆。 */
static bool testValueGraphCloneBlocked(const xvalue* pValue)
{
	xvalue* pClone;
	bool bBlocked;

	if ( pValue == NULL ) {
		return true;
	}
	xrtClearError();
	pClone = xrtValueClone(pValue);
	bBlocked =
		(pClone == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE);
	xrtValueRelease(pClone);
	return bBlocked;
}



/* 重置一次策略调用需要累计的保护结果。 */
static void testValueGraphGuardReset(testvaluegraphcontract* pState)
{
	pState->RootsBlocked = true;
	pState->ValuesBlocked = true;
	pState->SelfEqualBlocked = true;
}



/* 在策略回调中检查两侧根 Value 的活动保护。 */
static void testValueGraphCheckRoots(testvaluegraphcontract* pState)
{
	pState->RootsBlocked =
		pState->RootsBlocked &&
		testValueGraphCloneBlocked(pState->LeftRoot) &&
		testValueGraphCloneBlocked(pState->RightRoot);
	if ( pState->LeftRoot != NULL ) {
		xrtClearError();
		pState->SelfEqualBlocked =
			pState->SelfEqualBlocked &&
			!xrtValueEqual(pState->LeftRoot, pState->LeftRoot) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
}



/* 克隆整数句柄，并验证活动源路径不能重入。 */
static bool testValueGraphContractClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	testvaluegraphcontract* pState =
		(testvaluegraphcontract*)pUserData;
	int* pCopy;

	testValueGraphCheckRoots(pState);
	pState->ValuesBlocked =
		pState->ValuesBlocked &&
		testValueGraphCloneBlocked(pState->LeftValue);
	pCopy = (int*)xrtMalloc(sizeof(int));
	if ( pCopy == NULL ) {
		return false;
	}
	*pCopy = *(const int*)pHandle;
	*pClone = pCopy;
	pState->CloneCount++;
	return true;
}



/* 释放测试句柄。 */
static void testValueGraphContractDrop(ptr pHandle, ptr pUserData)
{
	testvaluegraphcontract* pState =
		(testvaluegraphcontract*)pUserData;

	xrtFree(pHandle);
	pState->DropCount++;
}



/* 按整数内容哈希，并验证当前元素与图根都处于保护状态。 */
static uint64 testValueGraphContractHash(ptr pHandle, ptr pUserData)
{
	testvaluegraphcontract* pState =
		(testvaluegraphcontract*)pUserData;
	const xvalue* pCurrent = pHandle == pState->LeftData
		? pState->LeftValue : pState->RightValue;

	testValueGraphCheckRoots(pState);
	pState->ValuesBlocked =
		pState->ValuesBlocked &&
		testValueGraphCloneBlocked(pCurrent);
	pState->HashCount++;
	return (uint64)*(const int*)pHandle;
}



/* 按整数内容比较，并验证两侧元素与活动祖先都处于保护状态。 */
static bool testValueGraphContractEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	testvaluegraphcontract* pState =
		(testvaluegraphcontract*)pUserData;

	testValueGraphCheckRoots(pState);
	pState->ValuesBlocked =
		pState->ValuesBlocked &&
		testValueGraphCloneBlocked(pState->LeftValue) &&
		testValueGraphCloneBlocked(pState->RightValue);
	pState->EqualCount++;
	return *(const int*)pLeft == *(const int*)pRight;
}



/* 创建一个由测试状态拥有策略域的整数句柄 Value。 */
static xvalue* testValueGraphContractHandle(
	int iValue,
	const xvaluehandleops* pOps,
	testvaluegraphcontract* pState
)
{
	int* pHandle = (int*)xrtMalloc(sizeof(int));
	ptr pTaken;

	if ( pHandle == NULL ) {
		return NULL;
	}
	*pHandle = iValue;
	pTaken = pHandle;
	return xrtValueHandleTake(&pTaken, pOps, pState);
}



/* 读取测试句柄的原生地址。 */
static ptr testValueGraphContractData(const xvalue* pValue)
{
	ptr pHandle = NULL;

	testRequire(
		xrtValueGetHandle(pValue, &pHandle, NULL, NULL),
		"graph contract expected Handle"
	);
	return pHandle;
}



/* 创建每层都重复引用同一子值的共享 DAG。 */
static xvalue* testValueGraphDiamond(xvalue* pLeaf, uint32 iDepth)
{
	xvalue* pCurrent = xrtValueRetain(pLeaf);

	for ( uint32 i = 0; (i < iDepth) && (pCurrent != NULL); i++ ) {
		xvalue* pParent = xrtValueArray();

		if ( (pParent == NULL) ||
			 !xrtValueArrayAppend(pParent, pCurrent) ||
			 !xrtValueArrayAppend(pParent, pCurrent) ) {
			xrtValueRelease(pParent);
			xrtValueRelease(pCurrent);
			return NULL;
		}
		xrtValueRelease(pCurrent);
		pCurrent = pParent;
	}
	return pCurrent;
}



/* 创建指定边深度的单分支值图。 */
static xvalue* testValueGraphChain(uint32 iDepth)
{
	xvalue* pCurrent = xrtValueInt(1);

	for ( uint32 i = 0; (i < iDepth) && (pCurrent != NULL); i++ ) {
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



/* 验证深克隆在 Clone 回调期间保护完整活动源路径。 */
static void testValueGraphCloneGuard(void)
{
	static const xvaluehandleops tOps = {
		testValueGraphContractClone,
		testValueGraphContractDrop,
		testValueGraphContractHash,
		testValueGraphContractEqual
	};
	testvaluegraphcontract tState = { 0 };
	xvalue* pRoot = xrtValueArray();
	xvalue* pHandle = testValueGraphContractHandle(7, &tOps, &tState);
	xvalue* pCopy;

	testRequire(
		(pRoot != NULL) && (pHandle != NULL) &&
		xrtValueArrayAppend(pRoot, pHandle),
		"graph clone guard fixture failed"
	);
	tState.LeftRoot = pRoot;
	tState.LeftValue = pHandle;
	tState.LeftData = testValueGraphContractData(pHandle);
	testValueGraphGuardReset(&tState);
	pCopy = xrtValueDeepClone(pRoot);
	testRequire(
		(pCopy != NULL) &&
		(tState.CloneCount == 1) &&
		tState.RootsBlocked &&
		tState.ValuesBlocked &&
		tState.SelfEqualBlocked,
		"graph Clone callback escaped active source guards"
	);
	tState.LeftRoot = NULL;
	tState.LeftValue = NULL;
	xrtValueRelease(pCopy);
	xrtValueRelease(pHandle);
	xrtValueRelease(pRoot);
	testRequire(tState.DropCount == 2, "graph clone guard leaked handles");
}



/* 验证共享 DAG 只比较一次叶值，并保护完整活动比较路径。 */
static void testValueGraphEqualMemo(void)
{
	static const xvaluehandleops tOps = {
		testValueGraphContractClone,
		testValueGraphContractDrop,
		testValueGraphContractHash,
		testValueGraphContractEqual
	};
	testvaluegraphcontract tState = { 0 };
	xvalue* pLeftLeaf = testValueGraphContractHandle(9, &tOps, &tState);
	xvalue* pRightLeaf = testValueGraphContractHandle(9, &tOps, &tState);
	xvalue* pLeft;
	xvalue* pRight;

	testRequire(
		(pLeftLeaf != NULL) && (pRightLeaf != NULL),
		"graph memo leaf fixture failed"
	);
	pLeft = testValueGraphDiamond(pLeftLeaf, 16);
	pRight = testValueGraphDiamond(pRightLeaf, 16);
	testRequire(
		(pLeft != NULL) && (pRight != NULL),
		"graph memo DAG fixture failed"
	);
	tState.LeftRoot = pLeft;
	tState.RightRoot = pRight;
	tState.LeftValue = pLeftLeaf;
	tState.RightValue = pRightLeaf;
	tState.LeftData = testValueGraphContractData(pLeftLeaf);
	tState.RightData = testValueGraphContractData(pRightLeaf);
	testValueGraphGuardReset(&tState);
	testRequire(
		xrtValueEqual(pLeft, pRight) &&
		(tState.EqualCount == 1) &&
		tState.RootsBlocked &&
		tState.ValuesBlocked &&
		tState.SelfEqualBlocked,
		"graph equality did not memoize or protect shared DAG"
	);
	tState.LeftRoot = NULL;
	tState.RightRoot = NULL;
	tState.LeftValue = NULL;
	tState.RightValue = NULL;
	xrtValueRelease(pLeftLeaf);
	xrtValueRelease(pRightLeaf);
	xrtValueRelease(pLeft);
	xrtValueRelease(pRight);
	testRequire(tState.DropCount == 2, "graph memo fixture leaked handles");
}



/* 验证嵌套 Set 比较期间保护图根和正在哈希的 Handle。 */
static void testValueGraphSetGuard(void)
{
	static const xvaluehandleops tOps = {
		testValueGraphContractClone,
		testValueGraphContractDrop,
		testValueGraphContractHash,
		testValueGraphContractEqual
	};
	testvaluegraphcontract tState = { 0 };
	xvalue* pLeftRoot = xrtValueArray();
	xvalue* pRightRoot = xrtValueArray();
	xvalue* pLeftSet = xrtValueSet();
	xvalue* pRightSet = xrtValueSet();
	xvalue* pLeftValue = testValueGraphContractHandle(11, &tOps, &tState);
	xvalue* pRightValue = testValueGraphContractHandle(11, &tOps, &tState);

	testRequire(
		(pLeftRoot != NULL) && (pRightRoot != NULL) &&
		(pLeftSet != NULL) && (pRightSet != NULL) &&
		(pLeftValue != NULL) && (pRightValue != NULL),
		"graph Set guard allocation failed"
	);
	tState.LeftData = testValueGraphContractData(pLeftValue);
	tState.RightData = testValueGraphContractData(pRightValue);
	testRequire(
		xrtValueSetAdd(pLeftSet, pLeftValue) &&
		xrtValueSetAdd(pRightSet, pRightValue) &&
		xrtValueArrayAppendTake(pLeftRoot, &pLeftSet) &&
		xrtValueArrayAppendTake(pRightRoot, &pRightSet),
		"graph Set guard fixture failed"
	);
	tState.LeftRoot = pLeftRoot;
	tState.RightRoot = pRightRoot;
	tState.LeftValue = pLeftValue;
	tState.RightValue = pRightValue;
	tState.HashCount = 0;
	tState.EqualCount = 0;
	testValueGraphGuardReset(&tState);
	testRequire(
		xrtValueEqual(pLeftRoot, pRightRoot) &&
		(tState.HashCount != 0) &&
		(tState.EqualCount != 0) &&
		tState.RootsBlocked &&
		tState.ValuesBlocked &&
		tState.SelfEqualBlocked,
		"graph Set callbacks escaped Value guards"
	);
	tState.LeftRoot = NULL;
	tState.RightRoot = NULL;
	tState.LeftValue = NULL;
	tState.RightValue = NULL;
	xrtValueRelease(pLeftValue);
	xrtValueRelease(pRightValue);
	xrtValueRelease(pLeftRoot);
	xrtValueRelease(pRightRoot);
	testRequire(tState.DropCount == 2, "graph Set guard leaked handles");
}



/* 失败克隆器故意遗留输出，库必须回收它并保留回调错误。 */
static bool testValueGraphBrokenClone(
	ptr pHandle,
	ptr* pClone,
	ptr pUserData
)
{
	int64 iValue;
	int* pCopy = (int*)xrtMalloc(sizeof(int));

	(void)pUserData;
	if ( pCopy == NULL ) {
		return false;
	}
	*pCopy = *(const int*)pHandle;
	*pClone = pCopy;
	(void)xrtValueGetInt(NULL, &iValue);
	return false;
}



/* 验证句柄比较策略和破约克隆输出都有确定口径。 */
static void testValueGraphHandleContracts(void)
{
	static const xvaluehandleops tUncomparable = {
		NULL,
		testValueGraphContractDrop,
		NULL,
		NULL
	};
	static const xvaluehandleops tBrokenClone = {
		testValueGraphBrokenClone,
		testValueGraphContractDrop,
		NULL,
		NULL
	};
	testvaluegraphcontract tState = { 0 };
	xvalue* pLeft = testValueGraphContractHandle(3, &tUncomparable, &tState);
	xvalue* pRight = testValueGraphContractHandle(3, &tUncomparable, &tState);
	xvalue* pBroken = testValueGraphContractHandle(5, &tBrokenClone, &tState);

	testRequire(
		(pLeft != NULL) && (pRight != NULL) && (pBroken != NULL),
		"graph handle contract fixture failed"
	);
	testRequire(xrtValueEqual(pLeft, pLeft), "handle identity should be equal");
	xrtClearError();
	testRequire(
		!xrtValueEqual(pLeft, pRight) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"uncomparable handles did not report type error"
	);
	xrtClearError();
	testRequire(
		(xrtValueDeepClone(pBroken) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(tState.DropCount == 1),
		"broken Clone output was not reclaimed with its error"
	);
	xrtValueRelease(pBroken);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	testRequire(tState.DropCount == 4, "graph handle contract leaked resources");
}



/* 验证结构相等不要求两侧具有相同的共享拓扑。 */
static void testValueGraphTopology(void)
{
	xvalue* pLeft = xrtValueArray();
	xvalue* pRight = xrtValueArray();
	xvalue* pShared = xrtValueObject();
	xvalue* pFirst = xrtValueObject();
	xvalue* pSecond = xrtValueObject();

	testRequire(
		(pLeft != NULL) && (pRight != NULL) &&
		(pShared != NULL) && (pFirst != NULL) && (pSecond != NULL) &&
		xrtValueObjectSetNew(
			pShared,
			XRT_STR_LITERAL("id"),
			xrtValueInt(1)
		) &&
		xrtValueObjectSetNew(
			pFirst,
			XRT_STR_LITERAL("id"),
			xrtValueInt(1)
		) &&
		xrtValueObjectSetNew(
			pSecond,
			XRT_STR_LITERAL("id"),
			xrtValueInt(1)
		) &&
		xrtValueArrayAppend(pLeft, pShared) &&
		xrtValueArrayAppend(pLeft, pShared) &&
		xrtValueArrayAppend(pRight, pFirst) &&
		xrtValueArrayAppend(pRight, pSecond),
		"graph topology fixture failed"
	);
	testRequire(
		xrtValueEqual(pLeft, pRight),
		"graph equality incorrectly required shared topology"
	);
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	xrtValueRelease(pShared);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
}



/* 验证图深度上限按从根开始的值层级统一执行。 */
static void testValueGraphDepth(void)
{
	xvalue* pAllowed = testValueGraphChain(XRT_VALUE_DEPTH_MAX - 1u);
	xvalue* pTooDeepLeft = testValueGraphChain(XRT_VALUE_DEPTH_MAX);
	xvalue* pTooDeepRight = testValueGraphChain(XRT_VALUE_DEPTH_MAX);
	xvalue* pCopy;

	testRequire(
		(pAllowed != NULL) &&
		(pTooDeepLeft != NULL) &&
		(pTooDeepRight != NULL),
		"graph depth fixture failed"
	);
	pCopy = xrtValueDeepClone(pAllowed);
	testRequire(pCopy != NULL, "maximum allowed graph depth failed");
	xrtValueRelease(pCopy);

	xrtClearError();
	testRequire(
		(xrtValueDeepClone(pTooDeepLeft) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"deep clone accepted graph beyond depth limit"
	);
	xrtClearError();
	testRequire(
		!xrtValueEqual(pTooDeepLeft, pTooDeepRight) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"graph equality accepted graph beyond depth limit"
	);
	xrtValueRelease(pTooDeepRight);
	xrtValueRelease(pTooDeepLeft);
	xrtValueRelease(pAllowed);
}



/* 验证空必需输入在任何图状态分配前报告参数错误。 */
static void testValueGraphArguments(void)
{
	xvalue* pValue = xrtValueInt(1);

	testRequire(pValue != NULL, "graph argument fixture failed");
	xrtClearError();
	testRequire(
		(xrtValueDeepClone(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"deep clone NULL error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtValueEqual(NULL, pValue) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"graph equality NULL error mismatch"
	);
	xrtValueRelease(pValue);
}



/* 运行 Value 图回调、身份、错误和深度合同回归。 */
int main(void)
{
	testValueGraphCloneGuard();
	testValueGraphEqualMemo();
	testValueGraphSetGuard();
	testValueGraphHandleContracts();
	testValueGraphTopology();
	testValueGraphDepth();
	testValueGraphArguments();
	printf("[PASS] value graph contract\n");
	return 0;
}
