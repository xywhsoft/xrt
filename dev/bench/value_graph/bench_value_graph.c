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
#define XRT_FEATURE_VALUE_GRAPH
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 创建包含指定数量独立 Object 节点的平坦图。 */
static xvalue* benchValueGraphFlat(uint32 iNodeCount)
{
	xvalue* pRoot = xrtValueArray();

	if ( (pRoot == NULL) || !xrtValueReserve(pRoot, iNodeCount) ) {
		xrtValueRelease(pRoot);
		return NULL;
	}
	for ( uint32 i = 0; i < iNodeCount; i++ ) {
		xvalue* pChild = xrtValueObject();

		if ( (pChild == NULL) ||
			 !xrtValueObjectSetNew(
				pChild,
				XRT_STR_LITERAL("id"),
				xrtValueInt((int64)i)
			 ) ||
			 !xrtValueArrayAppendTake(pRoot, &pChild) ) {
			xrtValueRelease(pChild);
			xrtValueRelease(pRoot);
			return NULL;
		}
	}
	return pRoot;
}



/* 创建每层重复引用同一子节点的共享 DAG。 */
static xvalue* benchValueGraphDiamond(uint32 iDepth)
{
	xvalue* pCurrent = xrtValueInt(1);

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



/* 运行大图深克隆、结构比较、共享 DAG 和标量快路基准。 */
int main(int argc, char** argv)
{
	uint32 iNodeCount = xbenchArgU32(argc, argv, 1, 1000u);
	uint32 iCloneRounds = xbenchArgU32(argc, argv, 2, 100u);
	uint32 iEqualRounds = xbenchArgU32(argc, argv, 3, 1000u);
	uint32 iDagRounds = xbenchArgU32(argc, argv, 4, 100000u);
	uint32 iScalarRounds = xbenchArgU32(argc, argv, 5, 1000000u);
	uint32 iDagDepth = xbenchArgU32(argc, argv, 6, 24u);
	xbenchtimer tTimer;
	xvalue* pFlat = benchValueGraphFlat(iNodeCount);
	xvalue* pFlatPeer = pFlat != NULL
		? xrtValueDeepClone(pFlat) : NULL;
	xvalue* pDagLeft = benchValueGraphDiamond(iDagDepth);
	xvalue* pDagRight = benchValueGraphDiamond(iDagDepth);
	xvalue* pScalar = xrtValueInt(7);
	uint64 iCloneElapsed;
	uint64 iEqualElapsed;
	uint64 iDagElapsed;
	uint64 iScalarElapsed;
	uint64 iChecksum = 0;
	int iResult = 1;

	if ( (iNodeCount == 0) || (iCloneRounds == 0) ||
		 (iEqualRounds == 0) || (iDagRounds == 0) ||
		 (iScalarRounds == 0) ||
		 (iDagDepth >= XRT_VALUE_DEPTH_MAX) ||
		 (pFlat == NULL) || (pFlatPeer == NULL) ||
		 (pDagLeft == NULL) || (pDagRight == NULL) ||
		 (pScalar == NULL) ||
		 !xrtValueEqual(pFlat, pFlatPeer) ||
		 !xrtValueEqual(pDagLeft, pDagRight) ) {
		goto cleanup;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCloneRounds; i++ ) {
		xvalue* pCopy = xrtValueDeepClone(pFlat);

		if ( pCopy == NULL ) {
			goto cleanup;
		}
		iChecksum += xrtValueCount(pCopy);
		xrtValueRelease(pCopy);
	}
	xbenchTimerStop(&tTimer);
	iCloneElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iEqualRounds; i++ ) {
		if ( !xrtValueEqual(pFlat, pFlatPeer) ) {
			goto cleanup;
		}
		iChecksum++;
	}
	xbenchTimerStop(&tTimer);
	iEqualElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iDagRounds; i++ ) {
		if ( !xrtValueEqual(pDagLeft, pDagRight) ) {
			goto cleanup;
		}
		iChecksum++;
	}
	xbenchTimerStop(&tTimer);
	iDagElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iScalarRounds; i++ ) {
		xvalue* pCopy = xrtValueDeepClone(pScalar);

		if ( pCopy != pScalar ) {
			xrtValueRelease(pCopy);
			goto cleanup;
		}
		iChecksum++;
		xrtValueRelease(pCopy);
	}
	xbenchTimerStop(&tTimer);
	iScalarElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt value graph benchmark\n");
	printf("node_count=%" PRIu32 "\n", iNodeCount);
	printf("clone_rounds=%" PRIu32 "\n", iCloneRounds);
	printf("equal_rounds=%" PRIu32 "\n", iEqualRounds);
	printf("dag_rounds=%" PRIu32 "\n", iDagRounds);
	printf("dag_depth=%" PRIu32 "\n", iDagDepth);
	printf("scalar_rounds=%" PRIu32 "\n", iScalarRounds);
	xbenchPrintMetricDouble(
		"deep_clone_nodes_per_sec",
		xbenchSafeRate(
			(uint64)iNodeCount * (uint64)iCloneRounds,
			iCloneElapsed
		)
	);
	xbenchPrintMetricDouble(
		"equal_nodes_per_sec",
		xbenchSafeRate(
			(uint64)iNodeCount * (uint64)iEqualRounds,
			iEqualElapsed
		)
	);
	xbenchPrintMetricDouble(
		"shared_dag_equal_ops_per_sec",
		xbenchSafeRate(iDagRounds, iDagElapsed)
	);
	xbenchPrintMetricDouble(
		"scalar_deep_clone_ops_per_sec",
		xbenchSafeRate(iScalarRounds, iScalarElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	iResult = 0;

cleanup:
	xrtValueRelease(pScalar);
	xrtValueRelease(pDagRight);
	xrtValueRelease(pDagLeft);
	xrtValueRelease(pFlatPeer);
	xrtValueRelease(pFlat);
	return iResult;
}
