#include "../bench_common.h"

#define XRT_FEATURE_TIME
#define XRT_FEATURE_WAIT
#define XRT_FEATURE_SYNC
#define XRT_FEATURE_MUTEX
#define XRT_FEATURE_COND
#define XRT_FEATURE_THREAD
#define XRT_FEATURE_CHANNEL
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 跨线程 Channel 基准共享生产参数和启动门。 */
typedef struct benchchanneltransfer {
	xchannel* Channel;
	uint32 Count;
	volatile long Start;
} benchchanneltransfer;



/* 等待统一启动后，按序向 Channel 发送非空整数指针值。 */
static int32 benchChannelProducer(ptr pData)
{
	benchchanneltransfer* pTransfer = (benchchanneltransfer*)pData;

	if ( (pTransfer == NULL) || (pTransfer->Channel == NULL) ) {
		return 1;
	}
	while ( xbenchAtomicLoad(&pTransfer->Start) == 0 ) {
		xrtThreadYield();
	}

	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		if (
			xrtChannelSend(
				pTransfer->Channel,
				(ptr)(uintptr_t)(i + 1u)
			) != XWAIT_OK
		) {
			return 2;
		}
	}
	return 0;
}



/* 测量同一线程上成对非阻塞发送与接收的固定成本。 */
static bool benchChannelLocal(uint32 iCount)
{
	xchannel tChannel;
	xbenchtimer tTimer;
	uint64 iElapsed;
	ptr pItem = NULL;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return false;
	}

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCount; i++ ) {
		if (
			(xrtChannelTrySend(
				&tChannel,
				(ptr)(uintptr_t)(i + 1u)
			) != XCHANNEL_OK) ||
			(xrtChannelTryRecv(&tChannel, &pItem) != XCHANNEL_OK) ||
			((uintptr_t)pItem != (uintptr_t)(i + 1u))
		) {
			(void)xrtChannelUnit(&tChannel);
			return false;
		}
	}
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("local_pairs", iCount);
	xbenchPrintMetricU64("local_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"local_items_per_sec",
		xbenchSafeRate(iCount, iElapsed)
	);
	return xrtChannelUnit(&tChannel);
}



/* 测量一个生产线程到当前消费线程的有缓冲或 rendezvous 传输。 */
static bool benchChannelTransfer(
	const char* sPrefix,
	size_t iCapacity,
	uint32 iCount
)
{
	benchchanneltransfer tTransfer;
	xchannel tChannel;
	xthread* pProducer;
	xbenchtimer tTimer;
	uint64 iElapsed;
	ptr pItem = NULL;

	memset(&tTransfer, 0, sizeof(tTransfer));
	if ( !xrtChannelInit(&tChannel, iCapacity) ) {
		return false;
	}
	tTransfer.Channel = &tChannel;
	tTransfer.Count = iCount;
	pProducer = xrtThreadCreate(
		benchChannelProducer,
		&tTransfer,
		0
	);
	if ( pProducer == NULL ) {
		(void)xrtChannelUnit(tTransfer.Channel);
		return false;
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&tTransfer.Start, 1);
	for ( uint32 i = 0; i < iCount; i++ ) {
		if (
			(xrtChannelRecv(tTransfer.Channel, &pItem) != XWAIT_OK) ||
			((uintptr_t)pItem != (uintptr_t)(i + 1u))
		) {
			xrtChannelClose(tTransfer.Channel);
			(void)xrtThreadWait(pProducer);
			xrtThreadDestroy(pProducer);
			(void)xrtChannelUnit(tTransfer.Channel);
			return false;
		}
	}
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);

	if (
		(xrtThreadWait(pProducer) != XWAIT_OK) ||
		(xrtThreadExitCode(pProducer) != 0)
	) {
		xrtThreadDestroy(pProducer);
		(void)xrtChannelUnit(tTransfer.Channel);
		return false;
	}
	xrtThreadDestroy(pProducer);

	printf("%s_capacity: %zu\n", sPrefix, iCapacity);
	printf("%s_count: %" PRIu32 "\n", sPrefix, iCount);
	printf("%s_elapsed_ns: %" PRIu64 "\n", sPrefix, iElapsed);
	printf(
		"%s_items_per_sec: %.3f\n",
		sPrefix,
		xbenchSafeRate(iCount, iElapsed)
	);
	return xrtChannelUnit(tTransfer.Channel);
}



/* 执行 Channel 内存与吞吐量基准。 */
int main(int argc, char** argv)
{
	uint32 iLocalCount = xbenchArgU32(
		argc,
		argv,
		1,
		1000000u
	);
	uint32 iBufferedCount = xbenchArgU32(
		argc,
		argv,
		2,
		500000u
	);
	uint32 iRendezvousCount = xbenchArgU32(
		argc,
		argv,
		3,
		100000u
	);

	if (
		(iLocalCount == 0) ||
		(iBufferedCount == 0) ||
		(iRendezvousCount == 0)
	) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}

	printf("xrt channel benchmark\n");
	xbenchPrintMetricU64("channel_object_bytes", sizeof(xchannel));
	xbenchPrintMetricU64(
		"buffered_4096_message_bytes",
		UINT64_C(4096) * sizeof(ptr)
	);
	xbenchPrintMetricU64("rendezvous_message_bytes", 0);
	if ( !benchChannelLocal(iLocalCount) ) {
		return 2;
	}
	if (
		!benchChannelTransfer(
			"buffered",
			4096u,
			iBufferedCount
		)
	) {
		return 3;
	}
	if (
		!benchChannelTransfer(
			"rendezvous",
			0,
			iRendezvousCount
		)
	) {
		return 4;
	}
	return 0;
}
