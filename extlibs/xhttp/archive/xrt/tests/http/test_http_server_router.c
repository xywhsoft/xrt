#include "../test.h"



typedef struct test_http_server_router {
	xnetengine* Engine;
	xhttpserver* Server;
	xatomic32 Gets;
	xatomic32 Posts;
	xatomic32 Deep;
	xatomic32 Streams;
	xatomic32 Fallbacks;
	xatomic32 Errors;
	xatomic32 Shutdowns;
	xatomic32 Releases;
	char StreamBody[32];
	size_t StreamSize;
} test_http_server_router;



typedef struct test_http_server_router_client {
	xnetstream* Stream;
	cstr Request;
	char Response[4096];
	size_t Size;
	xatomic32 Closed;
} test_http_server_router_client;



/* 在固定截止时间前等待异步状态发布。 */
static void testHttpServerRouterWait(
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



/* 比较参数视图与固定文本。 */
static bool testHttpServerRouterView(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sExpected, iSize) == 0);
}



/* GET 路由直接使用捕获参数构造固定小响应。 */
static void testHttpServerRouterGet(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;
	char Body[64];
	int iWritten;

	(void)pServer;
	(void)pRequest;
	testRequire(
		(iParamCount == 1u) &&
		testHttpServerRouterView(pParams[0].Name, "id"),
		"HTTP server GET route parameter mismatch"
	);
	iWritten = snprintf(
		Body,
		sizeof(Body),
		"id=%.*s",
		(int)pParams[0].Value.Size,
		pParams[0].Value.Data
	);
	testRequire(
		(iWritten > 0) && ((size_t)iWritten < sizeof(Body)) &&
		(xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain"),
			(xbytesview){ (cbytes)Body, (size_t)iWritten }
		 ) == XNET_RESULT_OK),
		"HTTP server GET route response failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Gets, 1, XMEMORY_RELEASE
	);
}



/* POST 路由验证方法专属参数名。 */
static void testHttpServerRouterPost(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		(iParamCount == 1u) &&
		testHttpServerRouterView(pParams[0].Name, "name") &&
		(xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("posted")
		 ) == XNET_RESULT_OK),
		"HTTP server POST route mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Posts, 1, XMEMORY_RELEASE
	);
}



/* 上传路由在 Header 阶段选择流式正文。 */
static xhttpserverbodypolicy testHttpServerRouterStreamHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	testRequire(
		(iParamCount == 1u) &&
		testHttpServerRouterView(pParams[0].Value, "alice"),
		"HTTP server stream Header route mismatch"
	);
	pState->StreamSize = 0;
	return XHTTP_SERVER_BODY_STREAM;
}



/* 上传路由收集所有流式片段。 */
static bool testHttpServerRouterStreamBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xbytesview Data,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	testRequire(
		Data.Size <= (sizeof(pState->StreamBody) -
		 pState->StreamSize),
		"HTTP server stream route body overflow"
	);
	memcpy(
		pState->StreamBody + pState->StreamSize,
		Data.Data,
		Data.Size
	);
	pState->StreamSize += Data.Size;
	return true;
}



/* 上传完成后验证没有建立第二份缓冲正文。 */
static void testHttpServerRouterStreamRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;
	xbytesview Body = xrtHttpServerRequestBody(pRequest);

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	testRequire(
		(Body.Size == 0) &&
		(pState->StreamSize == 4u) &&
		(memcmp(pState->StreamBody, "DATA", 4u) == 0) &&
		(xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("streamed")
		 ) == XNET_RESULT_OK),
		"HTTP server stream route completion mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Streams, 1, XMEMORY_RELEASE
	);
}



/* 任意方法路由提供轻量健康检查。 */
static void testHttpServerRouterAny(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	testRequire(
		xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("application/json"),
			XRT_BYTES_LITERAL("{\"ok\":true}")
		) == XNET_RESULT_OK,
		"HTTP server any route response failed"
	);
}



/* 最后一个 Router 引用释放时记录拥有型路由 Data 清理。 */
static void testHttpServerRouterRelease(ptr pData)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)xrtAtomic32FetchAdd(
		&pState->Releases, 1, XMEMORY_RELEASE
	);
}



