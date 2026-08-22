#include "../fixtures/tls_server.h"

#include <xrt/http_server_router_tls.h>



#if !defined(TEST_HTTP_SERVER_ROUTER_TLS_START)
	#define TEST_HTTP_SERVER_ROUTER_TLS_START \
		xrtHttpServerRouterStartTls
#endif



#if !defined(TEST_HTTP_SERVER_ROUTER_TLS_PASS)
	#define TEST_HTTP_SERVER_ROUTER_TLS_PASS \
		"[PASS] HTTPS server router"
#endif



/* TLS Router 夹具保存服务端、客户端和跨 Worker 终态。 */
typedef struct test_http_server_router_tls {
	xnetengine* Engine;
	xhttpserver* Server;
	xtlsstream* Client;
	xatomic32 Requested;
	xatomic32 ClientClosed;
	xatomic32 Shutdown;
	xatomic32 Errors;
	char Response[4096];
	size_t ResponseSize;
} test_http_server_router_tls;



/* 在固定截止时间前等待异步终态发布。 */
static void testHttpServerRouterTlsWait(
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



/* 比较路由参数与固定 ASCII 文本。 */
static bool testHttpServerRouterTlsView(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* TLS 路由验证安全传输逃生口和动态路径参数。 */
static void testHttpServerRouterTlsRoute(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router_tls* pState =
		(test_http_server_router_tls*)pData;

	(void)pRequest;
	testRequire(
		(pServer == pState->Server) &&
		xrtHttpServerSecure(pServer) &&
		xrtHttpConnSecure(pConnection) &&
		(xrtHttpConnTls(pConnection) != NULL) &&
		(iParamCount == 1u) &&
		testHttpServerRouterTlsView(pParams[0].Name, "id") &&
		testHttpServerRouterTlsView(pParams[0].Value, "42") &&
		(xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("application/json; charset=utf-8"),
			XRT_BYTES_LITERAL("{\"secure\":true,\"id\":42}")
		 ) == XNET_RESULT_OK),
		"HTTPS server Router handler mismatch"
	);
	xrtAtomic32Store(
		&pState->Requested, 1, XMEMORY_RELEASE
	);
}



/* 记录任何不应出现的 Server 错误。 */
static void testHttpServerRouterTlsError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_router_tls* pState =
		(test_http_server_router_tls*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	xrtAtomic32Store(
		&pState->Errors, 1, XMEMORY_RELEASE
	);
}



/* Server 完全排空后记录 Router 运行时已经完成生命周期。 */
static void testHttpServerRouterTlsShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_router_tls* pState =
		(test_http_server_router_tls*)pData;

	testRequire(
		pServer == pState->Server,
		"HTTPS server Router Shutdown object mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown, 1, XMEMORY_RELEASE
	);
}



/* 客户端握手完成后发送一条关闭连接的参数路由请求。 */
static void testHttpServerRouterTlsClientOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	static const char Request[] =
		"GET /secure/42 HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Connection: close\r\n\r\n";
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
		"HTTPS server Router client send failed"
	);
}



/* 客户端复制并消费服务端返回的全部 HTTP 明文。 */
static void testHttpServerRouterTlsClientRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_router_tls* pState =
		(test_http_server_router_tls*)pData;
	size_t iAvailable = xrtTlsStreamAvailable(pStream);

	testRequire(
		(iAvailable != 0) &&
		(iAvailable <= (sizeof(pState->Response) -
		 pState->ResponseSize)) &&
		(xrtNetBufPeek(
			pBuffer,
			0,
			pState->Response + pState->ResponseSize,
			iAvailable
		 ) == iAvailable) &&
		xrtTlsStreamConsume(pStream, iAvailable),
		"HTTPS server Router client consume failed"
	);
	pState->ResponseSize += iAvailable;
}



/* 收到服务端 close_notify 后回送认证关闭。 */
static void testHttpServerRouterTlsClientEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pData;
	testRequire(
		xrtTlsStreamClose(pStream),
		"HTTPS server Router client close_notify failed"
	);
}



/* 发布客户端 TLS Stream 的认证关闭终态。 */
static void testHttpServerRouterTlsClientClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_router_tls* pState =
		(test_http_server_router_tls*)pData;

	testRequire(
		(Result == XNET_RESULT_OK) &&
		(pError == NULL) &&
		(xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED),
		"HTTPS server Router client close mismatch"
	);
	xrtAtomic32Store(
		&pState->ClientClosed, 1, XMEMORY_RELEASE
	);
}



