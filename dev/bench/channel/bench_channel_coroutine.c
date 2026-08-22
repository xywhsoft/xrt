#include "../bench_common.h"

#define XRT_FEATURE_ATOMIC
#define XRT_FEATURE_TEMP_MEMORY
#define XRT_FEATURE_TIME
#define XRT_FEATURE_WAIT
#define XRT_FEATURE_THREAD
#define XRT_FEATURE_SYNC
#define XRT_FEATURE_MUTEX
#define XRT_FEATURE_COND
#define XRT_FEATURE_CANCEL
#define XRT_FEATURE_COROUTINE
#define XRT_FEATURE_COROUTINE_SCHEDULER
#define XRT_FEATURE_CHANNEL
#define XRT_FEATURE_CHANNEL_COROUTINE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 单 Channel 协程基准保存传输数量与校验和。 */
typedef struct benchchannelco {
	xchannel* Channel;
	uint32 Count;
	uint64 Checksum;
} benchchannelco;



/* 双 Channel Select 基准保存两个输入通道。 */
typedef struct benchchannelselectco {
	xchannel* First;
	xchannel* Second;
	uint32 Count;
	uint64 Checksum;
} benchchannelselectco;



/* 发送协程按序提交非空整数指针值。 */
static ptr benchChannelCoSend(ptr pData)
{
	benchchannelco* pTransfer = (benchchannelco*)pData;

	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		if (
			xrtChannelSendAwait(
				pTransfer->Channel,
				(ptr)(uintptr_t)(i + 1u)
			) != XWAIT_OK
		) {
			return NULL;
		}
	}
	return pData;
}



/* 接收协程消费全部值并计算校验和。 */
static ptr benchChannelCoRecv(ptr pData)
{
	benchchannelco* pTransfer = (benchchannelco*)pData;

	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		ptr pItem = NULL;

		if (
			xrtChannelRecvAwait(
				pTransfer->Channel,
				&pItem
			) != XWAIT_OK
		) {
			return NULL;
		}
		pTransfer->Checksum += (uintptr_t)pItem;
	}
	return pData;
}



/* 双 Channel 生产协程交替发送到两个输入通道。 */
static ptr benchChannelSelectCoSend(ptr pData)
{
	benchchannelselectco* pTransfer =
		(benchchannelselectco*)pData;

	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		xchannel* pChannel = (i & 1u) == 0 ?
			pTransfer->First : pTransfer->Second;

		if (
			xrtChannelSendAwait(
				pChannel,
				(ptr)(uintptr_t)(i + 1u)
			) != XWAIT_OK
		) {
			return NULL;
		}
	}
	return pData;
}



/* 双 Channel 消费协程通过 SelectAwait 接收任一输入。 */
static ptr benchChannelSelectCoRecv(ptr pData)
{
	benchchannelselectco* pTransfer =
		(benchchannelselectco*)pData;
	xchannelcase arrCase[2];
	ptr pFirst = NULL;
	ptr pSecond = NULL;

	arrCase[0] = xrtChannelCaseRecv(
		pTransfer->First,
		&pFirst
	);
	arrCase[1] = xrtChannelCaseRecv(
		pTransfer->Second,
		&pSecond
	);
	for ( uint32 i = 0; i < pTransfer->Count; i++ ) {
		xchannelselectresult tResult =
			xrtChannelSelectAwait(arrCase, 2u);

		if (
			(tResult.Wait != XWAIT_OK) ||
			(tResult.Index >= 2u)
		) {
			return NULL;
		}
		pTransfer->Checksum += (uintptr_t)(
			tResult.Index == 0 ? pFirst : pSecond
		);
	}
	return pData;
}



/* 测量两个协程经单 Channel 传输的持续吞吐。 */
static bool benchChannelCoTransfer(
	const char* sPrefix,
	size_t iCapacity,
	uint32 iCount
)
{
	benchchannelco tTransfer;
	xchannel tChannel;
	xcosched* pSched;
	xcoro* pSend;
	xcoro* pRecv;
	xbenchtimer tTimer;
	uint64 iElapsed;
	uint64 iExpected =
		((uint64)iCount * ((uint64)iCount + 1u)) / 2u;
	bool bResult;

	memset(&tTransfer, 0, sizeof(tTransfer));
	if ( !xrtChannelInit(&tChannel, iCapacity) ) {
		return false;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		(void)xrtChannelUnit(&tChannel);
		return false;
	}
	tTransfer.Channel = &tChannel;
	tTransfer.Count = iCount;
	pSend = xrtCoSpawn(pSched, benchChannelCoSend, &tTransfer, NULL);
	pRecv = xrtCoSpawn(pSched, benchChannelCoRecv, &tTransfer, NULL);
	if ( (pSend == NULL) || (pRecv == NULL) ) {
		(void)xrtCoSchedClose(pSched);
		(void)xrtCoSchedRun(pSched);
		xrtCoDestroy(pSend);
		xrtCoDestroy(pRecv);
		xrtCoSchedDestroy(pSched);
		(void)xrtChannelUnit(&tChannel);
		return false;
	}

	xbenchTimerStart(&tTimer);
	bResult = xrtCoSchedRun(pSched);
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);
	bResult =
		bResult &&
		(xrtCoResult(pSend) == &tTransfer) &&
		(xrtCoResult(pRecv) == &tTransfer) &&
		(tTransfer.Checksum == iExpected);

	printf("%s_capacity: %zu\n", sPrefix, iCapacity);
	printf("%s_count: %" PRIu32 "\n", sPrefix, iCount);
	printf("%s_elapsed_ns: %" PRIu64 "\n", sPrefix, iElapsed);
	printf(
		"%s_items_per_sec: %.3f\n",
		sPrefix,
		xbenchSafeRate(iCount, iElapsed)
	);
	xrtCoDestroy(pSend);
	xrtCoDestroy(pRecv);
	xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return xrtChannelUnit(&tChannel) && bResult;
}



