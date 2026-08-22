#include "../bench_common.h"

#include "../../../include/xrt.h"



/* 运行分块栈的预留热路径、按块增长和索引访问基线。 */
int main(int argc, char** argv)
{
	uint32 iRounds = xbenchArgU32(argc, argv, 1, 100000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 64u);
	uint32 iGrowthCount = xbenchArgU32(argc, argv, 3, 1000000u);
	uint32 iLookupCount = xbenchArgU32(argc, argv, 4, 4000000u);
	xblockstack tStack;
	xbenchtimer tTimer;
	uint64 iReservedElapsed;
	uint64 iGrowthElapsed;
	uint64 iLookupElapsed;
	uint64 iReservedOperations;
	uint64 iChecksum = 0;
	uint64 iValue = 0;
	uint64* pFirst = NULL;
	size_t iIndex = 0;

	if (
		(iRounds == 0) ||
		(iCapacity == 0) ||
		(iGrowthCount == 0) ||
		(iLookupCount == 0)
	) {
		fprintf(
			stderr,
			"rounds, capacity, growth count and lookup count must be non-zero.\n"
		);
		return 1;
	}
	if (
		!xrtBlockStackInit(&tStack, sizeof(uint64)) ||
		!xrtBlockStackReserve(&tStack, iCapacity)
	) {
		return 2;
	}
	iReservedOperations = (uint64)iRounds * iCapacity * 2u;

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iRounds; i++ ) {
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			iValue = (uint64)i + j;
			if ( !xrtBlockStackPush(&tStack, &iValue) ) {
				return 3;
			}
		}
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			if ( !xrtBlockStackPop(&tStack, &iValue) ) {
				return 4;
			}
			iChecksum += iValue;
		}
	}
	xbenchTimerStop(&tTimer);
	iReservedElapsed = xbenchTimerElapsedNs(&tTimer);
	xrtBlockStackUnit(&tStack);

	if ( !xrtBlockStackInit(&tStack, sizeof(uint64)) ) {
		return 5;
	}
	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iGrowthCount; i++ ) {
		uint64* pValue = (uint64*)xrtBlockStackAdd(&tStack);

		if ( pValue == NULL ) {
			return 6;
		}
		*pValue = i;
		if ( i == 0 ) {
			pFirst = pValue;
		}
	}
	xbenchTimerStop(&tTimer);
	iGrowthElapsed = xbenchTimerElapsedNs(&tTimer);
	if ( xrtBlockStackGet(&tStack, 0) != pFirst ) {
		return 7;
	}

	/* 循环顺序访问避免把随机数生成器成本混入容器访问结果。 */
	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iLookupCount; i++ ) {
		const uint64* pValue =
			(const uint64*)xrtBlockStackConstGet(&tStack, iIndex);

		if ( pValue == NULL ) {
			return 8;
		}
		iChecksum += *pValue;
		iIndex++;
		if ( iIndex == tStack.Count ) {
			iIndex = 0;
		}
	}
	xbenchTimerStop(&tTimer);
	iLookupElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt block stack benchmark\n");
	printf("rounds=%" PRIu32 "\n", iRounds);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	printf("growth_count=%" PRIu32 "\n", iGrowthCount);
	printf("lookup_count=%" PRIu32 "\n", iLookupCount);
	xbenchPrintMetricU64("reserved_operations", iReservedOperations);
	xbenchPrintMetricU64("reserved_push_pop_elapsed_ns", iReservedElapsed);
	xbenchPrintMetricDouble(
		"reserved_push_pop_ops_per_sec",
		xbenchSafeRate(iReservedOperations, iReservedElapsed)
	);
	xbenchPrintMetricU64("growth_elapsed_ns", iGrowthElapsed);
	xbenchPrintMetricDouble(
		"growth_add_ops_per_sec",
		xbenchSafeRate(iGrowthCount, iGrowthElapsed)
	);
	xbenchPrintMetricU64("lookup_elapsed_ns", iLookupElapsed);
	xbenchPrintMetricDouble(
		"indexed_get_ops_per_sec",
		xbenchSafeRate(iLookupCount, iLookupElapsed)
	);
	xbenchPrintMetricU64("final_capacity", tStack.Capacity);
	xbenchPrintMetricU64("block_count", tStack.Blocks.Count);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtBlockStackUnit(&tStack);
	return 0;
}
