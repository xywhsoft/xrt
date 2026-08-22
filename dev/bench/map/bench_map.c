#include "../bench_common.h"

#define XRT_FEATURE_HASH64
#define XRT_FEATURE_MAP
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 基准值同时验证查询命中内容没有被编译器消除。 */
typedef struct benchmapvalue {
	uint32 Index;
	uint64 Stamp;
} benchmapvalue;



/* 运行旧版 Dict 的百万插入和千万查询等价基线。 */
int main(int argc, char** argv)
{
	uint32 iInsertCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iQueryCount = xbenchArgU32(argc, argv, 2, 10000000u);
	xmap tMap;
	xbenchtimer tTimer;
	uint64 iInsertElapsed;
	uint64 iQueryElapsed;
	uint64 iChecksum = 0;
	char sKey[32];

	if ( (iInsertCount == 0) || (iQueryCount == 0) ) {
		fprintf(stderr, "insert and query counts must be non-zero.\n");
		return 1;
	}
	if ( !xrtMapInit(&tMap, sizeof(benchmapvalue)) ) {
		return 2;
	}
	if ( !xrtMapReserve(&tMap, iInsertCount) ) {
		xrtMapUnit(&tMap);
		return 3;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iInsertCount; i++ ) {
		benchmapvalue* pValue;
		int iKeySize = snprintf(sKey, sizeof(sKey), "key-idx-%" PRIu32, i);

		pValue = (benchmapvalue*)xrtMapGetOrAdd(
			&tMap,
			(xbytesview){ (cbytes)sKey, (size_t)iKeySize },
			NULL
		);
		if ( pValue == NULL ) {
			xrtMapUnit(&tMap);
			return 4;
		}
		pValue->Index = i;
		pValue->Stamp = ((uint64)i * UINT64_C(11400714819323198485));
	}
	xbenchTimerStop(&tTimer);
	iInsertElapsed = xbenchTimerElapsedNs(&tTimer);
	if ( xrtMapCount(&tMap) != iInsertCount ) {
		xrtMapUnit(&tMap);
		return 5;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iQueryCount; i++ ) {
		uint32 iIndex = i % iInsertCount;
		benchmapvalue* pValue;
		int iKeySize = snprintf(
			sKey,
			sizeof(sKey),
			"key-idx-%" PRIu32,
			iIndex
		);

		pValue = (benchmapvalue*)xrtMapGet(
			&tMap,
			(xbytesview){ (cbytes)sKey, (size_t)iKeySize }
		);
		if ( (pValue == NULL) || (pValue->Index != iIndex) ) {
			xrtMapUnit(&tMap);
			return 6;
		}
		iChecksum += pValue->Stamp;
	}
	xbenchTimerStop(&tTimer);
	iQueryElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt map benchmark\n");
	printf("insert_count=%" PRIu32 "\n", iInsertCount);
	printf("query_count=%" PRIu32 "\n", iQueryCount);
	xbenchPrintMetricU64("insert_elapsed_ns", iInsertElapsed);
	xbenchPrintMetricDouble(
		"insert_ops_per_sec",
		xbenchSafeRate(iInsertCount, iInsertElapsed)
	);
	xbenchPrintMetricU64("query_elapsed_ns", iQueryElapsed);
	xbenchPrintMetricDouble(
		"query_ops_per_sec",
		xbenchSafeRate(iQueryCount, iQueryElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtMapUnit(&tMap);
	return 0;
}
