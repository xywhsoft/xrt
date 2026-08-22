#include "../bench_common.h"

#define XRT_FEATURE_ATOMIC
#define XRT_FEATURE_TIME
#define XRT_FEATURE_WAIT
#define XRT_FEATURE_SYNC
#define XRT_FEATURE_MUTEX
#define XRT_FEATURE_COND
#define XRT_FEATURE_EVENT
#define XRT_FEATURE_THREAD
#define XRT_FEATURE_CHANNEL
#define XRT_FEATURE_CHANNEL_SELECT
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 双 Channel 传输基准保存生产数量和统一启动门。 */
typedef struct benchchannelselecttransfer {
	xchannel* First;
	xchannel* Second;
	uint32 Count;
	volatile long Start;
} benchchannelselecttransfer;



/* 向两个 Channel 交替发送唯一的非空整数指针值。 */
static int32 benchChannelSelectProducer(ptr pData)
{
	benchchannelselecttransfer* pTransfer =
		(benchchannelselecttransfer*)pData;

	if (
		(pTransfer == NULL) ||
		(pTransfer->First == NULL) ||
		(pTransfer->Second == NULL)
	) {
		return 1;
	}
	while ( xbenchAtomicLoad(&pTransfer->Start) == 0 ) {
		xrtThreadYield();
	}

	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		xchannel* pChannel = (i & 1u) == 0 ?
			pTransfer->First : pTransfer->Second;

		if (
			xrtChannelSend(
				pChannel,
				(ptr)(uintptr_t)(i + 1u)
			) != XWAIT_OK
		) {
			return 2;
		}
	}
	return 0;
}



/* 测量两个始终就绪接收 case 的轮转选择成本。 */
static bool benchChannelSelectReady(uint32 iCount)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannelcase arrCase[2];
	xbenchtimer tTimer;
	uint64 iElapsed;
	ptr pFirst = NULL;
	ptr pSecond = NULL;

	if ( !xrtChannelInit(&tFirst, 1u) ) {
		return false;
	}
	if ( !xrtChannelInit(&tSecond, 1u) ) {
		(void)xrtChannelUnit(&tFirst);
		return false;
	}
	if (
		(xrtChannelTrySend(
			&tFirst,
			(ptr)(uintptr_t)1u
		) != XCHANNEL_OK) ||
		(xrtChannelTrySend(
			&tSecond,
			(ptr)(uintptr_t)2u
		) != XCHANNEL_OK)
	) {
		(void)xrtChannelUnit(&tFirst);
		(void)xrtChannelUnit(&tSecond);
		return false;
	}
	arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	arrCase[1] = xrtChannelCaseRecv(&tSecond, &pSecond);

	xbenchTimerStart(&tTimer);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xchannelselectresult tResult = xrtChannelSelectTry(
			arrCase,
			2u
		);
		xchannel* pChannel;
		ptr pItem;

		if (
			(tResult.Wait != XWAIT_OK) ||
			(tResult.Index >= 2u)
		) {
			(void)xrtChannelUnit(&tFirst);
			(void)xrtChannelUnit(&tSecond);
			return false;
		}
		pChannel = tResult.Index == 0 ? &tFirst : &tSecond;
		pItem = tResult.Index == 0 ? pFirst : pSecond;
		if ( xrtChannelTrySend(pChannel, pItem) != XCHANNEL_OK ) {
			(void)xrtChannelUnit(&tFirst);
			(void)xrtChannelUnit(&tSecond);
			return false;
		}
	}
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("select_ready_count", iCount);
	xbenchPrintMetricU64("select_ready_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"select_ready_items_per_sec",
		xbenchSafeRate(iCount, iElapsed)
	);
	return
		xrtChannelUnit(&tFirst) &&
		xrtChannelUnit(&tSecond);
}



