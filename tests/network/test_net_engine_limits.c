#include <xrt/detail/wait.h>
#include "../test.h"



typedef struct testenginelimits {
	xatomic32 Started;
	xatomic32 Release;
	xatomic32 Hits;
	xatomic32 Cancelled;
	xatomic32 Closed;
} testenginelimits;



/* 等待测试状态到达目标，避免依赖平台休眠精度。 */
static void testEngineLimitsWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	double iDeadline = __xrtWaitAfter(3000);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!__xrtWaitExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 占住唯一 Worker，让主线程稳定填满有界命令队列。 */
static void testEngineLimitsBlock(xnetworker* pWorker, ptr pData)
{
	testenginelimits* pContext = (testenginelimits*)pData;
	uint32 iGeneration = xrtAtomic32FetchAdd(
		&pContext->Started,
		1,
		XMEMORY_ACQ_REL
	) + 1u;

	(void)pWorker;
	while ( xrtAtomic32Load(&pContext->Release, XMEMORY_ACQUIRE) <
		iGeneration ) {
		xrtThreadYield();
	}
}



/* 记录已经排空执行的普通任务。 */
static void testEngineLimitsHit(xnetworker* pWorker, ptr pData)
{
	testenginelimits* pContext = (testenginelimits*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(&pContext->Hits, 1, XMEMORY_RELEASE);
}



/* 记录受硬上限保护的 Timer 终态。 */
static void testEngineLimitsTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testenginelimits* pContext = (testenginelimits*)pData;

	(void)pWorker;
	testRequire(Id != 0, "limited timer returned zero identity");
	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Cancelled,
			1,
			XMEMORY_RELEASE
		);
	} else if ( Result == XNET_RESULT_CLOSED ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Closed,
			1,
			XMEMORY_RELEASE
		);
	} else {
		testRequire(false, "limited timer returned unexpected result");
	}
}



/* 命令队列背压期间重试异步取消，直到目标 Worker 恢复消费。 */
static void testEngineLimitsCancel(xnetengine* pEngine, uint64 Id)
{
	double iDeadline = __xrtWaitAfter(3000);

	for ( ;; ) {
		if ( xrtNetEngineTimerCancel(pEngine, Id) ) {
			return;
		}
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_AGAIN),
			"limited timer cancel returned an unexpected error");
		xrtClearError();
		testRequire(!__xrtWaitExpired(iDeadline),
			"limited timer cancel remained backpressured");
		xrtThreadYield();
	}
}



typedef struct testengineticker {
	xatomic32 Ticks, Cancelled, Closed;
	uint64 Id;
	bool StopOnThirdTick;
} testengineticker;

static void testEngineTickerTick(xnetworker* pWorker, uint64 Id, xnetresult Result, ptr pData)
{
	testengineticker* pState = pData;
	xnetengine* pEngine = xrtNetWorkerEngine(pWorker);
	if (!pState->Id) pState->Id = Id;
	testRequire(Id && Id == pState->Id && xrtNetWorkerIsCurrent(pWorker),
		"ticker keeps its worker and ID");
	if (Result == XNET_RESULT_OK) {
		uint64 Extra = xrtNetEngineAfter(pEngine, 0, 5000, testEngineLimitsTimer, NULL);
		xerrkind Error = xrtErrorKind(xrtGetError());
		testRequire(!Extra && (Error == XERR_AGAIN || (!pState->StopOnThirdTick && Error == XERR_CLOSED)),
			"ticker retains its last timer credit inside a tick");
		xrtClearError();
		uint32 Ticks = xrtAtomic32FetchAdd(&pState->Ticks, 1, XMEMORY_ACQ_REL) + 1;
		if (pState->StopOnThirdTick && Ticks == 3) {
			testRequire(xrtNetEngineTimerCancelCurrent(pEngine, Id),
				"ticker can cancel on its own worker");
		}
	} else if (Result == XNET_RESULT_CANCELLED) {
		xrtAtomic32FetchAdd(&pState->Cancelled, 1, XMEMORY_RELEASE);
	} else {
		testRequire(Result == XNET_RESULT_CLOSED, "ticker ends with a terminal result");
		xrtAtomic32FetchAdd(&pState->Closed, 1, XMEMORY_RELEASE);
	}
}

