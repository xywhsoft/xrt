#include "../test.h"



typedef struct testnetstats {
	xatomic32 Executed;
} testnetstats;



/* 记录一次真实执行的 Engine 任务。 */
static void testNetStatsPost(xnetworker* pWorker, ptr pData)
{
	testnetstats* pState = (testnetstats*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(
		&pState->Executed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证统计裁剪不改变状态量，并严格区分三档累计量。 */
int main(void)
{
	xnetengineconfig Config;
	xnetenginestats Stats;
	testnetstats State;
	xnetengine* pEngine;
	xdeadline iDeadline;

	memset(&State, 0, sizeof(State));
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	Config.CommandCapacity = 8;
	Config.TimerLimit = 8;
	Config.EventBatch = 4;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"stats-level engine start failed");
	testRequire(
		xrtNetEnginePost(pEngine, 0, testNetStatsPost, &State),
		"stats-level post failed"
	);
	iDeadline = xrtDeadlineAfter(2000000u);
	while ( xrtAtomic32Load(&State.Executed, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"stats-level post timed out");
		xrtThreadYield();
	}
	testRequire(xrtNetEngineStop(pEngine),
		"stats-level engine stop failed");
	testRequire(
		!xrtNetEnginePost(pEngine, 0, testNetStatsPost, &State),
		"stats-level stopped engine accepted a post"
	);
	xrtClearError();
	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"stats-level snapshot failed");
	testRequire(
		(Stats.State == XNET_ENGINE_STOPPED) &&
		(Stats.Workers == 1) &&
		(Stats.PendingCommands == 0) &&
		(Stats.ActiveTimers == 0),
		"stats-level state counters changed with the sampling tier"
	);

	#if XRT_NET_STATS_LEVEL == XNET_STATS_OFF
		testRequire(
			(Stats.PostsAccepted == 0) &&
			(Stats.PostsExecuted == 0) &&
			(Stats.PostsRejected == 0),
			"OFF stats retained a cumulative counter"
		);
	#elif XRT_NET_STATS_LEVEL == XNET_STATS_BASIC
		testRequire(
			(Stats.PostsAccepted == 0) &&
			(Stats.PostsExecuted == 0) &&
			(Stats.PostsRejected == 1),
			"BASIC stats did not isolate the rejected operation"
		);
	#else
		testRequire(
			(Stats.PostsAccepted == 1) &&
			(Stats.PostsExecuted == 1) &&
			(Stats.PostsRejected == 1),
			"FULL stats did not retain complete operation counters"
		);
	#endif

	testRequire(xrtNetEngineDestroy(pEngine),
		"stats-level engine destroy failed");
	return 0;
}