/* 在有界响应字节中查找一个固定标记。 */
static bool testHttpServerRouterTlsContains(
	const char* pData,
	size_t iSize,
	cstr sNeedle
)
{
	size_t iNeedle = strlen(sNeedle);
	size_t i;

	if ( iNeedle > iSize ) {
		return false;
	}
	for ( i = 0; i <= (iSize - iNeedle); i++ ) {
		if ( memcmp(pData + i, sNeedle, iNeedle) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 验证 TLS Router 的真实握手、分发、引用和认证关闭。 */
int main(void)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	test_http_server_router_tls State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpserverevents ServerEvents;
	xtlsclientconfig ClientConfig;
	xtlsstreamconfig StreamConfig;
	xtlsstreamevents ClientEvents;
	xtlsverifierconfig VerifierConfig;
	xhttpserverrouter* pRouter;
	xtlscontext* pServerContext;
	xtlscontext* pClientContext;
	xtlsidentity* pIdentity;
	xtlsverifier* pVerifier;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.ClientClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtAtomic32Init(&State.Errors, 0);
	pServerContext = testTlsServerContext();
	pClientContext = testTlsServerContext();
	pIdentity = testTlsServerIdentity();
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = testTlsServerAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	testRequire(
		(pServerContext != NULL) &&
		(pClientContext != NULL) &&
		(pIdentity != NULL) &&
		(pVerifier != NULL),
		"HTTPS server Router TLS fixture failed"
	);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTPS server Router Engine start failed"
	);
	pRouter = xrtHttpServerRouterCreate(NULL);
	testRequire(
		(pRouter != NULL) &&
		xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/secure/{id}"),
			testHttpServerRouterTlsRoute,
			&State
		) && xrtHttpServerRouterFreeze(pRouter),
		"HTTPS server Router registration failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTPS server Router loopback address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 2u;
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Context = pServerContext;
	TlsConfig.Handshake.Identity = pIdentity;
	TlsConfig.Handshake.RequireProtocol = true;
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Error = testHttpServerRouterTlsError;
	ServerEvents.Shutdown = testHttpServerRouterTlsShutdown;
	ServerEvents.Data = &State;
	State.Server = TEST_HTTP_SERVER_ROUTER_TLS_START(
		State.Engine,
		&ServerConfig,
		&TlsConfig,
		pRouter,
		&ServerEvents
	);
	testRequire(
		(State.Server != NULL) &&
		xrtHttpServerSecure(State.Server) &&
		xrtHttpServerLocal(State.Server, 0, &Address),
		"HTTPS server Router start failed"
	);
	xrtHttpServerRouterDestroy(pRouter);
	pRouter = NULL;
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsContextRelease(pServerContext);

	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pClientContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount =
		sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsStreamConfigInit(&StreamConfig);
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Open = testHttpServerRouterTlsClientOpen;
	ClientEvents.Read = testHttpServerRouterTlsClientRead;
	ClientEvents.End = testHttpServerRouterTlsClientEnd;
	ClientEvents.Close = testHttpServerRouterTlsClientClose;
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
		"HTTPS server Router client connect failed"
	);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsContextRelease(pClientContext);

	testHttpServerRouterTlsWait(
		&State.Requested,
		1u,
		"HTTPS server Router request did not arrive"
	);
	testHttpServerRouterTlsWait(
		&State.ClientClosed,
		1u,
		"HTTPS server Router client did not close"
	);
	testRequire(
		(xrtAtomic32Load(&State.Errors, XMEMORY_ACQUIRE) == 0) &&
		testHttpServerRouterTlsContains(
			State.Response,
			State.ResponseSize,
			"HTTP/1.1 200 OK"
		) && testHttpServerRouterTlsContains(
			State.Response,
			State.ResponseSize,
			"{\"secure\":true,\"id\":42}"
		),
		"HTTPS server Router response mismatch"
	);
	testRequire(
		xrtHttpServerDrain(State.Server),
		"HTTPS server Router drain failed"
	);
	testHttpServerRouterTlsWait(
		&State.Shutdown,
		1u,
		"HTTPS server Router did not shut down"
	);
	xrtTlsStreamDestroy(State.Client);
	xrtHttpServerDestroy(State.Server);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTPS server Router Engine destroy failed"
	);
	xrtClearError();
	testMemoryDebugDrain(
		"HTTPS server Router leaked memory"
	);
	puts(TEST_HTTP_SERVER_ROUTER_TLS_PASS);
	return 0;
}