/* 测量生产线程在两个 Channel 间分发时的持续选择吞吐。 */
static bool benchChannelSelectTransfer(uint32 iCount)
{
	xchannel tFirst;
	xchannel tSecond;
	benchchannelselecttransfer tTransfer;
	xchannelcase arrCase[2];
	xthread* pProducer;
	xbenchtimer tTimer;
	uint64 iElapsed;
	uint64 iChecksum = 0;
	uint64 iExpected =
		((uint64)iCount * ((uint64)iCount + 1u)) / 2u;
	ptr pFirst = NULL;
	ptr pSecond = NULL;

	if ( !xrtChannelInit(&tFirst, 64u) ) {
		return false;
	}
	if ( !xrtChannelInit(&tSecond, 64u) ) {
		(void)xrtChannelUnit(&tFirst);
		return false;
	}
	memset(&tTransfer, 0, sizeof(tTransfer));
	tTransfer.First = &tFirst;
	tTransfer.Second = &tSecond;
	tTransfer.Count = iCount;
	arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	arrCase[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
	pProducer = xrtThreadCreate(
		benchChannelSelectProducer,
		&tTransfer,
		0
	);
	if ( pProducer == NULL ) {
		(void)xrtChannelUnit(&tFirst);
		(void)xrtChannelUnit(&tSecond);
		return false;
	}

	xbenchTimerStart(&tTimer);
	xbenchAtomicStore(&tTransfer.Start, 1);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xchannelselectresult tResult = xrtChannelSelect(
			arrCase,
			2u
		);

		if (
			(tResult.Wait != XWAIT_OK) ||
			(tResult.Index >= 2u)
		) {
			xrtChannelClose(&tFirst);
			xrtChannelClose(&tSecond);
			(void)xrtThreadWait(pProducer);
			xrtThreadDestroy(pProducer);
			(void)xrtChannelUnit(&tFirst);
			(void)xrtChannelUnit(&tSecond);
			return false;
		}
		iChecksum += (uintptr_t)(
			tResult.Index == 0 ? pFirst : pSecond
		);
	}
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);
	if (
		(iChecksum != iExpected) ||
		(xrtThreadWait(pProducer) != XWAIT_OK) ||
		(xrtThreadExitCode(pProducer) != 0)
	) {
		xrtThreadDestroy(pProducer);
		(void)xrtChannelUnit(&tFirst);
		(void)xrtChannelUnit(&tSecond);
		return false;
	}
	xrtThreadDestroy(pProducer);

	xbenchPrintMetricU64("select_transfer_count", iCount);
	xbenchPrintMetricU64("select_transfer_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"select_transfer_items_per_sec",
		xbenchSafeRate(iCount, iElapsed)
	);
	xbenchPrintMetricU64("select_transfer_checksum", iChecksum);
	return
		xrtChannelUnit(&tFirst) &&
		xrtChannelUnit(&tSecond);
}



/* 执行 Channel Select 吞吐量基准。 */
int main(int argc, char** argv)
{
	uint32 iReadyCount = xbenchArgU32(
		argc,
		argv,
		1,
		1000000u
	);
	uint32 iTransferCount = xbenchArgU32(
		argc,
		argv,
		2,
		500000u
	);

	if ( (iReadyCount == 0) || (iTransferCount == 0) ) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}
	printf("xrt channel select benchmark\n");
	xbenchPrintMetricU64(
		"inline_case_limit",
		XRT_CHANNEL_SELECT_INLINE_CASES
	);
	xbenchPrintMetricU64(
		"select_waiter_bytes",
		sizeof(xrt_channel_select_waiter)
	);
	xbenchPrintMetricU64(
		"inline_waiter_bytes",
		sizeof(xrt_channel_select_waiter) *
		XRT_CHANNEL_SELECT_INLINE_CASES
	);
	if ( !benchChannelSelectReady(iReadyCount) ) {
		return 2;
	}
	if ( !benchChannelSelectTransfer(iTransferCount) ) {
		return 3;
	}
	return 0;
}
