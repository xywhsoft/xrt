#include "../test.h"
#include "../../src/internal/xrt_net_engine.h"



typedef struct testenginecontext {
	xnetengine* Engine;
	xnetpost EmbeddedPost;
	xnetpost ShutdownPost;
	xatomic32 Posts;
	xatomic32 EmbeddedPosts;
	xatomic32 WorkerZero;
	xatomic32 WorkerOne;
	xatomic32 Current;
	xatomic32 Fired;
	xatomic32 Cancelled;
	xatomic32 Closed;
	xatomic32 ShutdownPosts;
	xatomic32 Completion;
	xatomic32 StopRejected;
	xatomic32 BufferPool;
	xatomic32 WorkerMemory;
} testenginecontext;



/* 在测试截止时间前等待原子计数达到目标。 */
static void testEngineWait(xatomic32* pValue, uint32 iExpected, cstr sMessage)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(iDeadline), sMessage);
		xrtThreadYield();
	}
}



/* 记录任务亲和性和 Worker 当前线程身份。 */
static void testEnginePost(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;
	uint32 iIndex = xrtNetWorkerIndex(pWorker);

	if ( xrtNetWorkerIsCurrent(pWorker) &&
		 (xrtNetEngineCurrent(pContext->Engine) == pWorker) &&
		 (xrtNetWorkerEngine(pWorker) == pContext->Engine) ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Current,
			1,
			XMEMORY_RELAXED
		);
	}
	if ( iIndex == 0 ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->WorkerZero,
			1,
			XMEMORY_RELAXED
		);
	} else if ( iIndex == 1 ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->WorkerOne,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pContext->Posts,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证调用方存储的 Post 可无分配投递，并可在回调开始后原地复用。 */
static void testEngineEmbeddedPost(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;
	uint32 iPrevious = xrtAtomic32FetchAdd(
		&pContext->EmbeddedPosts,
		1,
		XMEMORY_ACQ_REL
	);

	if ( iPrevious == 0 ) {
		testRequire(xrtNetPost(
			pWorker,
			&pContext->EmbeddedPost,
			testEngineEmbeddedPost,
			pContext
		), "embedded post callback repost failed");
	}
}



/* 记录 Timer 关闭回调投递的固定点任务。 */
static void testEngineShutdownPost(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;

	testRequire(xrtNetWorkerIsCurrent(pWorker),
		"shutdown post ran outside its worker");
	(void)xrtAtomic32FetchAdd(
		&pContext->ShutdownPosts,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Worker 共享池采用创建时复制的配置并能回收自适应块。 */
static void testEngineBufferPool(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;
	xnetbufpool* pPool = xrtNetWorkerBufPool(pWorker);
	xnetbufpoolinfo Info;
	xnetbuf Buffer;

	testRequire(pPool != NULL, "worker buffer pool lookup failed");
	testRequire(xrtNetBufInit(&Buffer, pPool),
		"worker buffer init failed");
	testRequire(xrtNetBufAppend(&Buffer, "pool", 4),
		"worker buffer append failed");
	xrtNetBufClear(&Buffer);
	xrtNetBufPoolGet(pPool, &Info);
	testRequire((Info.LiveBlocks == 0) &&
		 (Info.CachedBlocks == 1) && (Info.CachedBytes == 128),
		"worker buffer pool config or reuse mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->BufferPool,
		1,
		XMEMORY_RELEASE
	);
}



/* Worker 小对象接口必须返回清零内存，并在归还后命中同一尺寸类缓存。 */
static void testEngineWorkerMemory(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;
	uint8* pFirst = (uint8*)xrtNetWorkerAlloc(pWorker, 24);
	uint8* pSecond;

	testRequire(pFirst != NULL, "worker memory allocation failed");
	for ( size_t i = 0; i < 24; i++ ) {
		testRequire(pFirst[i] == 0, "worker memory was not zero initialized");
	}
	memset(pFirst, 0xA5, 24);
	xrtNetWorkerFree(pWorker, pFirst, 24);
	pSecond = (uint8*)xrtNetWorkerAlloc(pWorker, 24);
	testRequire(pSecond != NULL, "worker cached memory allocation failed");
	for ( size_t i = 0; i < 24; i++ ) {
		testRequire(pSecond[i] == 0, "worker cached memory was not cleared");
	}
	xrtNetWorkerFree(pWorker, pSecond, 24);
	(void)xrtAtomic32FetchAdd(
		&pContext->WorkerMemory,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Worker 内停止请求被立即拒绝而不发生自等待。 */
static void testEngineStopInside(xnetworker* pWorker, ptr pData)
{
	testenginecontext* pContext = (testenginecontext*)pData;

	(void)pWorker;
	if ( !xrtNetEngineStop(pContext->Engine) &&
		 (xrtGetError() != NULL) &&
		 (xrtErrorKind(xrtGetError()) == XERR_STATE) ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->StopRejected,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtClearError();
}



/* 记录 Timer 的唯一终态。 */
static void testEngineTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	testenginecontext* pContext = (testenginecontext*)pData;

	testRequire((pWorker != NULL) && (Id != 0),
		"engine timer callback identity mismatch");
	if ( Result == XNET_RESULT_OK ) {
		(void)xrtAtomic32FetchAdd(&pContext->Fired, 1, XMEMORY_RELEASE);
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Cancelled,
			1,
			XMEMORY_RELEASE
		);
	} else if ( Result == XNET_RESULT_CLOSED ) {
		(void)xrtAtomic32FetchAdd(&pContext->Closed, 1, XMEMORY_RELEASE);
		testRequire(xrtNetPost(
			pWorker,
			&pContext->ShutdownPost,
			testEngineShutdownPost,
			pContext
		), "timer close callback post failed");
	} else {
		testRequire(false, "engine timer returned unexpected terminal result");
	}
}



/* 当前 Worker 调度返回时 Timer 必须已经入堆，可无分配地立即取消。 */
static void testEngineTimerCancelInside(
	xnetworker* pWorker,
	ptr pData
)
{
	testenginecontext* pContext = (testenginecontext*)pData;
	uint64 Id = xrtNetEngineAfter(
		pContext->Engine,
		xrtNetWorkerIndex(pWorker),
		5000000u,
		testEngineTimer,
		pContext
	);

	testRequire(
		Id != 0,
		"engine inner timer schedule failed"
	);
	testRequire(
		xrtNetEngineTimerCancelCurrent(
			pContext->Engine,
			Id
		),
		"engine current-worker timer was not immediately cancellable"
	);
}



/* 验证端口用户事件经 Completion 回到正确 Worker。 */
static void testEngineCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	testenginecontext* pContext = (testenginecontext*)pData;

	testRequire(xrtNetWorkerIsCurrent(pWorker),
		"engine completion ran outside its worker");
	testRequire((pEvent->Type == XNET_PORT_EVENT_USER) &&
		(pEvent->Id == 77u), "engine completion event mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->Completion,
		1,
		XMEMORY_RELEASE
	);
}



/* 覆盖生命周期、亲和任务、Timer 终态、Completion、统计和重启。 */
int main(void)
{
	xnetengineconfig Config;
	xnetbufpoolconfig BufferConfig;
	testenginecontext Context;
	xnetcompletion Completion;
	xnetenginestats Stats;
	xnetportconfig PortConfig;
	xnetengine* pEngine;
	uint64 iCancelTimer;
	uint64 iCloseTimer;
	uint64 iOperationOne;
	uint64 iOperationTwo;

	memset(&Context, 0, sizeof(Context));
	xrtNetPostInit(&Context.EmbeddedPost);
	xrtNetPostInit(&Context.ShutdownPost);
	testRequire(!xrtNetPostPending(&Context.EmbeddedPost),
		"new embedded post is pending");
	xrtNetEngineConfigInit(&Config);
	xrtNetBufPoolConfigInit(&BufferConfig);
	BufferConfig.BlockSize[0] = 128;
	Config.Workers = 2;
	Config.BufferPool = &BufferConfig;
	Config.CommandCapacity = 64;
	Config.TimerLimit = 64;
	Config.EventBatch = 16;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(pEngine != NULL, "engine create failed");
	BufferConfig.BlockSize[0] = 0;
	Context.Engine = pEngine;
	testRequire(xrtNetEngineWorkerCount(pEngine) == 2,
		"engine worker count mismatch");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_STOPPED,
		"new engine state mismatch");
	testRequire(xrtNetEngineWorker(pEngine, 0) != NULL,
		"engine worker lookup failed");
	iOperationOne = xrtNetWorkerOperationId(
		xrtNetEngineWorker(pEngine, 0)
	);
	iOperationTwo = xrtNetWorkerOperationId(
		xrtNetEngineWorker(pEngine, 1)
	);
	testRequire((iOperationOne != 0) && (iOperationTwo != 0) &&
		 (iOperationOne != iOperationTwo),
		"engine operation identity mismatch");
	testRequire(xrtNetEngineWorker(pEngine, 2) == NULL,
		"engine accepted out-of-range worker");
	xrtClearError();
	testRequire(xrtNetEngineStart(pEngine), "engine start failed");
	testRequire(xrtNetEngineStart(pEngine), "engine idempotent start failed");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_RUNNING,
		"running engine state mismatch");
	testRequire(
		xrtNetEnginePin(pEngine),
		"engine public lifecycle pin failed"
	);
	testRequire(
		!xrtNetEngineDestroy(pEngine) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtNetEngineState(pEngine) == XNET_ENGINE_RUNNING),
		"engine lifecycle pin did not block destroy"
	);
	xrtClearError();
	testRequire(
		xrtNetEngineUnpin(pEngine),
		"engine public lifecycle unpin failed"
	);
	testRequire(
		!xrtNetEngineUnpin(pEngine) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"engine accepted an unmatched lifecycle unpin"
	);
	xrtClearError();
	testRequire(xrtNetPost(
		xrtNetEngineWorker(pEngine, 0),
		&Context.EmbeddedPost,
		testEngineEmbeddedPost,
		&Context
	), "embedded post failed");
	testEngineWait(&Context.EmbeddedPosts, 2,
		"embedded post did not execute or repost");
	testRequire(!xrtNetPostPending(&Context.EmbeddedPost),
		"completed embedded post remains pending");
	testRequire(xrtNetPortGetConfig(
		xrtNetWorkerPort(xrtNetEngineWorker(pEngine, 0)),
		&PortConfig
	) && (PortConfig.Backend != XNET_PORT_AUTO) &&
		(PortConfig.WatchLimit != 0) &&
		(PortConfig.OperationLimit != 0),
		"engine worker port capacities were not resolved");
	#if (defined(_WIN32) || defined(_WIN64)) && \
		defined(XRT_FEATURE_NET_PORT_IOCP)
		testRequire(
			(PortConfig.Backend != XNET_PORT_IOCP) ||
			(PortConfig.OperationLimit >= 65536u),
			"engine IOCP automatic operation capacity is too small"
		);
	#endif
	testRequire(xrtNetWorkerBufPool(
		xrtNetEngineWorker(pEngine, 0)
	) == NULL, "foreign thread accessed worker buffer pool");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"foreign worker buffer pool error mismatch");
	xrtClearError();

	for ( uint64 i = 0; i < 4; i++ ) {
		testRequire(xrtNetEnginePost(pEngine, i, testEnginePost, &Context),
			"engine post failed");
	}
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineBufferPool,
		&Context
	), "engine buffer pool test post failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineWorkerMemory,
		&Context
	), "engine worker memory test post failed");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineStopInside,
		&Context
	), "engine inner stop test post failed");
	testEngineWait(&Context.Posts, 4, "engine posts did not execute");
	testEngineWait(&Context.BufferPool, 1,
		"worker buffer pool test did not execute");
	testEngineWait(&Context.WorkerMemory, 1,
		"worker memory test did not execute");
	testEngineWait(&Context.StopRejected, 1,
		"worker self-stop was not rejected");
	testRequire((xrtAtomic32Load(&Context.WorkerZero, XMEMORY_ACQUIRE) == 2) &&
		(xrtAtomic32Load(&Context.WorkerOne, XMEMORY_ACQUIRE) == 2) &&
		(xrtAtomic32Load(&Context.Current, XMEMORY_ACQUIRE) == 4),
		"engine affinity or current worker mismatch");

	testRequire(xrtNetEngineAfter(
		pEngine,
		0,
		0,
		testEngineTimer,
		&Context
	) != 0, "engine immediate timer failed");
	iCancelTimer = xrtNetEngineAfter(
		pEngine,
		1,
		5000000u,
		testEngineTimer,
		&Context
	);
	testRequire(iCancelTimer != 0, "engine cancellable timer failed");
	testRequire(xrtNetEngineTimerCancel(pEngine, iCancelTimer),
		"engine timer cancel request failed");
	testEngineWait(&Context.Fired, 1, "engine timer did not fire");
	testEngineWait(&Context.Cancelled, 1, "engine timer was not cancelled");
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testEngineTimerCancelInside,
		&Context
	), "engine inner timer test post failed");
	testEngineWait(
		&Context.Cancelled,
		2,
		"engine inner timer was not cancelled"
	);

	xrtNetCompletionInit(&Completion, testEngineCompletion, &Context);
	testRequire(xrtNetPortPost(
		xrtNetWorkerPort(xrtNetEngineWorker(pEngine, 1)),
		77,
		&Completion
	), "engine completion post failed");
	testEngineWait(&Context.Completion, 1,
		"engine completion was not dispatched");

	iCloseTimer = xrtNetEngineAfter(
		pEngine,
		0,
		5000000u,
		testEngineTimer,
		&Context
	);
	testRequire(iCloseTimer != 0, "engine close timer failed");
	testRequire(xrtNetEngineStop(pEngine), "engine stop failed");
	testRequire(xrtAtomic32Load(&Context.Closed, XMEMORY_ACQUIRE) == 1,
		"engine stop did not close pending timer");
	testRequire(xrtAtomic32Load(
		&Context.ShutdownPosts,
		XMEMORY_ACQUIRE
	) == 1, "engine stop lost timer close callback post");
	testRequire(!xrtNetPostPending(&Context.ShutdownPost),
		"engine stop retained a pending shutdown post");
	testRequire(xrtNetEngineState(pEngine) == XNET_ENGINE_STOPPED,
		"stopped engine state mismatch");
	testRequire(!xrtNetEnginePost(pEngine, 0, testEnginePost, &Context),
		"stopped engine accepted a post");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_CLOSED,
		"stopped engine post error mismatch");
	xrtClearError();

	testRequire(xrtNetEngineStats(pEngine, &Stats),
		"engine stats failed");
	testRequire((Stats.Workers == 2) && (Stats.PostsAccepted == 11) &&
		(Stats.PostsExecuted == 11) && (Stats.TimersAccepted == 4) &&
		(Stats.TimersFired == 1) && (Stats.TimersCancelled == 2) &&
		(Stats.TimersClosed == 1) && (Stats.Events >= 1) &&
		(Stats.ActiveTimers == 0) && (Stats.PendingCommands == 0),
		"engine stats mismatch");

	testRequire(xrtNetEngineStart(pEngine), "engine restart failed");
	testRequire(xrtNetEnginePost(pEngine, 0, testEnginePost, &Context),
		"restarted engine post failed");
	testEngineWait(&Context.Posts, 5,
		"restarted engine did not execute post");
	testRequire(xrtNetEngineDestroy(pEngine), "engine destroy failed");
	printf("[PASS] network engine\n");
	return 0;
}
