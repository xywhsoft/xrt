#include "../bench_common.h"

#include "../../../include/xrt.h"



/* 运行固定栈零拷贝槽、复制压栈和栈顶读取基线。 */
int main(int argc, char** argv)
{
	uint32 iRounds = xbenchArgU32(argc, argv, 1, 100000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 64u);
	uint64 pStorage[256];
	xfixedstack tStack;
	xbenchtimer tTimer;
	uint64 iAddElapsed;
	uint64 iPushElapsed;
	uint64 iPeekElapsed;
	uint64 iChecksum = 0;
	uint64 iOperations;
	uint64 iValue = 1;

	if ( (iRounds == 0) || (iCapacity == 0) ||
		 (iCapacity > (uint32)(sizeof(pStorage) / sizeof(pStorage[0]))) ) {
		fprintf(stderr, "rounds must be non-zero and capacity must be 1..256.\n");
		return 1;
	}
	if ( !xrtFixedStackInit(
		&tStack,
		pStorage,
		(size_t)iCapacity * sizeof(uint64),
		sizeof(uint64)
	) ) {
		return 2;
	}
	iOperations = (uint64)iRounds * iCapacity * 2u;

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iRounds; i++ ) {
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			uint64* pSlot = (uint64*)xrtFixedStackAdd(&tStack);

			if ( pSlot == NULL ) {
				return 3;
			}
			*pSlot = (uint64)i + j;
		}
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			if ( !xrtFixedStackPop(&tStack, &iValue) ) {
				return 4;
			}
			iChecksum += iValue;
		}
	}
	xbenchTimerStop(&tTimer);
	iAddElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iRounds; i++ ) {
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			iValue = (uint64)i + j;
			if ( !xrtFixedStackPush(&tStack, &iValue) ) {
				return 5;
			}
		}
		for ( uint32 j = 0; j < iCapacity; j++ ) {
			if ( !xrtFixedStackPop(&tStack, &iValue) ) {
				return 6;
			}
			iChecksum += iValue;
		}
	}
	xbenchTimerStop(&tTimer);
	iPushElapsed = xbenchTimerElapsedNs(&tTimer);

	if ( !xrtFixedStackPush(&tStack, &iValue) ) {
		return 7;
	}
	xbenchTimerStart(&tTimer);
	for ( uint64 i = 0; i < iOperations; i++ ) {
		const uint64* pTop = (const uint64*)xrtFixedStackConstTop(&tStack);

		if ( pTop == NULL ) {
			return 8;
		}
		iChecksum += (*pTop ^ i) & 1u;
	}
	xbenchTimerStop(&tTimer);
	iPeekElapsed = xbenchTimerElapsedNs(&tTimer);

	printf("xrt fixed stack benchmark\n");
	printf("rounds=%" PRIu32 "\n", iRounds);
	printf("capacity=%" PRIu32 "\n", iCapacity);
	xbenchPrintMetricU64("pair_operations", iOperations);
	xbenchPrintMetricU64("add_pop_elapsed_ns", iAddElapsed);
	xbenchPrintMetricDouble(
		"add_pop_ops_per_sec",
		xbenchSafeRate(iOperations, iAddElapsed)
	);
	xbenchPrintMetricU64("push_pop_elapsed_ns", iPushElapsed);
	xbenchPrintMetricDouble(
		"push_pop_ops_per_sec",
		xbenchSafeRate(iOperations, iPushElapsed)
	);
	xbenchPrintMetricU64("peek_elapsed_ns", iPeekElapsed);
	xbenchPrintMetricDouble(
		"peek_ops_per_sec",
		xbenchSafeRate(iOperations, iPeekElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtFixedStackUnit(&tStack);
	return 0;
}
