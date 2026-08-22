#include "../../src/internal/xrt_channel.h"
#include "../test.h"
#include "../test_thread.h"



#define TEST_CHANNEL_CANCEL_RACE_COUNT 256u
#define TEST_CHANNEL_CANCEL_SHARED_WAITERS 8u



/* 一个可取消接收保存独立输出和终态。 */
typedef struct testchannelcancelrecv {
	xchannel* Channel;
	xcancel* Cancel;
	ptr Item;
	xwaitresult Result;
} testchannelcancelrecv;



/* 一组竞争动作共享 Channel、取消令牌和发送值。 */
typedef struct testchannelcancelrace {
	xchannel* Channel;
	xcancel* Cancel;
	ptr Item;
	xchannelresult SendResult;
	bool CancelResult;
} testchannelcancelrace;



/* 在线程中执行无限可取消接收。 */
static int testChannelCancelRecv(ptr pData)
{
	testchannelcancelrecv* pRecv = (testchannelcancelrecv*)pData;

	pRecv->Result = xrtChannelRecvCancel(
		pRecv->Channel,
		&pRecv->Item,
		pRecv->Cancel
	);
	return 0;
}



/* 在线程中尝试提交一个有缓冲值。 */
static int testChannelCancelSend(ptr pData)
{
	testchannelcancelrace* pRace = (testchannelcancelrace*)pData;

	pRace->SendResult = xrtChannelTrySend(pRace->Channel, pRace->Item);
	return 0;
}



/* 在线程中请求共享令牌取消。 */
static int testChannelCancelRequest(ptr pData)
{
	testchannelcancelrace* pRace = (testchannelcancelrace*)pData;

	pRace->CancelResult = xrtCancelRequest(pRace->Cancel);
	return 0;
}



/* 在线程中关闭 Channel。 */
static int testChannelCancelClose(ptr pData)
{
	testchannelcancelrace* pRace = (testchannelcancelrace*)pData;

	xrtChannelClose(pRace->Channel);
	return 0;
}



/* 等待 Channel 出现精确数量的接收等待者。 */
static void testChannelCancelWaitReaders(
	xchannel* pChannel,
	size_t iExpected
)
{
	xrt_channel_impl* pImpl = (xrt_channel_impl*)pChannel;
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));

	for ( ;; ) {
		size_t iReaders;

		testRequire(
			xrtMutexLock(&pImpl->Mutex),
			"channel cancel race probe lock failed"
		);
		iReaders = pImpl->ReadWaiters;
		testRequire(
			xrtMutexUnlock(&pImpl->Mutex),
			"channel cancel race probe unlock failed"
		);
		if ( iReaders == iExpected ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"channel cancel race waiters did not converge"
		);
		xrtSleepUs(UINT64_C(1000));
	}
}



/* 交替启动两种动作，降低固定线程创建顺序造成的竞争偏置。 */
static void testChannelCancelRaceStart(
	testthread* pThreads,
	testthreadproc pFirst,
	testthreadproc pSecond,
	ptr pData,
	uint32 iIteration
)
{
	if ( (iIteration & 1u) == 0 ) {
		pThreads[0].Proc = pFirst;
		pThreads[1].Proc = pSecond;
	} else {
		pThreads[0].Proc = pSecond;
		pThreads[1].Proc = pFirst;
	}
	pThreads[0].Data = pData;
	pThreads[1].Data = pData;
	testThreadsStart(pThreads, 2u);
}



