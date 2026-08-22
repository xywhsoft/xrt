#include "../test.h"



#define TEST_ENGINE_TIMER_COUNT 1024u



typedef struct testenginetimerstate testenginetimerstate;



typedef struct testenginetimeritem {
	testenginetimerstate* State;
	uint64 Id;
	xatomic32 Terminals;
	xatomic32 Result;
} testenginetimeritem;



struct testenginetimerstate {
	xatomic32 Fired;
	xatomic32 Cancelled;
	testenginetimeritem Items[TEST_ENGINE_TIMER_COUNT];
};



/* 每个 Timer 必须只观察到一次且 ID 与提交结果一致。 */
static void testEngineStressTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testenginetimeritem* pItem = (testenginetimeritem*)pData;

	(void)pWorker;
	testRequire(Id == pItem->Id, "stress timer identity mismatch");
	testRequire(xrtAtomic32FetchAdd(
		&pItem->Terminals,
		1,
		XMEMORY_ACQ_REL
	) == 0, "stress timer completed more than once");
	xrtAtomic32Store(&pItem->Result, (uint32)Result, XMEMORY_RELEASE);
	if ( Result == XNET_RESULT_OK ) {
		(void)xrtAtomic32FetchAdd(
			&pItem->State->Fired,
			1,
			XMEMORY_RELEASE
		);
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtAtomic32FetchAdd(
			&pItem->State->Cancelled,
			1,
			XMEMORY_RELEASE
		);
	} else {
		testRequire(false, "stress timer returned unexpected result");
	}
}



/* 千级 Timer 同时覆盖堆扩容、ID 哈希、取消和唯一终态。 */
int main(void)
{
	xnetengineconfig Config;
	testenginetimerstate* pState;
	xnetenginestats Stats;
	xnetengine* pEngine;
	xdeadline iDeadline;

	pState = (testenginetimerstate*)calloc(1, sizeof(*pState));
	testRequire(pState != NULL, "stress timer state allocation failed");
	xrtNetEngineConfigInit(&Config);
	Config.Workers = 1;
	Config.CommandCapacity = 4096;
	Config.TimerLimit = TEST_ENGINE_TIMER_COUNT;
	Config.EventBatch = 16;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine != NULL, "stress engine create failed");
	testRequire(xrtNetEngineStart(pEngine), "stress engine start failed");

	iDeadline = xrtDeadlineAfter(750000u);
	for ( uint32 i = 0; i < TEST_ENGINE_TIMER_COUNT; i++ ) {
		testenginetimeritem* pItem = &pState->Items[i];

		pItem->State = pState;
		pItem->Id = xrtNetEngineSchedule(
			pEngine,
			0,
			iDeadline,
			testEngineStressTimer,
			pItem
		);
		testRequire(pItem->Id != 0, "stress timer schedule failed");
	}
	for ( uint32 i = 0; i < TEST_ENGINE_TIMER_COUNT; i += 2u ) {
		testRequire(xrtNetEngineTimerCancel(
			pEngine,
			pState->Items[i].Id
		), "stress timer cancel request failed");
	}

	iDeadline = xrtDeadlineAfter(5000000u);
	while ( (xrtAtomic32Load(&pState->Fired, XMEMORY_ACQUIRE) +
		xrtAtomic32Load(&pState->Cancelled, XMEMORY_ACQUIRE)) <
		TEST_ENGINE_TIMER_COUNT ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"stress timers did not reach terminal states");
		xrtThreadYield();
	}
	testRequire(xrtAtomic32Load(&pState->Fired, XMEMORY_ACQUIRE) ==
		(TEST_ENGINE_TIMER_COUNT / 2u), "stress fired count mismatch");
	testRequire(xrtAtomic32Load(&pState->Cancelled, XMEMORY_ACQUIRE) ==
		(TEST_ENGINE_TIMER_COUNT / 2u), "stress cancelled count mismatch");
	for ( uint32 i = 0; i < TEST_ENGINE_TIMER_COUNT; i++ ) {
		uint32 Result = xrtAtomic32Load(
			&pState->Items[i].Result,
			XMEMORY_ACQUIRE
		);

		testRequire(xrtAtomic32Load(
			&pState->Items[i].Terminals,
			XMEMORY_ACQUIRE
		) == 1, "stress timer terminal count mismatch");
		testRequire(Result == (uint32)((i & 1u) == 0 ?
			XNET_RESULT_CANCELLED : XNET_RESULT_OK),
			"stress timer terminal result mismatch");
	}
	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"stress engine stats failed");
	testRequire((Stats.TimersAccepted == TEST_ENGINE_TIMER_COUNT) &&
		(Stats.TimersFired == (TEST_ENGINE_TIMER_COUNT / 2u)) &&
		(Stats.TimersCancelled == (TEST_ENGINE_TIMER_COUNT / 2u)) &&
		(Stats.TimerErrors == 0) && (Stats.ActiveTimers == 0),
		"stress engine stats mismatch");
	testRequire(xrtNetEngineDestroy(pEngine), "stress engine destroy failed");
	free(pState);
	printf("[PASS] network engine timer stress\n");
	return 0;
}
