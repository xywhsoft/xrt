#include "../test.h"



#if !defined(TEST_TCP_DIAL_BACKEND)
	#define TEST_TCP_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct testdialedge {
	xnetengine* Engine;
	xatomic32 Entered;
	xatomic32 Gate;
	xatomic32 Done;
	xnetresult Result;
	xerrkind ErrorKind;
	xerrkind CauseKind;
	int32 ErrorCode;
	bool HasCause;
} testdialedge;



typedef struct testdialworkersubmit {
	xnetengine* Engine;
	xnetresolver* Resolver;
	xnetdialconfig Config;
	xatomic32 Submitted;
	xatomic32 Done;
	xnetdial* Dial;
	uint16 Port;
	bool InSubmit;
} testdialworkersubmit;



/* 在测试截止时间内等待原子计数达到下限。 */
static void testDialEdgeWait(
	const xatomic32* pValue,
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



/* 返回一个必然使用测试端口的 IPv4 回环地址。 */
static xnetaddrlist* testDialEdgeLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	testdialedge* pContext = (testdialedge*)pData;
	xnetaddr Address;

	(void)Family;
	if ( strncmp(sHost, "slow-", 5) == 0 ) {
		(void)xrtAtomic32FetchAdd(
			&pContext->Entered,
			1,
			XMEMORY_RELEASE
		);
		while ( xrtAtomic32Load(
			&pContext->Gate,
			XMEMORY_ACQUIRE
		) == 0 ) {
			xrtThreadYield();
		}
	}
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		return NULL;
	}
	return xrtNetAddrListCreate(&Address, 1);
}



/* 保存失败、取消或超时终态的稳定错误摘要。 */
static void testDialEdgeDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testdialedge* pContext = (testdialedge*)pData;

	testRequire(xrtNetEngineCurrent(pContext->Engine) != NULL,
		"dial edge callback did not run on a network worker");
	testRequire(pStream == NULL,
		"dial edge failure unexpectedly returned a stream");
	pContext->Result = Result;
	pContext->ErrorKind = xrtErrorKind(pError);
	pContext->CauseKind = xrtErrorKind(xrtErrorCause(pError));
	pContext->ErrorCode = xrtErrorCode(pError);
	pContext->HasCause = xrtErrorCause(pError) != NULL;
	testRequire(xrtNetDialError(pDial) == pError,
		"dial edge terminal error was not published");
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证缓存命中仍不会从同一 Worker 的 Dial 提交调用栈重入。 */
static void testDialEdgeWorkerDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testdialworkersubmit* pContext = (testdialworkersubmit*)pData;

	testRequire(xrtNetEngineCurrent(pContext->Engine) != NULL,
		"worker-submitted dial callback left the network worker");
	testRequire(!pContext->InSubmit,
		"worker-submitted dial callback reentered xrtNetDial");
	testRequire((pDial == pContext->Dial) &&
		(Result == XNET_RESULT_ERROR) && (pStream == NULL) &&
		(xrtErrorCode(pError) == XNET_ERROR_DIAL_CONNECT),
		"worker-submitted dial terminal mismatch");
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 从目标 Worker 提交一次必然命中 Resolver 缓存的快速失败 Dial。 */
static void testDialEdgeWorkerSubmit(
	xnetworker* pWorker,
	ptr pData
)
{
	testdialworkersubmit* pContext = (testdialworkersubmit*)pData;

	testRequire(xrtNetWorkerIsCurrent(pWorker),
		"dial submit task worker mismatch");
	pContext->InSubmit = true;
	pContext->Dial = xrtNetDial(
		pContext->Engine,
		pContext->Resolver,
		"failure.test",
		pContext->Port,
		&pContext->Config,
		NULL,
		NULL,
		testDialEdgeWorkerDone,
		pContext
	);
	testRequire(pContext->Dial != NULL,
		"worker-submitted cached dial failed synchronously");
	pContext->InSubmit = false;
	(void)xrtAtomic32FetchAdd(
		&pContext->Submitted,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立使用测试查询过程的独立 Resolver。 */
static xnetresolver* testDialEdgeResolver(testdialedge* pContext)
{
	xnetresolverconfig Config;

	xrtNetResolverConfigInit(&Config);
	Config.Workers = 2;
	Config.Lookup = testDialEdgeLookup;
	Config.LookupData = pContext;
	return xrtNetResolverCreate(&Config);
}



/* 取得一个已经释放、因此当前没有 Listener 的本地端口。 */
static uint16 testDialEdgeUnusedPort(void)
{
	xnetsocket Socket;
	xnetaddr Address;

	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Socket != NULL, "dial edge socket open failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Socket, &Address) &&
		xrtNetSocketLocal(Socket, &Address),
		"dial edge unused port allocation failed");
	testRequire(xrtNetSocketClose(Socket),
		"dial edge unused port socket close failed");
	return Address.Port;
}



/* 重试无副作用的活动对象检查，确保异步取消资源已经真正排空。 */
static void testDialEdgeDestroyEngine(xnetengine* pEngine)
{
	xdeadline iDeadline = xrtDeadlineAfter(5000000u);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(iDeadline),
			"dial edge engine retained an internal resource");
		xrtThreadYield();
	}
}



