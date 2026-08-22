#include "../bench_common.h"

#define XRT_FEATURE_HASH64
#define XRT_FEATURE_SET
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 把连续索引打散为稳定的非顺序集合键。 */
static uint64 benchSetKey(uint32 iIndex)
{
	uint64 iValue = (uint64)iIndex + UINT64_C(0x9E3779B97F4A7C15);

	iValue = (iValue ^ (iValue >> 30u)) * UINT64_C(0xBF58476D1CE4E5B9);
	iValue = (iValue ^ (iValue >> 27u)) * UINT64_C(0x94D049BB133111EB);
	return iValue ^ (iValue >> 31u);
}



/* 运行集合的百万插入和千万命中查询基线。 */
int main(int argc, char** argv)
{
	uint32 iInsertCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iQueryCount = xbenchArgU32(argc, argv, 2, 10000000u);
	xset tSet;
	xbenchtimer tTimer;
	uint64 iInsertElapsed;
	uint64 iQueryElapsed;
	uint64 iChecksum = 0;

	if ( (iInsertCount == 0) || (iQueryCount == 0) ) {
		fprintf(stderr, "insert and query counts must be non-zero.\n");
		return 1;
	}
	if ( !xrtSetInit(&tSet, sizeof(uint64)) ) {
		return 2;
	}
	if ( !xrtSetReserve(&tSet, iInsertCount) ) {
		xrtSetUnit(&tSet);
		return 3;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iInsertCount; i++ ) {
		uint64 iKey = benchSetKey(i);

		if ( !xrtSetAdd(&tSet, &iKey) ) {
			xrtSetUnit(&tSet);
			return 4;
		}
	}
	xbenchTimerStop(&tTimer);
	iInsertElapsed = xbenchTimerElapsedNs(&tTimer);
	if ( xrtSetCount(&tSet) != iInsertCount ) {
		xrtSetUnit(&tSet);
		return 5;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iQueryCount; i++ ) {
		uint32 iIndex = i % iInsertCount;
		uint64 iKey = benchSetKey(iIndex);
		const uint64* pStored = (const uint64*)xrtSetGet(&tSet, &iKey);

		if ( (pStored == NULL) || (*pStored != iKey) ) {
			xrtSetUnit(&tSet);
			return 6;
		}
		iChecksum += *pStored;
	}
	xbenchTimerStop(&tTimer);
	iQueryElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt set benchmark\n");
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

	xrtSetUnit(&tSet);
	return 0;
}
