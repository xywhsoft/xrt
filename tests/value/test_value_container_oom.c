#include "../test.h"



/* 按底层分配序号为容器操作注入 OOM。 */
typedef struct testvaluecontaineroom {
	size_t Calls;
	size_t FailAt;
} testvaluecontaineroom;



/* 在命中指定分配序号时拒绝新分配。 */
static ptr testValueContainerOomAlloc(ptr pContext, size_t iSize)
{
	testvaluecontaineroom* pState =
		(testvaluecontaineroom*)pContext;

	pState->Calls++;
	if ( (pState->FailAt != 0) &&
		 (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在命中指定分配序号时保留旧块并拒绝重分配。 */
static ptr testValueContainerOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvaluecontaineroom* pState =
		(testvaluecontaineroom*)pContext;

	pState->Calls++;
	if ( (pState->FailAt != 0) &&
		 (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放底层测试块。 */
static void testValueContainerOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建足以穿过小块缓存的四种容器。 */
static bool testValueContainerOomFixtures(
	xvalue** pArray,
	xvalue** pIntMap,
	xvalue** pObject,
	xvalue** pSet
)
{
	char sKey[48];

	*pArray = xrtValueArray();
	*pIntMap = xrtValueIntMap();
	*pObject = xrtValueObject();
	*pSet = xrtValueSet();
	if ( (*pArray == NULL) || (*pIntMap == NULL) ||
		 (*pObject == NULL) || (*pSet == NULL) ) {
		return false;
	}
	for ( size_t i = 0; i < 512; i++ ) {
		xvalue* pSetItem = xrtValueInt((int64)i);
		int iLength = snprintf(
			sKey,
			sizeof(sKey),
			"container-key-%04zu-owned",
			i
		);

		if ( (pSetItem == NULL) || (iLength <= 0) ||
			 !xrtValueArrayAppend(
				*pArray,
				xrtValueBool((i & 1u) != 0)
			 ) ||
			 !xrtValueIntMapSet(
				*pIntMap,
				(int64)i,
				xrtValueBool((i & 1u) != 0)
			 ) ||
			 !xrtValueObjectSet(
				*pObject,
				(xstrview){ sKey, (size_t)iLength },
				xrtValueBool((i & 1u) != 0)
			 ) ||
			 !xrtValueSetAddTake(*pSet, &pSetItem) ) {
			xrtValueRelease(pSetItem);
			return false;
		}
	}
	return true;
}



/* 验证拥有式迭代器分配失败不持有来源，也不掩盖内存错误。 */
static void testValueContainerOomIterator(
	testvaluecontaineroom* pState,
	xvalue* pArray
)
{
	xvalueiter* arrIterator[4096];
	xvalueiter* pIterator = NULL;
	size_t iCount = 0;

	pState->Calls = 0;
	pState->FailAt = 1;
	xrtClearError();
	while ( iCount < (sizeof(arrIterator) / sizeof(arrIterator[0])) ) {
		pIterator = xrtValueIterCreate(pArray);
		if ( pIterator == NULL ) {
			break;
		}
		arrIterator[iCount++] = pIterator;
	}
	testRequire(
		(pIterator == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(xrtValueCount(pArray) == 512),
		"owned iterator OOM changed source or error"
	);

	pState->FailAt = 0;
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtValueIterDestroy(arrIterator[i]);
	}
	pState->Calls = 0;
	pIterator = xrtValueIterCreate(pArray);
	testRequire(pIterator != NULL, "owned iterator did not recover after OOM");
	xrtValueIterDestroy(pIterator);
}



/* 验证无效编辑和同值写入不会触发 COW 分配。 */
static void testValueContainerOomNoOp(
	testvaluecontaineroom* pState,
	xvalue* pArray,
	xvalue* pIntMap,
	xvalue* pObject,
	xvalue* pSet
)
{
	xvalue* pArrayCopy = xrtValueClone(pArray);
	xvalue* pIntMapCopy = xrtValueClone(pIntMap);
	xvalue* pObjectCopy = xrtValueClone(pObject);
	xvalue* pSetCopy = xrtValueClone(pSet);
	xvalue* pArrayItem = xrtValueArrayGet(pArrayCopy, 0);
	xvalue* pIntMapItem = xrtValueIntMapGet(pIntMapCopy, 0);
	xvalue* pObjectItem = xrtValueObjectGet(
		pObjectCopy,
		XRT_STR_LITERAL("container-key-0000-owned")
	);
	xvalue* pSetItem = xrtValueInt(0);
	xvalue* pNested = xrtValueArray();
	xvalue* pNestedChild = xrtValueArray();

	testRequire(
		(pArrayCopy != NULL) && (pIntMapCopy != NULL) &&
		(pObjectCopy != NULL) && (pSetCopy != NULL) &&
		(pArrayItem != NULL) && (pIntMapItem != NULL) &&
		(pObjectItem != NULL) && (pSetItem != NULL) &&
		(pNested != NULL) && (pNestedChild != NULL) &&
		xrtValueArrayAppendTake(pNested, &pNestedChild),
		"container no-op OOM fixture failed"
	);

	pState->Calls = 0;
	pState->FailAt = 1;
	xrtClearError();
	testRequire(
		xrtValueArraySet(pArrayCopy, 0, pArrayItem) &&
		xrtValueIntMapSet(pIntMapCopy, 0, pIntMapItem) &&
		xrtValueObjectSet(
			pObjectCopy,
			XRT_STR_LITERAL("container-key-0000-owned"),
			pObjectItem
		) &&
		xrtValueSetAdd(pSetCopy, pSetItem) &&
		(pState->Calls == 0),
		"container no-op write allocated or failed"
	);
	testRequire(
		(xrtValueArrayEdit(pNested, 0) != NULL) &&
		(pState->Calls == 0),
		"unique nested edit allocated"
	);
	xrtClearError();
	testRequire(
		(xrtValueArrayEdit(pArrayCopy, 0) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_TYPE) &&
		(pState->Calls == 0),
		"scalar edit allocated before rejecting type"
	);
	pState->FailAt = 0;

	xrtValueRelease(pNested);
	xrtValueRelease(pSetItem);
	xrtValueRelease(pSetCopy);
	xrtValueRelease(pObjectCopy);
	xrtValueRelease(pIntMapCopy);
	xrtValueRelease(pArrayCopy);
}



/* 验证共享数组追加在每个 OOM 阶段都保持来源和内容。 */
static void testValueContainerOomArray(
	testvaluecontaineroom* pState,
	xvalue* pSource
)
{
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	for ( size_t iFailAt = 1; iFailAt <= 128; iFailAt++ ) {
		xvalue* pAttempt;
		xvalue* pItem;
		bool bResult;

		pState->FailAt = 0;
		pAttempt = xrtValueClone(pSource);
		pItem = xrtValueInt(1000);
		testRequire(
			(pAttempt != NULL) && (pItem != NULL),
			"array OOM attempt fixture failed"
		);
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueArrayAppendTake(pAttempt, &pItem);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				(pItem == NULL) &&
				(xrtValueCount(pAttempt) == 513),
				"array OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(pItem != NULL) &&
				(xrtValueCount(pAttempt) == 512) &&
				(xrtValueCount(pSource) == 512) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"array OOM exposed mutation or consumed source"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pItem);
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 1) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 4),
		"array OOM sweep missed failure or success paths"
	);
}



/* 验证共享 IntMap 设置在 OOM 时保持来源和原映射。 */
static void testValueContainerOomIntMap(
	testvaluecontaineroom* pState,
	xvalue* pSource
)
{
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	for ( size_t iFailAt = 1; iFailAt <= 128; iFailAt++ ) {
		xvalue* pAttempt;
		xvalue* pItem;
		bool bResult;

		pState->FailAt = 0;
		pAttempt = xrtValueClone(pSource);
		pItem = xrtValueInt(1000);
		testRequire(
			(pAttempt != NULL) && (pItem != NULL),
			"int map OOM attempt fixture failed"
		);
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueIntMapSetTake(
			pAttempt,
			INT64_C(1000000),
			&pItem
		);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				(pItem == NULL) &&
				(xrtValueCount(pAttempt) == 513),
				"int map OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(pItem != NULL) &&
				(xrtValueCount(pAttempt) == 512) &&
				!xrtValueIntMapHas(pAttempt, INT64_C(1000000)) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"int map OOM exposed mutation or consumed source"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pItem);
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 1) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 4),
		"int map OOM sweep missed failure or success paths"
	);
}