/* 测量两个 Channel 的协程 Select 持续吞吐。 */
static bool benchChannelSelectCoTransfer(uint32 iCount)
{
	benchchannelselectco tTransfer;
	xchannel tFirst;
	xchannel tSecond;
	xcosched* pSched;
	xcoro* pSend;
	xcoro* pRecv;
	xbenchtimer tTimer;
	uint64 iElapsed;
	uint64 iExpected =
		((uint64)iCount * ((uint64)iCount + 1u)) / 2u;
	bool bResult;

	memset(&tTransfer, 0, sizeof(tTransfer));
	if ( !xrtChannelInit(&tFirst, 64u) ) {
		return false;
	}
	if ( !xrtChannelInit(&tSecond, 64u) ) {
		(void)xrtChannelUnit(&tFirst);
		return false;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		(void)xrtChannelUnit(&tFirst);
		(void)xrtChannelUnit(&tSecond);
		return false;
	}
	tTransfer.First = &tFirst;
	tTransfer.Second = &tSecond;
	tTransfer.Count = iCount;
	pSend = xrtCoSpawn(
		pSched,
		benchChannelSelectCoSend,
		&tTransfer,
		NULL
	);
	pRecv = xrtCoSpawn(
		pSched,
		benchChannelSelectCoRecv,
		&tTransfer,
		NULL
	);
	if ( (pSend == NULL) || (pRecv == NULL) ) {
		(void)xrtCoSchedClose(pSched);
		(void)xrtCoSchedRun(pSched);
		xrtCoDestroy(pSend);
		xrtCoDestroy(pRecv);
		xrtCoSchedDestroy(pSched);
		(void)xrtChannelUnit(&tFirst);
		(void)xrtChannelUnit(&tSecond);
		return false;
	}

	xbenchTimerStart(&tTimer);
	bResult = xrtCoSchedRun(pSched);
	xbenchTimerStop(&tTimer);
	iElapsed = xbenchTimerElapsedNs(&tTimer);
	bResult =
		bResult &&
		(xrtCoResult(pSend) == &tTransfer) &&
		(xrtCoResult(pRecv) == &tTransfer) &&
		(tTransfer.Checksum == iExpected);

	xbenchPrintMetricU64("select_co_count", iCount);
	xbenchPrintMetricU64("select_co_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"select_co_items_per_sec",
		xbenchSafeRate(iCount, iElapsed)
	);
	xrtCoDestroy(pSend);
	xrtCoDestroy(pRecv);
	xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return
		xrtChannelUnit(&tFirst) &&
		xrtChannelUnit(&tSecond) &&
		bResult;
}



/* 执行 Channel 协程吞吐量基准。 */
int main(int argc, char** argv)
{
	uint32 iBufferedCount = xbenchArgU32(
		argc,
		argv,
		1,
		500000u
	);
	uint32 iRendezvousCount = xbenchArgU32(
		argc,
		argv,
		2,
		200000u
	);
	uint32 iSelectCount = xbenchArgU32(
		argc,
		argv,
		3,
		500000u
	);

	if (
		(iBufferedCount == 0) ||
		(iRendezvousCount == 0) ||
		(iSelectCount == 0)
	) {
		fprintf(stderr, "benchmark counts must be non-zero.\n");
		return 1;
	}
	printf("xrt channel coroutine benchmark\n");
	xbenchPrintMetricU64(
		"inline_case_limit",
		XRT_CHANNEL_SELECT_INLINE_CASES
	);
	xbenchPrintMetricU64(
		"select_waiter_bytes",
		sizeof(xrt_channel_select_waiter)
	);
	if (
		!benchChannelCoTransfer(
			"buffered_co",
			64u,
			iBufferedCount
		)
	) {
		return 2;
	}
	if (
		!benchChannelCoTransfer(
			"rendezvous_co",
			0,
			iRendezvousCount
		)
	) {
		return 3;
	}
	if ( !benchChannelSelectCoTransfer(iSelectCount) ) {
		return 4;
	}
	return 0;
}
