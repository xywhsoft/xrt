#include "../bench_common.h"

#define XRT_FEATURE_HASH64
#define XRT_FEATURE_POOL_PAGE
#define XRT_FEATURE_POOL
#define XRT_FEATURE_ARRAY
#define XRT_FEATURE_PTR_ARRAY
#define XRT_FEATURE_AVL
#define XRT_FEATURE_AVL_TREE
#define XRT_FEATURE_INT_MAP
#define XRT_FEATURE_MAP
#define XRT_FEATURE_SET
#define XRT_FEATURE_VALUE
#define XRT_FEATURE_VALUE_CONTAINER
#define XRT_FEATURE_VALUE_COLLECTION
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 为批量操作建立等规模且互不重叠的 Array、Object 和 Set。 */
static bool benchValueCollectionBuild(
	uint32 iBatchSize,
	xvalue* pArrayLeft,
	xvalue* pArrayRight,
	xvalue* pObjectLeft,
	xvalue* pObjectRight,
	xvalue* pSetLeft,
	xvalue* pSetRight
)
{
	if ( !xrtValueReserve(pArrayLeft, iBatchSize) ||
		 !xrtValueReserve(pArrayRight, iBatchSize) ||
		 !xrtValueReserve(pObjectLeft, iBatchSize) ||
		 !xrtValueReserve(pObjectRight, iBatchSize) ||
		 !xrtValueReserve(pSetLeft, iBatchSize) ||
		 !xrtValueReserve(pSetRight, iBatchSize) ) {
		return false;
	}
	for ( uint32 i = 0; i < iBatchSize; i++ ) {
		uint64 iLeftKey = i;
		uint64 iRightKey = (uint64)i + (uint64)iBatchSize;

		if (
			!xrtValueArrayAppend(
				pArrayLeft,
				xrtValueBool((i & 1u) != 0)
			) ||
			!xrtValueArrayAppend(
				pArrayRight,
				xrtValueBool((i & 1u) == 0)
			) ||
			!xrtValueObjectSet(
				pObjectLeft,
				(xstrview){ (cstr)&iLeftKey, sizeof(iLeftKey) },
				xrtValueBool(false)
			) ||
			!xrtValueObjectSet(
				pObjectRight,
				(xstrview){ (cstr)&iRightKey, sizeof(iRightKey) },
				xrtValueBool(true)
			) ||
			!xrtValueSetAddNew(
				pSetLeft,
				xrtValueInt((int64)iLeftKey)
			) ||
			!xrtValueSetAddNew(
				pSetRight,
				xrtValueInt((int64)iRightKey)
			)
		) {
			return false;
		}
	}
	return true;
}



