#ifndef TEST_WS_SERVER_ROUTER_TLS
	#define TEST_WS_SERVER_ROUTER_TLS 0
#endif

#ifndef TEST_WS_SERVER_ROUTER_NAME
	#define TEST_WS_SERVER_ROUTER_NAME "select"
#endif

#if TEST_WS_SERVER_ROUTER_TLS
	#include "../fixtures/tls_server.h"
#else
	#include "../test.h"
#endif



typedef struct test_ws_server_router {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpclient* Client;
	xhttpcall* Call;
	xhttpcall* CallbackCall;
	xhttpresponse* Response;
	xatomicptr ServerConnection;
	xatomicptr ClientConnection;
	xatomic32 Opened;
	xatomic32 ServerMessages;
	xatomic32 ClientMessages;
	xatomic32 ServerClosed;
	xatomic32 ClientClosed;
	xatomic32 HandshakeErrors;
	xatomic32 ServerErrors;
	xatomic32 Shutdowns;
	xatomic32 Releases;
	char ServerMessage[32];
	size_t ServerSize;
	char ClientMessage[32];
	size_t ClientSize;
} test_ws_server_router;



#if !TEST_WS_SERVER_ROUTER_TLS

typedef struct test_ws_server_router_http {
	xnetstream* Stream;
	cstr Request;
	char Response[2048];
	size_t Size;
	xatomic32 Closed;
} test_ws_server_router_http;

#endif



/* 在固定截止时间前等待异步状态发布。 */
static void testWsServerRouterWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pValue, XMEMORY_ACQUIRE
	) < iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 固定路由 Open 借用连接，测试显式保留一个调用方引用。 */
static void testWsServerRouterOpen(
	xhttpconn* pHttp,
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;
	xstrview Protocol = xrtWsConnProtocol(pConnection);

	testRequire(
		(pHttp != NULL) &&
		(pConnection != NULL) &&
		(xrtHttpConnSecure(pHttp) ==
		 (TEST_WS_SERVER_ROUTER_TLS != 0)) &&
		#if TEST_WS_SERVER_ROUTER_TLS
			(xrtWsConnTls(pConnection) != NULL) &&
			(xrtWsConnTcp(pConnection) == NULL) &&
		#else
			(xrtWsConnTcp(pConnection) != NULL) &&
		#endif
		(Protocol.Size == 7u) &&
		(memcmp(Protocol.Data, "chat.v1", 7u) == 0),
		"WebSocket Router Open or copied protocol mismatch"
	);
	xrtAtomicPtrStore(
		&pTest->ServerConnection,
		xrtWsConnRef(pConnection),
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pTest->Opened, 1u, XMEMORY_RELEASE
	);
}



/* 记录同步握手拒绝，并验证错误仍来自底层握手域。 */
static void testWsServerRouterHandshakeError(
	xhttpconn* pHttp,
	const xerror* pError,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	testRequire(
		(pHttp != NULL) &&
		(pError != NULL) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		 ) == 0),
		"WebSocket Router handshake error mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->HandshakeErrors, 1u, XMEMORY_RELEASE
	);
}



/* 记录 Router 与全部活动 WebSocket 连接退出后的最终清理。 */
static void testWsServerRouterRelease(ptr pData)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)xrtAtomic32FetchAdd(
		&pTest->Releases, 1u, XMEMORY_RELEASE
	);
}



/* 服务端开始接收一条新的逻辑消息。 */
static void testWsServerRouterServerBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_TEXT),
		"WebSocket Router server message type mismatch"
	);
	pTest->ServerSize = 0;
}



/* 服务端连续收集借用的消息片段。 */
static void testWsServerRouterServerData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		Data.Size <=
		(sizeof(pTest->ServerMessage) - pTest->ServerSize),
		"WebSocket Router server message exceeded capacity"
	);
	memcpy(
		pTest->ServerMessage + pTest->ServerSize,
		Data.Data,
		Data.Size
	);
	pTest->ServerSize += Data.Size;
}



