#include "../../src/internal/xrt_channel.h"
#include "../test.h"
#include "../test_thread.h"



/* 阻塞 Select 线程保存两个 case 和最终结果。 */
typedef struct testchannelselectop {
	xchannelcase Cases[2];
	size_t Count;
	xchannelselectresult Result;
} testchannelselectop;



/* 并发 TrySend 线程等待统一起点后记录流控结果。 */
typedef struct testchannelselectsend {
	xchannel* Channel;
	xevent* Start;
	ptr Item;
	xchannelresult Result;
} testchannelselectsend;



/* 在线程中阻塞选择任意 case。 */
static int testChannelSelectWorker(ptr pData)
{
	testchannelselectop* pOp = (testchannelselectop*)pData;

	pOp->Result = xrtChannelSelect(pOp->Cases, pOp->Count);
	return 0;
}



/* 在统一事件后尝试一次无缓冲发送。 */
static int testChannelSelectSendWorker(ptr pData)
{
	testchannelselectsend* pSend =
		(testchannelselectsend*)pData;

	if ( xrtEventWait(pSend->Start) != XWAIT_OK ) {
		return 1;
	}
	pSend->Result = xrtChannelTrySend(
		pSend->Channel,
		pSend->Item
	);
	return 0;
}



/* 等待每个 Channel 出现一个 Select 注册节点。 */
static void testChannelSelectAwaitRegistered(
	xchannel** pChannels,
	size_t iCount
)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(2000000));

	for ( ;; ) {
		size_t iReady = 0;

		for ( size_t i = 0; i < iCount; i++ ) {
			xrt_channel_impl* pImpl =
				(xrt_channel_impl*)pChannels[i];

			testRequire(
				xrtMutexLock(&pImpl->Mutex),
				"select registration probe lock failed"
			);
			if ( pImpl->SelectWaiters != NULL ) {
				iReady++;
			}
			testRequire(
				xrtMutexUnlock(&pImpl->Mutex),
				"select registration probe unlock failed"
			);
		}
		if ( iReady == iCount ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			"select registration did not converge"
		);
		xrtSleepUs(UINT64_C(1000));
	}
}



/* 验证有缓冲唤醒、关闭唤醒和活动注册生命周期保护。 */
static void testChannelSelectWake(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannel* arrChannel[2] = { &tFirst, &tSecond };
	testchannelselectop tOp;
	testthread tThread;
	ptr pFirst = NULL;
	ptr pSecond = NULL;

	testRequire(xrtChannelInit(&tFirst, 1u), "select wake first init failed");
	testRequire(xrtChannelInit(&tSecond, 1u), "select wake second init failed");
	memset(&tOp, 0, sizeof(tOp));
	tOp.Cases[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	tOp.Cases[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
	tOp.Count = 2u;
	tThread.Proc = testChannelSelectWorker;
	tThread.Data = &tOp;
	testThreadsStart(&tThread, 1u);
	testChannelSelectAwaitRegistered(arrChannel, 2u);

	xrtClearError();
	testRequire(
		!xrtChannelUnit(&tFirst),
		"channel unit accepted active Select registration"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"active Select unit error mismatch"
	);
	testRequire(
		xrtChannelTrySend(
			&tSecond,
			(ptr)(uintptr_t)77u
		) == XCHANNEL_OK,
		"select wake send failed"
	);
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tOp.Result.Wait == XWAIT_OK) &&
		(tOp.Result.Index == 1u) &&
		(tOp.Result.Result == XCHANNEL_OK) &&
		((uintptr_t)pSecond == 77u) &&
		(pFirst == NULL),
		"buffered Select wake result mismatch"
	);

	memset(&tOp, 0, sizeof(tOp));
	pFirst = (ptr)(uintptr_t)1u;
	tOp.Cases[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	tOp.Count = 1u;
	testThreadsStart(&tThread, 1u);
	testChannelSelectAwaitRegistered(arrChannel, 1u);
	xrtChannelClose(&tFirst);
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tOp.Result.Wait == XWAIT_OK) &&
		(tOp.Result.Index == 0) &&
		(tOp.Result.Result == XCHANNEL_CLOSED) &&
		(pFirst == NULL),
		"close did not wake Select"
	);

	testRequire(xrtChannelUnit(&tFirst), "select wake first unit failed");
	testRequire(xrtChannelUnit(&tSecond), "select wake second unit failed");
}



/* 验证无缓冲 Select 发送可与普通接收和 Select 接收配对。 */
static void testChannelSelectRendezvous(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannel* arrChannel[2] = { &tFirst, &tSecond };
	testchannelselectop tSend;
	testchannelselectop tRecv;
	testthread tThread;
	ptr pItem = NULL;

	testRequire(xrtChannelInit(&tFirst, 0), "select rendezvous first init failed");
	testRequire(xrtChannelInit(&tSecond, 0), "select rendezvous second init failed");
	memset(&tSend, 0, sizeof(tSend));
	tSend.Cases[0] = xrtChannelCaseSend(
		&tFirst,
		(ptr)(uintptr_t)11u
	);
	tSend.Cases[1] = xrtChannelCaseSend(
		&tSecond,
		(ptr)(uintptr_t)22u
	);
	tSend.Count = 2u;
	tThread.Proc = testChannelSelectWorker;
	tThread.Data = &tSend;
	testThreadsStart(&tThread, 1u);
	testChannelSelectAwaitRegistered(arrChannel, 2u);
	testRequire(
		xrtChannelRecvFor(
			&tSecond,
			&pItem,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"normal receiver did not pair with Select sender"
	);
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tSend.Result.Wait == XWAIT_OK) &&
		(tSend.Result.Index == 1u) &&
		((uintptr_t)pItem == 22u),
		"Select sender normal pairing mismatch"
	);

	/* 已注册 Select 接收者允许另一个 Select 原子提交发送。 */
	memset(&tRecv, 0, sizeof(tRecv));
	pItem = NULL;
	tRecv.Cases[0] = xrtChannelCaseRecv(&tFirst, &pItem);
	tRecv.Count = 1u;
	tThread.Data = &tRecv;
	testThreadsStart(&tThread, 1u);
	testChannelSelectAwaitRegistered(arrChannel, 1u);
	tSend.Count = 1u;
	tSend.Cases[0] = xrtChannelCaseSend(
		&tFirst,
		(ptr)(uintptr_t)33u
	);
	tSend.Result = xrtChannelSelect(
		tSend.Cases,
		tSend.Count
	);
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tSend.Result.Wait == XWAIT_OK) &&
		(tSend.Result.Index == 0) &&
		(tRecv.Result.Wait == XWAIT_OK) &&
		(tRecv.Result.Index == 0) &&
		((uintptr_t)pItem == 33u),
		"Select-to-Select rendezvous mismatch"
	);

	testRequire(xrtChannelUnit(&tFirst), "select rendezvous first unit failed");
	testRequire(xrtChannelUnit(&tSecond), "select rendezvous second unit failed");
}



