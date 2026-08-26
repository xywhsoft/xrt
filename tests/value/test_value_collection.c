#include "../test.h"



/* 记录批量提交后 Object 拥有值的最终释放顺序。 */
typedef struct testvaluecollectiondropstate {
	size_t Count;
	int Values[4];
} testvaluecollectiondropstate;



/* 释放测试句柄并记录整数值。 */
static void testValueCollectionHandleDrop(ptr pHandle, ptr pUserData)
{
	testvaluecollectiondropstate* pState =
		(testvaluecollectiondropstate*)pUserData;

	if ( pState->Count < 4 ) {
		pState->Values[pState->Count] = *(int*)pHandle;
	}
	pState->Count++;
	xrtFree(pHandle);
}



/* 创建由 Object 独占管理的整数测试句柄。 */
static xvalue* testValueCollectionHandle(
	int iValue,
	testvaluecollectiondropstate* pState
)
{
	static const xvaluehandleops tOps = {
		NULL,
		testValueCollectionHandleDrop,
		NULL,
		NULL
	};
	int* pHandle = (int*)xrtMalloc(sizeof(int));
	xvalue* pValue;

	if ( pHandle == NULL ) {
		return NULL;
	}
	*pHandle = iValue;
	pValue = xrtValueHandleTake((ptr*)&pHandle, &tOps, pState);
	if ( pValue == NULL ) {
		xrtFree(pHandle);
	}
	return pValue;
}



/* 读取动态值整数并立即验证类型。 */
static int64 testValueCollectionInt(const xvalue* pValue)
{
	int64 iValue = 0;

	testRequire(xrtValueGetInt(pValue, &iValue), "expected collection integer");
	return iValue;
}



/* 创建带两个整数元素的数组测试夹具。 */
static xvalue* testValueCollectionArray(int64 iFirst, int64 iSecond)
{
	xvalue* pArray = xrtValueArray();

	if ( (pArray == NULL) ||
		 !xrtValueArrayAppendNew(pArray, xrtValueInt(iFirst)) ||
		 !xrtValueArrayAppendNew(pArray, xrtValueInt(iSecond)) ) {
		xrtValueRelease(pArray);
		return NULL;
	}
	return pArray;
}



/* 验证数组扩展、连接、自扩展和提交期环检测。 */
static void testValueArrayCollection(void)
{
	xvalue* pTarget = testValueCollectionArray(1, 2);
	xvalue* pSource = testValueCollectionArray(3, 4);
	xvalue* pJoined;
	xvalue* pChild;
	xvalue* pParent;

	testRequire((pTarget != NULL) && (pSource != NULL), "array collection fixture failed");
	pJoined = xrtValueArrayConcat(pTarget, pSource);
	testRequire(
		(pJoined != NULL) && (xrtValueCount(pJoined) == 4) &&
		(testValueCollectionInt(xrtValueArrayGet(pJoined, 0)) == 1) &&
		(testValueCollectionInt(xrtValueArrayGet(pJoined, 3)) == 4),
		"array concat mismatch"
	);
	testRequire(xrtValueArrayExtend(pTarget, pSource), "array extend failed");
	testRequire(
		(xrtValueCount(pTarget) == 4) && (xrtValueCount(pSource) == 2),
		"array extend changed source"
	);
	testRequire(xrtValueArrayExtend(pTarget, pTarget), "array self extend failed");
	testRequire(
		(xrtValueCount(pTarget) == 8) &&
		(testValueCollectionInt(xrtValueArrayGet(pTarget, 4)) == 1) &&
		(testValueCollectionInt(xrtValueArrayGet(pTarget, 7)) == 4),
		"array self extend mismatch"
	);

	/* 来源先安全地持有目标，批量提交不得因此把目标改造成自环。 */
	pChild = testValueCollectionArray(7, 8);
	pParent = xrtValueArray();
	testRequire(
		(pChild != NULL) && (pParent != NULL) &&
		xrtValueArrayAppendNew(pParent, xrtValueInt(9)) &&
		xrtValueArrayAppend(pParent, pChild),
		"array commit cycle fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueArrayExtend(pChild, pParent),
		"array commit accepted target shell cycle"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtValueCount(pChild) == 2) &&
		(testValueCollectionInt(xrtValueArrayGet(pChild, 0)) == 7),
		"array cycle failure changed target"
	);

	xrtValueRelease(pParent);
	xrtValueRelease(pChild);
	xrtValueRelease(pJoined);
	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
}



