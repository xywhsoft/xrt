#include "../fixtures/tls_server.h"



#if !defined(TEST_TLS_DIAL_FUTURE_BACKEND)
	#define TEST_TLS_DIAL_FUTURE_BACKEND XNET_PORT_SELECT
#endif



/* 边界夹具控制阻塞解析，以确定性竞争取消与总超时。 */
typedef struct test_tls_dial_future_edge {
	xatomic32 Entered;
	xatomic32 Gate;
} test_tls_dial_future_edge;



/* 阻塞 slow 主机查询，普通查询返回一个无 Listener 的回环候选。 */
static xnetaddrlist* testTlsDialFutureEdgeLookup(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	test_tls_dial_future_edge* pContext =
		(test_tls_dial_future_edge*)pData;
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



/* 在测试截止时间前等待原子计数达到给定下限。 */
static void testTlsDialFutureEdgeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) < iExpected ) {
		testRequire(!xrtDeadlineExpired(Deadline), sMessage);
		xrtThreadYield();
	}
}



/* 建立使用确定性查询过程的独立 Resolver。 */
static xnetresolver* testTlsDialFutureEdgeResolver(
	test_tls_dial_future_edge* pContext
)
{
	xnetresolverconfig Config;

	xrtNetResolverConfigInit(&Config);
	Config.Workers = 1u;
	Config.Lookup = testTlsDialFutureEdgeLookup;
	Config.LookupData = pContext;
	return xrtNetResolverCreate(&Config);
}



/* 取得当前必然没有 Listener 的本地端口。 */
static uint16 testTlsDialFutureEdgeUnusedPort(void)
{
	xnetsocket Socket;
	xnetaddr Address;

	Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Socket != NULL,
		"TLS Dial Future edge socket open failed");
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetSocketBind(Socket, &Address) &&
		xrtNetSocketLocal(Socket, &Address),
		"TLS Dial Future unused port allocation failed");
	testRequire(xrtNetSocketClose(Socket),
		"TLS Dial Future unused port socket close failed");
	return Address.Port;
}



/* 等待 Future 到达指定终态。 */
static void testTlsDialFutureEdgeState(
	xfuture* pFuture,
	xfuturestate State,
	cstr sMessage
)
{
	testRequire((pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(5000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == State),
		sMessage);
}



/* 重试活动对象检查，确保取消后的 Resolver 和 Timer 已经排空。 */
static void testTlsDialFutureEdgeDestroyEngine(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(!xrtDeadlineExpired(Deadline),
			"TLS Dial Future retained an Engine resource");
		xrtThreadYield();
	}
}



