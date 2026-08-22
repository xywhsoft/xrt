#include "../bench_common.h"

#define XRT_FEATURE_HASH64
#define XRT_FEATURE_VALUE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 运行标量创建读取释放，以及哈希和数值相等热路径。 */
int main(int argc, char** argv)
{
	uint32 iCreateCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iQueryCount = xbenchArgU32(argc, argv, 2, 10000000u);
	xbenchtimer tTimer;
	xvalue* pInt;
	xvalue* pFloat;
	uint64 iCreateElapsed;
	uint64 iQueryElapsed;
	uint64 iChecksum = 0;

	if ( (iCreateCount == 0) || (iQueryCount == 0) ) {
		fprintf(stderr, "create and query counts must be non-zero.\n");
		return 1;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCreateCount; i++ ) {
		xvalue* pValue = xrtValueInt((int64)i);
		int64 iRead;

		if ( (pValue == NULL) ||
			!xrtValueGetInt(pValue, &iRead) ||
			(iRead != (int64)i) ) {
			xrtValueRelease(pValue);
			return 2;
		}
		iChecksum += (uint64)iRead;
		xrtValueRelease(pValue);
	}
	xbenchTimerStop(&tTimer);
	iCreateElapsed = xbenchTimerElapsedNs(&tTimer);

	pInt = xrtValueInt(42);
	pFloat = xrtValueFloat(42.0);
	if ( (pInt == NULL) || (pFloat == NULL) ) {
		xrtValueRelease(pFloat);
		xrtValueRelease(pInt);
		return 3;
	}
	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iQueryCount; i++ ) {
		uint64 iHash;

		if ( !xrtValueHash((i & 1u) != 0 ? pInt : pFloat, &iHash) ||
			!xrtValueScalarEqual(pInt, pFloat) ) {
			xrtValueRelease(pFloat);
			xrtValueRelease(pInt);
			return 4;
		}
		iChecksum ^= iHash + (uint64)i;
	}
	xbenchTimerStop(&tTimer);
	iQueryElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt value benchmark\n");
	printf("create_count=%" PRIu32 "\n", iCreateCount);
	printf("query_count=%" PRIu32 "\n", iQueryCount);
	xbenchPrintMetricU64("create_elapsed_ns", iCreateElapsed);
	xbenchPrintMetricDouble(
		"create_ops_per_sec",
		xbenchSafeRate(iCreateCount, iCreateElapsed)
	);
	xbenchPrintMetricU64("query_elapsed_ns", iQueryElapsed);
	xbenchPrintMetricDouble(
		"hash_equal_ops_per_sec",
		xbenchSafeRate(iQueryCount, iQueryElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtValueRelease(pFloat);
	xrtValueRelease(pInt);
	return 0;
}