/* 创建整数键合并的目标与来源夹具。 */
static void testValueIntMapFixtures(xvalue** pTarget, xvalue** pSource)
{
	*pTarget = xrtValueIntMap();
	*pSource = xrtValueIntMap();
	testRequire(
		(*pTarget != NULL) && (*pSource != NULL) &&
		xrtValueIntMapSetNew(*pTarget, 1, xrtValueInt(10)) &&
		xrtValueIntMapSetNew(*pTarget, 3, xrtValueInt(30)) &&
		xrtValueIntMapSetNew(*pSource, 1, xrtValueInt(11)) &&
		xrtValueIntMapSetNew(*pSource, 2, xrtValueInt(20)),
		"int map merge fixture failed"
	);
}



/* 验证整数键映射三种冲突策略及冲突失败原子性。 */
static void testValueIntMapCollection(void)
{
	xvalue* pTarget;
	xvalue* pSource;
	xvalue* pKeep;
	xvalue* pReplace;
	xvalue* pError;

	testValueIntMapFixtures(&pTarget, &pSource);
	pKeep = xrtValueClone(pTarget);
	pReplace = xrtValueClone(pTarget);
	pError = xrtValueClone(pTarget);
	testRequire(
		(pKeep != NULL) && (pReplace != NULL) && (pError != NULL),
		"int map merge clone failed"
	);
	testRequire(
		xrtValueIntMapMerge(pKeep, pSource, XVALUE_MERGE_KEEP) &&
		(testValueCollectionInt(xrtValueIntMapGet(pKeep, 1)) == 10) &&
		(testValueCollectionInt(xrtValueIntMapGet(pKeep, 2)) == 20),
		"int map keep merge mismatch"
	);
	testRequire(
		xrtValueIntMapMerge(pReplace, pSource, XVALUE_MERGE_REPLACE) &&
		(testValueCollectionInt(xrtValueIntMapGet(pReplace, 1)) == 11) &&
		(testValueCollectionInt(xrtValueIntMapGet(pReplace, 2)) == 20),
		"int map replace merge mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtValueIntMapMerge(pError, pSource, XVALUE_MERGE_ERROR),
		"int map conflict merge should fail"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(xrtValueCount(pError) == 2) &&
		!xrtValueIntMapHas(pError, 2) &&
		(testValueCollectionInt(xrtValueIntMapGet(pError, 1)) == 10),
		"int map conflict changed target"
	);

	xrtValueRelease(pError);
	xrtValueRelease(pReplace);
	xrtValueRelease(pKeep);
	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
}



/* 创建字符串键对象合并的目标与来源夹具。 */
static void testValueObjectFixtures(xvalue** pTarget, xvalue** pSource)
{
	*pTarget = xrtValueObject();
	*pSource = xrtValueObject();
	testRequire(
		(*pTarget != NULL) && (*pSource != NULL) &&
		xrtValueObjectSetNew(*pTarget, XRT_STR_LITERAL("a"), xrtValueInt(1)) &&
		xrtValueObjectSetNew(*pTarget, XRT_STR_LITERAL("c"), xrtValueInt(3)) &&
		xrtValueObjectSetNew(*pSource, XRT_STR_LITERAL("a"), xrtValueInt(10)) &&
		xrtValueObjectSetNew(*pSource, XRT_STR_LITERAL("b"), xrtValueInt(2)),
		"object merge fixture failed"
	);
}



