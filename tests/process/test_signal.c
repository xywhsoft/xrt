#include "../test.h"



typedef struct testsignalstate {
	xatomic32 Calls;
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Freed;
	uint64 CallbackThread;
	xsignalevent Event;
} testsignalstate;



/* 在有界时间内等待原子计数达到目标。 */
static bool testSignalWait(const xatomic32* pValue, uint32 iExpected)
{
	uint64 iDeadline = xrtClock() + UINT64_C(3000000);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		if ( xrtClock() >= iDeadline ) {
			return false;
		}
		xrtSleep(1u);
	}
	return true;
}



/* 普通回调保存事件后再发布调用次数。 */
static void testSignalCount(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	testsignalstate* pState = (testsignalstate*)pData;

	(void)pWatch;
	pState->CallbackThread = xrtThreadCurrentId();
	pState->Event = *pEvent;
	(void)xrtAtomic32FetchAdd(&pState->Calls, 1u, XMEMORY_RELEASE);
}



/* 阻塞回调用来验证其他线程注销时必须等待回调结束。 */
static void testSignalBlocking(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	testsignalstate* pState = (testsignalstate*)pData;

	(void)pWatch;
	(void)pEvent;
	xrtAtomic32Store(&pState->Entered, 1u, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pState->Release, XMEMORY_ACQUIRE) == 0u ) {
		xrtThreadYield();
	}
}



/* 回调允许注销自身，但必须拒绝等待自身完成的全局关闭。 */
static void testSignalSelfControl(
	xsignalwatch* pWatch,
	const xsignalevent* pEvent,
	ptr pData
)
{
	testsignalstate* pState = (testsignalstate*)pData;
	bool bOff;
	bool bShutdown;

	(void)pEvent;
	bOff = xrtSignalOff(pWatch);
	bShutdown = xrtSignalShutdown();
	if ( bOff ) {
		xrtAtomic32Store(&pState->Entered, 1u, XMEMORY_RELEASE);
	}
	if ( !bShutdown && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) ) {
		xrtAtomic32Store(&pState->Release, 1u, XMEMORY_RELEASE);
	}
	(void)xrtAtomic32FetchAdd(&pState->Calls, 1u, XMEMORY_RELEASE);
}



/* Owned 数据析构器只记录恰好一次的释放。 */
static void testSignalFreeData(ptr pData)
{
	testsignalstate* pState = (testsignalstate*)pData;

	(void)xrtAtomic32FetchAdd(&pState->Freed, 1u, XMEMORY_RELEASE);
}



typedef struct testsignaloffstate {
	xsignalwatch* Watch;
	xatomic32 Returned;
	bool Result;
} testsignaloffstate;



/* 从非调度线程注销监听。 */
static int32 testSignalOffThread(ptr pData)
{
	testsignaloffstate* pState = (testsignaloffstate*)pData;

	pState->Result = xrtSignalOff(pState->Watch);
	xrtAtomic32Store(&pState->Returned, 1u, XMEMORY_RELEASE);
	return 0;
}



/* 初始化不含动态资源的测试状态。 */
static void testSignalStateInit(testsignalstate* pState)
{
	memset(pState, 0, sizeof(*pState));
	xrtAtomic32Init(&pState->Calls, 0u);
	xrtAtomic32Init(&pState->Entered, 0u);
	xrtAtomic32Init(&pState->Release, 0u);
	xrtAtomic32Init(&pState->Freed, 0u);
}



