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
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 创建固定深度、每层共享两条边的 DAG。 */
static xvalue* benchValueContainerDag(void)
{
	xvalue* pNode = xrtValueArray();

	if ( (pNode == NULL) ||
		 !xrtValueArrayAppend(pNode, xrtValueBool(true)) ) {
		xrtValueRelease(pNode);
		return NULL;
	}
	for ( size_t i = 0; i < 64; i++ ) {
		xvalue* pParent = xrtValueArray();

		if ( (pParent == NULL) ||
			 !xrtValueArrayAppend(pParent, pNode) ||
			 !xrtValueArrayAppend(pParent, pNode) ) {
			xrtValueRelease(pParent);
			xrtValueRelease(pNode);
			return NULL;
		}
		xrtValueRelease(pNode);
		pNode = pParent;
	}
	return pNode;
}



/* 运行基础写入查询、COW 分离和共享 DAG 环检测热路径。 */
int main(int argc, char** argv)
{
	uint32 iArrayCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iObjectCount = xbenchArgU32(argc, argv, 2, 200000u);
	uint32 iCowRounds = xbenchArgU32(argc, argv, 3, 100u);
	uint32 iDagChecks = xbenchArgU32(argc, argv, 4, 1000u);
	const uint32 iCowSize = 100000u;
	xbenchtimer tTimer;
	xvalue* pArray = xrtValueArray();
	xvalue* pObject = xrtValueObject();
	xvalue* pCow = xrtValueArray();
	xvalue* pDag = NULL;
	uint64 iArrayElapsed;
	uint64 iObjectInsertElapsed;
	uint64 iObjectQueryElapsed;
	uint64 iCowElapsed;
	uint64 iDagElapsed;
	uint64 iChecksum = 0;
	int iResult = 0;

	if ( (iArrayCount == 0) || (iObjectCount == 0) ||
		 (iCowRounds == 0) || (iDagChecks == 0) ||
		 (pArray == NULL) || (pObject == NULL) || (pCow == NULL) ) {
		xrtValueRelease(pCow);
		xrtValueRelease(pObject);
		xrtValueRelease(pArray);
		return 1;
	}
	if ( !xrtValueReserve(pArray, iArrayCount) ||
		 !xrtValueReserve(pObject, iObjectCount) ||
		 !xrtValueReserve(pCow, iCowSize) ) {
		xrtValueRelease(pCow);
		xrtValueRelease(pObject);
		xrtValueRelease(pArray);
		return 2;
	}
	for ( uint32 i = 0; i < iCowSize; i++ ) {
		if ( !xrtValueArrayAppend(
			pCow,
			xrtValueBool((i & 1u) != 0)
		) ) {
			xrtValueRelease(pCow);
			xrtValueRelease(pObject);
			xrtValueRelease(pArray);
			return 3;
		}
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iArrayCount; i++ ) {
		if ( !xrtValueArrayAppend(
			pArray,
			xrtValueBool((i & 1u) != 0)
		) ) {
			iResult = 4;
			goto cleanup;
		}
	}
	xbenchTimerStop(&tTimer);
	iArrayElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iObjectCount; i++ ) {
		xstrview Key = { (cstr)&i, sizeof(i) };

		if ( !xrtValueObjectSet(
			pObject,
			Key,
			xrtValueBool((i & 1u) != 0)
		) ) {
			iResult = 5;
			goto cleanup;
		}
	}
	xbenchTimerStop(&tTimer);
	iObjectInsertElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iObjectCount; i++ ) {
		xstrview Key = { (cstr)&i, sizeof(i) };
		xvalue* pItem = xrtValueObjectGet(pObject, Key);

		if ( pItem == NULL ) {
			iResult = 6;
			goto cleanup;
		}
		iChecksum += xrtValueTruthy(pItem) ? 1u : 0u;
	}
	xbenchTimerStop(&tTimer);
	iObjectQueryElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCowRounds; i++ ) {
		xvalue* pCopy = xrtValueClone(pCow);

		if ( (pCopy == NULL) ||
			 !xrtValueArrayAppend(pCopy, xrtValueBool(true)) ) {
			xrtValueRelease(pCopy);
			iResult = 7;
			goto cleanup;
		}
		iChecksum += xrtValueCount(pCopy);
		xrtValueRelease(pCopy);
	}
	xbenchTimerStop(&tTimer);
	iCowElapsed = xbenchTimerElapsedNs(&tTimer);

	pDag = benchValueContainerDag();
	if ( pDag == NULL ) {
		iResult = 8;
		goto cleanup;
	}
	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iDagChecks; i++ ) {
		xvalue* pTarget = xrtValueArray();

		if ( (pTarget == NULL) ||
			 !xrtValueArrayAppend(pTarget, pDag) ) {
			xrtValueRelease(pTarget);
			iResult = 9;
			goto cleanup;
		}
		iChecksum += xrtValueCount(pTarget);
		xrtValueRelease(pTarget);
	}
	xbenchTimerStop(&tTimer);
	iDagElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt value container benchmark\n");
	printf("array_count=%" PRIu32 "\n", iArrayCount);
	printf("object_count=%" PRIu32 "\n", iObjectCount);
	printf("cow_rounds=%" PRIu32 "\n", iCowRounds);
	printf("dag_checks=%" PRIu32 "\n", iDagChecks);
	xbenchPrintMetricDouble(
		"array_append_ops_per_sec",
		xbenchSafeRate(iArrayCount, iArrayElapsed)
	);
	xbenchPrintMetricDouble(
		"object_insert_ops_per_sec",
		xbenchSafeRate(iObjectCount, iObjectInsertElapsed)
	);
	xbenchPrintMetricDouble(
		"object_query_ops_per_sec",
		xbenchSafeRate(iObjectCount, iObjectQueryElapsed)
	);
	xbenchPrintMetricDouble(
		"cow_copied_items_per_sec",
		xbenchSafeRate(
			(uint64)iCowRounds * (uint64)iCowSize,
			iCowElapsed
		)
	);
	xbenchPrintMetricDouble(
		"dag_checks_per_sec",
		xbenchSafeRate(iDagChecks, iDagElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

cleanup:
	xrtValueRelease(pDag);
	xrtValueRelease(pCow);
	xrtValueRelease(pObject);
	xrtValueRelease(pArray);
	return iResult;
}
