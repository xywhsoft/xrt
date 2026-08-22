#include "../internal/xrt_value.h"



#if defined(XRT_FEATURE_VALUE_COLLECTION)

/* 映射合并预检记录新增键、冲突和实际替换。 */
typedef struct xvaluemergeplan {
	size_t Missing;
	bool Conflict;
	bool Replacement;
} xvaluemergeplan;



/* Set 二元运算共用一个经过验证的分派入口。 */
typedef enum xvaluesetoperation {
	XVALUE_SET_UNION = 0,
	XVALUE_SET_INTERSECTION,
	XVALUE_SET_DIFFERENCE,
	XVALUE_SET_SYMMETRIC_DIFFERENCE
} xvaluesetoperation;



/* Set 关系判断共用一个经过验证和重入保护的分派入口。 */
typedef enum xvaluesetrelation {
	XVALUE_SET_SUBSET = 0,
	XVALUE_SET_SUPERSET,
	XVALUE_SET_DISJOINT,
	XVALUE_SET_EQUAL
} xvaluesetrelation;



/* 验证两个值都存在且具有相同的指定容器类型。 */
static bool __xrtValueCollectionPair(
	const xvalue* pLeft,
	const xvalue* pRight,
	xvaluetype Type
)
{
	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pLeft->Flags & XRT_VALUE_FLAG_BUSY) != 0) ||
		 ((pRight->Flags & XRT_VALUE_FLAG_BUSY) != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pLeft->Type != (uint16)Type) ||
		 (pRight->Type != (uint16)Type) ) {
		__xrtErrorSetType();
		return false;
	}
	return true;
}



/* 在底层 Set 策略回调期间保护两个 Value 外壳。 */
static bool __xrtValueCollectionProtect(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	const xvalue* tValues[2] = { pLeft, pRight };

	return __xrtValueCallbackProtect(tValues, 2);
}



/* 解除两个 Value 外壳的底层 Set 策略回调保护。 */
static void __xrtValueCollectionUnprotect(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	const xvalue* tValues[2] = { pLeft, pRight };

	__xrtValueCallbackUnprotect(tValues, 2);
}



