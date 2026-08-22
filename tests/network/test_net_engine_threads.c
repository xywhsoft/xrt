#include "../test.h"



#define TEST_ENGINE_PRODUCERS 4u
#define TEST_ENGINE_MIN_ACCEPTED 4096u



typedef struct testenginethreads {
	xnetengine* Engine;
	xatomic32 Started;
	xatomic32 Closed;
	xatomic32 Failure;
	xatomic64 StatsReads;
	xatomic64 Accepted;
	xatomic64 Executed;
} testenginethreads;



typedef struct testengineproducer {
	testenginethreads* State;
	uint64 Affinity;
} testengineproducer;



/* 记录所有已受理命令的唯一执行。 */
static void testEngineThreadsHit(xnetworker* pWorker, ptr pData)
{
	testenginethreads* pState = (testenginethreads*)pData;

	(void)pWorker;
	(void)xrtAtomic64FetchAdd(&pState->Executed, 1, XMEMORY_RELEASE);
}



/* 持续提交直到 Stop 关闭入口，队列背压只让出后重试。 */
static int32 testEngineThreadsProducer(ptr pData)
{
	testengineproducer* pProducer = (testengineproducer*)pData;
	testenginethreads* pState = pProducer->State;

	(void)xrtAtomic32FetchAdd(&pState->Started, 1, XMEMORY_RELEASE);
	for ( ;; ) {
		if ( xrtNetEnginePost(
			pState->Engine,
			pProducer->Affinity,
			testEngineThreadsHit,
			pState
		) ) {
			(void)xrtAtomic64FetchAdd(
				&pState->Accepted,
				1,
				XMEMORY_RELEASE
			);
			continue;
		}
		if ( (xrtGetError() != NULL) &&
			 (xrtErrorKind(xrtGetError()) == XERR_AGAIN) ) {
			xrtClearError();
			xrtThreadYield();
			continue;
		}
		if ( (xrtGetError() != NULL) &&
			 (xrtErrorKind(xrtGetError()) == XERR_CLOSED) ) {
			(void)xrtAtomic32FetchAdd(
				&pState->Closed,
				1,
				XMEMORY_RELEASE
			);
			return 0;
		}

		xrtAtomic32Store(&pState->Failure, 1, XMEMORY_RELEASE);
		return 1;
	}
}



/* 在 Stop 释放 Worker 运行资源期间持续读取只依赖原子状态的统计快照。 */
static int32 testEngineThreadsStats(ptr pData)
{
	testenginethreads* pState = (testenginethreads*)pData;

	while ( xrtNetEngineState(pState->Engine) != XNET_ENGINE_STOPPED ) {
		xnetenginestats Stats;

		if ( !xrtNetEngineStats(pState->Engine, &Stats) ||
			 (Stats.Workers != 2) ) {
			xrtAtomic32Store(
				&pState->Failure,
				1,
				XMEMORY_RELEASE
			);
			return 1;
		}
		(void)xrtAtomic64FetchAdd(
			&pState->StatsReads,
			1,
			XMEMORY_RELEASE
		);
		xrtThreadYield();
	}
	return 0;
}