static void testEngineTicker(void)
{
	xnetengineconfig Config;
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	Config.TimerLimit = 1;
	xnetengine* pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine && xrtNetEngineStart(pEngine), "start ticker engine");
	testengineticker Once = {.StopOnThirdTick = true}, UntilStop = {0};
	testRequire(!xrtNetEngineEvery(pEngine, 0, 0, testEngineTickerTick, &Once) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "ticker requires a positive period");
	xrtClearError();
	testRequire(!xrtNetEngineEvery(pEngine, 0, (int64)UINT32_MAX + 1, testEngineTickerTick, &Once) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "ticker rejects a period beyond its range");
	xrtClearError();
	uint64 Id = xrtNetEngineEvery(pEngine, 0, 5, testEngineTickerTick, &Once);
	testRequire(Id != 0, "start timer with stable credit");
	testEngineLimitsWait(&Once.Cancelled, 1, "wait for ticker self-cancellation");
	xnetenginestats Stats;
	testRequire(xrtNetEngineStats(pEngine, &Stats) && Stats.ActiveTimers == 0 &&
		Stats.TimersAccepted == 1 && Stats.TimersFired == 3 && Stats.TimersCancelled == 1 &&
		xrtAtomic32Load(&Once.Ticks, XMEMORY_ACQUIRE) == 3, "tick and terminal statistics are distinct");

	uint64 Next = xrtNetEngineEvery(pEngine, 0, 5, testEngineTickerTick, &UntilStop);
	testRequire(Next && Next != Id, "cancelled ticker releases its credit and ID");
	testEngineLimitsWait(&UntilStop.Ticks, 2, "ticker continues on the released credit");
	testRequire(xrtNetEngineStop(pEngine) &&
		xrtAtomic32Load(&UntilStop.Closed, XMEMORY_ACQUIRE) == 1 &&
		xrtAtomic32Load(&Once.Cancelled, XMEMORY_ACQUIRE) == 1 &&
		xrtAtomic32Load(&Once.Ticks, XMEMORY_ACQUIRE) == 3,
		"engine shutdown retires the remaining ticker once");
	testRequire(xrtNetEngineDestroy(pEngine), "destroy ticker engine");
}



/* 验证命令容量、Timer 上限和停止排空均为硬契约。 */
int main(void)
{
	xnetengineconfig Config;
	testenginelimits Context;
	xnetenginestats Stats;
	xnetengine* pEngine;
	uint64 iTimerOne;
	uint64 iTimerTwo;

	memset(&Context, 0, sizeof(Context));
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	Config.CommandCapacity = 2;
	Config.NodeCacheBytes = 256;
	Config.TimerLimit = 2;
	Config.EventBatch = 4;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine != NULL, "limited engine create failed");
	testRequire(xrtNetEngineStart(pEngine), "limited engine start failed");

	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineLimitsBlock,
		&Context
	), "limited blocker post failed");
	testEngineLimitsWait(&Context.Started, 1,
		"limited blocker did not start");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineLimitsHit,
		&Context
	), "limited queue first post failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineLimitsHit,
		&Context
	), "limited queue second post failed");
	testRequire(!xrtNetEnginePost(
		pEngine,
		0,
		testEngineLimitsHit,
		&Context
	), "full engine queue accepted a third post");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"full engine queue error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);
	testEngineLimitsWait(&Context.Hits, 2,
		"limited engine did not drain accepted posts");

	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineLimitsBlock,
		&Context
	), "timer blocker post failed");
	testEngineLimitsWait(&Context.Started, 2,
		"timer blocker did not start");
	iTimerOne = xrtNetEngineAfter(
		pEngine,
		0,
		5000,
		testEngineLimitsTimer,
		&Context
	);
	iTimerTwo = xrtNetEngineAfter(
		pEngine,
		0,
		5000,
		testEngineLimitsTimer,
		&Context
	);
	testRequire((iTimerOne != 0) && (iTimerTwo != 0) &&
		(iTimerOne != iTimerTwo), "limited timers were not accepted uniquely");
	testRequire(xrtNetEngineAfter(
		pEngine,
		0,
		5000,
		testEngineLimitsTimer,
		&Context
	) == 0, "timer hard limit accepted an extra timer");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_AGAIN,
		"timer hard limit error mismatch");
	xrtClearError();
	xrtAtomic32Store(&Context.Release, 2, XMEMORY_RELEASE);
	testEngineLimitsCancel(pEngine, iTimerOne);
	testEngineLimitsWait(&Context.Cancelled, 1,
		"limited timer was not cancelled");

	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"limited engine stats failed");
	testRequire((Stats.PostsRejected >= 1) &&
		(Stats.TimersRejected >= 1) && (Stats.ActiveTimers == 1),
		"limited engine stats did not expose pressure");
	testRequire(xrtNetEngineStop(pEngine), "limited engine stop failed");
	testRequire(xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 1,
		"limited engine did not close remaining timer");
	testRequire(xrtNetEngineStop(pEngine),
		"limited engine idempotent stop failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"limited engine destroy failed");

	Config.EventBatch = 0;
	testRequire(xrtNetEngineCreate(&Config) == NULL,
		"engine accepted zero event batch");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid engine config error mismatch");
	xrtClearError();

	#if SIZE_MAX <= UINT32_MAX
		Config.EventBatch = 4;
		Config.TimerLimit =
			(SIZE_MAX / sizeof(void*)) + 1u;
		testRequire(xrtNetEngineCreate(&Config) == NULL,
			"engine accepted an overflowing timer heap limit");
		testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
			"timer heap overflow error mismatch");
		xrtClearError();
	#endif
	testEngineTicker();
	printf("[PASS] network engine limits and ticker lifetimes\n");
	return 0;
}