/* 深层路由验证超过八个栈内参数后的精确重匹配。 */
static void testHttpServerRouterDeep(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		(iParamCount == 9u) &&
		testHttpServerRouterView(pParams[0].Name, "a") &&
		testHttpServerRouterView(pParams[0].Value, "1") &&
		testHttpServerRouterView(pParams[8].Name, "i") &&
		testHttpServerRouterView(pParams[8].Value, "9") &&
		(xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("deep")
		 ) == XNET_RESULT_OK),
		"HTTP server deep route mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Deep, 1, XMEMORY_RELEASE
	);
}



/* 第二个 Server 用原始 Request 事件处理所有未命中请求。 */
static void testHttpServerRouterFallback(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		xrtHttpConnReply(
			pConnection,
			418,
			XRT_STR_LITERAL("text/plain"),
			XRT_BYTES_LITERAL("fallback")
		) == XNET_RESULT_OK,
		"HTTP server router fallback response failed"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Fallbacks, 1, XMEMORY_RELEASE
	);
}



/* 记录意外路由或 Server 错误。 */
static void testHttpServerRouterError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors, 1, XMEMORY_RELEASE
	);
}



/* 记录每个 Router Server 的最终关闭。 */
static void testHttpServerRouterShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_router* pState =
		(test_http_server_router*)pData;

	testRequire(
		xrtHttpServerState(pServer) == XHTTP_SERVER_CLOSED,
		"HTTP server router shutdown state mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Shutdowns, 1, XMEMORY_RELEASE
	);
}



/* Client 打开后发送一条要求关闭连接的完整请求。 */
static void testHttpServerRouterClientOpen(
	xnetstream* pStream,
	ptr pData
)
{
	test_http_server_router_client* pClient =
		(test_http_server_router_client*)pData;

	testRequire(
		xrtNetStreamSend(
			pStream,
			pClient->Request,
			strlen(pClient->Request)
		) == XNET_RESULT_OK,
		"HTTP server router client send failed"
	);
}



/* Client 连续消费响应，不为测试建立固定 8K 网络缓冲。 */
static void testHttpServerRouterClientRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	test_http_server_router_client* pClient =
		(test_http_server_router_client*)pData;
	size_t iAvailable = xrtNetBufSize(pBuffer);

	(void)pStream;
	testRequire(
		iAvailable < (sizeof(pClient->Response) - pClient->Size),
		"HTTP server router response exceeded test capacity"
	);
	testRequire(
		xrtNetBufRead(
			pBuffer,
			pClient->Response + pClient->Size,
			iAvailable
		) == iAvailable,
		"HTTP server router client consume failed"
	);
	pClient->Size += iAvailable;
	pClient->Response[pClient->Size] = '\0';
}



/* 对端结束写方向后关闭 Client。 */
static void testHttpServerRouterClientEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pData;
	(void)xrtNetStreamClose(pStream);
}



/* 发布 Client 终态。 */
static void testHttpServerRouterClientClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_router_client* pClient =
		(test_http_server_router_client*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	xrtAtomic32Store(
		&pClient->Closed, 1, XMEMORY_RELEASE
	);
}



/* 运行一条真实本地 TCP 请求并返回完整响应文本。 */
static cstr testHttpServerRouterRequest(
	test_http_server_router* pState,
	const xnetaddr* pAddress,
	cstr sRequest,
	test_http_server_router_client* pClient
)
{
	xnetstreamconfig Config;
	xnetstreamevents Events;

	memset(pClient, 0, sizeof(*pClient));
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Init(&pClient->Closed, 0);
	pClient->Request = sRequest;
	Events.Open = testHttpServerRouterClientOpen;
	Events.Read = testHttpServerRouterClientRead;
	Events.End = testHttpServerRouterClientEnd;
	Events.Close = testHttpServerRouterClientClose;
	xrtNetStreamConfigInit(&Config);
	Config.ReadSize = 256u;
	Config.ReadLimit = sizeof(pClient->Response);
	Config.WriteHighWater = 1024u;
	Config.WriteLowWater = 512u;
	Config.WriteLimit = 2048u;
	pClient->Stream = xrtNetStreamConnect(
		pState->Engine,
		pAddress,
		0,
		&Config,
		&Events,
		pClient
	);
	testRequire(
		pClient->Stream != NULL,
		"HTTP server router client connect failed"
	);
	testHttpServerRouterWait(
		&pClient->Closed,
		1,
		"HTTP server router client did not close"
	);
	xrtNetStreamDestroy(pClient->Stream);
	return pClient->Response;
}