/* 服务端验证完整消息并回复一条 Text。 */
static void testWsServerRouterServerEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	testRequire(
		(pTest->ServerSize == 5u) &&
		(memcmp(pTest->ServerMessage, "hello", 5u) == 0) &&
		(xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("world")
		 ) == XNET_RESULT_OK),
		"WebSocket Router server message or reply mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerMessages, 1u, XMEMORY_RELEASE
	);
}



/* 服务端连接关闭后发布生命周期状态。 */
static void testWsServerRouterServerClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		(pClose != NULL) &&
		((pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0),
		"WebSocket Router server Close was not clean"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerClosed, 1u, XMEMORY_RELEASE
	);
}



/* 客户端开始接收服务端回复。 */
static void testWsServerRouterClientBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		(pInfo != NULL) &&
		(pInfo->Opcode == XWS_OPCODE_TEXT),
		"WebSocket Router client message type mismatch"
	);
	pTest->ClientSize = 0;
}



/* 客户端连续收集借用的回复片段。 */
static void testWsServerRouterClientData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		Data.Size <=
		(sizeof(pTest->ClientMessage) - pTest->ClientSize),
		"WebSocket Router client message exceeded capacity"
	);
	memcpy(
		pTest->ClientMessage + pTest->ClientSize,
		Data.Data,
		Data.Size
	);
	pTest->ClientSize += Data.Size;
}



/* 客户端验证完整回复，但把 Close 延后到 HTTP Server 退出以后。 */
static void testWsServerRouterClientEnd(
	xwsconn* pConnection,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		(pTest->ClientSize == 5u) &&
		(memcmp(pTest->ClientMessage, "world", 5u) == 0),
		"WebSocket Router client reply mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ClientMessages, 1u, XMEMORY_RELEASE
	);
}



/* 客户端连接关闭后发布生命周期状态。 */
static void testWsServerRouterClientClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pConnection;
	testRequire(
		(pClose != NULL) &&
		((pClose->Flags & XWS_CONN_CLOSE_CLEAN) != 0),
		"WebSocket Router client Close was not clean"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->ClientClosed, 1u, XMEMORY_RELEASE
	);
}



/* 任一已建立 WebSocket 会话错误都会使回环测试失败。 */
static void testWsServerRouterConnectionError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pConnection;
	(void)pError;
	(void)pData;
	testRequire(false, "WebSocket Router connection reported an error");
}



/* 客户端握手完成后接管 Call、Response 和 Connection，并发送首条消息。 */
static void testWsServerRouterConnected(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	testRequire(
		(pCall != NULL) &&
		(Result == XNET_RESULT_OK) &&
		(pConnection != NULL) &&
		#if TEST_WS_SERVER_ROUTER_TLS
			(xrtWsConnTls(pConnection) != NULL) &&
			(xrtWsConnTcp(pConnection) == NULL) &&
		#else
			(xrtWsConnTcp(pConnection) != NULL) &&
		#endif
		(pResponse != NULL) &&
		(pError == NULL) &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_SWITCHING_PROTOCOLS),
		"WebSocket Router client handshake failed"
	);
	pTest->CallbackCall = pCall;
	pTest->Response = pResponse;
	xrtAtomicPtrStore(
		&pTest->ClientConnection,
		pConnection,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("hello")
		) == XNET_RESULT_OK,
		"WebSocket Router client initial send failed"
	);
}



/* 在客户端所属 Worker 上发起标准关闭握手。 */
static void testWsServerRouterCloseTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;
	xwsconn* pConnection = (xwsconn*)xrtAtomicPtrLoad(
		&pTest->ClientConnection,
		XMEMORY_ACQUIRE
	);

	(void)pWorker;
	testRequire(
		(pConnection != NULL) &&
		(xrtWsConnClose(
			pConnection,
			XWS_CLOSE_NORMAL,
			XRT_STR_LITERAL("done")
		 ) == XNET_RESULT_OK),
		"WebSocket Router client Close submission failed"
	);
}



