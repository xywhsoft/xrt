#include "../../src/internal/xrt_channel.h"
#include "../test.h"
#include "../test_thread.h"



/* 单次发送或接收线程共享的状态。 */
typedef struct testchannelop {
	xchannel* Channel;
	ptr Item;
	xwaitresult Result;
} testchannelop;



/* 在线程中无限等待发送。 */
static int testChannelSendWorker(ptr pData)
{
	testchannelop* pOp = (testchannelop*)pData;

	pOp->Result = xrtChannelSend(pOp->Channel, pOp->Item);
	return 0;
}



/* 在线程中无限等待接收。 */
static int testChannelRecvWorker(ptr pData)
{
	testchannelop* pOp = (testchannelop*)pData;

	pOp->Result = xrtChannelRecv(pOp->Channel, &pOp->Item);
	return 0;
}



/* 等待 Channel 进入指定读写等待者数量。 */
static void testChannelAwaitWaiters(
	xchannel* pChannel,
	size_t iReaders,
	size_t iWriters
)
{
	xrt_channel_impl* pImpl = (xrt_channel_impl*)pChannel;
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));

	for ( ;; ) {
		size_t iActualReaders;
		size_t iActualWriters;

		testRequire(
			xrtMutexLock(&pImpl->Mutex),
			"channel waiter probe lock failed"
		);
		iActualReaders = pImpl->ReadWaiters;
		iActualWriters = pImpl->WriteWaiters;
		testRequire(
			xrtMutexUnlock(&pImpl->Mutex),
			"channel waiter probe unlock failed"
		);
		if (
			(iActualReaders == iReaders) &&
			(iActualWriters == iWriters)
		) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"channel waiter count did not converge"
		);
		xrtSleepUs(UINT64_C(1000));
	}
}



/* 验证有缓冲 Channel 的读写唤醒和关闭广播。 */
static void testChannelBufferedWait(void)
{
	xchannel tChannel;
	testthread tThread;
	testthread arrReaders[2];
	testchannelop tOp;
	testchannelop arrRecv[2];
	ptr pItem = NULL;

	testRequire(xrtChannelInit(&tChannel, 1u), "buffered wait channel init failed");
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)1u) == XCHANNEL_OK,
		"buffered wait prefill failed"
	);
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Item = (ptr)(uintptr_t)2u;
	tThread.Proc = testChannelSendWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelAwaitWaiters(&tChannel, 0, 1);
	testRequire(
		xrtChannelRecv(&tChannel, &pItem) == XWAIT_OK,
		"buffered wait first receive failed"
	);
	testRequire((uintptr_t)pItem == 1u, "buffered wait FIFO first value mismatch");
	testThreadsJoin(&tThread, 1);
	testRequire(tOp.Result == XWAIT_OK, "full channel sender was not woken");
	testRequire(
		xrtChannelRecv(&tChannel, &pItem) == XWAIT_OK,
		"buffered wait second receive failed"
	);
	testRequire((uintptr_t)pItem == 2u, "buffered wait FIFO second value mismatch");

	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tThread.Proc = testChannelRecvWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelAwaitWaiters(&tChannel, 1, 0);
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)3u) == XCHANNEL_OK,
		"buffered waiter wake send failed"
	);
	testThreadsJoin(&tThread, 1);
	testRequire(
		(tOp.Result == XWAIT_OK) && ((uintptr_t)tOp.Item == 3u),
		"empty channel receiver was not woken"
	);

	/* Unit 必须拒绝仍有等待者的对象。 */
	memset(arrRecv, 0, sizeof(arrRecv));
	for ( size_t i = 0; i < 2u; i++ ) {
		arrRecv[i].Channel = &tChannel;
		arrReaders[i].Proc = testChannelRecvWorker;
		arrReaders[i].Data = &arrRecv[i];
	}
	testThreadsStart(arrReaders, 2);
	testChannelAwaitWaiters(&tChannel, 2, 0);
	xrtClearError();
	testRequire(
		!xrtChannelUnit(&tChannel),
		"channel unit accepted active waiters"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"active channel unit error mismatch"
	);
	xrtChannelClose(&tChannel);
	testThreadsJoin(arrReaders, 2);
	for ( size_t i = 0; i < 2u; i++ ) {
		testRequire(
			arrRecv[i].Result == XWAIT_CLOSED,
			"close did not wake buffered receiver"
		);
	}
	testRequire(xrtChannelUnit(&tChannel), "buffered wait channel unit failed");
}