/* 验证信号表、真实投递、生命周期、并发注销和无上限监听。 */
int main(void)
{
	testsignalstate State;
	testsignalstate OnceState;
	testsignalstate BlockState;
	testsignaloffstate OffState;
	xsignalwatch* pWatch;
	xsignalwatch* pReference;
	xsignalwatch* pOnce;
	xthread* pOffThread;
	xsignalwatch* arrWatches[160];
	uint64 iMainThread = xrtThreadCurrentId();

	testRequire(xrtSignalSupported(XSIGNAL_INT),
		"interrupt signal was not supported");
	testRequire(xrtSignalHealthy(), "new signal dispatcher state was unhealthy");
	testRequire(strcmp(xrtSignalName(XSIGNAL_INT), "INT") == 0,
		"interrupt signal name mismatch");
	testRequire(strcmp(xrtSignalName((xsignal)9999), "UNKNOWN") == 0,
		"unknown signal name mismatch");
	testRequire(!xrtSignalSupported((xsignal)9999),
		"unknown signal was reported supported");
	testRequire(xrtSignalOn(XSIGNAL_INT, NULL, NULL) == NULL,
		"null signal callback was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null callback error mismatch");
	xrtClearError();

	testSignalStateInit(&State);
	testSignalStateInit(&OnceState);
	testRequire(xrtSignalClear(XSIGNAL_INT), "signal clear failed");
	pWatch = xrtSignalOn(XSIGNAL_INT, testSignalCount, &State);
	pOnce = xrtSignalOnce(XSIGNAL_INT, testSignalCount, &OnceState);
	testRequire((pWatch != NULL) && (pOnce != NULL),
		"signal watch creation failed");
	testRequire(xrtSignalActive(pWatch) && xrtSignalActive(pOnce),
		"new signal watch was inactive");
	testRequire(xrtSignalCode(pOnce) == XSIGNAL_INT,
		"signal watch code mismatch");
	testRequire(xrtSignalRaise(XSIGNAL_INT), "first signal raise failed");
	testRequire(testSignalWait(&State.Calls, 1u),
		"persistent signal callback timed out");
	testRequire(testSignalWait(&OnceState.Calls, 1u),
		"once signal callback timed out");
	testRequire(!xrtSignalActive(pOnce),
		"once signal watch remained active");
	testRequire(State.CallbackThread != iMainThread,
		"signal callback ran in the raising thread");
	testRequire((State.Event.Code == XSIGNAL_INT) &&
		(State.Event.Count >= 1u) &&
		(State.Event.Total >= State.Event.Count) &&
		(strcmp(State.Event.Name, "INT") == 0),
		"signal event snapshot mismatch");
	testRequire(xrtSignalRaise(XSIGNAL_INT), "second signal raise failed");
	testRequire(testSignalWait(&State.Calls, 2u),
		"second persistent callback timed out");
	xrtSleep(20u);
	testRequire(xrtAtomic32Load(&OnceState.Calls, XMEMORY_ACQUIRE) == 1u,
		"once callback ran more than once");
	testRequire(xrtSignalReceived(XSIGNAL_INT) &&
		(xrtSignalCount(XSIGNAL_INT) >= 2u),
		"signal count did not advance");
	xrtSignalFree(pOnce);
	xrtSignalFree(pWatch);

	testSignalStateInit(&State);
	pWatch = xrtSignalOn(XSIGNAL_TERM, testSignalCount, &State);
	testRequire(pWatch != NULL, "terminate signal watch creation failed");
	testRequire(xrtSignalRaise(XSIGNAL_TERM),
		"first terminate signal raise failed");
	testRequire(testSignalWait(&State.Calls, 1u),
		"first terminate signal callback timed out");
	testRequire(xrtSignalRaise(XSIGNAL_TERM),
		"second terminate signal raise failed");
	testRequire(testSignalWait(&State.Calls, 2u),
		"second terminate signal callback timed out");
	xrtSignalFree(pWatch);

	testRequire(xrtSignalClear(XSIGNAL_INT), "second signal clear failed");
	testRequire(xrtSignalCount(XSIGNAL_INT) == 0u,
		"signal count did not clear");
	testRequire(xrtSignalIgnore(XSIGNAL_INT), "signal ignore failed");
	testRequire(xrtSignalRaise(XSIGNAL_INT), "ignored signal raise failed");
	xrtSleep(20u);
	testRequire(xrtSignalCount(XSIGNAL_INT) == 0u,
		"ignored signal entered the XRT counter");
	testRequire(xrtSignalRestore(XSIGNAL_INT), "signal restore failed");

	testSignalStateInit(&State);
	for ( size_t i = 0; i < 160u; i++ ) {
		arrWatches[i] = xrtSignalOn(XSIGNAL_INT, testSignalCount, &State);
		testRequire(arrWatches[i] != NULL,
			"large signal watch set creation failed");
	}
	testRequire(xrtSignalRaise(XSIGNAL_INT),
		"large signal watch set raise failed");
	testRequire(testSignalWait(&State.Calls, 160u),
		"large signal watch set was truncated");
	for ( size_t i = 0; i < 160u; i++ ) {
		xrtSignalFree(arrWatches[i]);
	}

	testSignalStateInit(&State);
	pWatch = xrtSignalOnOwned(
		XSIGNAL_INT,
		testSignalCount,
		&State,
		testSignalFreeData
	);
	testRequire(pWatch != NULL, "owned signal watch creation failed");
	pReference = xrtSignalRef(pWatch);
	testRequire(pReference == pWatch, "signal watch reference failed");
	xrtSignalFree(pWatch);
	testRequire(xrtAtomic32Load(&State.Freed, XMEMORY_ACQUIRE) == 0u,
		"owned data was released before the last reference");
	xrtSignalFree(pReference);
	testRequire(xrtAtomic32Load(&State.Freed, XMEMORY_ACQUIRE) == 1u,
		"owned signal data was not released exactly once");

	testSignalStateInit(&State);
	pWatch = xrtSignalOn(XSIGNAL_INT, testSignalSelfControl, &State);
	testRequire(pWatch != NULL, "self-control signal watch creation failed");
	testRequire(xrtSignalRaise(XSIGNAL_INT),
		"self-control signal raise failed");
	testRequire(testSignalWait(&State.Calls, 1u),
		"self-control signal callback timed out");
	testRequire((xrtAtomic32Load(&State.Entered, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&State.Release, XMEMORY_ACQUIRE) == 1u),
		"self-control signal contract mismatch");
	testRequire(!xrtSignalActive(pWatch),
		"self-unwatched signal remained active");
	xrtSignalFree(pWatch);

	testSignalStateInit(&BlockState);
	memset(&OffState, 0, sizeof(OffState));
	xrtAtomic32Init(&OffState.Returned, 0u);
	pWatch = xrtSignalOn(XSIGNAL_INT, testSignalBlocking, &BlockState);
	testRequire(pWatch != NULL, "blocking signal watch creation failed");
	testRequire(xrtSignalRaise(XSIGNAL_INT),
		"blocking signal raise failed");
	testRequire(testSignalWait(&BlockState.Entered, 1u),
		"blocking callback did not start");
	OffState.Watch = pWatch;
	pOffThread = xrtThreadCreate(testSignalOffThread, &OffState, 0u);
	testRequire(pOffThread != NULL, "signal off thread creation failed");
	xrtSleep(20u);
	testRequire(xrtAtomic32Load(&OffState.Returned, XMEMORY_ACQUIRE) == 0u,
		"signal off returned while callback was active");
	xrtAtomic32Store(&BlockState.Release, 1u, XMEMORY_RELEASE);
	testRequire(xrtThreadWait(pOffThread) == XWAIT_OK,
		"signal off thread wait failed");
	testRequire(OffState.Result, "signal off thread failed");
	xrtThreadDestroy(pOffThread);
	xrtSignalFree(pWatch);

	testRequire(xrtSignalShutdown(), "signal shutdown failed");
	testSignalStateInit(&State);
	pWatch = xrtSignalOn(XSIGNAL_INT, testSignalCount, &State);
	testRequire(pWatch != NULL, "signal restart watch creation failed");
	testRequire(xrtSignalRaise(XSIGNAL_INT), "signal restart raise failed");
	testRequire(testSignalWait(&State.Calls, 1u),
		"signal restart callback timed out");
	xrtSignalFree(pWatch);
	testRequire(xrtSignalShutdown(), "second signal shutdown failed");
	testRequire(xrtSignalHealthy(), "shutdown signal state was unhealthy");
	testMemoryDebugDrain("signal test leaked memory");

	printf("[PASS] signal\n");
	return 0;
}
