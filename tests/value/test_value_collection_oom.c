#include "../test.h"



/* 按底层分配序号注入失败。 */
typedef struct testvaluecollectionoom {
	size_t Calls;
	size_t FailAt;
} testvaluecollectionoom;



/* 在命中序号时拒绝分配。 */
static ptr testValueCollectionOomAlloc(ptr pContext, size_t iSize)
{
	testvaluecollectionoom* pState = (testvaluecollectionoom*)pContext;

	pState->Calls++;
	if ( (pState->FailAt != 0) && (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	return malloc(iSize);
}



/* 在命中序号时保留旧块并拒绝重分配。 */
static ptr testValueCollectionOomRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	testvaluecollectionoom* pState = (testvaluecollectionoom*)pContext;

	pState->Calls++;
	if ( (pState->FailAt != 0) && (pState->Calls == pState->FailAt) ) {
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放底层测试块。 */
static void testValueCollectionOomFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建足以穿过小块缓存的批量对象来源。 */
static xvalue* testValueCollectionOomSource(void)
{
	xvalue* pSource = xrtValueObject();
	char sKey[64];

	if ( pSource == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < 512; i++ ) {
		int iLength = snprintf(
			sKey,
			sizeof(sKey),
			"collection-key-%04zu-with-owned-storage",
			i
		);

		if ( (iLength <= 0) || !xrtValueObjectSet(
			pSource,
			(xstrview){ sKey, (size_t)iLength },
			xrtValueBool((i & 1u) != 0)
		) ) {
			xrtValueRelease(pSource);
			return NULL;
		}
	}
	return pSource;
}



/* 验证空操作、共享 backing 和子集合并不触发底层分配。 */
static void testValueCollectionNoAllocation(
	testvaluecollectionoom* pState,
	xvalue* pObject,
	xvalue* pEmptyObject
)
{
	xvalue* pShared = xrtValueClone(pObject);
	xvalue* pArray = xrtValueArray();
	xvalue* pEmptyArray = xrtValueArray();
	xvalue* pSet = xrtValueSet();
	xvalue* pSubset = xrtValueSet();

	testRequire(
		(pShared != NULL) &&
		(pArray != NULL) &&
		(pEmptyArray != NULL) &&
		(pSet != NULL) &&
		(pSubset != NULL) &&
		xrtValueArrayAppendNew(pArray, xrtValueInt(1)) &&
		xrtValueSetAddNew(pSet, xrtValueInt(1)) &&
		xrtValueSetAddNew(pSet, xrtValueInt(2)) &&
		xrtValueSetAddNew(pSubset, xrtValueInt(2)),
		"collection no-allocation fixture failed"
	);

	pState->Calls = 0;
	pState->FailAt = 1;
	testRequire(
		xrtValueArrayExtend(pArray, pEmptyArray) &&
		xrtValueObjectMerge(
			pShared,
			pObject,
			XVALUE_MERGE_REPLACE
		) &&
		xrtValueObjectMerge(
			pShared,
			pEmptyObject,
			XVALUE_MERGE_ERROR
		) &&
		xrtValueSetMerge(pSet, pSubset) &&
		!xrtValueSetIsDisjoint(pSet, pSubset),
		"collection logical no-op attempted allocation"
	);
	xrtClearError();
	testRequire(
		!xrtValueObjectMerge(
			pObject,
			pObject,
			XVALUE_MERGE_ERROR
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_EXISTS) &&
		(pState->Calls == 0),
		"collection no-op allocation or self-conflict mismatch"
	);

	pState->FailAt = 0;
	xrtValueRelease(pSubset);
	xrtValueRelease(pSet);
	xrtValueRelease(pEmptyArray);
	xrtValueRelease(pArray);
	xrtValueRelease(pShared);
}



/* 验证数组扩展在准备和提交分配失败时保持目标不变。 */
static void testValueCollectionArrayOom(testvaluecollectionoom* pState)
{
	xvalue* pTarget = xrtValueArray();
	xvalue* pSource = xrtValueArray();
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	testRequire(
		(pTarget != NULL) &&
		(pSource != NULL) &&
		xrtValueArrayAppendNew(pTarget, xrtValueInt(999)),
		"collection array OOM fixture failed"
	);
	for ( size_t i = 0; i < 512; i++ ) {
		testRequire(
			xrtValueArrayAppendNew(pSource, xrtValueInt((int64)i)),
			"collection array OOM source failed"
		);
	}

	for ( size_t iFailAt = 1; iFailAt <= 32; iFailAt++ ) {
		xvalue* pAttempt;
		bool bResult;
		int64 iValue = 0;

		pState->FailAt = 0;
		pAttempt = xrtValueClone(pTarget);
		testRequire(pAttempt != NULL, "collection array OOM clone failed");
		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueArrayExtend(pAttempt, pSource);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				xrtValueCount(pAttempt) == 513,
				"collection array OOM success count mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtValueCount(pAttempt) == 1) &&
				xrtValueGetInt(
					xrtValueArrayGet(pAttempt, 0),
					&iValue
				) &&
				(iValue == 999),
				"collection array OOM changed target"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 1) && (iSuccesses >= 2) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 1) && (iSuccesses >= 2),
		"collection array OOM sweep missed failure or success paths"
	);

	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
}



/* 验证 Set 并集在底层结果和 Value 接管各分配点都失败原子。 */
static void testValueCollectionSetOom(testvaluecollectionoom* pState)
{
	xvalue* pLeft = xrtValueSet();
	xvalue* pRight = xrtValueSet();
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	testRequire(
		(pLeft != NULL) && (pRight != NULL),
		"collection set OOM fixture failed"
	);
	for ( size_t i = 0; i < 128; i++ ) {
		testRequire(
			xrtValueSetAddNew(pLeft, xrtValueInt((int64)i)) &&
			xrtValueSetAddNew(
				pRight,
				xrtValueInt((int64)(i + 128))
			),
			"collection set OOM source failed"
		);
	}

	for ( size_t iFailAt = 1; iFailAt <= 320; iFailAt++ ) {
		xvalue* pUnion;

		pState->Calls = 0;
		pState->FailAt = iFailAt;
		xrtClearError();
		pUnion = xrtValueSetUnion(pLeft, pRight);
		if ( pUnion != NULL ) {
			iSuccesses++;
			testRequire(
				(xrtValueCount(pUnion) == 256) &&
				xrtValueSetHas(pUnion, xrtValueInt(0)) &&
				xrtValueSetHas(pUnion, xrtValueInt(255)),
				"collection set OOM success mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(xrtValueCount(pLeft) == 128) &&
				(xrtValueCount(pRight) == 128) &&
				!xrtValueSetHas(pLeft, xrtValueInt(255)) &&
				!xrtValueSetHas(pRight, xrtValueInt(0)),
				"collection set OOM changed an input"
			);
		}
		pState->FailAt = 0;
		xrtValueRelease(pUnion);
		if ( (iFailures >= 3) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 3) && (iSuccesses >= 4),
		"collection set OOM sweep missed failure or success paths"
	);

	xrtValueRelease(pRight);
	xrtValueRelease(pLeft);
}



/* 验证批量对象合并在各分配失败点都不暴露部分结果。 */
int main(void)
{
	testvaluecollectionoom tState = { 0, 0 };
	xallocator tAllocator = {
		&tState,
		testValueCollectionOomAlloc,
		testValueCollectionOomRealloc,
		testValueCollectionOomFree
	};
	xvalue* pTarget;
	xvalue* pSource;
	xvalue* pEmpty;
	size_t iFailures = 0;
	size_t iSuccesses = 0;

	testRequire(
		xrtSetAllocator(&tAllocator),
		"failed to install collection OOM allocator"
	);
	pTarget = xrtValueObject();
	pSource = testValueCollectionOomSource();
	pEmpty = xrtValueObject();
	testRequire(
		(pTarget != NULL) && (pSource != NULL) && (pEmpty != NULL) &&
		xrtValueObjectSetNew(
			pTarget,
			XRT_STR_LITERAL("base"),
			xrtValueInt(7)
		),
		"collection OOM fixture failed"
	);
	testValueCollectionNoAllocation(&tState, pTarget, pEmpty);
	testValueCollectionArrayOom(&tState);
	testValueCollectionSetOom(&tState);

	/* 每次从同一 COW 快照起步，失败后目标必须仍只有原始键。 */
	for ( size_t iFailAt = 1; iFailAt <= 96; iFailAt++ ) {
		xvalue* pAttempt;
		bool bResult;

		tState.FailAt = 0;
		pAttempt = xrtValueClone(pTarget);
		testRequire(pAttempt != NULL, "collection OOM attempt clone failed");
		tState.Calls = 0;
		tState.FailAt = iFailAt;
		xrtClearError();
		bResult = xrtValueObjectMerge(
			pAttempt,
			pSource,
			XVALUE_MERGE_REPLACE
		);
		if ( bResult ) {
			iSuccesses++;
			testRequire(
				xrtValueCount(pAttempt) == 513,
				"collection OOM successful merge count mismatch"
			);
		} else {
			iFailures++;
			testRequire(
				xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"collection OOM failure kind mismatch"
			);
			testRequire(
				(xrtValueCount(pAttempt) == 1) &&
				xrtValueObjectHas(pAttempt, XRT_STR_LITERAL("base")) &&
				!xrtValueObjectHas(
					pAttempt,
					XRT_STR_LITERAL("collection-key-0000-with-owned-storage")
				),
				"collection OOM exposed partial merge"
			);
		}
		tState.FailAt = 0;
		xrtValueRelease(pAttempt);
		if ( (iFailures >= 4) && (iSuccesses >= 4) ) {
			break;
		}
	}
	testRequire(
		(iFailures >= 4) && (iSuccesses >= 4),
		"collection OOM sweep missed failure or success paths"
	);
	xrtValueRelease(pEmpty);
	xrtValueRelease(pSource);
	xrtValueRelease(pTarget);
	printf("[PASS] value collection OOM\n");
	return 0;
}
