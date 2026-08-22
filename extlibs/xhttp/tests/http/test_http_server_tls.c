#include "../fixtures/tls_server.h"



#if !defined(TEST_HTTP_SERVER_TLS_BACKEND)
	#define TEST_HTTP_SERVER_TLS_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_SERVER_TLS_BACKEND_NAME "select"
#endif



/* HTTPS 夹具保存跨 Worker 生命周期和响应字节。 */
typedef struct test_http_server_tls {
	xnetengine* Engine;
	xhttpserver* Server;
	xtlsstream* Client;
	xatomic32 Opened;
	xatomic32 Requested;
	xatomic32 ConnectionClosed;
	xatomic32 ClientClosed;
	xatomic32 Shutdown;
	char Response[4096];
	size_t ResponseSize;
} test_http_server_tls;



/* 在截止时间前等待一个跨 Worker 终态。 */
static void testHttpServerTlsWait(
	xatomic32* pValue,
	cstr sMessage
)
{
	xdeadline iDeadline = xrtDeadlineAfter(10000000u);

	while ( xrtAtomic32Load(
		pValue,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(iDeadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 判断字符串视图是否等于一个零结尾字面量。 */
static bool testHttpServerTlsViewEqual(
	xstrview View,
	cstr sText
)
{
	size_t iSize = strlen(sText);

	return (View.Size == iSize) &&
		(memcmp(View.Data, sText, iSize) == 0);
}



/* TLS 握手完成时验证传输逃生口和协商结果。 */
static void testHttpServerTlsOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;
	xtlsstream* pTls = xrtHttpConnTls(pConnection);
	xbytesview Protocol;
	xhttpconnstats Stats;

	testRequire(
		(pServer == pState->Server) &&
		xrtHttpServerSecure(pServer) &&
		xrtHttpConnSecure(pConnection) &&
		(pTls != NULL) &&
		(xrtHttpConnTcp(pConnection) ==
		 xrtTlsStreamTransport(pTls)) &&
		xrtTlsSessionProtocol(
			xrtTlsStreamSession(pTls),
			&Protocol
		) &&
		testTlsServerViewEqual(
			Protocol,
			XRT_BYTES_LITERAL("http/1.1")
		) &&
		xrtHttpConnStats(pConnection, &Stats) &&
		Stats.Secure,
		"HTTPS server Open contract mismatch"
	);
	xrtAtomic32Store(
		&pState->Opened,
		1,
		XMEMORY_RELEASE
	);
}



/* 依次响应两条流水线请求，第二条由请求头触发连接关闭。 */
static void testHttpServerTlsRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	uint32 iRequest = xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_ACQ_REL
	);

	(void)pServer;
	if ( iRequest == 0 ) {
		testRequire(
			testHttpServerTlsViewEqual(Target, "/one") &&
			(xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("one")
			 ) == XNET_RESULT_OK),
			"HTTPS first response failed"
		);
	} else {
		testRequire(
			(iRequest == 1u) &&
			testHttpServerTlsViewEqual(Target, "/two") &&
			(xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL(
					"application/json; charset=utf-8"
				),
				XRT_BYTES_LITERAL(
					"{\"code\":200,\"msg\":\"OK\"}"
				)
			 ) == XNET_RESULT_OK),
			"HTTPS second response failed"
		);
	}
}



/* 验证 HTTPS Connection 通过认证关闭正常退出。 */
static void testHttpServerTlsConnectionClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;

	testRequire(
		(pServer == pState->Server) &&
		xrtHttpConnSecure(pConnection) &&
		(Result == XNET_RESULT_OK) &&
		(pError == NULL),
		"HTTPS server Connection close mismatch"
	);
	xrtAtomic32Store(
		&pState->ConnectionClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* HTTPS Server 不应在正常测试路径发布错误。 */
static void testHttpServerTlsError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)pData;
	testRequire(false, "HTTPS server emitted an unexpected error");
}



/* 记录 Listener 和全部 Connection 已经退出。 */
static void testHttpServerTlsShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;

	testRequire(
		pServer == pState->Server,
		"HTTPS server Shutdown object mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 客户端握手完成后发送两条流水线 HTTP/1.1 请求。 */
static void testHttpServerTlsClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	static const char Request[] =
		"GET /one HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"\r\n"
		"GET /two HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Connection: close\r\n"
		"\r\n";
	size_t iWritten = 0;

	(void)pData;
	testRequire(
		(xrtTlsStreamSend(
			pStream,
			Request,
			sizeof(Request) - 1u,
			&iWritten
		 ) == XTLS_OK) &&
		(iWritten == (sizeof(Request) - 1u)),
		"HTTPS client request send failed"
	);
}



/* 客户端复制并消费服务端返回的全部 HTTP 明文。 */
static void testHttpServerTlsClientRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;
	size_t iAvailable = xrtTlsStreamAvailable(pStream);

	testRequire(
		(iAvailable != 0) &&
		(iAvailable <=
		 (sizeof(pState->Response) -
		  pState->ResponseSize)) &&
		(xrtNetBufPeek(
			pBuffer,
			0,
			pState->Response + pState->ResponseSize,
			iAvailable
		 ) == iAvailable) &&
		xrtTlsStreamConsume(pStream, iAvailable),
		"HTTPS client response consume failed"
	);
	pState->ResponseSize += iAvailable;
}



/* 收到服务端 close_notify 后回送认证关闭。 */
static void testHttpServerTlsClientEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS client close_notify failed"
	);
}