/* 覆盖连接耗尽、Future 取消、跨阶段总超时和入口参数边界。 */
int main(void)
{
	test_tls_dial_future_edge Failure;
	test_tls_dial_future_edge Cancel;
	test_tls_dial_future_edge Timeout;
	xnetengineconfig EngineConfig;
	xtlsclientconfig ClientConfig;
	xtlsdialconfig DialConfig;
	xtlscontext* pTlsContext;
	xnetengine* pEngine;
	xnetresolver* pFailureResolver;
	xnetresolver* pCancelResolver;
	xnetresolver* pTimeoutResolver;
	xfuture* pFailure;
	xfuture* pCancel;
	xfuture* pTimeout;
	const xerror* pError;
	uint16 iPort;

	memset(&Failure, 0, sizeof(Failure));
	memset(&Cancel, 0, sizeof(Cancel));
	memset(&Timeout, 0, sizeof(Timeout));
	pTlsContext = testTlsServerContext();
	testRequire(pTlsContext != NULL,
		"TLS Dial Future edge context creation failed");
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pTlsContext;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_TLS_DIAL_FUTURE_BACKEND;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"TLS Dial Future edge engine start failed");
	pFailureResolver = testTlsDialFutureEdgeResolver(&Failure);
	pCancelResolver = testTlsDialFutureEdgeResolver(&Cancel);
	pTimeoutResolver = testTlsDialFutureEdgeResolver(&Timeout);
	testRequire((pFailureResolver != NULL) &&
		(pCancelResolver != NULL) &&
		(pTimeoutResolver != NULL),
		"TLS Dial Future edge resolver creation failed");
	iPort = testTlsDialFutureEdgeUnusedPort();

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.MaxAttempts = 1u;
	DialConfig.Transport.Stream.ConnectTimeout = UINT64_C(1000000);
	pFailure = xrtTlsDialAsync(
		pEngine,
		pFailureResolver,
		"failure.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL
	);
	testTlsDialFutureEdgeState(
		pFailure,
		XFUTURE_FAILED,
		"TLS Dial Future exhaustion terminal mismatch"
	);
	pError = xrtFutureError(pFailure);
	testRequire((pError != NULL) &&
		((xrtErrorKind(pError) == XERR_IO) ||
		 (xrtErrorKind(pError) == XERR_TIMEOUT)) &&
		(xrtErrorCause(pError) != NULL),
		"TLS Dial Future exhaustion error chain mismatch");

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.Timeout = 0;
	pCancel = xrtTlsDialAsync(
		pEngine,
		pCancelResolver,
		"slow-cancel.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL
	);
	testRequire(pCancel != NULL,
		"TLS Dial Future cancellation submit failed");
	testTlsDialFutureEdgeWait(
		&Cancel.Entered,
		1u,
		"TLS Dial Future cancellation lookup did not start"
	);
	testRequire(xrtFutureCancel(pCancel),
		"TLS Dial Future cancellation request failed");
	testTlsDialFutureEdgeState(
		pCancel,
		XFUTURE_CANCELLED,
		"TLS Dial Future cancellation was not confirmed"
	);
	xrtAtomic32Store(&Cancel.Gate, 1, XMEMORY_RELEASE);

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.Family = XNET_FAMILY_IPV4;
	DialConfig.Transport.Timeout = 0;
	DialConfig.Timeout = UINT64_C(20000);
	pTimeout = xrtTlsDialAsync(
		pEngine,
		pTimeoutResolver,
		"slow-timeout.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL
	);
	testRequire(pTimeout != NULL,
		"TLS Dial Future timeout submit failed");
	testTlsDialFutureEdgeWait(
		&Timeout.Entered,
		1u,
		"TLS Dial Future timeout lookup did not start"
	);
	testTlsDialFutureEdgeState(
		pTimeout,
		XFUTURE_FAILED,
		"TLS Dial Future total timeout terminal mismatch"
	);
	pError = xrtFutureError(pTimeout);
	testRequire((pError != NULL) &&
		(xrtErrorKind(pError) == XERR_TIMEOUT) &&
		(xrtErrorCause(pError) != NULL),
		"TLS Dial Future timeout error chain mismatch");
	xrtAtomic32Store(&Timeout.Gate, 1, XMEMORY_RELEASE);

	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Transport.MaxAttempts = 0;
	xrtClearError();
	testRequire((xrtTlsDialAsync(
		pEngine,
		pFailureResolver,
		"invalid.tls.test",
		iPort,
		&ClientConfig,
		&DialConfig,
		NULL,
		NULL
	) == NULL) && (xrtGetError() != NULL),
		"TLS Dial Future invalid attempt limit was accepted");
	xrtClearError();
	testRequire((xrtTlsDialAsync(
		pEngine,
		pFailureResolver,
		"invalid.tls.test",
		0,
		&ClientConfig,
		NULL,
		NULL,
		NULL
	) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"TLS Dial Future zero port was accepted");
	xrtClearError();

	xrtFutureDestroy(pFailure);
	xrtFutureDestroy(pCancel);
	xrtFutureDestroy(pTimeout);
	testRequire(xrtNetResolverDestroy(pFailureResolver) &&
		xrtNetResolverDestroy(pCancelResolver) &&
		xrtNetResolverDestroy(pTimeoutResolver),
		"TLS Dial Future edge resolver destroy failed");
	testTlsDialFutureEdgeDestroyEngine(pEngine);
	xrtTlsContextRelease(pTlsContext);
	printf("[PASS] TLS Dial Future edge terminals\n");
	return 0;
}
