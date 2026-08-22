#include "../bench_common.h"

#include "../../../include/xrt.h"



/* 运行连续动态栈的预留热路径和摊销增长基线。 */
int main(int argc, char** argv)
{
	uint32 iRounds = xbenchArgU32(argc, argv, 1, 100000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 64u);
	uint32 iGrowthCount = xbenchArgU32(argc, argv, 3, 1000000u);
	xstack tStack;
	xbenchtimer tTimer;
	uint64 iReservedElapsed;
	uint64 iGrowthElapsed;
	uint64 iReservedOperations;
	uint64 iChecksum = 0;
	uint64 iValue = 0;

	if ( (iRounds == 0) || (iCapacity == 0) || (iGrowthCount == 0) ) {
		fprintf(stderr, "rounds, capacity and growth count must be non-zero.\n");
		return 1;
	}
	if ( !xrtStackInit(&tStack, sizeof(uint64)) ||
		 !xrtStackReserve(&tStack, iCapacity) ) {
		return 2;
	}
	iReservedOperations = (uint64)iRounds * iCapacity * 2u;

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iRounds; i++ ) {
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			iValue = (uint64)i + j;
			if ( !xrtStackPush(&tStack, &iValue) ) {
				return 3;
			}
		}
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			if ( !xrtStackPop(&tStack, &iValue) ) {
				return 4;
			}
			iChecksum += iValue;
		}
	}
	xbenchTimerStop(&tTimer);
	iReservedElapsed = xbenchTimerElapsedNs(&tTimer);
	xrtStackUnit(&tStack);

	if ( !xrtStackInit(&tStack, sizeof(uint64)) ) {
		return 5;
	}
	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iGrowthCount; i++ ) {
		uint64* pValue = (uint64*)xrtStackAdd(&tStack);

		if ( pValue == NULL ) {
			return 6;
		}
		*pValue = i;
	}
	xbenchTimerStop(&tTimer);
	iGrowthElapsed = xbenchTimerElapsedNs(&tTimer);
	while ( xrtStackPop(&tStack, &iValue) ) {
		iChecksum += iValue;
	}
	xrtClearError();

	printf("xrt stack benchmark\n");
	printf("rounds=%" PRIu32 "\n", iRounds);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	printf("growth_count=%" PRIu32 "\n", iGrowthCount);
	xbenchPrintMetricU64("reserved_operations", iReservedOperations);
	xbenchPrintMetricU64("reserved_push_pop_elapsed_ns", iReservedElapsed);
	xbenchPrintMetricDouble(
		"reserved_push_pop_ops_per_sec",
		xbenchSafeRate(iReservedOperations, iReservedElapsed)
	);
	xbenchPrintMetricU64("growth_elapsed_ns", iGrowthElapsed);
	xbenchPrintMetricDouble(
		"growth_push_ops_per_sec",
		xbenchSafeRate(iGrowthCount, iGrowthElapsed)
	);
	xbenchPrintMetricU64("final_capacity", tStack.Capacity);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtStackUnit(&tStack);
	return 0;
}
