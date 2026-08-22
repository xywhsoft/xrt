#include "../../src/internal/xrt_channel.h"
#include "../../src/internal/xrt_coroutine.h"
#include "../test.h"
#include "../test_thread.h"



/* 单路 Channel 协程操作保存输入、输出和等待结果。 */
typedef struct testchannelawait {
	xchannel* Channel;
	ptr Input;
	ptr Output;
	xwaitresult Result;
	uint64 Timeout;
	bool Send;
	bool CheckNextPark;
} testchannelawait;



/* 多路协程等待保存 case 与最终选择结果。 */
typedef struct testchannelselectawait {
	xchannelcase Cases[2];
	xchannelselectresult Result;
	size_t Count;
} testchannelselectawait;



/* 取消辅助协程在目标挂起后发出协作取消。 */
typedef struct testchannelawaitcancel {
	xcoro* Target;
} testchannelawaitcancel;



/* 内部资源等待令牌测试保存每一步的等待结果。 */
typedef struct testcowaittoken {
	xwaitresult Early;
	xwaitresult AfterClose;
	xwaitresult Generic;
	xwaitresult Final;
} testcowaittoken;



/* 跨线程生产者延迟发送，覆盖调度器外部唤醒。 */
static int testChannelAwaitThreadSend(ptr pData)
{
	testchannelawait* pContext = (testchannelawait*)pData;

	#if defined(_WIN32) || defined(_WIN64)
		Sleep(10);
	#else
		struct timespec tTime = { 0, 10000000 };

		(void)nanosleep(&tTime, NULL);
	#endif
	return xrtChannelTrySend(
		pContext->Channel,
		pContext->Input
	) == XCHANNEL_OK ? 0 : 1;
}



/* 在调度协程中执行一次发送或接收等待。 */
static ptr testChannelAwaitProc(ptr pData)
{
	testchannelawait* pContext = (testchannelawait*)pData;

	if ( pContext->Send ) {
		pContext->Result = pContext->Timeout == UINT64_MAX ?
			xrtChannelSendAwait(
				pContext->Channel,
				pContext->Input
			) :
			xrtChannelSendAwaitFor(
				pContext->Channel,
				pContext->Input,
				pContext->Timeout
			);
	} else {
		pContext->Result = pContext->Timeout == UINT64_MAX ?
			xrtChannelRecvAwait(
				pContext->Channel,
				&pContext->Output
			) :
			xrtChannelRecvAwaitFor(
				pContext->Channel,
				&pContext->Output,
				pContext->Timeout
			);
	}
	if ( pContext->CheckNextPark ) {
		testRequire(
			xrtCoParkFor(UINT64_C(1000)) == XWAIT_TIMEOUT,
			"channel await leaked a wake into the next park"
		);
	}
	return pContext;
}



/* 在调度协程中等待多个 Channel case。 */
static ptr testChannelSelectAwaitProc(ptr pData)
{
	testchannelselectawait* pContext =
		(testchannelselectawait*)pData;

	pContext->Result = xrtChannelSelectAwait(
		pContext->Cases,
		pContext->Count
	);
	return pContext;
}



/* 让出一次后取消已经挂入 Channel 的目标协程。 */
static ptr testChannelAwaitCancelProc(ptr pData)
{
	testchannelawaitcancel* pContext =
		(testchannelawaitcancel*)pData;

	testRequire(
		xrtCoSleep(0) == XWAIT_OK,
		"channel await cancel helper yield failed"
	);
	testRequire(
		xrtCoCancel(pContext->Target),
		"channel await cancel request failed"
	);
	return pContext;
}