/* 验证对象合并保持目标键位置并按来源顺序追加新键。 */
static void testValueObjectCollection(void)
{
	const char* arrOrder[] = { "a", "c", "b" };
	xvalue* pTarget;
	xvalue* pSource;
	xvalue* pKeep;
	xvalue* pReplace;
	xvalue* pEmpty;
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	size_t i = 0;

	testValueObjectFixtures(&pTarget, &pSource);
	pKeep = xrtValueClone(pTarget);
	pReplace = xrtValueClone(pTarget);
	pEmpty = xrtValueObject();
	testRequire(
		(pKeep != NULL) && (pReplace != NULL) && (pEmpty != NULL) &&
		xrtValueObjectMerge(pKeep, pSource, XVALUE_MERGE_KEEP) &&
		xrtValueObjectMerge(pReplace, pSource, XVALUE_MERGE_REPLACE) &&
		xrtValueObjectMerge(pReplace, pEmpty, XVALUE_MERGE_ERROR),
		"object merge failed"
	);
	testRequire(
		(testValueCollectionInt(xrtValueObjectGet(pKeep, XRT_STR_LITERAL("a"))) == 1) &&
		(testValueCollectionInt(xrtValueObjectGet(pReplace, XRT_STR_LITERAL("a"))) == 10),
		"object conflict policy mismatch"
	);
	testRequire(xrtValueIterBegin(pReplace, &tIterator), "object merge iterator failed");
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		(void)pItem;
		testRequire(
			(i < 3) && (Key.Type == XVALUE_KEY_STRING) &&
			(Key.String.Size == 1) && (Key.String.Data[0] == arrOrder[i][0]),
			"object merge order mismatch"
		);
		i++;
	}
	xrtValueIterEnd(&tIterator);
	testRequire(i == 3, "object merge count mismatch");
	xrtClearError();
	testRequire(
		!xrtValueObjectMerge(
			pKeep,
			pSource,
			(xvaluemergepolicy)99
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"object invalid merge policy mismatch"
	);

	xrtValueRelease(pEmpty);
	xrtValueRelease(pReplace);
	xrtValueRelease(pKeep);
	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
}



/* 验证对象批量提交保留目标的 LIFO 析构策略。 */
static void testValueObjectCollectionLifo(void)
{
	testvaluecollectiondropstate tState = { 0 };
	xvalue* pTarget = xrtValueObjectLifo();
	xvalue* pSource = xrtValueObject();

	testRequire(
		(pTarget != NULL) && (pSource != NULL) &&
		xrtValueObjectSetNew(
			pSource,
			XRT_STR_LITERAL("first"),
			testValueCollectionHandle(1, &tState)
		) &&
		xrtValueObjectSetNew(
			pSource,
			XRT_STR_LITERAL("second"),
			testValueCollectionHandle(2, &tState)
		) &&
		xrtValueObjectMerge(pTarget, pSource, XVALUE_MERGE_KEEP),
		"LIFO object merge fixture failed"
	);
	xrtValueRelease(pSource);
	testRequire(tState.Count == 0, "LIFO object merge dropped shared values early");
	xrtValueRelease(pTarget);
	testRequire(
		(tState.Count == 2) &&
		(tState.Values[0] == 2) &&
		(tState.Values[1] == 1),
		"object merge replaced the target LIFO destruction policy"
	);
}



/* 创建具有部分重叠元素的左右 Set 夹具。 */
static void testValueSetFixtures(xvalue** pLeft, xvalue** pRight)
{
	*pLeft = xrtValueSet();
	*pRight = xrtValueSet();
	testRequire(
		(*pLeft != NULL) && (*pRight != NULL) &&
		xrtValueSetAddNew(*pLeft, xrtValueInt(1)) &&
		xrtValueSetAddNew(*pLeft, xrtValueInt(2)) &&
		xrtValueSetAddNew(*pRight, xrtValueFloat(2.0)) &&
		xrtValueSetAddNew(*pRight, xrtValueInt(3)),
		"set collection fixture failed"
	);
}