/* 验证发送与取消竞争只产生一次提交或一次保留。 */
static void testChannelCancelReadyRace(void)
{
	for ( uint32 i = 0; i < TEST_CHANNEL_CANCEL_RACE_COUNT; i++ ) {
		xchannel tChannel;
		testchannelcancelrecv tRecv;
		testchannelcancelrace tRace;
		testthread tRecvThread;
		testthread arrRaceThreads[2];
		ptr pRemaining = NULL;

		testRequire(
			xrtChannelInit(&tChannel, 1u),
			"channel cancel ready race init failed"
		);
		memset(&tRecv, 0, sizeof(tRecv));
		memset(&tRace, 0, sizeof(tRace));
		tRecv.Channel = &tChannel;
		tRecv.Cancel = xrtCancelCreate();
		testRequire(tRecv.Cancel != NULL, "channel cancel ready race token failed");
		tRecvThread.Proc = testChannelCancelRecv;
		tRecvThread.Data = &tRecv;
		testThreadsStart(&tRecvThread, 1u);
		testChannelCancelWaitReaders(&tChannel, 1u);

		tRace.Channel = &tChannel;
		tRace.Cancel = tRecv.Cancel;
		tRace.Item = (ptr)(uintptr_t)(i + 1u);
		testChannelCancelRaceStart(
			arrRaceThreads,
			testChannelCancelSend,
			testChannelCancelRequest,
			&tRace,
			i
		);
		testThreadsJoin(arrRaceThreads, 2u);
		testThreadsJoin(&tRecvThread, 1u);

		testRequire(tRace.CancelResult, "channel cancel ready race request failed");
		testRequire(
			tRace.SendResult == XCHANNEL_OK,
			"channel cancel ready race send failed"
		);
		if ( tRecv.Result == XWAIT_OK ) {
			testRequire(
				tRecv.Item == tRace.Item,
				"channel cancel ready race received wrong item"
			);
			testRequire(
				xrtChannelTryRecv(&tChannel, &pRemaining) == XCHANNEL_EMPTY,
				"channel cancel ready race duplicated item"
			);
		} else {
			testRequire(
				(tRecv.Result == XWAIT_CANCELLED) && (tRecv.Item == NULL),
				"channel cancel ready race terminal mismatch"
			);
			testRequire(
				(xrtChannelTryRecv(&tChannel, &pRemaining) == XCHANNEL_OK) &&
				(pRemaining == tRace.Item),
				"channel cancel ready race lost retained item"
			);
		}

		xrtCancelDestroy(tRecv.Cancel);
		testRequire(
			xrtChannelUnit(&tChannel),
			"channel cancel ready race left a stale callback"
		);
	}
}



/* 验证关闭与取消竞争只返回对应的一个终态。 */
static void testChannelCancelCloseRace(void)
{
	for ( uint32 i = 0; i < TEST_CHANNEL_CANCEL_RACE_COUNT; i++ ) {
		xchannel tChannel;
		testchannelcancelrecv tRecv;
		testchannelcancelrace tRace;
		testthread tRecvThread;
		testthread arrRaceThreads[2];

		testRequire(
			xrtChannelInit(&tChannel, 1u),
			"channel cancel close race init failed"
		);
		memset(&tRecv, 0, sizeof(tRecv));
		memset(&tRace, 0, sizeof(tRace));
		tRecv.Channel = &tChannel;
		tRecv.Cancel = xrtCancelCreate();
		testRequire(tRecv.Cancel != NULL, "channel cancel close race token failed");
		tRecvThread.Proc = testChannelCancelRecv;
		tRecvThread.Data = &tRecv;
		testThreadsStart(&tRecvThread, 1u);
		testChannelCancelWaitReaders(&tChannel, 1u);

		tRace.Channel = &tChannel;
		tRace.Cancel = tRecv.Cancel;
		testChannelCancelRaceStart(
			arrRaceThreads,
			testChannelCancelClose,
			testChannelCancelRequest,
			&tRace,
			i
		);
		testThreadsJoin(arrRaceThreads, 2u);
		testThreadsJoin(&tRecvThread, 1u);
		testRequire(tRace.CancelResult, "channel cancel close race request failed");
		testRequire(
			((tRecv.Result == XWAIT_CLOSED) ||
			 (tRecv.Result == XWAIT_CANCELLED)) &&
			(tRecv.Item == NULL),
			"channel cancel close race terminal mismatch"
		);

		xrtCancelDestroy(tRecv.Cancel);
		testRequire(
			xrtChannelUnit(&tChannel),
			"channel cancel close race left a stale callback"
		);
	}
}