/* 记录客户端 TLS Stream 已经完成认证关闭。 */
static void testHttpServerTlsClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_tls* pState =
		(test_http_server_tls*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED),
		"HTTPS client TLS close mismatch"
	);
	xrtAtomic32Store(
		&pState->ClientClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在响应字节中统计一个固定标记。 */
static size_t testHttpServerTlsCount(
	const char* pData,
	size_t iSize,
	cstr sNeedle
)
{
	size_t iNeedle = strlen(sNeedle);
	size_t iCount = 0;
	size_t i;

	if ( iNeedle > iSize ) {
		return 0;
	}
	for ( i = 0; i <= (iSize - iNeedle); i++ ) {
		if ( memcmp(pData + i, sNeedle, iNeedle) == 0 ) {
			iCount++;
		}
	}
	return iCount;
}



/* 验证 HTTPS 启动、配置快照、流水线、短块输出和认证关闭。 */
int main(void)
{
	static const xstrview ClientProtocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_server_tls State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpserverevents ServerEvents;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ClientEvents;
	xtlsverifierconfig VerifierConfig;
	xtlscontext* pServerContext;
	xtlscontext* pClientContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xhttpserverstats ServerStats;
	xnetaddr Address;
	char sProtocol[] = "http/1.1";
	xstrview Protocol = {
		sProtocol,
		sizeof(sProtocol) - 1u
	};

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Opened, 0);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.ConnectionClosed, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	pServerContext = testTlsServerContext();
	pClientContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	testRequire(
		(pServerContext != NULL) &&
		(pClientContext != NULL) &&
		(pIdentity != NULL),
		"HTTPS fixture TLS objects failed"
	);
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		pVerifier != NULL,
		"HTTPS client verifier creation failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_TLS_BACKEND;
	EngineConfig.Workers = 2u;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS Engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	), "HTTPS loopback address failed");
	ServerConfig.Network.Listen.AcceptConcurrency = 4u;
	ServerConfig.Network.Listen.Stream.ReadSize = 7u;
	ServerConfig.WriteSize = 7u;
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pServerContext;
	TlsConfig.Handshake.Identity = pIdentity;
	TlsConfig.Handshake.Protocols = &Protocol;
	TlsConfig.Handshake.ProtocolCount = 1u;
	TlsConfig.Handshake.RequireProtocol = true;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Open = testHttpServerTlsOpen;
	ServerEvents.Request = testHttpServerTlsRequest;
	ServerEvents.Close = testHttpServerTlsConnectionClose;
	ServerEvents.Error = testHttpServerTlsError;
	ServerEvents.Shutdown = testHttpServerTlsShutdown;
	ServerEvents.Data = &State;
	State.Server = xrtHttpServerStartTls(
		State.Engine,
		&ServerConfig,
		&TlsConfig,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerSecure(State.Server) &&
		xrtHttpServerLocal(State.Server, 0, &Address) &&
		xrtHttpServerStats(State.Server, &ServerStats) &&
		ServerStats.Secure,
		"HTTPS Server start failed"
	);

	/*
	 * Server 必须已经保留 Context、Identity 并深复制 ALPN；
	 * 释放和改写调用方配置后，新连接仍应完成握手。
	 */
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pServerContext);
	memcpy(sProtocol, "invalid!", sizeof(sProtocol) - 1u);

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pClientContext;
	ClientConfig.ServerName =
		XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = ClientProtocols;
	ClientConfig.ProtocolCount =
		sizeof(ClientProtocols) /
		sizeof(ClientProtocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&StreamConfig);
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpServerTlsClientOpen;
	ClientEvents.Read = testHttpServerTlsClientRead;
	ClientEvents.End = testHttpServerTlsClientEnd;
	ClientEvents.Close = testHttpServerTlsClientClose;
	State.Client = xrtTlsStreamConnect(
		State.Engine,
		&Address,
		1u,
		NULL,
		&ClientConfig,
		&StreamConfig,
		&ClientEvents,
		&State
	);
	testRequire(
		State.Client != NULL,
		"HTTPS client connect failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pClientContext);

	testHttpServerTlsWait(
		&State.Opened,
		"HTTPS server did not publish Open"
	);
	testHttpServerTlsWait(
		&State.ConnectionClosed,
		"HTTPS server Connection did not close"
	);
	testHttpServerTlsWait(
		&State.ClientClosed,
		"HTTPS client TLS Stream did not close"
	);
	testRequire(
		(xrtAtomic32Load(
			&State.Requested,
			XMEMORY_ACQUIRE
		 ) == 2u) &&
		(testHttpServerTlsCount(
			State.Response,
			State.ResponseSize,
			"HTTP/1.1 200"
		 ) == 2u) &&
		(testHttpServerTlsCount(
			State.Response,
			State.ResponseSize,
			"one"
		 ) == 1u) &&
		(testHttpServerTlsCount(
			State.Response,
			State.ResponseSize,
			"{\"code\":200,\"msg\":\"OK\"}"
		 ) == 1u),
		"HTTPS pipelined response mismatch"
	);

	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTPS Server drain failed"
	);
	testHttpServerTlsWait(
		&State.Shutdown,
		"HTTPS Server did not shut down"
	);
	xrtTlsStreamDestroy(State.Client);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS Engine destroy failed"
	);
	printf(
		"[PASS] HTTPS server runtime (%s)\n",
		TEST_HTTP_SERVER_TLS_BACKEND_NAME
	);
	return 0;
}