/* 启动一个动态端口 Router Server。 */
static xhttpserver* testHttpServerRouterStart(
	test_http_server_router* pState,
	xhttpserverrouter* pRouter,
	bool bFallback,
	xnetaddr* pAddress
)
{
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpserver* pServer;

	xrtHttpServerConfigInit(&Config);
	testRequire(
		xrtNetAddrLoopback(
			&Config.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server router loopback address failed"
	);
	Config.Network.Listen.AcceptConcurrency = 1u;
	Config.HeaderTimeout = UINT64_C(10000000);
	Config.RequestTimeout = UINT64_C(10000000);
	Config.IdleTimeout = UINT64_C(10000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = bFallback ?
		testHttpServerRouterFallback : NULL;
	Events.Error = testHttpServerRouterError;
	Events.Shutdown = testHttpServerRouterShutdown;
	Events.Data = pState;
	pServer = xrtHttpServerRouterStart(
		pState->Engine,
		&Config,
		pRouter,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, pAddress),
		"HTTP server router start failed"
	);
	pState->Server = pServer;
	return pServer;
}



/* 排空并销毁当前 Server。 */
static void testHttpServerRouterStop(
	test_http_server_router* pState,
	uint32 iShutdowns
)
{
	testRequire(
		xrtHttpServerDrain(pState->Server),
		"HTTP server router drain failed"
	);
	testHttpServerRouterWait(
		&pState->Shutdowns,
		iShutdowns,
		"HTTP server router shutdown did not complete"
	);
	xrtHttpServerDestroy(pState->Server);
	pState->Server = NULL;
}



/* 运行高层 Router 注册、真实网络分发、默认响应和生命周期门禁。 */
int main(void)
{
	test_http_server_router State;
	test_http_server_router_client Client;
	xnetengineconfig EngineConfig;
	xhttpserverrouter* pRouter;
	xhttpserverrouteevents Stream;
	xnetaddr Address;
	cstr sResponse;
	cstr sBody;
	char Method[32];
	int iMethod;
	size_t i;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Gets, 0);
	xrtAtomic32Init(&State.Posts, 0);
	xrtAtomic32Init(&State.Deep, 0);
	xrtAtomic32Init(&State.Streams, 0);
	xrtAtomic32Init(&State.Fallbacks, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Shutdowns, 0);
	xrtAtomic32Init(&State.Releases, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	State.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(State.Engine != NULL) &&
		xrtNetEngineStart(State.Engine),
		"HTTP server router engine start failed"
	);
	pRouter = xrtHttpServerRouterCreate(NULL);
	testRequire(pRouter != NULL, "HTTP server router create failed");
	xrtHttpServerRouteEventsInit(&Stream);
	Stream.Headers = testHttpServerRouterStreamHeaders;
	Stream.Body = testHttpServerRouterStreamBody;
	Stream.Request = testHttpServerRouterStreamRequest;
	Stream.Data = &State;
	testRequire(
		xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/users/{id}"),
			testHttpServerRouterGet,
			&State
		) && xrtHttpServerPost(
			pRouter,
			XRT_STR_LITERAL("/users/{name}"),
			testHttpServerRouterPost,
			&State
		) && xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/upload/{name}"),
			&Stream
		) && xrtHttpServerAny(
			pRouter,
			XRT_STR_LITERAL("/health"),
			testHttpServerRouterAny,
			&State
		) && xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL(
				"/deep/{a}/{b}/{c}/{d}/{e}/{f}/{g}/{h}/{i}"
			),
			testHttpServerRouterDeep,
			&State
		),
		"HTTP server route registration failed"
	);
	xrtHttpServerRouteEventsInit(&Stream);
	Stream.Request = testHttpServerRouterAny;
	Stream.Release = testHttpServerRouterRelease;
	Stream.Data = &State;
	testRequire(
		xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/owned"),
			&Stream
		),
		"HTTP server owned route registration failed"
	);
	for ( i = 0; i < 17u; i++ ) {
		iMethod = snprintf(
			Method, sizeof(Method),
			"METHOD%010u", (unsigned)i
		);
		testRequire(
			(iMethod == 16) && xrtHttpServerRoute(
				pRouter,
				(xstrview){ Method, (size_t)iMethod },
				XRT_STR_LITERAL("/many"),
				testHttpServerRouterAny,
				&State
			),
			"HTTP server many-method route registration failed"
		);
	}
	testRequire(
		(xrtHttpServerRouterCount(pRouter) == 23u) &&
		xrtHttpServerRouterFreeze(pRouter) &&
		xrtHttpServerRouterFrozen(pRouter),
		"HTTP server route freeze failed"
	);

	testHttpServerRouterStart(
		&State, pRouter, false, &Address
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"GET /users/42 HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(sResponse, "id=42") != NULL),
		"HTTP server GET route wire response mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"HEAD /users/42 HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	sBody = strstr(sResponse, "\r\n\r\n");
	testRequire(
		(strstr(sResponse, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(sResponse, "Content-Length: 5") != NULL) &&
		(sBody != NULL) &&
		(strstr(sBody + 4, "id=42") == NULL),
		"HTTP server HEAD route fallback mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"POST /users/alice HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n"
		"Content-Length: 0\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(sResponse, "posted") != NULL),
		"HTTP server POST route wire response mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"DELETE /users/42 HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 405 Method Not Allowed") != NULL) &&
		(strstr(sResponse, "Allow: GET, HEAD, POST, OPTIONS") != NULL),
		"HTTP server router 405 Allow mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"OPTIONS /users/42 HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 204 No Content") != NULL) &&
		(strstr(sResponse, "Allow: GET, HEAD, POST, OPTIONS") != NULL),
		"HTTP server router automatic OPTIONS mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"GET /missing HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		strstr(sResponse, "HTTP/1.1 404 Not Found") != NULL,
		"HTTP server router 404 mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"POST /upload/alice HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n"
		"Content-Length: 4\r\n\r\nDATA",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(sResponse, "streamed") != NULL),
		"HTTP server streaming route wire response mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"PATCH /health HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		strstr(sResponse, "{\"ok\":true}") != NULL,
		"HTTP server any-method route mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"GET /deep/1/2/3/4/5/6/7/8/9 HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(sResponse, "deep") != NULL),
		"HTTP server deep route wire response mismatch"
	);
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"OTHER /many HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 405 Method Not Allowed") != NULL) &&
		(strstr(
			sResponse,
			"Allow: METHOD0000000000, METHOD0000000001"
		 ) != NULL) &&
		(strstr(sResponse, "METHOD0000000016, OPTIONS\r\n") != NULL),
		"HTTP server dynamic Allow response mismatch"
	);
	testRequire(
		(xrtAtomic32Load(&State.Gets, XMEMORY_ACQUIRE) == 2u) &&
		(xrtAtomic32Load(&State.Posts, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&State.Deep, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&State.Streams, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&State.Errors, XMEMORY_ACQUIRE) == 0),
		"HTTP server router callback counts mismatch"
	);
	testHttpServerRouterStop(&State, 1u);
	testRequire(
		xrtAtomic32Load(
			&State.Releases, XMEMORY_ACQUIRE
		) == 0,
		"HTTP server Router released owned route too early"
	);

	testHttpServerRouterStart(
		&State, pRouter, true, &Address
	);
	xrtHttpServerRouterDestroy(pRouter);
	pRouter = NULL;
	sResponse = testHttpServerRouterRequest(
		&State,
		&Address,
		"GET /missing HTTP/1.1\r\n"
		"Host: router.test\r\nConnection: close\r\n\r\n",
		&Client
	);
	testRequire(
		(strstr(sResponse, "HTTP/1.1 418") != NULL) &&
		(strstr(sResponse, "fallback") != NULL) &&
		(xrtAtomic32Load(
			&State.Fallbacks, XMEMORY_ACQUIRE
		 ) == 1u),
		"HTTP server router unmatched fallback mismatch"
	);
	testHttpServerRouterStop(&State, 2u);
	testHttpServerRouterWait(
		&State.Releases,
		1u,
		"HTTP server Router owned route release did not complete"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Releases, XMEMORY_ACQUIRE
		) == 1,
		"HTTP server Router owned route release mismatch"
	);
	testRequire(
		xrtNetEngineDestroy(State.Engine),
		"HTTP server router engine destroy failed"
	);
	puts("[PASS] HTTP server router");
	return 0;
}
