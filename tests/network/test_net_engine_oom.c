#include "../test.h"



#define TEST_ENGINE_OOM_TIMER_COUNT 256u



typedef struct testengineoom {
	xatomic32 Fail;
	xatomic64 Attempts;
	xatomic32 Barrier;
	xatomic32 Started;
	xatomic32 Release;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Unexpected;
	xatomic32 CacheReady;
	xatomic32 CacheCancelled;
	xatomic32 CacheUnexpected;
	xatomic64 CacheTimer;
} testengineoom;



/* 允许阶段转发系统分配，故障阶段拒绝全部底层新内存。 */
static ptr testEngineOomAlloc(ptr pData, size_t iSize)
{
	testengineoom* pContext = (testengineoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : malloc(iSize);
}



/* 故障阶段拒绝扩容，正常阶段保持标准 realloc 语义。 */
static ptr testEngineOomRealloc(ptr pData, ptr pMemory, size_t iSize)
{
	testengineoom* pContext = (testengineoom*)pData;

	(void)xrtAtomic64FetchAdd(&pContext->Attempts, 1, XMEMORY_RELAXED);
	return xrtAtomic32Load(&pContext->Fail, XMEMORY_ACQUIRE) != 0 ?
		NULL : realloc(pMemory, iSize);
}



/* 释放正常阶段已经取得的底层内存。 */
static void testEngineOomFree(ptr pData, ptr pMemory)
{
	(void)pData;
	free(pMemory);
}



/* 在截止时间前等待跨线程测试状态到达目标。 */
static void testEngineOomWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 排在 Timer 命令之后的屏障用于确认前 256 个 Timer 已进入最小堆。 */
static void testEngineOomBarrier(xnetworker* pWorker, ptr pData)
{
	testengineoom* pContext = (testengineoom*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(&pContext->Barrier, 1, XMEMORY_RELEASE);
}



/* 占住唯一 Worker，使第 257 个 Timer 在受理后才进入故障扩容路径。 */
static void testEngineOomBlock(xnetworker* pWorker, ptr pData)
{
	testengineoom* pContext = (testengineoom*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(&pContext->Started, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(&pContext->Release, XMEMORY_ACQUIRE) == 0 ) {
		xrtThreadYield();
	}
}



/* 每个受理的 Timer 必须以错误或关闭终态恰好结束一次。 */
static void testEngineOomTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testengineoom* pContext = (testengineoom*)pData;

	(void)pWorker;
	testRequire(Id != 0, "engine OOM timer returned zero identity");
	if ( Result == XNET_RESULT_ERROR ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Errors,
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
		(void)xrtAtomic32FetchAdd(
			&pContext->Unexpected,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 验证到期节点在回调前已回收，周期回调可在 OOM 下直接复用。 */
static void testEngineOomNodeCache(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testengineoom* pContext = (testengineoom*)pData;

	testRequire(Id != 0, "cached engine timer returned zero identity");
	if ( Result == XNET_RESULT_OK ) {
		uint64 iBefore = xrtAtomic64Load(
			&pContext->Attempts,
			XMEMORY_ACQUIRE
		);
		uint64 iAfter;
		uint64 iNext;

		xrtAtomic32Store(&pContext->Fail, 1, XMEMORY_RELEASE);
		iNext = xrtNetEngineAfter(
			xrtNetWorkerEngine(pWorker),
			xrtNetWorkerIndex(pWorker),
			60000000u,
			testEngineOomNodeCache,
			pContext
		);
		iAfter = xrtAtomic64Load(
			&pContext->Attempts,
			XMEMORY_ACQUIRE
		);
		xrtAtomic32Store(&pContext->Fail, 0, XMEMORY_RELEASE);
		if ( (iNext != 0) && (iAfter == iBefore) ) {
			xrtAtomic64Store(
				&pContext->CacheTimer,
				iNext,
				XMEMORY_RELEASE
			);
		} else {
			(void)xrtAtomic32FetchAdd(
				&pContext->CacheUnexpected,
				1,
				XMEMORY_RELEASE
			);
		}
		(void)xrtAtomic32FetchAdd(
			&pContext->CacheReady,
			1,
			XMEMORY_RELEASE
		);
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->CacheCancelled,
			1,
			XMEMORY_RELEASE
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pContext->CacheUnexpected,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 验证创建、启动和受理后 Timer 扩容的 OOM 回滚与唯一终态。 */
int main(void)
{
	testengineoom Context;
	xallocator Allocator;
	xnetengineconfig Config;
	xnetenginestats Stats;
	xnetengine* pEngine;
	xdeadline iTimerDeadline;
	uint64 iFailedTimer;
	uint64 iCachedTimer;
	uint64 iUncachedTimer;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Fail, 0);
	xrtAtomic64Init(&Context.Attempts, 0);
	xrtAtomic32Init(&Context.Barrier, 0);
	xrtAtomic32Init(&Context.Started, 0);
	xrtAtomic32Init(&Context.Release, 0);
	xrtAtomic32Init(&Context.Errors, 0);
	xrtAtomic32Init(&Context.Closed, 0);
	xrtAtomic32Init(&Context.Unexpected, 0);
	xrtAtomic32Init(&Context.CacheReady, 0);
	xrtAtomic32Init(&Context.CacheCancelled, 0);
	xrtAtomic32Init(&Context.CacheUnexpected, 0);
	xrtAtomic64Init(&Context.CacheTimer, 0);
	Allocator.Context = &Context;
	Allocator.Alloc = testEngineOomAlloc;
	Allocator.Realloc = testEngineOomRealloc;
	Allocator.Free = testEngineOomFree;
	testRequire(xrtSetAllocator(&Allocator),
		"engine OOM allocator install failed");
	xrtNetEngineConfigInit(&Config);
	testRequire(Config.NodeCacheBytes != 0,
		"engine default node cache is disabled");
	Config.Workers = 1;
	Config.CommandCapacity = 1024;
	Config.NodeCacheBytes = 64;
	Config.TimerLimit = TEST_ENGINE_OOM_TIMER_COUNT + 1u;
	Config.EventBatch = 4;

	/* 首次分配失败不得留下半构造 Engine。 */
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(xrtNetEngineCreate(&Config) == NULL,
		"engine create survived initial OOM");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"engine create OOM error mismatch");
	xrtClearError();

	/* Start 失败后必须回到可再次启动的停止状态。 */
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine != NULL, "engine create after OOM failed");
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	testRequire(!xrtNetEngineStart(pEngine),
		"engine start survived OOM");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_STOPPED,
		"engine start OOM did not roll back state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"engine start OOM error mismatch");
	xrtClearError();

	/* 先把最小堆稳定扩展到 256 项。 */
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	testRequire(xrtNetEngineStart(pEngine),
		"engine start after OOM failed");

	/* 回调内关闭分配器，重排只能复用刚刚终结的 Timer 节点。 */
	testRequire(xrtNetEngineAfter(
		pEngine,
		0,
		1000u,
		testEngineOomNodeCache,
		&Context
	) != 0, "engine timer cache warmup failed");
	testEngineOomWait(&Context.CacheReady, 1,
		"engine timer cache callback did not run");
	testRequire(xrtAtomic32Load(
		&Context.CacheUnexpected,
		XMEMORY_ACQUIRE
	) == 0, "engine timer callback could not reuse cached node");
	iCachedTimer = xrtAtomic64Load(
		&Context.CacheTimer,
		XMEMORY_ACQUIRE
	);
	testRequire(iCachedTimer != 0,
		"engine timer cache did not preserve replacement identity");
	testRequire(xrtNetEngineStats(pEngine, &Stats) &&
		(Stats.NodeCacheHits != 0) &&
		(Stats.NodeCachedBytes <= Config.NodeCacheBytes),
		"engine node cache statistics did not record timer reuse");
	testRequire(xrtNetEngineTimerCancel(pEngine, iCachedTimer),
		"engine cached timer cancel failed");
	testEngineOomWait(&Context.CacheCancelled, 1,
		"engine cached timer was not cancelled");

	iTimerDeadline = xrtDeadlineAfter(60000000u);
	for ( uint32 i = 0; i < TEST_ENGINE_OOM_TIMER_COUNT; i++ ) {
		testRequire(xrtNetEngineSchedule(
			pEngine,
			0,
			iTimerDeadline,
			testEngineOomTimer,
			&Context
		) != 0, "engine OOM prefill timer failed");
	}
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineOomBarrier,
		&Context
	), "engine OOM barrier post failed");
	testEngineOomWait(&Context.Barrier, 1,
		"engine OOM timer prefill did not drain");

	/* 第 257 个 Timer 先受理，再在 Worker 上确定性触发大块扩容失败。 */
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineOomBlock,
		&Context
	), "engine OOM blocker post failed");
	testEngineOomWait(&Context.Started, 1,
		"engine OOM blocker did not start");
	iFailedTimer = xrtNetEngineSchedule(
		pEngine,
		0,
		iTimerDeadline,
		testEngineOomTimer,
		&Context
	);
	testRequire(iFailedTimer != 0,
		"engine OOM terminal timer was not accepted");
	xrtAtomic32Store(&Context.Fail, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&Context.Release, 1, XMEMORY_RELEASE);
	testEngineOomWait(&Context.Errors, 1,
		"accepted engine timer did not report OOM");
	xrtAtomic32Store(&Context.Fail, 0, XMEMORY_RELEASE);
	testRequire(xrtAtomic32Load(
		&Context.Unexpected,
		XMEMORY_ACQUIRE
	) == 0, "engine OOM produced an unexpected timer result");

	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"engine OOM stats failed");
	testRequire(
		(Stats.TimersAccepted == (TEST_ENGINE_OOM_TIMER_COUNT + 3u)) &&
		(Stats.TimersFired == 1) &&
		(Stats.TimersCancelled == 1) &&
		(Stats.TimerErrors == 1) &&
		(Stats.ActiveTimers == TEST_ENGINE_OOM_TIMER_COUNT),
		"engine OOM stats did not preserve accepted timers"
	);

	/* 销毁必须关闭其余 Timer，且不得重复终结失败的 Timer。 */
	testRequire(xrtNetEngineDestroy(pEngine),
		"engine destroy after OOM failed");
	testRequire(xrtAtomic32Load(&Context.Errors, XMEMORY_ACQUIRE) == 1,
		"engine OOM timer completed more than once");
	testRequire(xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) ==
		TEST_ENGINE_OOM_TIMER_COUNT,
		"engine OOM destroy did not close retained timers");
	testRequire(xrtAtomic32Load(&Context.Unexpected, XMEMORY_ACQUIRE) == 0,
		"engine OOM destroy produced an unexpected result");
	testRequire(xrtAtomic64Load(&Context.Attempts, XMEMORY_ACQUIRE) != 0,
		"engine OOM allocator observed no attempts");

	/* 零预算只关闭 Worker 小节点缓存，不影响全局堆自己的尺寸类缓存。 */
	Config.NodeCacheBytes = 0;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"uncached engine start failed");
	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"uncached engine initial stats failed");
	iCachedTimer = Stats.NodeCacheMisses;
	iUncachedTimer = xrtNetEngineAfter(
		pEngine,
		0,
		60000000u,
		testEngineOomTimer,
		&Context
	);
	testRequire(iUncachedTimer != 0,
		"uncached engine timer schedule failed");
	testRequire(xrtNetEngineStats(pEngine, &Stats) &&
		(Stats.NodeCacheHits == 0) &&
		(Stats.NodeCacheMisses > iCachedTimer) &&
		(Stats.NodeCachedBytes == 0),
		"zero engine node cache did not expose direct allocations");
	testRequire(xrtNetEngineDestroy(pEngine),
		"uncached engine destroy failed");
	printf("[PASS] network engine OOM\n");
	return 0;
}
