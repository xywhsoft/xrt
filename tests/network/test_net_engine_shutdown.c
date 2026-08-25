#include "../test.h"
#include "../../src/internal/xrt_net_engine.h"



#define TEST_ENGINE_SHUTDOWN_TIMEOUT 5000000u



typedef struct testengineshutdown {
	xnetengine* Engine;
	xnetpost PublicPost;
	__xrt_net_engine_internal InternalA;
	__xrt_net_engine_internal InternalB;
	xatomic32 Entered;
	xatomic32 PublicExecuted;
	xatomic32 PublicRejected;
	xatomic32 InternalExecuted;
	xatomic32 InternalRejected;
	bool PublicLoop;
	bool StopResult;
	xerrkind StopKind;
	int32 StopCode;
} testengineshutdown;



/* 在截止时间前等待 Worker 进入指定测试阶段。 */
static void testEngineShutdownWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(TEST_ENGINE_SHUTDOWN_TIMEOUT);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 让首个回调留在 Worker，直到 Stop 已经进入状态转换。 */
static void testEngineShutdownEntered(testengineshutdown* pState)
{
	(void)xrtAtomic32FetchAdd(&pState->Entered, 1, XMEMORY_RELEASE);
	while ( xrtNetEngineState(pState->Engine) == XNET_ENGINE_RUNNING ) {
		xrtThreadYield();
	}
}



/* 停机期间持续复用同一公开 Post，直到封口明确拒绝。 */
static void testEngineShutdownPublic(xnetworker* pWorker, ptr pData)
{
	testengineshutdown* pState = (testengineshutdown*)pData;
	uint32 iExecuted = xrtAtomic32FetchAdd(
		&pState->PublicExecuted,
		1,
		XMEMORY_ACQ_REL
	);

	if ( !pState->PublicLoop ) {
		return;
	}
	if ( iExecuted == 0 ) {
		testEngineShutdownEntered(pState);
	}
	if ( !xrtNetPost(
		pWorker,
		&pState->PublicPost,
		testEngineShutdownPublic,
		pState
	) ) {
		if ( (xrtGetError() != NULL) &&
			 (xrtErrorKind(xrtGetError()) == XERR_CLOSED) ) {
			(void)xrtAtomic32FetchAdd(
				&pState->PublicRejected,
				1,
				XMEMORY_RELEASE
			);
		}
		xrtClearError();
	}
}



static void testEngineShutdownInternalA(xnetworker* pWorker, ptr pData);