/* 验证资源代际通知不会泄漏，也不会吞掉独立的通用唤醒。 */
static ptr testCoWaitTokenProc(ptr pData)
{
	testcowaittoken* pContext = (testcowaittoken*)pData;
	xrt_co_wait tWait;
	xcoro* pCurrent = xrtCoCurrent();

	testRequire(
		__xrtCoWaitOpen(pCurrent, &tWait),
		"resource wait token open failed"
	);
	__xrtCoWaitWake(&tWait);
	pContext->Early = __xrtCoWaitParkUntil(
		&tWait,
		XRT_DEADLINE_NEVER
	);
	__xrtCoWaitClose(&tWait);

	testRequire(
		__xrtCoWaitOpen(pCurrent, &tWait),
		"resource wait stale token open failed"
	);
	__xrtCoWaitWake(&tWait);
	__xrtCoWaitClose(&tWait);
	pContext->AfterClose = xrtCoParkFor(UINT64_C(1000));

	testRequire(
		__xrtCoWaitOpen(pCurrent, &tWait),
		"resource wait generic wake open failed"
	);
	testRequire(
		xrtCoWake(pCurrent),
		"resource wait generic wake failed"
	);
	__xrtCoWaitClose(&tWait);
	pContext->Generic = xrtCoParkFor(UINT64_C(1000));
	pContext->Final = xrtCoParkFor(UINT64_C(1000));
	return pContext;
}