/* 记录底层 HTTP Server 不应出现的运行错误。 */
static void testWsServerRouterHttpError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pTest->ServerErrors, 1u, XMEMORY_RELEASE
	);
}



/* HTTP Server 完整退出后发布 Shutdown。 */
static void testWsServerRouterShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_server_router* pTest =
		(test_ws_server_router*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"WebSocket Router HTTP shutdown state mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Shutdowns, 1u, XMEMORY_RELEASE
	);
}



#if !TEST_WS_SERVER_ROUTER_TLS

/* 原始 HTTP Client 打开后提交一条要求关闭的请求。 */
static void testWsServerRouterHttpOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_ws_server_router_http* pClient =
		(test_ws_server_router_http*)pData;

	testRequire(
		xrtNetStreamSend(
			pStream,
			pClient->Request,
			strlen(pClient->Request)
		) == XNET_RESULT_OK,
		"WebSocket Router raw HTTP send failed"
	);
}



/* 原始 HTTP Client 连续消费响应，不建立固定网络缓冲。 */
static void testWsServerRouterHttpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_ws_server_router_http* pClient =
		(test_ws_server_router_http*)pData;
	size_t iAvailable = xrtNetBufSize(pBuffer);

	(void)pStream;
	testRequire(
		iAvailable <
		(sizeof(pClient->Response) - pClient->Size),
		"WebSocket Router raw HTTP response exceeded capacity"
	);
	testRequire(
		xrtNetBufRead(
			pBuffer,
			pClient->Response + pClient->Size,
			iAvailable
		) == iAvailable,
		"WebSocket Router raw HTTP consume failed"
	);
	pClient->Size += iAvailable;
	pClient->Response[pClient->Size] = '\0';
}



/* 对端结束写方向后关闭原始 HTTP Client。 */
static void testWsServerRouterHttpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 发布原始 HTTP Client 唯一终态。 */
static void testWsServerRouterHttpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_ws_server_router_http* pClient =
		(test_ws_server_router_http*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pClient->Closed, 1u, XMEMORY_RELEASE
	);
}



/* 执行一条真实 TCP HTTP 请求并返回完整响应。 */
static cstr testWsServerRouterHttpRequest(
	test_ws_server_router* pTest,
	const xnetaddr* pAddress,
	cstr sRequest,
	test_ws_server_router_http* pClient
)
{
	xnetstreamconfig Config;
	xnetstreamevents Events;

	memset(pClient, 0, sizeof(*pClient));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&pClient->Closed, 0);
	pClient->Request = sRequest;
	Events.Open = testWsServerRouterHttpOpen;
	Events.Read = testWsServerRouterHttpRead;
	Events.End = testWsServerRouterHttpEnd;
	Events.Close = testWsServerRouterHttpClose;
	xrtNetStreamConfigInit(&Config);
	Config.ReadSize = 256u;
	Config.ReadLimit = sizeof(pClient->Response);
	Config.WriteHighWater = 1024u;
	Config.WriteLowWater = 512u;
	Config.WriteLimit = 2048u;
	pClient->Stream = xrtNetStreamConnect(
		pTest->Engine,
		pAddress,
		0,
		&Config,
		&Events,
		pClient
	);
	testRequire(
		pClient->Stream != NULL,
		"WebSocket Router raw HTTP connect failed"
	);
	testWsServerRouterWait(
		&pClient->Closed,
		1u,
		"WebSocket Router raw HTTP client did not close"
	);
	xrtNetStreamDestroy(pClient->Stream);
	return pClient->Response;
}

#endif