/* 内部节点 B 回投 A，封口后只记录一次拒绝。 */
static void testEngineShutdownInternalB(xnetworker* pWorker, ptr pData)
{
	testengineshutdown* pState = (testengineshutdown*)pData;

	(void)xrtAtomic32FetchAdd(
		&pState->InternalExecuted,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !__xrtNetEnginePostInternal(
		pWorker,
		&pState->InternalA,
		testEngineShutdownInternalA,
		pState
	) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->InternalRejected,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 内部节点 A 在首次执行时等待 Stop，随后与 B 形成投递环。 */
static void testEngineShutdownInternalA(xnetworker* pWorker, ptr pData)
{
	testengineshutdown* pState = (testengineshutdown*)pData;
	uint32 iExecuted = xrtAtomic32FetchAdd(
		&pState->InternalExecuted,
		1,
		XMEMORY_ACQ_REL
	);

	if ( iExecuted == 0 ) {
		testEngineShutdownEntered(pState);
	}
	if ( !__xrtNetEnginePostInternal(
		pWorker,
		&pState->InternalB,
		testEngineShutdownInternalB,
		pState
	) ) {
		(void)xrtAtomic32FetchAdd(
			&pState->InternalRejected,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 从非 Worker 线程执行 Stop，保存结构化错误供主线程验证。 */
static int32 testEngineShutdownStop(ptr pData)
{
	testengineshutdown* pState = (testengineshutdown*)pData;
	const xerror* pError;

	xrtClearError();
	pState->StopResult = xrtNetEngineStop(pState->Engine);
	pError = xrtGetError();
	pState->StopKind = pError != NULL ? xrtErrorKind(pError) : XERR_NONE;
	pState->StopCode = pError != NULL ? xrtErrorCode(pError) : 0;
	xrtClearError();
	return 0;
}



/* 要求 Stop 在有限时间内完成，避免回归用例自身无限等待。 */
static void testEngineShutdownJoin(testengineshutdown* pState)
{
	xthread* pThread = xrtThreadCreate(
		testEngineShutdownStop,
		pState,
		0
	);

	testRequire(pThread != NULL, "engine shutdown thread creation failed");
	testRequire(xrtThreadWaitUntil(
		pThread,
		xrtDeadlineAfter(TEST_ENGINE_SHUTDOWN_TIMEOUT)
	) == XWAIT_OK, "engine shutdown did not terminate");
	testRequire(xrtThreadExitCode(pThread) == 0,
		"engine shutdown thread failed");
	xrtThreadDestroy(pThread);
}



/* 验证公开 Post 环、内部投递环和停机后重启契约。 */
int main(void)
{
	testengineshutdown State;
	xnetengineconfig Config;
	xnetenginestats Stats;
	uint32 iPublicBefore;
	uint32 iEntered;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Entered, 0);
	xrtAtomic32Init(&State.PublicExecuted, 0);
	xrtAtomic32Init(&State.PublicRejected, 0);
	xrtAtomic32Init(&State.InternalExecuted, 0);
	xrtAtomic32Init(&State.InternalRejected, 0);
	testRequire(xrtNetPostInit(&State.PublicPost),
		"shutdown public post init failed");
	xrtNetEngineConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	Config.Workers = 1;
	Config.CommandCapacity = 16;
	State.Engine = xrtNetEngineCreate(&Config);
	testRequire((State.Engine != NULL) && xrtNetEngineStart(State.Engine),
		"shutdown engine setup failed");

	/* 公开嵌入节点无限自投递必须被封口。 */
	State.PublicLoop = true;
	testRequire(xrtNetPost(
		xrtNetEngineWorker(State.Engine, 0),
		&State.PublicPost,
		testEngineShutdownPublic,
		&State
	), "shutdown public loop submission failed");
	testEngineShutdownWait(
		&State.Entered,
		1,
		"shutdown public loop did not enter"
	);
	testEngineShutdownJoin(&State);
	testRequire(!State.StopResult && (State.StopKind == XERR_STATE) &&
		(State.StopCode == XNET_ERROR_ENGINE_STOP),
		"public post shutdown stall error mismatch");
	testRequire(xrtNetEngineState(State.Engine) == XNET_ENGINE_STOPPED,
		"public post stall did not leave a stopped engine");
	testRequire(!xrtNetPostPending(&State.PublicPost) &&
		(xrtAtomic32Load(
			&State.PublicRejected,
			XMEMORY_ACQUIRE
		) == 1), "public post stall retained a pending command");
	testRequire(xrtNetEngineStats(State.Engine, &Stats) &&
		(Stats.ShutdownStalls == 1) && (Stats.PendingCommands == 0) &&
		(Stats.PostsAccepted == Stats.PostsExecuted),
		"public post shutdown stall stats mismatch");

	/* 重启后先证明旧节点没有复活，再构造内部 A/B 环。 */
	State.PublicLoop = false;
	iPublicBefore = xrtAtomic32Load(
		&State.PublicExecuted,
		XMEMORY_ACQUIRE
	);
	testRequire(xrtNetEngineStart(State.Engine),
		"engine restart after public stall failed");
	testRequire(xrtNetPost(
		xrtNetEngineWorker(State.Engine, 0),
		&State.PublicPost,
		testEngineShutdownPublic,
		&State
	), "normal post after restart failed");
	testEngineShutdownWait(
		&State.PublicExecuted,
		iPublicBefore + 1u,
		"normal post after restart did not execute"
	);
	iEntered = xrtAtomic32Load(&State.Entered, XMEMORY_ACQUIRE);
	testRequire(__xrtNetEnginePostInternal(
		xrtNetEngineWorker(State.Engine, 0),
		&State.InternalA,
		testEngineShutdownInternalA,
		&State
	), "shutdown internal loop submission failed");
	testEngineShutdownWait(
		&State.Entered,
		iEntered + 1u,
		"shutdown internal loop did not enter"
	);
	testEngineShutdownJoin(&State);
	testRequire(!State.StopResult && (State.StopKind == XERR_STATE) &&
		(State.StopCode == XNET_ERROR_ENGINE_STOP),
		"internal post shutdown stall error mismatch");
	testRequire((xrtAtomic32Load(
		&State.InternalRejected,
		XMEMORY_ACQUIRE
	) == 1) && xrtNetEngineStats(State.Engine, &Stats) &&
		(Stats.ShutdownStalls == 2) && (Stats.PendingCommands == 0),
		"internal post shutdown stall was not sealed cleanly");

	/* 本轮停机失败标志不得污染下一次正常运行。 */
	testRequire(xrtNetEngineStart(State.Engine),
		"engine restart after internal stall failed");
	testRequire(xrtNetEngineStop(State.Engine),
		"clean stop after shutdown stall failed");
	testRequire(xrtNetEngineStats(State.Engine, &Stats) &&
		(Stats.ShutdownStalls == 2) && (Stats.PendingCommands == 0),
		"shutdown stall counter did not remain cumulative");
	testRequire(xrtNetEngineDestroy(State.Engine),
		"shutdown stall engine destroy failed");
	printf("[PASS] network engine bounded shutdown\n");
	return 0;
}
