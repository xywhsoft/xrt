#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_SERVER_TLS_BACKEND)
	#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "select"
#endif



/* 保存两个失败握手、Server 事件顺序和最终统计。 */
typedef struct test_http_server_tls_handshake {
	xhttpserver* Server;
	xatomic32 HttpOpened;
	xatomic32 Requested;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 TimeoutErrors;
	xatomic32 ProtocolErrors;
	xatomic32 Shutdown;
} test_http_server_tls_handshake;



/* 保存一个原始 TCP 客户端及其握手攻击模式。 */
typedef struct test_http_server_tls_handshake_client {
	xatomic32 Opened;
	xatomic32 Closed;
	bool Malformed;
} test_http_server_tls_handshake_client;



/* 在测试截止时间前等待一个原子计数达到目标值。 */
static void testHttpServerTlsHandshakeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 等待 Close 回调返回后的 Server 连接统计完成归还。 */
static void testHttpServerTlsHandshakeWaitStats(
	xhttpserver* pServer,
	xhttpserverstats* pStats
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	for ( ;; ) {
		testRequire(
			xrtHttpServerStats(pServer, pStats),
			"HTTPS handshake statistics query failed"
		);
		if ( pStats->Connections == 0 ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTPS handshake statistics did not settle"
		);
		xrtThreadYield();
	}
}



/* 失败握手绝不能进入 HTTP Open。 */
static void testHttpServerTlsHandshakeOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	test_http_server_tls_handshake* pTest =
		(test_http_server_tls_handshake*)pData;

	(void)pServer;
	(void)pConnection;
	(void)xrtAtomic32FetchAdd(
		&pTest->HttpOpened,
		1,
		XMEMORY_RELEASE
	);
}



