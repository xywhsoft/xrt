#include "../test.h"



/* 高级集合回调测试共用的父外壳与观测结果。 */
typedef struct testvaluecollectionstate {
	xvalue* Left;
	xvalue* Right;
	size_t DropCount;
	bool HashEntered;
	bool LeftBlocked;
	bool RightBlocked;
	bool PairBlocked;
	bool DropCloneBlocked;
	bool DropWriteBlocked;
} testvaluecollectionstate;



/* 释放句柄并验证批量提交释放旧值时不能重入目标对象。 */
static void testValueCollectionDrop(ptr pHandle, ptr pUserData)
{
	testvaluecollectionstate* pState =
		(testvaluecollectionstate*)pUserData;
	xvalue* pClone;

	pState->DropCount++;
	if ( pState->Left != NULL ) {
		xrtClearError();
		pClone = xrtValueClone(pState->Left);
		pState->DropCloneBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);

		xrtClearError();
		pState->DropWriteBlocked = !xrtValueObjectSetNew(
			pState->Left,
			XRT_STR_LITERAL("reentry"),
			xrtValueInt(99)
		) && (xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	xrtFree(pHandle);
}



/* 计算句柄哈希并验证 Set 高级运算同时保护左右 Value 外壳。 */
static uint64 testValueCollectionHash(ptr pHandle, ptr pUserData)
{
	testvaluecollectionstate* pState =
		(testvaluecollectionstate*)pUserData;
	xvalue* pClone;

	if ( !pState->HashEntered && (pState->Left != NULL) ) {
		pState->HashEntered = true;

		xrtClearError();
		pClone = xrtValueClone(pState->Left);
		pState->LeftBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);

		xrtClearError();
		pClone = xrtValueClone(pState->Right);
		pState->RightBlocked =
			(pClone == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
		xrtValueRelease(pClone);

		xrtClearError();
		pState->PairBlocked =
			!xrtValueSetEqual(pState->Left, pState->Right) &&
			(xrtErrorKind(xrtGetError()) == XERR_STATE);
	}
	return (uint64)*(const int*)pHandle;
}



/* 按整数内容比较两个测试句柄。 */
static bool testValueCollectionEqual(
	ptr pLeft,
	ptr pRight,
	ptr pUserData
)
{
	(void)pUserData;
	return *(const int*)pLeft == *(const int*)pRight;
}



/* 创建由测试策略拥有的整数句柄值。 */
static xvalue* testValueCollectionHandle(
	int iValue,
	const xvaluehandleops* pOps,
	testvaluecollectionstate* pState
)
{
	int* pHandle = (int*)xrtMalloc(sizeof(int));
	xvalue* pValue;

	if ( pHandle == NULL ) {
		return NULL;
	}
	*pHandle = iValue;
	pValue = xrtValueHandleTake((ptr*)&pHandle, pOps, pState);
	if ( pValue == NULL ) {
		xrtFree(pHandle);
	}
	return pValue;
}



/* 重置一次 Set 高级运算的回调观测结果。 */
static void testValueCollectionResetHash(testvaluecollectionstate* pState)
{
	pState->HashEntered = false;
	pState->LeftBlocked = false;
	pState->RightBlocked = false;
	pState->PairBlocked = false;
}



/* 验证映射提交释放被替换值期间，目标新状态不可重入。 */
static void testValueCollectionCommitReentry(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueCollectionDrop,
		testValueCollectionHash,
		testValueCollectionEqual
	};
	testvaluecollectionstate tState = { 0 };
	xvalue* pTarget = xrtValueObject();
	xvalue* pSource = xrtValueObject();
	int64 iValue = 0;

	testRequire(
		(pTarget != NULL) && (pSource != NULL) &&
		xrtValueObjectSetNew(
			pTarget,
			XRT_STR_LITERAL("value"),
			testValueCollectionHandle(7, &tOps, &tState)
		) &&
		xrtValueObjectSetNew(
			pSource,
			XRT_STR_LITERAL("value"),
			xrtValueInt(8)
		),
		"collection commit reentry fixture failed"
	);
	tState.Left = pTarget;
	testRequire(
		xrtValueObjectMerge(
			pTarget,
			pSource,
			XVALUE_MERGE_REPLACE
		),
		"collection commit reentry merge failed"
	);
	testRequire(
		(tState.DropCount == 1) &&
		tState.DropCloneBlocked &&
		tState.DropWriteBlocked &&
		xrtValueGetInt(
			xrtValueObjectGet(pTarget, XRT_STR_LITERAL("value")),
			&iValue
		) &&
		(iValue == 8) &&
		!xrtValueObjectHas(
			pTarget,
			XRT_STR_LITERAL("reentry")
		),
		"collection commit release escaped parent guard"
	);

	tState.Left = NULL;
	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
}



/* 验证 Set 合并、二元代数和关系判断都保护两个父外壳。 */
static void testValueCollectionSetReentry(void)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueCollectionDrop,
		testValueCollectionHash,
		testValueCollectionEqual
	};
	testvaluecollectionstate tState = { 0 };
	xvalue* pLeft = xrtValueSet();
	xvalue* pRight = xrtValueSet();
	xvalue* pUnion;
	xvalue* pMerged;

	testRequire(
		(pLeft != NULL) && (pRight != NULL) &&
		xrtValueSetAddNew(
			pLeft,
			testValueCollectionHandle(1, &tOps, &tState)
		) &&
		xrtValueSetAddNew(
			pRight,
			testValueCollectionHandle(2, &tOps, &tState)
		),
		"collection set reentry fixture failed"
	);

	tState.Left = pLeft;
	tState.Right = pRight;
	testValueCollectionResetHash(&tState);
	pUnion = xrtValueSetUnion(pLeft, pRight);
	testRequire(
		(pUnion != NULL) &&
		(xrtValueCount(pUnion) == 2) &&
		tState.HashEntered &&
		tState.LeftBlocked &&
		tState.RightBlocked &&
		tState.PairBlocked,
		"set binary operation escaped Value guard"
	);

	testValueCollectionResetHash(&tState);
	testRequire(
		xrtValueSetIsDisjoint(pLeft, pRight) &&
		tState.HashEntered &&
		tState.LeftBlocked &&
		tState.RightBlocked &&
		tState.PairBlocked,
		"set relation escaped Value guard"
	);

	pMerged = xrtValueClone(pLeft);
	testRequire(pMerged != NULL, "set merge reentry clone failed");
	tState.Left = pMerged;
	tState.Right = pRight;
	testValueCollectionResetHash(&tState);
	testRequire(
		xrtValueSetMerge(pMerged, pRight) &&
		(xrtValueCount(pMerged) == 2) &&
		tState.HashEntered &&
		tState.LeftBlocked &&
		tState.RightBlocked &&
		tState.PairBlocked,
		"set merge escaped Value guard"
	);

	tState.Left = NULL;
	tState.Right = NULL;
	xrtValueRelease(pMerged);
	xrtValueRelease(pUnion);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
	testRequire(
		tState.DropCount == 2,
		"collection set reentry fixture leaked handles"
	);
}



/* 运行 Value 高级集合提交与回调重入合同回归。 */
int main(void)
{
	testValueCollectionCommitReentry();
	testValueCollectionSetReentry();
	printf("[PASS] value collection contract\n");
	return 0;
}