/* 验证同一令牌可以同步唤醒多个等待，并在返回前摘除全部监听。 */
static void testChannelCancelShared(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	testchannelcancelrecv arrRecv[TEST_CHANNEL_CANCEL_SHARED_WAITERS];
	testthread arrThreads[TEST_CHANNEL_CANCEL_SHARED_WAITERS];

	testRequire(xrtChannelInit(&tChannel, 1u), "shared cancel channel init failed");
	pCancel = xrtCancelCreate();
	testRequire(pCancel != NULL, "shared channel cancel token failed");
	memset(arrRecv, 0, sizeof(arrRecv));
	for ( size_t i = 0; i < TEST_CHANNEL_CANCEL_SHARED_WAITERS; i++ ) {
		arrRecv[i].Channel = &tChannel;
		arrRecv[i].Cancel = pCancel;
		arrThreads[i].Proc = testChannelCancelRecv;
		arrThreads[i].Data = &arrRecv[i];
	}
	testThreadsStart(arrThreads, TEST_CHANNEL_CANCEL_SHARED_WAITERS);
	testChannelCancelWaitReaders(&tChannel, TEST_CHANNEL_CANCEL_SHARED_WAITERS);
	testRequire(xrtCancelRequest(pCancel), "shared channel cancel request failed");
	testThreadsJoin(arrThreads, TEST_CHANNEL_CANCEL_SHARED_WAITERS);
	for ( size_t i = 0; i < TEST_CHANNEL_CANCEL_SHARED_WAITERS; i++ ) {
		testRequire(
			(arrRecv[i].Result == XWAIT_CANCELLED) &&
			(arrRecv[i].Item == NULL),
			"shared channel cancel waiter mismatch"
		);
	}
	xrtCancelDestroy(pCancel);
	testRequire(
		xrtChannelUnit(&tChannel),
		"shared channel cancel left a stale callback"
	);
}



/* 验证父令牌取消可以直接中断使用子令牌的 Channel 等待。 */
static void testChannelCancelParent(void)
{
	xchannel tChannel;
	xcancel* pParent = xrtCancelCreate();
	xcancel* pChild;
	testchannelcancelrecv tRecv;
	testthread tThread;

	testRequire(pParent != NULL, "parent channel cancel token failed");
	pChild = xrtCancelChild(pParent);
	testRequire(pChild != NULL, "child channel cancel token failed");
	testRequire(xrtChannelInit(&tChannel, 0), "parent cancel channel init failed");
	memset(&tRecv, 0, sizeof(tRecv));
	tRecv.Channel = &tChannel;
	tRecv.Cancel = pChild;
	tThread.Proc = testChannelCancelRecv;
	tThread.Data = &tRecv;
	testThreadsStart(&tThread, 1u);
	testChannelCancelWaitReaders(&tChannel, 1u);
	testRequire(xrtCancelRequest(pParent), "parent channel cancel request failed");
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tRecv.Result == XWAIT_CANCELLED) && (tRecv.Item == NULL),
		"parent channel cancellation mismatch"
	);
	xrtCancelDestroy(pChild);
	xrtCancelDestroy(pParent);
	testRequire(xrtChannelUnit(&tChannel), "parent cancel channel unit failed");
}



/* 验证便利入口及就绪、关闭、取消和截止时间的稳定优先级。 */
static void testChannelCancelConvenience(void)
{
	xchannel tChannel;
	xcancel* pCancel = xrtCancelCreate();
	ptr pItem = NULL;

	testRequire(pCancel != NULL, "channel cancel convenience token failed");
	testRequire(xrtChannelInit(&tChannel, 1u), "channel cancel convenience init failed");
	testRequire(xrtCancelRequest(pCancel), "channel cancel convenience request failed");
	testRequire(
		xrtChannelSendCancel(&tChannel, (ptr)(uintptr_t)91u, pCancel) == XWAIT_OK,
		"ready SendCancel lost to cancellation"
	);
	testRequire(
		(xrtChannelRecvForCancel(&tChannel, &pItem, 0, pCancel) == XWAIT_OK) &&
		((uintptr_t)pItem == 91u),
		"ready RecvForCancel lost to cancellation"
	);
	testRequire(
		xrtChannelRecvCancel(&tChannel, &pItem, pCancel) == XWAIT_CANCELLED,
		"RecvCancel did not report cancellation"
	);
	testRequire(
		xrtChannelRecvForCancel(&tChannel, &pItem, 0, NULL) == XWAIT_TIMEOUT,
		"null-token RecvForCancel deadline mismatch"
	);
	xrtChannelClose(&tChannel);
	testRequire(
		xrtChannelSendCancel(&tChannel, NULL, pCancel) == XWAIT_CLOSED,
		"closed SendCancel lost to cancellation"
	);
	testRequire(
		xrtChannelRecvCancel(&tChannel, &pItem, pCancel) == XWAIT_CLOSED,
		"closed RecvCancel lost to cancellation"
	);
	xrtCancelDestroy(pCancel);
	testRequire(xrtChannelUnit(&tChannel), "channel cancel convenience unit failed");
}



/* 执行 Channel 取消竞争与便利入口测试。 */
int main(void)
{
	testChannelCancelReadyRace();
	testChannelCancelCloseRace();
	testChannelCancelShared();
	testChannelCancelParent();
	testChannelCancelConvenience();
	printf("[PASS] channel cancel races\n");
	return 0;
}