/* 验证不同 Channel 上的并发发送只能提交一个 Select case。 */
static void testChannelSelectSingleWinner(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannel* arrChannel[2] = { &tFirst, &tSecond };
	xevent tStart;
	testchannelselectop tSelect;
	testchannelselectsend arrSend[2];
	testthread tSelectThread;
	testthread arrThread[2];
	ptr pFirst = NULL;
	ptr pSecond = NULL;
	size_t iSuccess = 0;

	testRequire(xrtChannelInit(&tFirst, 0), "winner first init failed");
	testRequire(xrtChannelInit(&tSecond, 0), "winner second init failed");
	testRequire(xrtEventInit(&tStart, true, false), "winner start event init failed");
	memset(&tSelect, 0, sizeof(tSelect));
	tSelect.Cases[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	tSelect.Cases[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
	tSelect.Count = 2u;
	tSelectThread.Proc = testChannelSelectWorker;
	tSelectThread.Data = &tSelect;
	testThreadsStart(&tSelectThread, 1u);
	testChannelSelectAwaitRegistered(arrChannel, 2u);

	for ( size_t i = 0; i < 2u; i++ ) {
		arrSend[i].Channel = arrChannel[i];
		arrSend[i].Start = &tStart;
		arrSend[i].Item = (ptr)(uintptr_t)(i + 1u);
		arrSend[i].Result = XCHANNEL_ERROR;
		arrThread[i].Proc = testChannelSelectSendWorker;
		arrThread[i].Data = &arrSend[i];
	}
	testThreadsStart(arrThread, 2u);
	testRequire(xrtEventSet(&tStart), "winner start event set failed");
	testThreadsJoin(arrThread, 2u);
	testThreadsJoin(&tSelectThread, 1u);

	for ( size_t i = 0; i < 2u; i++ ) {
		if ( arrSend[i].Result == XCHANNEL_OK ) {
			iSuccess++;
		} else {
			testRequire(
				arrSend[i].Result == XCHANNEL_FULL,
				"losing rendezvous send result mismatch"
			);
		}
	}
	testRequire(iSuccess == 1u, "multiple Select cases committed");
	testRequire(
		(tSelect.Result.Wait == XWAIT_OK) &&
		(tSelect.Result.Index < 2u),
		"single-winner Select result mismatch"
	);
	testRequire(
		tSelect.Result.Index == 0 ?
			((uintptr_t)pFirst == 1u) :
			((uintptr_t)pSecond == 2u),
		"single-winner Select payload mismatch"
	);

	testRequire(xrtEventUnit(&tStart), "winner start event unit failed");
	testRequire(xrtChannelUnit(&tFirst), "winner first unit failed");
	testRequire(xrtChannelUnit(&tSecond), "winner second unit failed");
}



/* 验证真实 deadline 返回后已经移除全部注册节点。 */
static void testChannelSelectTimeout(void)
{
	xchannel tChannel;
	xchannelcase tCase;
	xchannelselectresult tResult;
	xrt_channel_impl* pImpl;
	ptr pItem = (ptr)(uintptr_t)1u;
	uint64 iStarted;
	uint64 iElapsed;

	testRequire(xrtChannelInit(&tChannel, 0), "select timeout init failed");
	tCase = xrtChannelCaseRecv(&tChannel, &pItem);
	iStarted = xrtClock();
	tResult = xrtChannelSelectFor(
		&tCase,
		1u,
		UINT64_C(20000)
	);
	iElapsed = xrtClock() - iStarted;
	testRequire(
		(tResult.Wait == XWAIT_TIMEOUT) &&
		(tResult.Index == XCHANNEL_SELECT_NONE),
		"select timeout result mismatch"
	);
	testRequire((uintptr_t)pItem == 1u, "timeout modified receive output");
	testRequire(iElapsed >= UINT64_C(10000), "select timeout returned too early");
	testRequire(iElapsed < UINT64_C(2000000), "select timeout returned too late");
	pImpl = (xrt_channel_impl*)&tChannel;
	testRequire(
		pImpl->SelectWaiters == NULL,
		"select timeout leaked registration"
	);
	testRequire(xrtChannelUnit(&tChannel), "select timeout unit failed");
}



/* 执行 Channel Select 并发合同测试。 */
int main(void)
{
	testChannelSelectWake();
	testChannelSelectRendezvous();
	testChannelSelectSingleWinner();
	testChannelSelectTimeout();
	printf("[PASS] channel select threads\n");
	return 0;
}
