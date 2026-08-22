#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_SERVER_TLS_BACKEND)
	#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "select"
#endif



/* HTTPS 慢读夹具记录排空边界、写超时与双方终态。 */
typedef struct test_http_server_tls_write {
	xbytesview Body;
	xatomic32 Requested;
	xatomic32 Readable;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 ClientClosed;
	xatomic32 Shutdown;
	xerrkind ErrorKind;
	int32 ErrorCode;
	char ErrorOperation[64];
} test_http_server_tls_write;



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerTlsWriteWait(
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



/* 提交足以压满 TLS 明文、TCP 接收和服务端发送预算的大响应。 */
static void testHttpServerTlsWriteRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;

	(void)pServer;
	testRequire(
		xrtHttpServerRequestTarget(pRequest).Size == 6u,
		"HTTPS slow-reader request target mismatch"
	);
	testRequire(
		xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("application/octet-stream"),
			pState->Body
		) == XNET_RESULT_OK,
		"HTTPS large response submission failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存唯一写超时的结构化分类。 */
static void testHttpServerTlsWriteError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;
	cstr sOperation;
	size_t iSize;

	(void)pServer;
	(void)pConnection;
	testRequire(
		pError != NULL,
		"HTTPS slow-reader error is null"
	);
	sOperation = xrtErrorOperation(pError);
	iSize = strlen(sOperation);
	testRequire(
		iSize < sizeof(pState->ErrorOperation),
		"HTTPS slow-reader operation is too long"
	);
	pState->ErrorKind = xrtErrorKind(pError);
	pState->ErrorCode = xrtErrorCode(pError);
	memcpy(pState->ErrorOperation, sOperation, iSize + 1u);
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证服务端只以写超时关闭一次连接。 */
static void testHttpServerTlsWriteClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;
	xhttpserverstats Stats;

	(void)pConnection;
	(void)Result;
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) ==
		 XHTTP_SERVER_ERROR_TIMEOUT_WRITE),
		"HTTPS slow-reader close cause mismatch"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Connections == 0u),
		"HTTPS close callback observed a stale connection count"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录监听器和全部 HTTPS 连接均已退出。 */
static void testHttpServerTlsWriteShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTPS slow-reader shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 握手完成后缩小内核接收缓冲并发送关闭连接的短请求。 */
static void testHttpServerTlsWriteClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	static const char sRequest[] =
		"GET /large HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n";
	xnetstream* pTransport = xrtTlsStreamTransport(pStream);
	size_t iWritten = 0;

	(void)pData;
	testRequire(
		(pTransport != NULL) &&
		xrtNetSocketSet(
			xrtNetStreamSocket(pTransport),
			XNET_OPTION_RECEIVE_BUFFER,
			1024
		),
		"HTTPS slow-reader receive buffer setup failed"
	);
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			sRequest,
			sizeof(sRequest) - 1u,
			&iWritten
		 ) == XTLS_OK) &&
		(iWritten == (sizeof(sRequest) - 1u)),
		"HTTPS slow-reader request send failed"
	);
}



/*
 * 故意不消费明文，使 TLS PlainLimit、内核接收窗口和服务端发送预算
 * 依次形成背压。
 */
static void testHttpServerTlsWriteClientRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;

	(void)pBuffer;
	testRequire(
		xrtTlsStreamAvailable(pStream) != 0,
		"HTTPS slow-reader received an empty plaintext edge"
	);
	xrtAtomic32Store(
		&pState->Readable,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录服务端中止传输后客户端 TLS Stream 的终态。 */
static void testHttpServerTlsWriteClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls_write* pState =
		(test_http_server_tls_write*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pState->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 HTTPS 响应必须在组合 Drain 后完成且慢读仍受写超时约束。 */
int main(void)
{
	test_http_server_tls_write State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpserverevents ServerEvents;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ClientEvents;
	xtlsverifierconfig VerifierConfig;
	xtlslimits ClientLimits;
	xtlscontext* pServerContext;
	xtlscontext* pClientContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xtlsstream* pClient;
	xhttpserverstats Stats;
	xnetaddr Address;
	bytes pBody;
	size_t iBodySize = 8u * 1024u * 1024u;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Readable, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	pBody = (bytes)xrtMalloc(iBodySize);
	testRequire(
		pBody != NULL,
		"HTTPS slow-reader body allocation failed"
	);
	memset(pBody, 'T', iBodySize);
	State.Body = (xbytesview){ pBody, iBodySize };

	pServerContext = testTlsServerContext();
	xrtTlsLimitsInit(&ClientLimits);
	ClientLimits.PlainLimit = XTLS_RECORD_PLAINTEXT_MAX;
	pClientContext = testTlsServerContextWithLimits(
		&ClientLimits
	);
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pServerContext != NULL) &&
		(pClientContext != NULL) &&
		(pIdentity != NULL),
		"HTTPS slow-reader TLS fixture setup failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS slow-reader verifier creation failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_TLS_BACKEND;
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTPS slow-reader engine start failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS slow-reader address setup failed"
	);
	ServerConfig.Network.Listen.Stream.WriteHighWater =
		128u * 1024u;
	ServerConfig.Network.Listen.Stream.WriteLowWater =
		64u * 1024u;
	ServerConfig.Network.Listen.Stream.WriteLimit =
		XTLS_SEND_LIMIT_DEFAULT;
	ServerConfig.WriteSize = 16u * 1024u;
	ServerConfig.WriteTimeout = UINT64_C(500000);
	ServerConfig.RequestTimeout = UINT64_C(2000000);
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pServerContext;
	TlsConfig.Handshake.Identity = pIdentity;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testHttpServerTlsWriteRequest;
	ServerEvents.Error = testHttpServerTlsWriteError;
	ServerEvents.Close = testHttpServerTlsWriteClose;
	ServerEvents.Shutdown = testHttpServerTlsWriteShutdown;
	ServerEvents.Data = &State;
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		&ServerEvents
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTPS slow-reader server start failed"
	);
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pServerContext);

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pClientContext;
	ClientConfig.ServerName =
		XRT_STR_LITERAL("example.com");
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&StreamConfig);
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpServerTlsWriteClientOpen;
	ClientEvents.Read = testHttpServerTlsWriteClientRead;
	ClientEvents.Close = testHttpServerTlsWriteClientClose;
	pClient = xrtTlsStreamConnect(
		pEngine,
		&Address,
		1u,
		NULL,
		&ClientConfig,
		&StreamConfig,
		&ClientEvents,
		&State
	);
	testRequire(
		pClient != NULL,
		"HTTPS slow-reader client connect failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pClientContext);

	testHttpServerTlsWriteWait(
		&State.Requested,
		1,
		"HTTPS slow-reader request callback missing"
	);
	testHttpServerTlsWriteWait(
		&State.Readable,
		1,
		"HTTPS slow-reader plaintext edge missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Requests == 1u) &&
		(Stats.Responses == 0u) &&
		(Stats.Connections == 1u),
		"HTTPS response completed before TLS Drain"
	);

	testHttpServerTlsWriteWait(
		&State.Errors,
		1,
		"HTTPS slow-reader write timeout missing"
	);
	testHttpServerTlsWriteWait(
		&State.Closed,
		1,
		"HTTPS slow-reader connection did not close"
	);
	testRequire(
		xrtTlsStreamAbort(pClient),
		"HTTPS slow-reader client abort failed"
	);
	testHttpServerTlsWriteWait(
		&State.ClientClosed,
		1,
		"HTTPS slow-reader client did not close"
	);
	testRequire(
		(State.ErrorKind == XERR_TIMEOUT) &&
		(State.ErrorCode ==
		 XHTTP_SERVER_ERROR_TIMEOUT_WRITE) &&
		(strcmp(
			State.ErrorOperation,
			"write-http-response"
		 ) == 0),
		"HTTPS slow-reader error contract mismatch"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 1u) &&
		(Stats.Requests == 1u) &&
		(Stats.Responses == 0u) &&
		(Stats.ProtocolErrors == 0u) &&
		(Stats.Timeouts == 1u) &&
		(Stats.Connections == 0u),
		"HTTPS slow-reader statistics mismatch"
	);

	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTPS slow-reader server drain failed"
	);
	testHttpServerTlsWriteWait(
		&State.Shutdown,
		1,
		"HTTPS slow-reader shutdown missing"
	);
	xrtTlsStreamDestroy(pClient);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTPS slow-reader engine destroy failed"
	);
	xrtFree(pBody);
	printf(
		"[PASS] HTTPS slow-reader write timeout (%s)\n",
		TEST_HTTP_SERVER_TLS_BACKEND_NAME
	);
	return 0;
}