/* 验证映射冲突策略属于公开枚举。 */
static bool __xrtValueMergePolicyValid(xvaluemergepolicy Policy)
{
	if ( (Policy < XVALUE_MERGE_KEEP) ||
		 (Policy > XVALUE_MERGE_ERROR) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 提交完整准备容器并始终释放其临时外壳。 */
static bool __xrtValueCollectionCommit(
	xvalue* pTarget,
	xvalue* pPrepared
)
{
	bool bResult = __xrtValueContainerCommit(pTarget, pPrepared);

	/* 释放被替换 backing 时，旧值 Drop 不能重入已经提交的新目标。 */
	if ( bResult ) {
		pTarget->Flags |= XRT_VALUE_FLAG_BUSY;
	}
	xrtValueRelease(pPrepared);
	if ( bResult ) {
		pTarget->Flags &= ~XRT_VALUE_FLAG_BUSY;
	}
	return bResult;
}



/* 把来源数组快照逐项追加到已经独立的准备数组。 */
static bool __xrtValueArrayAppendAll(
	xvalue* pTarget,
	const xvalue* pSource
)
{
	xvalueiter tIterator;
	xvalue* pItem;

	if ( !xrtValueIterBegin(pSource, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, NULL)) != NULL ) {
		if ( !xrtValueArrayAppend(pTarget, pItem) ) {
			xrtValueIterEnd(&tIterator);
			return false;
		}
	}
	xrtValueIterEnd(&tIterator);
	return true;
}



/* 失败原子地把来源数组全部追加到目标数组，允许来源与目标相同。 */
XRT_API bool xrtValueArrayExtend(
	xvalue* pTarget,
	const xvalue* pSource
)
{
	size_t iTargetCount;
	size_t iSourceCount;
	xvalue* pPrepared;

	if ( !__xrtValueCollectionPair(pTarget, pSource, XVALUE_ARRAY) ) {
		return false;
	}
	iSourceCount = xrtValueCount(pSource);
	if ( iSourceCount == 0 ) {
		return true;
	}
	iTargetCount = xrtValueCount(pTarget);
	if ( iSourceCount > (SIZE_MAX - iTargetCount) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	if ( iTargetCount == 0 ) {
		pPrepared = xrtValueClone(pSource);
		return pPrepared != NULL
			? __xrtValueCollectionCommit(pTarget, pPrepared)
			: false;
	}

	/* 所有写入先落到 COW 准备外壳，完整成功后才替换目标。 */
	pPrepared = xrtValueClone(pTarget);
	if ( pPrepared == NULL ) {
		return false;
	}
	if ( !xrtValueReserve(pPrepared, iTargetCount + iSourceCount) ||
		 !__xrtValueArrayAppendAll(pPrepared, pSource) ) {
		xrtValueRelease(pPrepared);
		return false;
	}
	return __xrtValueCollectionCommit(pTarget, pPrepared);
}



/* 创建按左右顺序连接的新数组。 */
XRT_API xvalue* xrtValueArrayConcat(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	xvalue* pResult;

	if ( !__xrtValueCollectionPair(pLeft, pRight, XVALUE_ARRAY) ) {
		return NULL;
	}
	if ( xrtValueCount(pLeft) == 0 ) {
		return xrtValueClone(pRight);
	}
	if ( xrtValueCount(pRight) == 0 ) {
		return xrtValueClone(pLeft);
	}
	pResult = xrtValueClone(pLeft);
	if ( pResult == NULL ) {
		return NULL;
	}
	if ( !xrtValueArrayExtend(pResult, pRight) ) {
		xrtValueRelease(pResult);
		return NULL;
	}
	return pResult;
}



/* 判断指定映射是否已有迭代键。 */
static bool __xrtValueMapHasKey(
	const xvalue* pMap,
	xvaluetype Type,
	const xvaluekey* pKey
)
{
	return Type == XVALUE_INT_MAP
		? xrtValueIntMapHas(pMap, pKey->Integer)
		: xrtValueObjectHas(pMap, pKey->String);
}



/* 使用迭代键把借用值写入指定映射。 */
static bool __xrtValueMapSetItem(
	xvalue* pMap,
	xvaluetype Type,
	const xvaluekey* pKey,
	const xvalue* pItem
)
{
	return Type == XVALUE_INT_MAP
		? xrtValueIntMapSet(pMap, pKey->Integer, pItem)
		: xrtValueObjectSet(pMap, pKey->String, pItem);
}



/* 预检新增键数量和冲突，使冲突错误优先于准备阶段分配失败。 */
static bool __xrtValueMapPlan(
	const xvalue* pTarget,
	const xvalue* pSource,
	xvaluetype Type,
	xvaluemergeplan* pPlan
)
{
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	xvalue* pExisting;

	memset(pPlan, 0, sizeof(xvaluemergeplan));
	if ( !xrtValueIterBegin(pSource, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		pExisting = Type == XVALUE_INT_MAP
			? xrtValueIntMapGet(pTarget, Key.Integer)
			: xrtValueObjectGet(pTarget, Key.String);
		if ( pExisting != NULL ) {
			pPlan->Conflict = true;
			pPlan->Replacement =
				pPlan->Replacement || (pExisting != pItem);
		} else if ( pPlan->Missing == SIZE_MAX ) {
			xrtValueIterEnd(&tIterator);
			__xrtErrorSetSizeOverflow();
			return false;
		} else {
			pPlan->Missing++;
		}
	}
	xrtValueIterEnd(&tIterator);
	return true;
}



/* 按策略把来源映射快照应用到准备映射。 */
static bool __xrtValueMapApply(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluetype Type,
	xvaluemergepolicy Policy
)
{
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;

	if ( !xrtValueIterBegin(pSource, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		if ( (Policy == XVALUE_MERGE_KEEP) &&
			 __xrtValueMapHasKey(pTarget, Type, &Key) ) {
			continue;
		}
		if ( !__xrtValueMapSetItem(pTarget, Type, &Key, pItem) ) {
			xrtValueIterEnd(&tIterator);
			return false;
		}
	}
	xrtValueIterEnd(&tIterator);
	return true;
}



/* 实现 IntMap 与 Object 共用的失败原子合并。 */
static bool __xrtValueMapMerge(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluetype Type,
	xvaluemergepolicy Policy
)
{
	xvaluemergeplan Plan;
	size_t iTargetCount;
	xvalue* pPrepared;

	if ( !__xrtValueCollectionPair(pTarget, pSource, Type) ||
		 !__xrtValueMergePolicyValid(Policy) ) {
		return false;
	}
	if ( xrtValueCount(pSource) == 0 ) {
		return true;
	}
	if ( pTarget->Data.Backing == pSource->Data.Backing ) {
		if ( Policy == XVALUE_MERGE_ERROR ) {
			__xrtErrorSetExists();
			return false;
		}
		return true;
	}
	iTargetCount = xrtValueCount(pTarget);
	if ( iTargetCount == 0 ) {
		pPrepared = xrtValueClone(pSource);
		return pPrepared != NULL
			? __xrtValueCollectionCommit(pTarget, pPrepared)
			: false;
	}
	if ( !__xrtValueMapPlan(pTarget, pSource, Type, &Plan) ) {
		return false;
	}
	if ( (Policy == XVALUE_MERGE_ERROR) && Plan.Conflict ) {
		__xrtErrorSetExists();
		return false;
	}
	if ( (Plan.Missing == 0) &&
		 ((Policy == XVALUE_MERGE_KEEP) || !Plan.Replacement) ) {
		return true;
	}
	if ( Plan.Missing > (SIZE_MAX - iTargetCount) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}

	/* Object 可预留最终容量，树形 IntMap 直接使用其逐项插入路径。 */
	pPrepared = xrtValueClone(pTarget);
	if ( pPrepared == NULL ) {
		return false;
	}
	if ( ((Type == XVALUE_OBJECT) &&
		 !xrtValueReserve(pPrepared, iTargetCount + Plan.Missing)) ||
		 !__xrtValueMapApply(pPrepared, pSource, Type, Policy) ) {
		xrtValueRelease(pPrepared);
		return false;
	}
	return __xrtValueCollectionCommit(pTarget, pPrepared);
}



/* 按冲突策略失败原子地合并两个整数键映射。 */
XRT_API bool xrtValueIntMapMerge(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluemergepolicy Policy
)
{
	return __xrtValueMapMerge(
		pTarget,
		pSource,
		XVALUE_INT_MAP,
		Policy
	);
}



/* 按冲突策略失败原子地合并两个对象，并保留目标已有键位置。 */
XRT_API bool xrtValueObjectMerge(
	xvalue* pTarget,
	const xvalue* pSource,
	xvaluemergepolicy Policy
)
{
	return __xrtValueMapMerge(
		pTarget,
		pSource,
		XVALUE_OBJECT,
		Policy
	);
}



/* 失败原子地把来源集合中的缺失元素追加到目标集合。 */
XRT_API bool xrtValueSetMerge(
	xvalue* pTarget,
	const xvalue* pSource
)
{
	const xset* pTargetItems;
	const xset* pSourceItems;
	xset* pMerged;
	xvalue* pPrepared;
	bool bSubset;

	if ( !__xrtValueCollectionPair(pTarget, pSource, XVALUE_SET) ) {
		return false;
	}
	if ( (pTarget->Data.Backing == pSource->Data.Backing) ||
		 (xrtValueCount(pSource) == 0) ) {
		return true;
	}
	if ( xrtValueCount(pTarget) == 0 ) {
		pPrepared = xrtValueClone(pSource);
		return pPrepared != NULL
			? __xrtValueCollectionCommit(pTarget, pPrepared)
			: false;
	}
	pTargetItems = __xrtValueSetItems(pTarget);
	pSourceItems = __xrtValueSetItems(pSource);
	if ( (pTargetItems == NULL) || (pSourceItems == NULL) ) {
		return false;
	}
	if ( !__xrtValueCollectionProtect(pTarget, pSource) ) {
		return false;
	}
	bSubset = xrtSetIsSubset(pSourceItems, pTargetItems, false);
	if ( bSubset ) {
		__xrtValueCollectionUnprotect(pTarget, pSource);
		return true;
	}

	/* 通用 Set 已压实失败原子代数和稳定顺序，Value 层直接复用。 */
	pMerged = xrtSetUnion(pTargetItems, pSourceItems);
	__xrtValueCollectionUnprotect(pTarget, pSource);
	if ( pMerged == NULL ) {
		return false;
	}
	pPrepared = __xrtValueSetAdopt(pMerged);
	if ( pPrepared == NULL ) {
		return false;
	}
	return __xrtValueCollectionCommit(pTarget, pPrepared);
}



/* 创建指定 Set 二元运算的 Value 结果。 */
static xvalue* __xrtValueSetBinary(
	const xvalue* pLeft,
	const xvalue* pRight,
	xvaluesetoperation Operation
)
{
	const xset* pLeftItems;
	const xset* pRightItems;
	xset* pResult;
	size_t iLeftCount;
	size_t iRightCount;

	if ( !__xrtValueCollectionPair(pLeft, pRight, XVALUE_SET) ) {
		return NULL;
	}
	iLeftCount = __xrtValueContainerCount(pLeft);
	iRightCount = __xrtValueContainerCount(pRight);
	if ( pLeft->Data.Backing == pRight->Data.Backing ) {
		return (
			(Operation == XVALUE_SET_UNION) ||
			(Operation == XVALUE_SET_INTERSECTION)
		)
			? xrtValueClone(pLeft)
			: xrtValueSet();
	}
	if ( iLeftCount == 0 ) {
		return (
			(Operation == XVALUE_SET_UNION) ||
			(Operation == XVALUE_SET_SYMMETRIC_DIFFERENCE)
		)
			? xrtValueClone(pRight)
			: xrtValueSet();
	}
	if ( iRightCount == 0 ) {
		return Operation == XVALUE_SET_INTERSECTION
			? xrtValueSet()
			: xrtValueClone(pLeft);
	}
	pLeftItems = __xrtValueSetItems(pLeft);
	pRightItems = __xrtValueSetItems(pRight);
	if ( (pLeftItems == NULL) || (pRightItems == NULL) ) {
		return NULL;
	}
	if ( !__xrtValueCollectionProtect(pLeft, pRight) ) {
		return NULL;
	}
	if ( Operation == XVALUE_SET_UNION ) {
		pResult = xrtSetUnion(pLeftItems, pRightItems);
	} else if ( Operation == XVALUE_SET_INTERSECTION ) {
		pResult = xrtSetIntersection(pLeftItems, pRightItems);
	} else if ( Operation == XVALUE_SET_DIFFERENCE ) {
		pResult = xrtSetDifference(pLeftItems, pRightItems);
	} else {
		pResult = xrtSetSymmetricDifference(pLeftItems, pRightItems);
	}
	__xrtValueCollectionUnprotect(pLeft, pRight);
	return pResult != NULL ? __xrtValueSetAdopt(pResult) : NULL;
}



/* 创建两个集合的并集，结果先保持左集合顺序。 */
XRT_API xvalue* xrtValueSetUnion(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetBinary(pLeft, pRight, XVALUE_SET_UNION);
}



/* 创建两个集合的交集，结果保持左集合顺序。 */
XRT_API xvalue* xrtValueSetIntersection(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetBinary(
		pLeft,
		pRight,
		XVALUE_SET_INTERSECTION
	);
}



/* 创建左集合相对右集合的差集。 */
XRT_API xvalue* xrtValueSetDifference(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetBinary(pLeft, pRight, XVALUE_SET_DIFFERENCE);
}



/* 创建两个集合的对称差集。 */
XRT_API xvalue* xrtValueSetSymmetricDifference(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetBinary(
		pLeft,
		pRight,
		XVALUE_SET_SYMMETRIC_DIFFERENCE
	);
}



/* 判断左集合是否为右集合的子集，可选择严格子集。 */
static bool __xrtValueSetRelation(
	const xvalue* pLeft,
	const xvalue* pRight,
	xvaluesetrelation Relation,
	bool bProper
)
{
	const xset* pLeftItems;
	const xset* pRightItems;
	bool bResult;

	if ( !__xrtValueCollectionPair(pLeft, pRight, XVALUE_SET) ) {
		return false;
	}
	pLeftItems = __xrtValueSetItems(pLeft);
	pRightItems = __xrtValueSetItems(pRight);
	if ( (pLeftItems == NULL) || (pRightItems == NULL) ||
		 !__xrtValueCollectionProtect(pLeft, pRight) ) {
		return false;
	}
	if ( Relation == XVALUE_SET_SUBSET ) {
		bResult = xrtSetIsSubset(pLeftItems, pRightItems, bProper);
	} else if ( Relation == XVALUE_SET_SUPERSET ) {
		bResult = xrtSetIsSuperset(pLeftItems, pRightItems, bProper);
	} else if ( Relation == XVALUE_SET_DISJOINT ) {
		bResult = xrtSetIsDisjoint(pLeftItems, pRightItems);
	} else {
		bResult = xrtSetEqual(pLeftItems, pRightItems);
	}
	__xrtValueCollectionUnprotect(pLeft, pRight);
	return bResult;
}



/* 判断左集合是否为右集合的子集，可选择严格子集。 */
XRT_API bool xrtValueSetIsSubset(
	const xvalue* pLeft,
	const xvalue* pRight,
	bool bProper
)
{
	return __xrtValueSetRelation(
		pLeft,
		pRight,
		XVALUE_SET_SUBSET,
		bProper
	);
}



/* 判断左集合是否为右集合的超集，可选择严格超集。 */
XRT_API bool xrtValueSetIsSuperset(
	const xvalue* pLeft,
	const xvalue* pRight,
	bool bProper
)
{
	return __xrtValueSetRelation(
		pLeft,
		pRight,
		XVALUE_SET_SUPERSET,
		bProper
	);
}



/* 判断两个集合是否没有任何共同元素。 */
XRT_API bool xrtValueSetIsDisjoint(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetRelation(
		pLeft,
		pRight,
		XVALUE_SET_DISJOINT,
		false
	);
}



/* 判断两个集合是否拥有相同的标量元素。 */
XRT_API bool xrtValueSetEqual(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	return __xrtValueSetRelation(
		pLeft,
		pRight,
		XVALUE_SET_EQUAL,
		false
	);
}

#endif