/* 验证有缓冲快速路径、关闭和非协程调用边界。 */
static void testChannelAwaitBasic(xcosched* pSched)
{
	testchannelawait tContext;
	xchannel tChannel;
	xcoro* pCo;
	ptr pOutside = (ptr)(uintptr_t)91u;

	testRequire(
		xrtChannelInit(&tChannel, 1u),
		"channel await basic init failed"
	);
	testRequire(
		xrtChannelTrySend(
			&tChannel,
			(ptr)(uintptr_t)17u
		) == XCHANNEL_OK,
		"channel await basic setup failed"
	);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Channel = &tChannel;
	tContext.Timeout = UINT64_MAX;
	tContext.CheckNextPark = true;
	pCo = xrtCoSpawn(pSched, testChannelAwaitProc, &tContext, NULL);
	testRequire(pCo != NULL, "channel await basic spawn failed");
	testRequire(xrtCoSchedRun(pSched), "channel await basic run failed");
	testRequire(
		(tContext.Result == XWAIT_OK) &&
		((uintptr_t)tContext.Output == 17u),
		"channel await basic receive mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "channel await basic destroy failed");

	xrtChannelClose(&tChannel);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Channel = &tChannel;
	tContext.Timeout = UINT64_MAX;
	pCo = xrtCoSpawn(pSched, testChannelAwaitProc, &tContext, NULL);
	testRequire(pCo != NULL, "closed channel await spawn failed");
	testRequire(xrtCoSchedRun(pSched), "closed channel await run failed");
	testRequire(
		(tContext.Result == XWAIT_CLOSED) &&
		(tContext.Output == NULL),
		"closed channel await result mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "closed channel await destroy failed");

	xrtClearError();
	testRequire(
		xrtChannelRecvAwaitFor(
			&tChannel,
			&pOutside,
			0
		) == XWAIT_ERROR,
		"channel await succeeded outside a coroutine"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		((uintptr_t)pOutside == 91u),
		"outside-coroutine channel await error mismatch"
	);
	testRequire(xrtChannelUnit(&tChannel), "channel await basic unit failed");
}



/* 验证无缓冲发送和接收协程直接配对。 */
static void testChannelAwaitRendezvous(xcosched* pSched)
{
	testchannelawait tRecv;
	testchannelawait tSend;
	xchannel tChannel;
	xcoro* pRecv;
	xcoro* pSend;

	testRequire(
		xrtChannelInit(&tChannel, 0),
		"channel await rendezvous init failed"
	);
	memset(&tRecv, 0, sizeof(tRecv));
	memset(&tSend, 0, sizeof(tSend));
	tRecv.Channel = &tChannel;
	tRecv.Timeout = UINT64_MAX;
	tSend.Channel = &tChannel;
	tSend.Input = (ptr)(uintptr_t)33u;
	tSend.Timeout = UINT64_MAX;
	tSend.Send = true;
	pRecv = xrtCoSpawn(pSched, testChannelAwaitProc, &tRecv, NULL);
	pSend = xrtCoSpawn(pSched, testChannelAwaitProc, &tSend, NULL);
	testRequire(
		(pRecv != NULL) && (pSend != NULL),
		"channel await rendezvous spawn failed"
	);
	testRequire(
		xrtCoSchedRun(pSched),
		"channel await rendezvous run failed"
	);
	testRequire(
		(tRecv.Result == XWAIT_OK) &&
		(tSend.Result == XWAIT_OK) &&
		((uintptr_t)tRecv.Output == 33u),
		"channel await rendezvous result mismatch"
	);
	testRequire(xrtCoDestroy(pRecv), "rendezvous receiver destroy failed");
	testRequire(xrtCoDestroy(pSend), "rendezvous sender destroy failed");
	testRequire(
		xrtChannelUnit(&tChannel),
		"channel await rendezvous unit failed"
	);
}



/* 验证多路选择只提交被发送的 case。 */
static void testChannelAwaitSelect(xcosched* pSched)
{
	testchannelselectawait tSelect;
	testchannelawait tSend;
	xchannel tFirst;
	xchannel tSecond;
	xcoro* pSelect;
	xcoro* pSend;
	ptr pFirst = (ptr)(uintptr_t)1u;
	ptr pSecond = (ptr)(uintptr_t)2u;

	testRequire(xrtChannelInit(&tFirst, 0), "await select first init failed");
	testRequire(xrtChannelInit(&tSecond, 0), "await select second init failed");
	memset(&tSelect, 0, sizeof(tSelect));
	memset(&tSend, 0, sizeof(tSend));
	tSelect.Cases[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
	tSelect.Cases[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
	tSelect.Count = 2u;
	tSend.Channel = &tSecond;
	tSend.Input = (ptr)(uintptr_t)72u;
	tSend.Timeout = UINT64_MAX;
	tSend.Send = true;
	pSelect = xrtCoSpawn(
		pSched,
		testChannelSelectAwaitProc,
		&tSelect,
		NULL
	);
	pSend = xrtCoSpawn(pSched, testChannelAwaitProc, &tSend, NULL);
	testRequire(
		(pSelect != NULL) && (pSend != NULL),
		"channel select await spawn failed"
	);
	testRequire(xrtCoSchedRun(pSched), "channel select await run failed");
	testRequire(
		(tSelect.Result.Wait == XWAIT_OK) &&
		(tSelect.Result.Index == 1u) &&
		(tSelect.Result.Result == XCHANNEL_OK) &&
		((uintptr_t)pFirst == 1u) &&
		((uintptr_t)pSecond == 72u) &&
		(tSend.Result == XWAIT_OK),
		"channel select await result mismatch"
	);
	testRequire(xrtCoDestroy(pSelect), "select await receiver destroy failed");
	testRequire(xrtCoDestroy(pSend), "select await sender destroy failed");
	testRequire(xrtChannelUnit(&tFirst), "await select first unit failed");
	testRequire(xrtChannelUnit(&tSecond), "await select second unit failed");
}



/* 验证跨线程唤醒和活动等待生命周期保护。 */
static void testChannelAwaitThread(xcosched* pSched)
{
	testchannelawait tContext;
	testthread tThread;
	xrt_channel_impl* pImpl;
	xchannel tChannel;
	xcoro* pCo;

	testRequire(
		xrtChannelInit(&tChannel, 1u),
		"channel await thread init failed"
	);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Channel = &tChannel;
	tContext.Input = (ptr)(uintptr_t)88u;
	tContext.Timeout = UINT64_MAX;
	pCo = xrtCoSpawn(pSched, testChannelAwaitProc, &tContext, NULL);
	testRequire(pCo != NULL, "channel await thread spawn failed");
	(void)xrtCoSchedStep(pSched);

	pImpl = (xrt_channel_impl*)&tChannel;
	testRequire(
		pImpl->SelectWaiters != NULL,
		"channel await did not register before parking"
	);
	xrtClearError();
	testRequire(
		!xrtChannelUnit(&tChannel),
		"channel unit accepted active coroutine waiter"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"active coroutine waiter unit error mismatch"
	);

	tThread.Proc = testChannelAwaitThreadSend;
	tThread.Data = &tContext;
	testThreadsStart(&tThread, 1u);
	testRequire(xrtCoSchedRun(pSched), "cross-thread channel await run failed");
	testThreadsJoin(&tThread, 1u);
	testRequire(
		(tThread.Result == 0) &&
		(tContext.Result == XWAIT_OK) &&
		((uintptr_t)tContext.Output == 88u),
		"cross-thread channel await result mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "cross-thread await destroy failed");
	testRequire(xrtChannelUnit(&tChannel), "channel await thread unit failed");
}



/* 验证真实超时与协程取消都清理注册节点。 */
static void testChannelAwaitStop(xcosched* pSched)
{
	testchannelawait tTimeout;
	testchannelawait tCancel;
	testchannelawaitcancel tHelper;
	xrt_channel_impl* pImpl;
	xchannel tChannel;
	xcoro* pTimeout;
	xcoro* pCancel;
	uint64 iStarted;
	uint64 iElapsed;

	testRequire(
		xrtChannelInit(&tChannel, 1u),
		"channel await stop init failed"
	);
	memset(&tTimeout, 0, sizeof(tTimeout));
	tTimeout.Channel = &tChannel;
	tTimeout.Timeout = UINT64_C(20000);
	iStarted = xrtClock();
	pTimeout = xrtCoSpawn(
		pSched,
		testChannelAwaitProc,
		&tTimeout,
		NULL
	);
	testRequire(pTimeout != NULL, "channel await timeout spawn failed");
	testRequire(xrtCoSchedRun(pSched), "channel await timeout run failed");
	iElapsed = xrtClock() - iStarted;
	testRequire(
		(tTimeout.Result == XWAIT_TIMEOUT) &&
		(iElapsed >= UINT64_C(10000)) &&
		(iElapsed < UINT64_C(2000000)),
		"channel await timeout mismatch"
	);
	testRequire(xrtCoDestroy(pTimeout), "channel await timeout destroy failed");

	memset(&tCancel, 0, sizeof(tCancel));
	memset(&tHelper, 0, sizeof(tHelper));
	tCancel.Channel = &tChannel;
	tCancel.Timeout = UINT64_MAX;
	pCancel = xrtCoSpawn(pSched, testChannelAwaitProc, &tCancel, NULL);
	testRequire(pCancel != NULL, "channel await cancel spawn failed");
	tHelper.Target = pCancel;
	testRequire(
		xrtCoGo(
			pSched,
			testChannelAwaitCancelProc,
			&tHelper,
			NULL
		),
		"channel await cancel helper spawn failed"
	);
	testRequire(xrtCoSchedRun(pSched), "channel await cancel run failed");
	testRequire(
		tCancel.Result == XWAIT_CANCELLED,
		"channel await cancellation mismatch"
	);
	testRequire(xrtCoDestroy(pCancel), "channel await cancel destroy failed");

	pImpl = (xrt_channel_impl*)&tChannel;
	testRequire(
		pImpl->SelectWaiters == NULL,
		"channel await stop leaked registration"
	);
	testRequire(xrtChannelUnit(&tChannel), "channel await stop unit failed");
}



/* 验证资源等待令牌严格隔离本代际通知与通用唤醒。 */
static void testCoWaitToken(xcosched* pSched)
{
	testcowaittoken tContext;
	xcoro* pCo;

	memset(&tContext, 0, sizeof(tContext));
	pCo = xrtCoSpawn(pSched, testCoWaitTokenProc, &tContext, NULL);
	testRequire(pCo != NULL, "resource wait token spawn failed");
	testRequire(xrtCoSchedRun(pSched), "resource wait token run failed");
	testRequire(
		(tContext.Early == XWAIT_OK) &&
		(tContext.AfterClose == XWAIT_TIMEOUT) &&
		(tContext.Generic == XWAIT_OK) &&
		(tContext.Final == XWAIT_TIMEOUT),
		"resource wait token isolation mismatch"
	);
	testRequire(xrtCoDestroy(pCo), "resource wait token destroy failed");
}



/* 执行 Channel 协程适配的完整合同测试。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();

	testRequire(pSched != NULL, "channel await scheduler create failed");
	testChannelAwaitBasic(pSched);
	testChannelAwaitRendezvous(pSched);
	testChannelAwaitSelect(pSched);
	testChannelAwaitThread(pSched);
	testChannelAwaitStop(pSched);
	testCoWaitToken(pSched);
	testRequire(xrtCoSchedDestroy(pSched), "channel await scheduler destroy failed");
	testRequire(xrtCoThreadDetach(), "channel await runtime detach failed");
	printf("[PASS] channel coroutine\n");
	return 0;
}