/* 运行批量复制、映射合并、Set 代数、关系和 COW 恒等热路径。 */
int main(int argc, char** argv)
{
	uint32 iBatchSize = xbenchArgU32(argc, argv, 1, 10000u);
	uint32 iBatchRounds = xbenchArgU32(argc, argv, 2, 100u);
	uint32 iRelationRounds = xbenchArgU32(argc, argv, 3, 1000u);
	uint32 iNoopRounds = xbenchArgU32(argc, argv, 4, 1000000u);
	xbenchtimer tTimer;
	xvalue* pArrayLeft = xrtValueArray();
	xvalue* pArrayRight = xrtValueArray();
	xvalue* pObjectLeft = xrtValueObject();
	xvalue* pObjectRight = xrtValueObject();
	xvalue* pSetLeft = xrtValueSet();
	xvalue* pSetRight = xrtValueSet();
	xvalue* pObjectShared = NULL;
	uint64 iArrayElapsed;
	uint64 iObjectElapsed;
	uint64 iSetElapsed;
	uint64 iRelationElapsed;
	uint64 iNoopElapsed;
	uint64 iChecksum = 0;
	int iResult = 1;

	if ( (iBatchSize == 0) || (iBatchRounds == 0) ||
		 (iRelationRounds == 0) || (iNoopRounds == 0) ||
		 (pArrayLeft == NULL) || (pArrayRight == NULL) ||
		 (pObjectLeft == NULL) || (pObjectRight == NULL) ||
		 (pSetLeft == NULL) || (pSetRight == NULL) ||
		 !benchValueCollectionBuild(
			iBatchSize,
			pArrayLeft,
			pArrayRight,
			pObjectLeft,
			pObjectRight,
			pSetLeft,
			pSetRight
		 ) ) {
		goto cleanup;
	}
	pObjectShared = xrtValueClone(pObjectLeft);
	if ( pObjectShared == NULL ) {
		goto cleanup;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iBatchRounds; i++ ) {
		xvalue* pResult = xrtValueClone(pArrayLeft);

		if ( (pResult == NULL) ||
			 !xrtValueArrayExtend(pResult, pArrayRight) ) {
			xrtValueRelease(pResult);
			goto cleanup;
		}
		iChecksum += xrtValueCount(pResult);
		xrtValueRelease(pResult);
	}
	xbenchTimerStop(&tTimer);
	iArrayElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iBatchRounds; i++ ) {
		xvalue* pResult = xrtValueClone(pObjectLeft);

		if ( (pResult == NULL) ||
			 !xrtValueObjectMerge(
				pResult,
				pObjectRight,
				XVALUE_MERGE_REPLACE
			 ) ) {
			xrtValueRelease(pResult);
			goto cleanup;
		}
		iChecksum += xrtValueCount(pResult);
		xrtValueRelease(pResult);
	}
	xbenchTimerStop(&tTimer);
	iObjectElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iBatchRounds; i++ ) {
		xvalue* pResult = xrtValueSetUnion(pSetLeft, pSetRight);

		if ( pResult == NULL ) {
			goto cleanup;
		}
		iChecksum += xrtValueCount(pResult);
		xrtValueRelease(pResult);
	}
	xbenchTimerStop(&tTimer);
	iSetElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iRelationRounds; i++ ) {
		if ( !xrtValueSetIsDisjoint(pSetLeft, pSetRight) ) {
			goto cleanup;
		}
		iChecksum++;
	}
	xbenchTimerStop(&tTimer);
	iRelationElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iNoopRounds; i++ ) {
		if ( !xrtValueObjectMerge(
			pObjectShared,
			pObjectLeft,
			XVALUE_MERGE_REPLACE
		) ) {
			goto cleanup;
		}
		iChecksum++;
	}
	xbenchTimerStop(&tTimer);
	iNoopElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt value collection benchmark\n");
	printf("batch_size=%" PRIu32 "\n", iBatchSize);
	printf("batch_rounds=%" PRIu32 "\n", iBatchRounds);
	printf("relation_rounds=%" PRIu32 "\n", iRelationRounds);
	printf("noop_rounds=%" PRIu32 "\n", iNoopRounds);
	xbenchPrintMetricDouble(
		"array_extend_items_per_sec",
		xbenchSafeRate(
			(uint64)iBatchSize * (uint64)iBatchRounds,
			iArrayElapsed
		)
	);
	xbenchPrintMetricDouble(
		"object_merge_items_per_sec",
		xbenchSafeRate(
			(uint64)iBatchSize * (uint64)iBatchRounds,
			iObjectElapsed
		)
	);
	xbenchPrintMetricDouble(
		"set_union_items_per_sec",
		xbenchSafeRate(
			(uint64)iBatchSize * 2u * (uint64)iBatchRounds,
			iSetElapsed
		)
	);
	xbenchPrintMetricDouble(
		"set_disjoint_items_per_sec",
		xbenchSafeRate(
			(uint64)iBatchSize * (uint64)iRelationRounds,
			iRelationElapsed
		)
	);
	xbenchPrintMetricDouble(
		"shared_map_noop_ops_per_sec",
		xbenchSafeRate(iNoopRounds, iNoopElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	iResult = 0;

cleanup:
	xrtValueRelease(pObjectShared);
	xrtValueRelease(pSetRight);
	xrtValueRelease(pSetLeft);
	xrtValueRelease(pObjectRight);
	xrtValueRelease(pObjectLeft);
	xrtValueRelease(pArrayRight);
	xrtValueRelease(pArrayLeft);
	return iResult;
}
