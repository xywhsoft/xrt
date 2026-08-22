#include "../../../dev/bench/bench_common.h"

#define XRUNTIME_MODULE_TYPED_QUEUE_SPSC
#define XRUNTIME_MODULE_TYPED_QUEUE_MPSC
#define XRUNTIME_MODULE_TYPED_QUEUE_MPMC
#include <xruntime.h>



/* 执行一轮 SPSC 类型值入队和出队。 */
static bool xbenchTypedSPSC(
	xtypedspscqueue* pQueue,
	uint32 iCount,
	uint64* pChecksum
)
{
	for ( uint32 i = 0u; i < iCount; i++ ) {
		uint64 iInput = i + 1u;
		uint64 iOutput = 0u;

		if (
			(xrtTypedSPSCQueueTryPush(pQueue, &iInput) != XQUEUE_OK) ||
			(xrtTypedSPSCQueueTryPop(pQueue, &iOutput) != XQUEUE_OK)
		) {
			return false;
		}
		*pChecksum ^= iOutput + i;
	}
	return true;
}



/* 执行一轮 MPSC 类型值入队和出队。 */
static bool xbenchTypedMPSC(
	xtypedmpscqueue* pQueue,
	uint32 iCount,
	uint64* pChecksum
)
{
	for ( uint32 i = 0u; i < iCount; i++ ) {
		uint64 iInput = i + 1u;
		uint64 iOutput = 0u;

		if (
			(xrtTypedMPSCQueueTryPush(pQueue, &iInput) != XQUEUE_OK) ||
			(xrtTypedMPSCQueueTryPop(pQueue, &iOutput) != XQUEUE_OK)
		) {
			return false;
		}
		*pChecksum ^= iOutput + i;
	}
	return true;
}



/* 执行一轮 MPMC 类型值入队和出队。 */
static bool xbenchTypedMPMC(
	xtypedmpmcqueue* pQueue,
	uint32 iCount,
	uint64* pChecksum
)
{
	for ( uint32 i = 0u; i < iCount; i++ ) {
		uint64 iInput = i + 1u;
		uint64 iOutput = 0u;

		if (
			(xrtTypedMPMCQueueTryPush(pQueue, &iInput) != XQUEUE_OK) ||
			(xrtTypedMPMCQueueTryPop(pQueue, &iOutput) != XQUEUE_OK)
		) {
			return false;
		}
		*pChecksum ^= iOutput + i;
	}
	return true;
}



/* 测量三种预分配类型队列不分配内存的单值传递热路径。 */
int main(int argc, char** argv)
{
	uint32 iCount = xbenchArgU32(argc, argv, 1, 5000000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 1024u);
	const xrttype* pType = xrtTypeUInt64();
	xtypedspscqueue SPSC = { 0 };
	xtypedmpscqueue MPSC = { 0 };
	xtypedmpmcqueue MPMC = { 0 };
	xbenchtimer Timer;
	uint64 iSPSCElapsed;
	uint64 iMPSCElapsed;
	uint64 iMPMCElapsed;
	uint64 iChecksum = 0u;

	if ( (iCount == 0u) || (iCapacity == 0u) ) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}
	if (
		!xrtTypedSPSCQueueInit(&SPSC, pType, iCapacity) ||
		!xrtTypedMPSCQueueInit(&MPSC, pType, iCapacity) ||
		!xrtTypedMPMCQueueInit(&MPMC, pType, iCapacity)
	) {
		xrtTypedSPSCQueueUnit(&SPSC);
		xrtTypedMPSCQueueUnit(&MPSC);
		xrtTypedMPMCQueueUnit(&MPMC);
		return 2;
	}

	xbenchTimerStart(&Timer);
	if ( !xbenchTypedSPSC(&SPSC, iCount, &iChecksum) ) {
		return 3;
	}
	xbenchTimerStop(&Timer);
	iSPSCElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	if ( !xbenchTypedMPSC(&MPSC, iCount, &iChecksum) ) {
		return 4;
	}
	xbenchTimerStop(&Timer);
	iMPSCElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	if ( !xbenchTypedMPMC(&MPMC, iCount, &iChecksum) ) {
		return 5;
	}
	xbenchTimerStop(&Timer);
	iMPMCElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt typed queue benchmark\n");
	xbenchPrintMetricDouble(
		"typed_spsc_transfers_per_sec",
		xbenchSafeRate(iCount, iSPSCElapsed)
	);
	xbenchPrintMetricDouble(
		"typed_mpsc_transfers_per_sec",
		xbenchSafeRate(iCount, iMPSCElapsed)
	);
	xbenchPrintMetricDouble(
		"typed_mpmc_transfers_per_sec",
		xbenchSafeRate(iCount, iMPMCElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtTypedSPSCQueueUnit(&SPSC);
	xrtTypedMPSCQueueUnit(&MPSC);
	xrtTypedMPMCQueueUnit(&MPMC);
	return 0;
}
