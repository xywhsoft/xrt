#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_DIAL_BACKEND)
	#define TEST_TLS_DIAL_BACKEND XNET_PORT_SELECT
#endif



typedef struct test_tls_dial_edge {
	xnetengine* Engine;
	xatomic32 Entered;
	xatomic32 Gate;
	xatomic32 Done;
	xnetresult Result;
	xerrkind ErrorKind;
	xtlsdialstate State;
	bool HasCause;
} test_tls_dial_edge;



/* 在测试截止时间内等待一个原子计数达到下限。 */
static void testTlsDialEdgeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 慢查询由主线程开闸，普通查询返回一个无 Listener 的回环候选。 */
static xnetaddrlist* testTlsDialEdgeLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	test_tls_dial_edge* pContext =
		(test_tls_dial_edge*)pData;
	xnetaddr Address;

	(void)Family;
	if ( strncmp(sHost, "slow-", 5u) == 0 ) {
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
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 保存失败、取消或全过程超时的稳定错误摘要。 */
static void testTlsDialEdgeDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	test_tls_dial_edge* pContext =
		(test_tls_dial_edge*)pData;

	testRequire(xrtNetEngineCurrent(pContext->Engine) != NULL,
		"TLS dial edge callback did not run on a network worker");
	testRequire((pStream == NULL) && (pError != NULL),
		"TLS dial edge failure returned an invalid terminal");
	pContext->Result = Result;
	pContext->ErrorKind = xrtErrorKind(pError);
	pContext->State = xrtTlsDialState(pDial);
	pContext->HasCause = xrtErrorCause(pError) != NULL;
	testRequire(xrtTlsDialError(pDial) == pError,
		"TLS dial terminal error was not published");
	(void)xrtAtomic32FetchAdd(
		&pContext->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立使用确定性查询过程的独立 Resolver。 */
static xnetresolver* testTlsDialEdgeResolver(
	test_tls_dial_edge* pContext
)
{
	xnetresolverconfig Config;

	xrtNetResolverConfigInit(&Config);
	Config.Workers = 2u;
	Config.Lookup = testTlsDialEdgeLookup;
	Config.LookupData = pContext;
	return xrtNetResolverCreate(&Config);
}



/* 取得一个已经释放、当前必然没有 Listener 的本地端口。 */
static uint16 testTlsDialEdgeUnusedPort(void)
{
	xnetsocket Socket;
	xnetaddr Address;

	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Socket != NULL, "TLS dial edge socket open failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Socket, &Address) &&
		xrtNetSocketLocal(Socket, &Address),
		"TLS dial edge unused port allocation failed");
	testRequire(xrtNetSocketClose(Socket),
		"TLS dial edge unused port socket close failed");
	return Address.Port;
}



/* 重试活动对象检查，确保取消后的 Resolver 和 Timer 已经排空。 */
static void testTlsDialEdgeDestroyEngine(xnetengine* pEngine)
{
	xdeadline Deadline = xrtDeadlineAfter(5000000u);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(Deadline),
			"TLS dial edge engine retained an internal resource");
		xrtThreadYield();
	}
}