/* 覆盖真实 Router Upgrade、标准拒绝和跨 Server 生命周期的 Data 持有。 */
int main(void)
{
	#if TEST_WS_SERVER_ROUTER_TLS
		static const xstrview TlsProtocols[] = {
			XRT_STR_INIT("http/1.1")
		};
	#endif
	test_ws_server_router Test;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xwsserverrouteconfig RouteConfig;
	xwsclientconfig WsClientConfig;
	xwsconnevents ClientEvents;
	xhttpserverrouter* pRouter;
	xnetaddr Address;
	#if TEST_WS_SERVER_ROUTER_TLS
		xhttpservertlsconfig ServerTls;
		xtlsverifierconfig VerifierConfig;
		xtlscontext* pTlsContext;
		xtlsidentity* pTlsIdentity;
		xtlsverifier* pTlsVerifier;
	#else
		test_ws_server_router_http Http;
	#endif
	xwsconn* pClient;
	xwsconn* pServer;
	#if !TEST_WS_SERVER_ROUTER_TLS
		cstr sResponse;
	#endif
	char Url[128];
	int iUrl;

	#if TEST_WS_SERVER_ROUTER_TLS
		pTlsContext = testTlsServerContext();
		pTlsIdentity = testTlsServerIdentity();
		testRequire(
			(pTlsContext != NULL) &&
			(pTlsIdentity != NULL),
			"WebSocket Router TLS fixture creation failed"
		);
		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = testTlsServerAccept;
		pTlsVerifier = xrtTlsVerifierCreate(&VerifierConfig);
		testRequire(
			pTlsVerifier != NULL,
			"WebSocket Router TLS verifier creation failed"
		);
	#endif
	memset(&Test, 0, sizeof(Test));
	xrtAtomicPtrInit(&Test.ServerConnection, NULL);
	xrtAtomicPtrInit(&Test.ClientConnection, NULL);
	xrtAtomic32Init(&Test.Opened, 0);
	xrtAtomic32Init(&Test.ServerMessages, 0);
	xrtAtomic32Init(&Test.ClientMessages, 0);
	xrtAtomic32Init(&Test.ServerClosed, 0);
	xrtAtomic32Init(&Test.ClientClosed, 0);
	xrtAtomic32Init(&Test.HandshakeErrors, 0);
	xrtAtomic32Init(&Test.ServerErrors, 0);
	xrtAtomic32Init(&Test.Shutdowns, 0);
	xrtAtomic32Init(&Test.Releases, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket Router engine start failed"
	);
	pRouter = xrtHttpServerRouterCreate(NULL);
	testRequire(
		pRouter != NULL,
		"WebSocket Router creation failed"
	);
	xrtWsServerRouteConfigInit(&RouteConfig);
	RouteConfig.Server.Protocols =
		XRT_STR_LITERAL("chat.v2, chat.v1");
	RouteConfig.Events.MessageBegin =
		testWsServerRouterServerBegin;
	RouteConfig.Events.MessageData =
		testWsServerRouterServerData;
	RouteConfig.Events.MessageEnd =
		testWsServerRouterServerEnd;
	RouteConfig.Events.Error =
		testWsServerRouterConnectionError;
	RouteConfig.Events.Close =
		testWsServerRouterServerClose;
	RouteConfig.Open = testWsServerRouterOpen;
	RouteConfig.Error = testWsServerRouterHandshakeError;
	RouteConfig.Release = testWsServerRouterRelease;
	RouteConfig.Data = &Test;
	testRequire(
		xrtWsServerRoute(
			pRouter,
			XRT_STR_LITERAL("/chat"),
			&RouteConfig
		) && xrtHttpServerRouterFreeze(pRouter),
		"WebSocket route registration or freeze failed"
	);

	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket Router loopback address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 1u;
	ServerConfig.HeaderTimeout = UINT64_C(10000000);
	ServerConfig.RequestTimeout = UINT64_C(10000000);
	ServerConfig.IdleTimeout = UINT64_C(10000000);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Error = testWsServerRouterHttpError;
	ServerEvents.Shutdown = testWsServerRouterShutdown;
	ServerEvents.Data = &Test;
	#if TEST_WS_SERVER_ROUTER_TLS
		xrtHttpServerTlsConfigInit(&ServerTls);
		ServerTls.Handshake.Context = pTlsContext;
		ServerTls.Handshake.Identity = pTlsIdentity;
		ServerTls.Handshake.Protocols = TlsProtocols;
		ServerTls.Handshake.ProtocolCount =
			sizeof(TlsProtocols) / sizeof(TlsProtocols[0]);
		ServerTls.Handshake.RequireProtocol = true;
		Test.Server = xrtHttpServerRouterStartTls(
			Test.Engine,
			&ServerConfig,
			&ServerTls,
			pRouter,
			&ServerEvents
		);
	#else
		Test.Server = xrtHttpServerRouterStart(
			Test.Engine,
			&ServerConfig,
			pRouter,
			&ServerEvents
		);
	#endif
	testRequire(
		(Test.Server != NULL) &&
		xrtHttpServerLocal(Test.Server, 0, &Address),
		"WebSocket Router server start failed"
	);
	xrtHttpServerRouterDestroy(pRouter);
	pRouter = NULL;

	#if !TEST_WS_SERVER_ROUTER_TLS
		sResponse = testWsServerRouterHttpRequest(
		&Test,
		&Address,
		"OPTIONS /chat HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Http
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 204 No Content") != NULL) &&
		(strstr(sResponse, "Allow: GET, OPTIONS") != NULL),
		"WebSocket Router OPTIONS response mismatch"
	);
	sResponse = testWsServerRouterHttpRequest(
		&Test,
		&Address,
		"POST /chat HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n"
		"Content-Length: 0\r\n\r\n",
		&Http
	);
	testRequire(
		(strstr(
			sResponse,
			"HTTP/1.1 405 Method Not Allowed"
		 ) != NULL) &&
		(strstr(sResponse, "Allow: GET, OPTIONS") != NULL),
		"WebSocket Router method response mismatch"
	);
	sResponse = testWsServerRouterHttpRequest(
		&Test,
		&Address,
		"GET /chat HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: Upgrade, close\r\n"
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Version: 12\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n",
		&Http
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 426 Upgrade Required") != NULL) &&
		(strstr(sResponse, "Upgrade: websocket") != NULL) &&
		(strstr(sResponse, "Sec-WebSocket-Version: 13") != NULL),
		"WebSocket Router version rejection mismatch"
	);
	sResponse = testWsServerRouterHttpRequest(
		&Test,
		&Address,
		"GET /chat HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: Upgrade, close\r\n"
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n",
		&Http
	);
	testRequire(
		strstr(sResponse, "HTTP/1.1 400 Bad Request") != NULL,
		"WebSocket Router malformed handshake response mismatch"
	);
	sResponse = testWsServerRouterHttpRequest(
		&Test,
		&Address,
		"GET /chat HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: Upgrade, close\r\n"
		"Upgrade: websocket\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Content-Length: 1048576\r\n\r\n",
		&Http
	);
	testRequire(
		strstr(sResponse, "HTTP/1.1 400 Bad Request") != NULL,
		"WebSocket Router body declaration rejection mismatch"
	);
	#endif

	xrtHttpClientConfigInit(&ClientConfig);
	#if TEST_WS_SERVER_ROUTER_TLS
		ClientConfig.TlsContext = pTlsContext;
		ClientConfig.TlsVerifier = pTlsVerifier;
		ClientConfig.SystemTrust = false;
	#endif
	Test.Client = xrtHttpClientCreate(
		Test.Engine, &ClientConfig
	);
	testRequire(
		Test.Client != NULL,
		"WebSocket Router HTTP client creation failed"
	);
	iUrl = snprintf(
		Url,
		sizeof(Url),
		#if TEST_WS_SERVER_ROUTER_TLS
			"wss://127.0.0.1:%u/chat",
		#else
			"ws://127.0.0.1:%u/chat",
		#endif
		(unsigned)Address.Port
	);
	testRequire(
		(iUrl > 0) && ((size_t)iUrl < sizeof(Url)),
		"WebSocket Router client URL overflow"
	);
	xrtWsClientConfigInit(&WsClientConfig);
	WsClientConfig.Protocols = XRT_STR_LITERAL("chat.v1");
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.MessageBegin = testWsServerRouterClientBegin;
	ClientEvents.MessageData = testWsServerRouterClientData;
	ClientEvents.MessageEnd = testWsServerRouterClientEnd;
	ClientEvents.Error = testWsServerRouterConnectionError;
	ClientEvents.Close = testWsServerRouterClientClose;
	Test.Call = xrtWsConnect(
		Test.Client,
		(xstrview) { Url, (size_t)iUrl },
		&WsClientConfig,
		&ClientEvents,
		&Test,
		testWsServerRouterConnected,
		&Test
	);
	testRequire(
		Test.Call != NULL,
		"WebSocket Router client connect submission failed"
	);
	testWsServerRouterWait(
		&Test.Opened,
		1u,
		"WebSocket Router Open callback missing"
	);
	testWsServerRouterWait(
		&Test.ServerMessages,
		1u,
		"WebSocket Router server message missing"
	);
	testWsServerRouterWait(
		&Test.ClientMessages,
		1u,
		"WebSocket Router client reply missing"
	);
	testRequire(
		(Test.CallbackCall == Test.Call) &&
		(xrtAtomic32Load(
			&Test.HandshakeErrors, XMEMORY_ACQUIRE
		 ) == (TEST_WS_SERVER_ROUTER_TLS ? 0u : 3u)) &&
		(xrtAtomic32Load(
			&Test.ServerErrors, XMEMORY_ACQUIRE
		 ) == 0),
		"WebSocket Router callback or error counts mismatch"
	);

	testRequire(
		xrtHttpServerDrain(Test.Server),
		"WebSocket Router HTTP server drain failed"
	);
	testWsServerRouterWait(
		&Test.Shutdowns,
		1u,
		"WebSocket Router HTTP server did not shut down"
	);
	xrtHttpServerDestroy(Test.Server);
	Test.Server = NULL;
	testRequire(
		xrtAtomic32Load(
			&Test.Releases, XMEMORY_ACQUIRE
		) == 0,
		"WebSocket Router released Data while a connection was active"
	);

	pClient = (xwsconn*)xrtAtomicPtrLoad(
		&Test.ClientConnection, XMEMORY_ACQUIRE
	);
	pServer = (xwsconn*)xrtAtomicPtrLoad(
		&Test.ServerConnection, XMEMORY_ACQUIRE
	);
	testRequire(
		(pClient != NULL) &&
		(pServer != NULL) &&
		xrtNetEnginePost(
			Test.Engine,
			xrtNetWorkerIndex(xrtWsConnWorker(pClient)),
			testWsServerRouterCloseTask,
			&Test
		),
		"WebSocket Router Close task post failed"
	);
	testWsServerRouterWait(
		&Test.ClientClosed,
		1u,
		"WebSocket Router client Close missing"
	);
	testWsServerRouterWait(
		&Test.ServerClosed,
		1u,
		"WebSocket Router server Close missing"
	);
	testWsServerRouterWait(
		&Test.Releases,
		1u,
		"WebSocket Router final Data release missing"
	);
	testRequire(
		xrtAtomic32Load(
			&Test.Releases, XMEMORY_ACQUIRE
		) == 1u,
		"WebSocket Router Data released more than once"
	);

	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
	xrtHttpResponseDestroy(Test.Response);
	xrtHttpCallDestroy(Test.Call);
	xrtHttpClientDestroy(Test.Client);
	#if TEST_WS_SERVER_ROUTER_TLS
		xrtTlsVerifierRelease(pTlsVerifier);
		xrtTlsIdentityRelease(pTlsIdentity);
		xrtTlsContextRelease(pTlsContext);
	#endif
	testRequire(
		xrtNetEngineDestroy(Test.Engine),
		"WebSocket Router engine destroy failed"
	);
	printf(
		"[PASS] WebSocket server router (%s)\n",
		TEST_WS_SERVER_ROUTER_NAME
	);
	return 0;
}