/* 验证共享 Object 设置在 OOM 时保持来源和原对象。 */
static void testValueContainerOomObject(
	testvaluecontaineroom* pState,
	xvalue* pSource
)
{
	static const xstrview NewKey =
		XRT_STR_INIT("container-new-key-with-owned-storage");
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	for ( size_t iFailAt = 1; iFailAt <= 128; iFailAt++ ) {
		xvalue* pAttempt;
		xvalue* pItem;
		bool bResult;

		pState->FailAt = 0;
		pAttempt = xrtValueClone(pSource);
		pItem = xrtValueInt(1000);
		testRequire(
			(pAttempt != NULL) && (pItem != NULL),
			"object OOM attempt fixture failed"
		);
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueObjectSetTake(
			pAttempt,
			NewKey,
			&pItem
		);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				(pItem == NULL) &&
				(xrtValueCount(pAttempt) == 513),
				"object OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(pItem != NULL) &&
				(xrtValueCount(pAttempt) == 512) &&
				!xrtValueObjectHas(pAttempt, NewKey) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"object OOM exposed mutation or consumed source"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pItem);
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 1) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 4),
		"object OOM sweep missed failure or success paths"
	);
}



/* 验证共享 Set 加入在 OOM 时保持来源和原集合。 */
static void testValueContainerOomSet(
	testvaluecontaineroom* pState,
	xvalue* pSource
)
{
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	for ( size_t iFailAt = 1; iFailAt <= 128; iFailAt++ ) {
		xvalue* pAttempt;
		xvalue* pItem;
		bool bResult;

		pState->FailAt = 0;
		pAttempt = xrtValueClone(pSource);
		pItem = xrtValueInt(1000000);
		testRequire(
			(pAttempt != NULL) && (pItem != NULL),
			"set OOM attempt fixture failed"
		);
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueSetAddTake(pAttempt, &pItem);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				(pItem == NULL) &&
				(xrtValueCount(pAttempt) == 513),
				"set OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(pItem != NULL) &&
				(xrtValueCount(pAttempt) == 512) &&
				!xrtValueSetHas(pAttempt, pItem) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"set OOM exposed mutation or consumed source"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pItem);
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 1) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 4),
		"set OOM sweep missed failure or success paths"
	);
}