/* 验证 Value Set 完整代数、顺序、关系判断和原地合并。 */
static void testValueSetCollection(void)
{
	int64 arrUnionOrder[] = { 1, 2, 3 };
	xvalue* pLeft;
	xvalue* pRight;
	xvalue* pUnion;
	xvalue* pIntersection;
	xvalue* pDifference;
	xvalue* pSymmetric;
	xvalue* pMerged;
	xvalueiter tIterator;
	xvalue* pItem;
	size_t i = 0;

	testValueSetFixtures(&pLeft, &pRight);
	pUnion = xrtValueSetUnion(pLeft, pRight);
	pIntersection = xrtValueSetIntersection(pLeft, pRight);
	pDifference = xrtValueSetDifference(pLeft, pRight);
	pSymmetric = xrtValueSetSymmetricDifference(pLeft, pRight);
	testRequire(
		(pUnion != NULL) && (pIntersection != NULL) &&
		(pDifference != NULL) && (pSymmetric != NULL),
		"set algebra failed"
	);
	testRequire(
		(xrtValueCount(pUnion) == 3) &&
		(xrtValueCount(pIntersection) == 1) &&
		(xrtValueCount(pDifference) == 1) &&
		(xrtValueCount(pSymmetric) == 2),
		"set algebra count mismatch"
	);
	testRequire(xrtValueIterBegin(pUnion, &tIterator), "set union iterator failed");
	while ( (pItem = xrtValueIterNext(&tIterator, NULL)) != NULL ) {
		testRequire(
			(i < 3) && (testValueCollectionInt(pItem) == arrUnionOrder[i]),
			"set union order mismatch"
		);
		i++;
	}
	xrtValueIterEnd(&tIterator);
	testRequire(i == 3, "set union iteration count mismatch");
	testRequire(
		xrtValueSetIsSubset(pIntersection, pUnion, true) &&
		xrtValueSetIsSuperset(pUnion, pLeft, true) &&
		!xrtValueSetIsSubset(pUnion, pLeft, false) &&
		!xrtValueSetIsDisjoint(pLeft, pRight) &&
		xrtValueSetIsDisjoint(pDifference, pRight),
		"set relation mismatch"
	);
	pMerged = xrtValueClone(pLeft);
	testRequire(
		(pMerged != NULL) && xrtValueSetMerge(pMerged, pRight) &&
		(xrtValueCount(pMerged) == 3) &&
		xrtValueSetEqual(pMerged, pUnion),
		"set merge mismatch"
	);

	xrtValueRelease(pMerged);
	xrtValueRelease(pSymmetric);
	xrtValueRelease(pDifference);
	xrtValueRelease(pIntersection);
	xrtValueRelease(pUnion);
	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
}