/* 验证多生产者与 Stop 竞争时不丢失已受理命令，也不访问已释放队列。 */
int main(void)
{
	xnetengineconfig Config;
	testenginethreads State;
	testengineproducer Producers[TEST_ENGINE_PRODUCERS];
	xthread* Threads[TEST_ENGINE_PRODUCERS];
	xthread* StatsThread;
	xnetenginestats Stats;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	memset(Threads, 0, sizeof(Threads));
	xrtAtomic32Init(&State.Started, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Failure, 0);
	xrtAtomic64Init(&State.StatsReads, 0);
	xrtAtomic64Init(&State.Accepted, 0);
	xrtAtomic64Init(&State.Executed, 0);
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	Config.CommandCapacity = 64;
	Config.NodeCacheBytes = 4096;
	Config.EventBatch = 8;
	State.Engine = xrtNetEngineCreate(&Config);
	testRequire(State.Engine != NULL, "threaded engine create failed");
	testRequire(xrtNetEngineStart(State.Engine),
		"threaded engine start failed");
	StatsThread = xrtThreadCreate(
		testEngineThreadsStats,
		&State,
		0
	);
	testRequire(StatsThread != NULL,
		"threaded engine stats observer create failed");

	/* 每个生产者固定路由到一个 Worker，两个 Worker 均承受竞争。 */
	for ( uint32 i = 0; i < TEST_ENGINE_PRODUCERS; i++ ) {
		Producers[i].State = &State;
		Producers[i].Affinity = i;
		Threads[i] = xrtThreadCreate(
			testEngineThreadsProducer,
			&Producers[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"threaded engine producer create failed");
	}

	/* 在生产者保持活跃时执行 Stop，放大提交侧生命周期竞态。 */
	iDeadline = xrtDeadlineAfter(5000000u);
	while ( (xrtAtomic32Load(&State.Started, XMEMORY_ACQUIRE) <
		TEST_ENGINE_PRODUCERS) ||
		(xrtAtomic64Load(&State.Accepted, XMEMORY_ACQUIRE) <
		TEST_ENGINE_MIN_ACCEPTED) ) {
		testRequire(xrtAtomic32Load(
			&State.Failure,
			XMEMORY_ACQUIRE
		) == 0, "threaded engine producer failed before stop");
		testRequire(!xrtDeadlineExpired(iDeadline),
			"threaded engine producers made no progress");
		xrtThreadYield();
	}
	testRequire(xrtNetEngineStop(State.Engine),
		"threaded engine stop failed");
	testRequire(xrtThreadWait(StatsThread) == XWAIT_OK,
		"threaded engine stats observer wait failed");
	testRequire(xrtThreadExitCode(StatsThread) == 0,
		"threaded engine stats observer returned failure");
	xrtThreadDestroy(StatsThread);

	/* Stop 返回前必须排空全部已受理命令，并关闭所有提交者。 */
	for ( uint32 i = 0; i < TEST_ENGINE_PRODUCERS; i++ ) {
		testRequire(xrtThreadWait(Threads[i]) == XWAIT_OK,
			"threaded engine producer wait failed");
		testRequire(xrtThreadExitCode(Threads[i]) == 0,
			"threaded engine producer returned failure");
		xrtThreadDestroy(Threads[i]);
	}
	testRequire(xrtAtomic32Load(&State.Failure, XMEMORY_ACQUIRE) == 0,
		"threaded engine producer observed an unexpected error");
	testRequire(xrtAtomic64Load(&State.StatsReads, XMEMORY_ACQUIRE) != 0,
		"threaded engine stats observer made no progress");
	testRequire(xrtAtomic32Load(&State.Closed, XMEMORY_ACQUIRE) ==
		TEST_ENGINE_PRODUCERS,
		"threaded engine did not close every producer");
	testRequire(xrtAtomic64Load(&State.Accepted, XMEMORY_ACQUIRE) ==
		xrtAtomic64Load(&State.Executed, XMEMORY_ACQUIRE),
		"threaded engine lost an accepted command");

	testRequire(xrtNetEngineStats(State.Engine, &Stats),
		"threaded engine stats failed");
	testRequire(
		(Stats.State == XNET_ENGINE_STOPPED) &&
		(Stats.PostsAccepted == xrtAtomic64Load(
			&State.Accepted,
			XMEMORY_ACQUIRE
		)) &&
		(Stats.PostsExecuted == xrtAtomic64Load(
			&State.Executed,
			XMEMORY_ACQUIRE
		)) &&
		(Stats.PendingCommands == 0),
		"threaded engine stats mismatch"
	);
	testRequire(xrtNetEngineDestroy(State.Engine),
		"threaded engine destroy failed");
	printf("[PASS] network engine submit-stop threads\n");
	return 0;
}