/* 验证 rendezvous 配对、关闭撤回和单值关闭竞争。 */
static void testChannelRendezvousWait(void)
{
	xchannel tChannel;
	testthread tThread;
	testthread arrReaders[2];
	testchannelop tOp;
	testchannelop arrRecv[2];
	ptr pItem = NULL;
	size_t iOk = 0;
	size_t iClosed = 0;

	testRequire(xrtChannelInit(&tChannel, 0), "rendezvous wait init failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Item = (ptr)(uintptr_t)11u;
	tThread.Proc = testChannelSendWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelAwaitWaiters(&tChannel, 0, 1);
	testRequire(
		xrtChannelTryRecv(&tChannel, &pItem) == XCHANNEL_OK,
		"rendezvous did not expose waiting sender"
	);
	testRequire((uintptr_t)pItem == 11u, "rendezvous sender payload mismatch");
	testThreadsJoin(&tThread, 1);
	testRequire(tOp.Result == XWAIT_OK, "rendezvous sender completion mismatch");

	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tThread.Proc = testChannelRecvWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelAwaitWaiters(&tChannel, 1, 0);
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)12u) == XCHANNEL_OK,
		"rendezvous try send did not pair"
	);
	testThreadsJoin(&tThread, 1);
	testRequire(
		(tOp.Result == XWAIT_OK) && ((uintptr_t)tOp.Item == 12u),
		"rendezvous receiver payload mismatch"
	);

	/* 关闭撤回尚未配对的阻塞发送。 */
	memset(&tOp, 0, sizeof(tOp));
	tOp.Channel = &tChannel;
	tOp.Item = (ptr)(uintptr_t)13u;
	tThread.Proc = testChannelSendWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1);
	testChannelAwaitWaiters(&tChannel, 0, 1);
	xrtChannelClose(&tChannel);
	testThreadsJoin(&tThread, 1);
	testRequire(
		tOp.Result == XWAIT_CLOSED,
		"close did not withdraw rendezvous sender"
	);
	testRequire(xrtChannelReset(&tChannel), "rendezvous close reset failed");

	/* 一个已提交值和关闭竞争时只能产生一次成功接收。 */
	memset(arrRecv, 0, sizeof(arrRecv));
	for ( size_t i = 0; i < 2u; i++ ) {
		arrRecv[i].Channel = &tChannel;
		arrReaders[i].Proc = testChannelRecvWorker;
		arrReaders[i].Data = &arrRecv[i];
	}
	testThreadsStart(arrReaders, 2);
	testChannelAwaitWaiters(&tChannel, 2, 0);
	testRequire(
		xrtChannelTrySend(&tChannel, (ptr)(uintptr_t)14u) == XCHANNEL_OK,
		"rendezvous close race send failed"
	);
	xrtChannelClose(&tChannel);
	testThreadsJoin(arrReaders, 2);
	for ( size_t i = 0; i < 2u; i++ ) {
		if ( arrRecv[i].Result == XWAIT_OK ) {
			iOk++;
			testRequire(
				(uintptr_t)arrRecv[i].Item == 14u,
				"rendezvous close race payload mismatch"
			);
		} else if ( arrRecv[i].Result == XWAIT_CLOSED ) {
			iClosed++;
		}
	}
	testRequire(
		(iOk == 1u) && (iClosed == 1u),
		"rendezvous close race result cardinality mismatch"
	);
	testRequire(xrtChannelUnit(&tChannel), "rendezvous wait unit failed");
}



#define TEST_CHANNEL_PRODUCERS 4u
#define TEST_CHANNEL_CONSUMERS 4u
#define TEST_CHANNEL_ITEMS 2000u
#define TEST_CHANNEL_TOTAL \
	(TEST_CHANNEL_PRODUCERS * TEST_CHANNEL_ITEMS)



/* 压力测试共享一次性接收位图。 */
typedef struct testchannelstress {
	xchannel Channel;
	xmutex Lock;
	uint8 Seen[TEST_CHANNEL_TOTAL];
	size_t Received;
} testchannelstress;



/* 每个生产者发送互不重叠的编号区间。 */
typedef struct testchannelproducer {
	testchannelstress* State;
	size_t Base;
} testchannelproducer;



/* 在线程中发送一个生产者区间。 */
static int testChannelProducer(ptr pData)
{
	testchannelproducer* pProducer = (testchannelproducer*)pData;

	for ( size_t i = 0; i < TEST_CHANNEL_ITEMS; i++ ) {
		size_t iValue = pProducer->Base + i;

		if (
			xrtChannelSend(
				&pProducer->State->Channel,
				(ptr)(uintptr_t)(iValue + 1u)
			) != XWAIT_OK
		) {
			return 1;
		}
	}
	return 0;
}