/* 失败握手绝不能进入 HTTP Request。 */
static void testHttpServerTlsHandshakeRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_tls_handshake* pTest =
		(test_http_server_tls_handshake*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)xrtAtomic32FetchAdd(
		&pTest->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证握手失败提升为稳定 Server TLS 错误并保留底层原因。 */
static void testHttpServerTlsHandshakeError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_handshake* pTest =
		(test_http_server_tls_handshake*)pData;
	const xerror* pCause;
	xerrkind Kind;

	testRequire(
		(pServer == pTest->Server) &&
		(pConnection != NULL) &&
		xrtHttpConnSecure(pConnection) &&
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.server"
		 ) == 0) &&
		(xrtErrorCode(pError) == XHTTP_SERVER_ERROR_TLS) &&
		(strcmp(
			xrtErrorOperation(pError),
			"run-http-connection"
		 ) == 0),
		"HTTPS handshake error promotion mismatch"
	);
	pCause = xrtErrorCause(pError);
	testRequire(
		pCause != NULL,
		"HTTPS handshake error lost its TLS cause"
	);
	Kind = xrtErrorKind(pCause);
	testRequire(
		(Kind == XERR_TIMEOUT) ||
		(Kind == XERR_PROTOCOL),
		"HTTPS handshake cause kind mismatch"
	);
	if ( Kind == XERR_TIMEOUT ) {
		(void)xrtAtomic32FetchAdd(
			&pTest->TimeoutErrors,
			1,
			XMEMORY_RELEASE
		);
	} else {
		(void)xrtAtomic32FetchAdd(
			&pTest->ProtocolErrors,
			1,
			XMEMORY_RELEASE
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pTest->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Error 先于唯一 Close 发布且 Close 观察同一稳定错误。 */
static void testHttpServerTlsHandshakeClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_handshake* pTest =
		(test_http_server_tls_handshake*)pData;
	uint32 iClosed = xrtAtomic32Load(
		&pTest->Closed,
		XMEMORY_ACQUIRE
	);

	(void)Result;
	testRequire(
		(pServer == pTest->Server) &&
		(pConnection != NULL) &&
		(pError != NULL) &&
		(xrtErrorCode(pError) == XHTTP_SERVER_ERROR_TLS) &&
		(xrtAtomic32Load(
			&pTest->Errors,
			XMEMORY_ACQUIRE
		 ) > iClosed),
		"HTTPS handshake Error and Close ordering mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 已经排空 Listener 和全部失败连接。 */
static void testHttpServerTlsHandshakeShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_tls_handshake* pTest =
		(test_http_server_tls_handshake*)pData;

	testRequire(
		(pServer == pTest->Server) &&
		(xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED),
		"HTTPS handshake shutdown mismatch"
	);
	xrtAtomic32Store(
		&pTest->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 原始 TCP 打开后可选择静默等待，或发送明文触发 TLS 协议错误。 */
static void testHttpServerTlsHandshakeClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	static const char sMalformed[] =
		"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
	test_http_server_tls_handshake_client* pClient =
		(test_http_server_tls_handshake_client*)pData;

	if ( pClient->Malformed ) {
		testRequire(
			xrtNetStreamSend(
				pStream,
				sMalformed,
				sizeof(sMalformed) - 1u
			) == XNET_RESULT_OK,
			"HTTPS malformed handshake send failed"
		);
	}
	xrtAtomic32Store(
		&pClient->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* TLS 告警正常结束对端写方向后，回送本地关闭以完成 TCP 终态。 */
static void testHttpServerTlsHandshakeClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtNetStreamClose(pStream),
		"HTTPS malformed client close after End failed"
	);
}



/* 记录原始 TCP 客户端已经观察到服务端终止。 */
static void testHttpServerTlsHandshakeClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_handshake_client* pClient =
		(test_http_server_tls_handshake_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pClient->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 建立一个不会执行 TLS 客户端握手的原始 TCP 连接。 */
static xnetstream* testHttpServerTlsHandshakeConnect(
	xnetengine* pEngine,
	const xnetaddr* pAddress,
	test_http_server_tls_handshake_client* pClient
)
{
	xnetstreamconfig Config;
	xnetstreamevents Events;

	xrtNetStreamConfigInit(&Config);
	memset(&Events, 0, sizeof(Events));
	Events.Open = testHttpServerTlsHandshakeClientOpen;
	Events.End = testHttpServerTlsHandshakeClientEnd;
	Events.Close = testHttpServerTlsHandshakeClientClose;
	return xrtNetStreamConnect(
		pEngine,
		pAddress,
		1u,
		&Config,
		&Events,
		pClient
	);
}



/* 验证握手超时、非法首记录、错误顺序、统计和排空边界。 */
int main(void)
{
	test_http_server_tls_handshake Test;
	test_http_server_tls_handshake_client Silent;
	test_http_server_tls_handshake_client Malformed;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpserverevents ServerEvents;
	xtlscontext* pContext;
	xtlsidentity* pIdentity;
	xnetengine* pEngine;
	xnetstream* pSilent;
	xnetstream* pMalformed;
	xhttpserverstats Stats;
	xnetaddr Address;

	memset(&Test, 0, sizeof(Test));
	memset(&Silent, 0, sizeof(Silent));
	memset(&Malformed, 0, sizeof(Malformed));
	Malformed.Malformed = true;
	pContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pContext != NULL) && (pIdentity != NULL),
		"HTTPS handshake fixtures failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_TLS_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTPS handshake Engine start failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS handshake address setup failed"
	);
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pContext;
	TlsConfig.Handshake.Identity = pIdentity;
	TlsConfig.Stream.HandshakeTimeout = UINT64_C(100000);
	TlsConfig.Stream.CloseTimeout = UINT64_C(100000);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Open = testHttpServerTlsHandshakeOpen;
	ServerEvents.Request = testHttpServerTlsHandshakeRequest;
	ServerEvents.Error = testHttpServerTlsHandshakeError;
	ServerEvents.Close = testHttpServerTlsHandshakeClose;
	ServerEvents.Shutdown = testHttpServerTlsHandshakeShutdown;
	ServerEvents.Data = &Test;
	Test.Server = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		&ServerEvents
	);
	testRequire(
		(Test.Server != NULL) &&
		xrtHttpServerLocal(Test.Server, 0, &Address),
		"HTTPS handshake Server start failed"
	);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pContext);

	pSilent = testHttpServerTlsHandshakeConnect(
		pEngine,
		&Address,
		&Silent
	);
	testRequire(
		pSilent != NULL,
		"HTTPS silent client creation failed"
	);
	testHttpServerTlsHandshakeWait(
		&Silent.Opened,
		1u,
		"HTTPS silent client did not open"
	);
	testHttpServerTlsHandshakeWait(
		&Test.Errors,
		1u,
		"HTTPS handshake timeout Error missing"
	);
	testHttpServerTlsHandshakeWait(
		&Test.Closed,
		1u,
		"HTTPS handshake timeout Close missing"
	);
	testHttpServerTlsHandshakeWait(
		&Silent.Closed,
		1u,
		"HTTPS silent client did not close"
	);
	testHttpServerTlsHandshakeWaitStats(Test.Server, &Stats);
	testRequire(
		(Stats.Accepted == 1u) &&
		(Stats.Connections == 0u) &&
		(Stats.Timeouts == 1u),
		"HTTPS handshake timeout statistics mismatch"
	);

	pMalformed = testHttpServerTlsHandshakeConnect(
		pEngine,
		&Address,
		&Malformed
	);
	testRequire(
		pMalformed != NULL,
		"HTTPS malformed client creation failed"
	);
	testHttpServerTlsHandshakeWait(
		&Malformed.Opened,
		1u,
		"HTTPS malformed client did not open"
	);
	testHttpServerTlsHandshakeWait(
		&Test.Errors,
		2u,
		"HTTPS malformed handshake Error missing"
	);
	testHttpServerTlsHandshakeWait(
		&Test.Closed,
		2u,
		"HTTPS malformed handshake Close missing"
	);
	testHttpServerTlsHandshakeWait(
		&Malformed.Closed,
		1u,
		"HTTPS malformed client did not close"
	);
	testHttpServerTlsHandshakeWaitStats(Test.Server, &Stats);
	testRequire(
		(xrtAtomic32Load(
			&Test.HttpOpened,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.Requested,
			XMEMORY_ACQUIRE
		 ) == 0) &&
		(xrtAtomic32Load(
			&Test.TimeoutErrors,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(xrtAtomic32Load(
			&Test.ProtocolErrors,
			XMEMORY_ACQUIRE
		 ) == 1u) &&
		(Stats.Accepted == 2u) &&
		(Stats.Rejected == 0u) &&
		(Stats.Requests == 0u) &&
		(Stats.ProtocolErrors == 0u) &&
		(Stats.Timeouts == 1u) &&
		(Stats.Connections == 0u),
		"HTTPS failed handshake contract mismatch"
	);

	testRequire(
		xrtHttpServerDrain(Test.Server),
		"HTTPS handshake Server drain failed"
	);
	testHttpServerTlsHandshakeWait(
		&Test.Shutdown,
		1u,
		"HTTPS handshake Server shutdown missing"
	);
	xrtNetStreamDestroy(pMalformed);
	xrtNetStreamDestroy(pSilent);
	xrtHttpServerDestroy(Test.Server);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTPS handshake Engine destroy failed"
	);
	printf(
		"[PASS] HTTPS server handshake failures (%s)\n",
		TEST_HTTP_SERVER_TLS_BACKEND_NAME
	);
	return 0;
}