/* 验证已经完成 COW 分离后，最终写入分配失败仍保持来源。 */
static void testValueContainerOomFinalWrite(
	testvaluecontaineroom* pState,
	xvalue* pArray,
	xvalue* pObject
)
{
	char sLongKey[8192];
	xstrview LongKey = { sLongKey, sizeof(sLongKey) };
	xvalue* pArrayAttempt = xrtValueClone(pArray);
	xvalue* pObjectAttempt = xrtValueClone(pObject);
	xvalue* pArrayItem = xrtValueInt(2000);
	xvalue* pObjectItem = xrtValueInt(3000);

	memset(sLongKey, 'k', sizeof(sLongKey));
	testRequire(
		(pArrayAttempt != NULL) && (pObjectAttempt != NULL) &&
		(pArrayItem != NULL) && (pObjectItem != NULL) &&
		xrtValueTrim(pArrayAttempt) &&
		xrtValueReserve(pObjectAttempt, 1024),
		"final-write OOM fixture failed"
	);

	pState->Calls = 0;
	pState->FailAt = 1;
	xrtClearError();
	testRequire(
		!xrtValueArrayAppendTake(pArrayAttempt, &pArrayItem) &&
		(pArrayItem != NULL) &&
		(xrtValueCount(pArrayAttempt) == 512) &&
		(pState->Calls != 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"array final-write OOM changed ownership or content"
	);

	pState->Calls = 0;
	pState->FailAt = 1;
	xrtClearError();
	testRequire(
		!xrtValueObjectSetTake(
			pObjectAttempt,
			LongKey,
			&pObjectItem
		) &&
		(pObjectItem != NULL) &&
		(xrtValueCount(pObjectAttempt) == 512) &&
		!xrtValueObjectHas(pObjectAttempt, LongKey) &&
		(pState->Calls != 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"object final-write OOM changed ownership or content"
	);
	pState->FailAt = 0;

	xrtValueRelease(pObjectItem);
	xrtValueRelease(pArrayItem);
	xrtValueRelease(pObjectAttempt);
	xrtValueRelease(pArrayAttempt);
}



/* 验证大型值图去重分配失败不会移交来源或留下部分边。 */
static void testValueContainerOomGraph(testvaluecontaineroom* pState)
{
	xvalue* pGraph = xrtValueArray();
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	testRequire(pGraph != NULL, "graph OOM root failed");
	for ( size_t i = 0; i < 512; i++ ) {
		xvalue* pChild = xrtValueArray();

		testRequire(
			(pChild != NULL) &&
			xrtValueArrayAppendTake(pGraph, &pChild),
			"graph OOM child fixture failed"
		);
	}

	for ( size_t iFailAt = 1; iFailAt <= 64; iFailAt++ ) {
		xvalue* pTarget;
		xvalue* pOwned;
		bool bResult;

		pState->FailAt = 0;
		pTarget = xrtValueArray();
		pOwned = xrtValueRetain(pGraph);
		testRequire(
			(pTarget != NULL) && (pOwned != NULL),
			"graph OOM attempt fixture failed"
		);
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueArrayAppendTake(pTarget, &pOwned);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				(pOwned == NULL) &&
				(xrtValueCount(pTarget) == 1),
				"graph OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(pOwned != NULL) &&
				(xrtValueCount(pTarget) == 0) &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"graph OOM consumed source or exposed edge"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pOwned);
		xrtValueRelease(pTarget);
		if ( (iFailures >= 1) && (iSuccesses >= 1) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 1),
		"graph OOM sweep missed failure or recovery"
	);
	xrtValueRelease(pGraph);
}



/* 运行四种 Value 容器的 COW 与所有权 OOM 回归。 */
int main(void)
{
	testvaluecontaineroom tState = { 0, 0 };
	xallocator tAllocator = {
		&tState,
		testValueContainerOomAlloc,
		testValueContainerOomRealloc,
		testValueContainerOomFree
	};
	xvalue* pArray = NULL;
	xvalue* pIntMap = NULL;
	xvalue* pObject = NULL;
	xvalue* pSet = NULL;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install container OOM allocator"
	);
	testRequire(
		testValueContainerOomFixtures(
			&pArray,
			&pIntMap,
			&pObject,
			&pSet
		),
		"container OOM fixtures failed"
	);
	testValueContainerOomIterator(&tState, pArray);
	testValueContainerOomNoOp(
		&tState,
		pArray,
		pIntMap,
		pObject,
		pSet
	);
	testValueContainerOomArray(&tState, pArray);
	testValueContainerOomIntMap(&tState, pIntMap);
	testValueContainerOomObject(&tState, pObject);
	testValueContainerOomSet(&tState, pSet);
	testValueContainerOomFinalWrite(&tState, pArray, pObject);
	testValueContainerOomGraph(&tState);

	tState.FailAt = 0;
	xrtValueRelease(pSet);
	xrtValueRelease(pObject);
	xrtValueRelease(pIntMap);
	xrtValueRelease(pArray);
	printf("[PASS] value container OOM\n");
	return 0;
}