/* 多个消费者共同验证每个编号只接收一次。 */
static int testChannelConsumer(ptr pData)
{
	testchannelstress* pState = (testchannelstress*)pData;
	ptr pItem = NULL;

	for ( ;; ) {
		xwaitresult iResult = xrtChannelRecv(&pState->Channel, &pItem);
		size_t iValue;

		if ( iResult == XWAIT_CLOSED ) {
			return 0;
		}
		if ( iResult != XWAIT_OK ) {
			return 1;
		}
		iValue = (size_t)(uintptr_t)pItem;
		if ( (iValue == 0) || (iValue > TEST_CHANNEL_TOTAL) ) {
			return 2;
		}
		iValue--;
		if ( !xrtMutexLock(&pState->Lock) ) {
			return 3;
		}
		if ( pState->Seen[iValue] != 0 ) {
			(void)xrtMutexUnlock(&pState->Lock);
			return 4;
		}
		pState->Seen[iValue] = 1;
		pState->Received++;
		if ( !xrtMutexUnlock(&pState->Lock) ) {
			return 5;
		}
	}
}



/* 验证多个生产者和消费者下无丢失、无重复。 */
static void testChannelStress(void)
{
	testchannelstress tState;
	testchannelproducer arrProducer[TEST_CHANNEL_PRODUCERS];
	testthread arrProducerThread[TEST_CHANNEL_PRODUCERS];
	testthread arrConsumerThread[TEST_CHANNEL_CONSUMERS];

	memset(&tState, 0, sizeof(tState));
	testRequire(xrtMutexInit(&tState.Lock), "channel stress lock init failed");
	testRequire(xrtChannelInit(&tState.Channel, 64u), "channel stress init failed");
	for ( size_t i = 0; i < TEST_CHANNEL_CONSUMERS; i++ ) {
		arrConsumerThread[i].Proc = testChannelConsumer;
		arrConsumerThread[i].Data = &tState;
	}
	for ( size_t i = 0; i < TEST_CHANNEL_PRODUCERS; i++ ) {
		arrProducer[i].State = &tState;
		arrProducer[i].Base = i * TEST_CHANNEL_ITEMS;
		arrProducerThread[i].Proc = testChannelProducer;
		arrProducerThread[i].Data = &arrProducer[i];
	}

	testThreadsStart(arrConsumerThread, TEST_CHANNEL_CONSUMERS);
	testThreadsStart(arrProducerThread, TEST_CHANNEL_PRODUCERS);
	testThreadsJoin(arrProducerThread, TEST_CHANNEL_PRODUCERS);
	for ( size_t i = 0; i < TEST_CHANNEL_PRODUCERS; i++ ) {
		testRequire(
			arrProducerThread[i].Result == 0,
			"channel stress producer failed"
		);
	}
	xrtChannelClose(&tState.Channel);
	testThreadsJoin(arrConsumerThread, TEST_CHANNEL_CONSUMERS);
	for ( size_t i = 0; i < TEST_CHANNEL_CONSUMERS; i++ ) {
		testRequire(
			arrConsumerThread[i].Result == 0,
			"channel stress consumer failed"
		);
	}
	testRequire(
		tState.Received == TEST_CHANNEL_TOTAL,
		"channel stress receive count mismatch"
	);
	for ( size_t i = 0; i < TEST_CHANNEL_TOTAL; i++ ) {
		testRequire(tState.Seen[i] == 1, "channel stress lost item");
	}
	testRequire(xrtChannelUnit(&tState.Channel), "channel stress unit failed");
	testRequire(xrtMutexUnit(&tState.Lock), "channel stress lock unit failed");
}



/* 验证真实定时等待不会过早或无限延迟。 */
static void testChannelTimeout(void)
{
	xchannel tChannel;
	ptr pItem = NULL;
	uint64 iStarted;
	uint64 iElapsed;

	testRequire(xrtChannelInit(&tChannel, 1u), "channel timeout init failed");
	iStarted = xrtClock();
	testRequire(
		xrtChannelRecvFor(
			&tChannel,
			&pItem,
			UINT64_C(20000)
		) == XWAIT_TIMEOUT,
		"channel receive timeout result mismatch"
	);
	iElapsed = xrtClock() - iStarted;
	testRequire(iElapsed >= UINT64_C(10000), "channel timeout returned too early");
	testRequire(iElapsed < UINT64_C(2000000), "channel timeout returned too late");
	testRequire(xrtChannelUnit(&tChannel), "channel timeout unit failed");
}



/* 执行 Channel 并发合同测试。 */
int main(void)
{
	testChannelBufferedWait();
	testChannelRendezvousWait();
	testChannelStress();
	testChannelTimeout();
	printf("[PASS] channel threads\n");
	return 0;
}