/* 验证 TCP 耗尽、显式取消、跨阶段总超时和入口参数边界。 */
int main(void)
{
	test_tls_dial_edge Failure;
	test_tls_dial_edge Cancel;
	test_tls_dial_edge Timeout;
	xnetengineconfig EngineConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xnetdialstats Stats;
	xtlscontext* pTlsContext;
	xnetengine* pEngine;
	xnetresolver* pFailureResolver;
	xnetresolver* pCancelResolver;
	xnetresolver* pTimeoutResolver;
	xtlsdial* pFailure;
	xtlsdial* pCancel;
	xtlsdial* pTimeout;
	uint16 iPort;
	bool bStats;

	memset(&Failure, 0, sizeof(Failure));
	memset(&Cancel, 0, sizeof(Cancel));
	memset(&Timeout, 0, sizeof(Timeout));
	pTlsContext = testTlsServerContext();
	testRequire(pTlsContext != NULL,
		"TLS dial edge context creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_DIAL_BACKEND;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS dial edge engine start failed");
	Failure.Engine = pEngine;
	Cancel.Engine = pEngine;
	Timeout.Engine = pEngine;
	pFailureResolver = testTlsDialEdgeResolver(&Failure);
	pCancelResolver = testTlsDialEdgeResolver(&Cancel);
	pTimeoutResolver = testTlsDialEdgeResolver(&Timeout);
	testRequire((pFailureResolver != NULL) &&
		(pCancelResolver != NULL) && (pTimeoutResolver != NULL),
		"TLS dial edge resolver creation failed");
	iPort = testTlsDialEdgeUnusedPort();

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.MaxAttempts = 1u;
	DialConfig.Transport.Stream.ConnectTimeout = 1000000u;
	pFailure = xrtTlsDial(
		pEngine,
		pFailureResolver,
		"failure.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialEdgeDone,
		&Failure
	);
	testRequire(pFailure != NULL, "TLS dial exhaustion submit failed");
	testTlsDialEdgeWait(&Failure.Done, 1u,
		"TLS dial exhaustion did not finish");
	memset(&Stats, 0, sizeof(Stats));
	bStats = xrtTlsDialTransportStats(pFailure, &Stats);
	if ( !bStats ||
		(Failure.Result != XNET_RESULT_ERROR) ||
		((Failure.ErrorKind != XERR_IO) &&
		 (Failure.ErrorKind != XERR_TIMEOUT)) ||
		(Failure.State != XTLS_DIAL_FAILED) ||
		!Failure.HasCause || (Stats.AttemptsStarted != 1u) ||
		(Stats.AttemptsFailed != 1u) || Stats.HasWinner ) {
		fprintf(
			stderr,
			"[TLS Dial] exhaustion: result=%d kind=%d state=%d "
			"cause=%d started=%u failed=%u winner=%d\n",
			(int)Failure.Result,
			(int)Failure.ErrorKind,
			(int)Failure.State,
			(int)Failure.HasCause,
			Stats.AttemptsStarted,
			Stats.AttemptsFailed,
			(int)Stats.HasWinner
		);
	}
	testRequire((Failure.Result == XNET_RESULT_ERROR) &&
		((Failure.ErrorKind == XERR_IO) ||
		 (Failure.ErrorKind == XERR_TIMEOUT)) &&
		(Failure.State == XTLS_DIAL_FAILED) &&
		Failure.HasCause && bStats &&
		(Stats.AttemptsStarted == 1u) &&
		(Stats.AttemptsFailed == 1u) && !Stats.HasWinner,
		"TLS dial exhaustion terminal mismatch");

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.Timeout = 0;
	pCancel = xrtTlsDial(
		pEngine,
		pCancelResolver,
		"slow-cancel.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialEdgeDone,
		&Cancel
	);
	testRequire(pCancel != NULL, "TLS dial cancellation submit failed");
	testTlsDialEdgeWait(&Cancel.Entered, 1u,
		"TLS dial cancellation lookup did not start");
	testRequire(xrtTlsDialCancel(pCancel),
		"TLS dial cancellation request was rejected");
	testTlsDialEdgeWait(&Cancel.Done, 1u,
		"TLS dial cancellation did not finish");
	testRequire((Cancel.Result == XNET_RESULT_CANCELLED) &&
		(Cancel.ErrorKind == XERR_CANCELLED) &&
		(Cancel.State == XTLS_DIAL_CANCELLED) &&
		!xrtTlsDialCancel(pCancel),
		"TLS dial cancellation terminal mismatch");
	xrtAtomic32Store(&Cancel.Gate, 1, XMEMORY_RELEASE);

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.Timeout = 0;
	DialConfig.Timeout = 20000u;
	pTimeout = xrtTlsDial(
		pEngine,
		pTimeoutResolver,
		"slow-timeout.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialEdgeDone,
		&Timeout
	);
	testRequire(pTimeout != NULL, "TLS dial total timeout submit failed");
	testTlsDialEdgeWait(&Timeout.Entered, 1u,
		"TLS dial total timeout lookup did not start");
	testTlsDialEdgeWait(&Timeout.Done, 1u,
		"TLS dial total timeout did not finish");
	testRequire((Timeout.Result == XNET_RESULT_TIMEOUT) &&
		(Timeout.ErrorKind == XERR_TIMEOUT) &&
		(Timeout.State == XTLS_DIAL_FAILED) && Timeout.HasCause,
		"TLS dial total timeout terminal mismatch");
	xrtAtomic32Store(&Timeout.Gate, 1, XMEMORY_RELEASE);

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.MaxAttempts = 0;
	xrtClearError();
	testRequire((xrtTlsDial(
		pEngine,
		pFailureResolver,
		"invalid.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL,
		testTlsDialEdgeDone,
		&Failure
	) == NULL) && (xrtGetError() != NULL),
		"TLS dial invalid attempt limit was accepted");
	xrtClearError();
	testRequire((xrtTlsDial(
		pEngine,
		pFailureResolver,
		"invalid.tls.test",
		0,
		&ClientConfig,
		NULL,
		NULL,
		NULL,
		testTlsDialEdgeDone,
		&Failure
	) == NULL) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS dial zero port was accepted");
	xrtClearError();
	testRequire(!xrtTlsDialCancel(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS dial null cancellation was accepted");
	xrtClearError();

	xrtTlsDialDestroy(pFailure);
	xrtTlsDialDestroy(pCancel);
	xrtTlsDialDestroy(pTimeout);
	testRequire(xrtNetResolverDestroy(pFailureResolver) &&
		xrtNetResolverDestroy(pCancelResolver) &&
		xrtNetResolverDestroy(pTimeoutResolver),
		"TLS dial edge resolver destroy failed");
	testTlsDialEdgeDestroyEngine(pEngine);
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] managed TLS dial edge terminals\n");
	return 0;
}