/* 验证耗尽、取消、DNS 总超时和入口参数边界。 */
int main(void)
{
	testdialedge Failure;
	testdialedge Cancel;
	testdialedge Timeout;
	testdialworkersubmit WorkerSubmit;
	xnetengineconfig EngineConfig;
	xnetdialconfig DialConfig;
	xnetdialstats Stats;
	xnetenginestats EngineStats;
	xnetengine* pEngine;
	xnetengine* pStoppedEngine;
	xnetresolver* pFailureResolver;
	xnetresolver* pCancelResolver;
	xnetresolver* pTimeoutResolver;
	xnetdial* pFailure;
	xnetdial* pCancel;
	xnetdial* pTimeout;
	uint16 iPort;

	memset(&Failure, 0, sizeof(Failure));
	memset(&Cancel, 0, sizeof(Cancel));
	memset(&Timeout, 0, sizeof(Timeout));
	memset(&WorkerSubmit, 0, sizeof(WorkerSubmit));
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TCP_DIAL_BACKEND;
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"dial edge engine start failed");
	Failure.Engine = pEngine;
	Cancel.Engine = pEngine;
	Timeout.Engine = pEngine;
	pFailureResolver = testDialEdgeResolver(&Failure);
	pCancelResolver = testDialEdgeResolver(&Cancel);
	pTimeoutResolver = testDialEdgeResolver(&Timeout);
	testRequire((pFailureResolver != NULL) &&
		(pCancelResolver != NULL) && (pTimeoutResolver != NULL),
		"dial edge resolver create failed");
	iPort = testDialEdgeUnusedPort();
	xrtNetDialConfigInit(&DialConfig);
	testRequire(
		xrtNetDialConfigValid(&DialConfig),
		"default Dial configuration is invalid"
	);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.MaxAttempts = 1;
	DialConfig.Stream.ConnectTimeout = 1000000u;
	pFailure = xrtNetDial(
		pEngine,
		pFailureResolver,
		"failure.test",
		iPort,
		&DialConfig,
		NULL,
		NULL,
		testDialEdgeDone,
		&Failure
	);
	testRequire(pFailure != NULL, "dial exhaustion submit failed");
	testDialEdgeWait(&Failure.Done, 1, "dial exhaustion did not finish");
	testRequire((Failure.Result == XNET_RESULT_ERROR) &&
		(Failure.ErrorKind == Failure.CauseKind) &&
		((Failure.ErrorKind == XERR_IO) ||
		 (Failure.ErrorKind == XERR_TIMEOUT)) &&
		(Failure.ErrorCode == XNET_ERROR_DIAL_CONNECT) &&
		Failure.HasCause && xrtNetDialStats(pFailure, &Stats) &&
		(Stats.State == XNET_DIAL_FAILED) &&
		(Stats.AttemptsStarted == 1) &&
		(Stats.AttemptsFailed == 1) && !Stats.HasWinner,
		"dial exhaustion terminal mismatch");

	WorkerSubmit.Engine = pEngine;
	WorkerSubmit.Resolver = pFailureResolver;
	WorkerSubmit.Config = DialConfig;
	WorkerSubmit.Port = iPort;
	testRequire(xrtNetEnginePost(
		pEngine,
		0,
		testDialEdgeWorkerSubmit,
		&WorkerSubmit
	), "worker-submitted dial task was rejected");
	testDialEdgeWait(&WorkerSubmit.Submitted, 1,
		"worker-submitted dial task did not return");
	testDialEdgeWait(&WorkerSubmit.Done, 1,
		"worker-submitted cached dial did not finish");
	testRequire(xrtNetEngineStats(pEngine, &EngineStats) &&
		(EngineStats.NodeCacheHits != 0) &&
		(EngineStats.NodeCachedBytes != 0),
		"dial candidate did not use the bounded worker node cache");

	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.Timeout = 0;
	pCancel = xrtNetDial(
		pEngine,
		pCancelResolver,
		"slow-cancel.test",
		iPort,
		&DialConfig,
		NULL,
		NULL,
		testDialEdgeDone,
		&Cancel
	);
	testRequire(pCancel != NULL, "dial cancellation submit failed");
	testDialEdgeWait(&Cancel.Entered, 1,
		"dial cancellation lookup did not start");
	testRequire(!xrtNetEngineDestroy(pEngine) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtNetEngineState(pEngine) == XNET_ENGINE_RUNNING),
		"active dial did not retain the network engine lifecycle");
	xrtClearError();
	testRequire(xrtNetDialCancel(pCancel),
		"dial cancellation request was rejected");
	testDialEdgeWait(&Cancel.Done, 1, "dial cancellation did not finish");
	testRequire((Cancel.Result == XNET_RESULT_CANCELLED) &&
		(Cancel.ErrorKind == XERR_CANCELLED) &&
		(Cancel.ErrorCode == XNET_ERROR_DIAL_CONNECT) &&
		(xrtNetDialState(pCancel) == XNET_DIAL_CANCELLED) &&
		!xrtNetDialCancel(pCancel),
		"dial cancellation terminal mismatch");
	xrtAtomic32Store(&Cancel.Gate, 1, XMEMORY_RELEASE);

	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Family = XNET_FAMILY_IPV4;
	DialConfig.Timeout = 20000u;
	pTimeout = xrtNetDial(
		pEngine,
		pTimeoutResolver,
		"slow-timeout.test",
		iPort,
		&DialConfig,
		NULL,
		NULL,
		testDialEdgeDone,
		&Timeout
	);
	testRequire(pTimeout != NULL, "dial timeout submit failed");
	testDialEdgeWait(&Timeout.Entered, 1,
		"dial timeout lookup did not start");
	testDialEdgeWait(&Timeout.Done, 1, "dial DNS timeout did not finish");
	testRequire((Timeout.Result == XNET_RESULT_TIMEOUT) &&
		(Timeout.ErrorKind == XERR_TIMEOUT) &&
		(Timeout.ErrorCode == XNET_ERROR_DIAL_CONNECT) &&
		(xrtNetDialState(pTimeout) == XNET_DIAL_FAILED),
		"dial DNS timeout terminal mismatch");
	xrtAtomic32Store(&Timeout.Gate, 1, XMEMORY_RELEASE);

	xrtNetDialConfigInit(&DialConfig);
	DialConfig.MaxAttempts = 0;
	testRequire(
		!xrtNetDialConfigValid(&DialConfig) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_DIAL_CONFIG),
		"public Dial config validation accepted zero attempts"
	);
	xrtClearError();
	testRequire(xrtNetDial(
		pEngine,
		pFailureResolver,
		"invalid.test",
		iPort,
		&DialConfig,
		NULL,
		NULL,
		testDialEdgeDone,
		&Failure
	) == NULL && (xrtErrorCode(xrtGetError()) == XNET_ERROR_DIAL_CONFIG),
		"dial invalid attempt limit was accepted");
	xrtClearError();
	testRequire(xrtNetDial(
		pEngine,
		pFailureResolver,
		"invalid.test",
		0,
		NULL,
		NULL,
		NULL,
		testDialEdgeDone,
		&Failure
	) == NULL && (xrtErrorCode(xrtGetError()) == XNET_ERROR_DIAL_CONFIG),
		"dial zero port was accepted");
	xrtClearError();
	pStoppedEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(pStoppedEngine != NULL,
		"dial stopped-engine fixture create failed");
	testRequire(xrtNetDial(
		pStoppedEngine,
		pFailureResolver,
		"closed.test",
		iPort,
		NULL,
		NULL,
		NULL,
		testDialEdgeDone,
		&Failure
	) == NULL && (xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_DIAL_CREATE) &&
		(strcmp(xrtErrorOperation(xrtGetError()), "create-dial") == 0),
		"dial stopped engine error mismatch");
	xrtClearError();
	testRequire(xrtNetEngineDestroy(pStoppedEngine),
		"dial stopped-engine fixture destroy failed");

	xrtNetDialDestroy(pFailure);
	xrtNetDialDestroy(WorkerSubmit.Dial);
	xrtNetDialDestroy(pCancel);
	xrtNetDialDestroy(pTimeout);
	testRequire(xrtNetResolverDestroy(pFailureResolver) &&
		xrtNetResolverDestroy(pCancelResolver) &&
		xrtNetResolverDestroy(pTimeoutResolver),
		"dial edge resolver destroy failed");
	testDialEdgeDestroyEngine(pEngine);
	printf("[PASS] managed TCP dial edge terminals\n");
	return 0;
}