/* 验证空目标和共享 backing 快路径保持 COW 与环检测语义。 */
static void testValueCollectionFastPaths(void)
{
	xvalue* pArraySource = testValueCollectionArray(1, 2);
	xvalue* pArrayTarget = xrtValueArray();
	xvalue* pArrayCycle = xrtValueArray();
	xvalue* pArrayParent = xrtValueArray();
	xvalue* pObjectSource = xrtValueObject();
	xvalue* pObjectTarget = xrtValueObject();
	xvalue* pObjectCycle = xrtValueObject();
	xvalue* pObjectParent = xrtValueObject();
	xvalue* pSetSource = xrtValueSet();
	xvalue* pSetTarget = xrtValueSet();
	xvalue* pSetShared;
	xvalue* pSetUnion;
	xvalue* pSetDifference;

	testRequire(
		(pArraySource != NULL) &&
		(pArrayTarget != NULL) &&
		(pArrayCycle != NULL) &&
		(pArrayParent != NULL) &&
		(pObjectSource != NULL) &&
		(pObjectTarget != NULL) &&
		(pObjectCycle != NULL) &&
		(pObjectParent != NULL) &&
		(pSetSource != NULL) &&
		(pSetTarget != NULL),
		"collection fast-path fixture failed"
	);

	testRequire(
		xrtValueArrayExtend(pArrayTarget, pArraySource) &&
		xrtValueArraySetNew(pArrayTarget, 0, xrtValueInt(9)) &&
		(testValueCollectionInt(
			xrtValueArrayGet(pArraySource, 0)
		 ) == 1) &&
		(testValueCollectionInt(
			xrtValueArrayGet(pArrayTarget, 0)
		 ) == 9),
		"empty array fast path broke COW isolation"
	);
	testRequire(
		xrtValueArrayAppend(pArrayParent, pArrayCycle),
		"array fast-path cycle fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueArrayExtend(pArrayCycle, pArrayParent) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtValueCount(pArrayCycle) == 0),
		"empty array fast path bypassed cycle guard"
	);

	testRequire(
		xrtValueObjectSetNew(
			pObjectSource,
			XRT_STR_LITERAL("value"),
			xrtValueInt(1)
		) &&
		xrtValueObjectMerge(
			pObjectTarget,
			pObjectSource,
			XVALUE_MERGE_REPLACE
		) &&
		xrtValueObjectSetNew(
			pObjectTarget,
			XRT_STR_LITERAL("value"),
			xrtValueInt(2)
		) &&
		(testValueCollectionInt(xrtValueObjectGet(
			pObjectSource,
			XRT_STR_LITERAL("value")
		 )) == 1),
		"empty object fast path broke COW isolation"
	);
	testRequire(
		xrtValueObjectSet(
			pObjectParent,
			XRT_STR_LITERAL("child"),
			pObjectCycle
		),
		"object fast-path cycle fixture failed"
	);
	xrtClearError();
	testRequire(
		!xrtValueObjectMerge(
			pObjectCycle,
			pObjectParent,
			XVALUE_MERGE_KEEP
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtValueCount(pObjectCycle) == 0),
		"empty object fast path bypassed cycle guard"
	);

	testRequire(
		xrtValueSetAddNew(pSetSource, xrtValueInt(1)) &&
		xrtValueSetMerge(pSetTarget, pSetSource) &&
		xrtValueSetAddNew(pSetTarget, xrtValueInt(2)) &&
		(xrtValueCount(pSetSource) == 1) &&
		(xrtValueCount(pSetTarget) == 2),
		"empty set fast path broke COW isolation"
	);
	pSetShared = xrtValueClone(pSetSource);
	pSetUnion = xrtValueSetUnion(pSetSource, pSetShared);
	pSetDifference = xrtValueSetDifference(pSetSource, pSetShared);
	testRequire(
		(pSetShared != NULL) &&
		(pSetUnion != NULL) &&
		(pSetDifference != NULL) &&
		xrtValueSetEqual(pSetSource, pSetUnion) &&
		(xrtValueCount(pSetDifference) == 0),
		"shared set backing identity mismatch"
	);

	xrtValueRelease(pSetDifference);
	xrtValueRelease(pSetUnion);
	xrtValueRelease(pSetShared);
	xrtValueRelease(pSetTarget);
	xrtValueRelease(pSetSource);
	xrtValueRelease(pObjectParent);
	xrtValueRelease(pObjectCycle);
	xrtValueRelease(pObjectTarget);
	xrtValueRelease(pObjectSource);
	xrtValueRelease(pArrayParent);
	xrtValueRelease(pArrayCycle);
	xrtValueRelease(pArrayTarget);
	xrtValueRelease(pArraySource);
}



/* 验证高级集合入口保持严格类型错误。 */
static void testValueCollectionErrors(void)
{
	xvalue* pArray = xrtValueArray();
	xvalue* pSet = xrtValueSet();
	xvalue* pResult;

	testRequire((pArray != NULL) && (pSet != NULL), "collection error fixture failed");
	xrtClearError();
	pResult = xrtValueSetUnion(pArray, pSet);
	testRequire(
		(pResult == NULL) && (xrtErrorKind(xrtGetError()) == XERR_TYPE),
		"collection type error mismatch"
	);
	xrtValueRelease(pSet);
	xrtValueRelease(pArray);
}



/* 运行动态值高级集合回归。 */
int main(void)
{
	testValueArrayCollection();
	testValueIntMapCollection();
	testValueObjectCollection();
	testValueObjectCollectionLifo();
	testValueSetCollection();
	testValueCollectionFastPaths();
	testValueCollectionErrors();
	printf("[PASS] value collection\n");
	return 0;
}
