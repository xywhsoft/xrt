#include "../test.h"
#include "../../src/internal/xrt_net_engine.h"



#define TEST_ENGINE_INTERNAL_COUNT 1024u



/* 公平性测试保留阻塞门、内部洪峰和普通命令的并发观测。 */
typedef struct testenginefairness {
	xnetcompletion Gate;
	__xrt_net_engine_internal Commands[TEST_ENGINE_INTERNAL_COUNT];
	xatomic32 GateEntered;
	xatomic32 GateRelease;
	xatomic32 InternalExecuted;
	xatomic32 MarkerExecuted;
	xatomic32 MarkerObserved;
	xatomic32 TimerExecuted;
	xatomic32 TimerObserved;
	xatomic32 ProbeExecuted;
	xatomic32 ProbeObserved;
	xatomic32 Failed;
} testenginefairness;



/* 在有限截止时间内等待原子状态达到目标值。 */
static bool testEngineFairnessWait(xatomic32* pValue, uint32 iExpected)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) != iExpected ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 端口回调阻塞 Worker，使测试线程能够原子构造完整内部命令洪峰。 */
static void testEngineFairnessGate(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	testenginefairness* pState = (testenginefairness*)pData;

	(void)pWorker;
	if ( (pEvent == NULL) || (pEvent->Type != XNET_PORT_EVENT_USER) ) {
		xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(&pState->GateEntered, 1, XMEMORY_RELEASE);
	while ( xrtAtomic32Load(
		&pState->GateRelease,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
}



/* 每个内部命令只记录一次执行，不分配内存或继续投递。 */
static void testEngineFairnessInternal(xnetworker* pWorker, ptr pData)
{
	testenginefairness* pState = (testenginefairness*)pData;

	(void)pWorker;
	(void)xrtAtomic32FetchAdd(
		&pState->InternalExecuted,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 普通命令记录自己执行前已经消费的内部命令数量。 */
static void testEngineFairnessMarker(xnetworker* pWorker, ptr pData)
{
	testenginefairness* pState = (testenginefairness*)pData;
	uint32 iObserved = xrtAtomic32Load(
		&pState->InternalExecuted,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	xrtAtomic32Store(
		&pState->MarkerObserved,
		iObserved,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pState->MarkerExecuted, 1, XMEMORY_RELEASE);
}



/* 到期 Timer 记录内部洪峰推进位置，证明 Timer 每轮仍有执行机会。 */
static void testEngineFairnessTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testenginefairness* pState = (testenginefairness*)pData;
	uint32 iObserved = xrtAtomic32Load(
		&pState->InternalExecuted,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	(void)Id;
	if ( Result != XNET_RESULT_OK ) {
		xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(
		&pState->TimerObserved,
		iObserved,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pState->TimerExecuted, 1, XMEMORY_RELEASE);
}



/* 后续端口事件记录内部洪峰推进位置，证明 Worker 会返回 IO 分发。 */
static void testEngineFairnessProbe(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	testenginefairness* pState = (testenginefairness*)pData;
	uint32 iObserved = xrtAtomic32Load(
		&pState->InternalExecuted,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	if ( (pEvent == NULL) || (pEvent->Type != XNET_PORT_EVENT_USER) ) {
		xrtAtomic32Store(&pState->Failed, 1, XMEMORY_RELEASE);
	}
	xrtAtomic32Store(
		&pState->ProbeObserved,
		iObserved,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(&pState->ProbeExecuted, 1, XMEMORY_RELEASE);
}



/* 内部生命周期洪峰不能饿死普通命令、Timer 或下一轮端口等待。 */
int main(void)
{
	xnetengineconfig Config;
	testenginefairness State;
	xnetenginestats Stats;
	xnetcompletion Probe;
	xnetengine* pEngine;
	xnetworker* pWorker;
	xnetport* pPort;
	uint32 iObserved;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.GateEntered, 0);
	xrtAtomic32Init(&State.GateRelease, 0);
	xrtAtomic32Init(&State.InternalExecuted, 0);
	xrtAtomic32Init(&State.MarkerExecuted, 0);
	xrtAtomic32Init(&State.MarkerObserved, 0);
	xrtAtomic32Init(&State.TimerExecuted, 0);
	xrtAtomic32Init(&State.TimerObserved, 0);
	xrtAtomic32Init(&State.ProbeExecuted, 0);
	xrtAtomic32Init(&State.ProbeObserved, 0);
	xrtAtomic32Init(&State.Failed, 0);
	xrtNetCompletionInit(&State.Gate, testEngineFairnessGate, &State);
	xrtNetCompletionInit(&Probe, testEngineFairnessProbe, &State);

	xrtNetEngineConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	Config.Workers = 1;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"fairness engine setup failed");
	pWorker = xrtNetEngineWorker(pEngine, 0);
	pPort = xrtNetWorkerPort(pWorker);
	testRequire((pWorker != NULL) && (pPort != NULL) &&
		xrtNetPortPost(pPort, 1, &State.Gate),
		"fairness gate post failed");
	testRequire(testEngineFairnessWait(&State.GateEntered, 1),
		"fairness gate did not enter");

	for ( uint32 i = 0; i < TEST_ENGINE_INTERNAL_COUNT; i++ ) {
		__xrtNetEnginePostInternal(
			pWorker,
			&State.Commands[i],
			testEngineFairnessInternal,
			&State
		);
	}
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineFairnessMarker,
		&State
	) && (xrtNetEngineAfter(
		pEngine,
		0,
		0,
		testEngineFairnessTimer,
		&State
	) != 0) && xrtNetPortPost(pPort, 2, &Probe),
		"fairness follow-up work submission failed");
	xrtAtomic32Store(&State.GateRelease, 1, XMEMORY_RELEASE);

	testRequire(testEngineFairnessWait(&State.MarkerExecuted, 1) &&
		testEngineFairnessWait(&State.TimerExecuted, 1) &&
		testEngineFairnessWait(&State.ProbeExecuted, 1) &&
		testEngineFairnessWait(
			&State.InternalExecuted,
			TEST_ENGINE_INTERNAL_COUNT
		), "fairness command drain timed out");
	iObserved = xrtAtomic32Load(
		&State.MarkerObserved,
		XMEMORY_ACQUIRE
	);
	testRequire((iObserved != 0) &&
		(iObserved < TEST_ENGINE_INTERNAL_COUNT) &&
		(xrtAtomic32Load(
			&State.TimerObserved,
			XMEMORY_ACQUIRE
		) < TEST_ENGINE_INTERNAL_COUNT) &&
		(xrtAtomic32Load(
			&State.ProbeObserved,
			XMEMORY_ACQUIRE
		) < TEST_ENGINE_INTERNAL_COUNT) &&
		(xrtAtomic32Load(&State.Failed, XMEMORY_ACQUIRE) == 0),
		"internal command flood starved public work");
	testRequire(xrtNetEngineStats(pEngine, &Stats) &&
		(Stats.PendingCommands == 0) && (Stats.ActiveTimers == 0),
		"fairness engine retained pending commands");
	testRequire(xrtNetEngineStop(pEngine) &&
		xrtNetEngineDestroy(pEngine),
		"fairness engine cleanup failed");
	return 0;
}
