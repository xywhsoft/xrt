#include "../test.h"

#include <xrt/http_server_mux.h>



/* 每个热替换 Router 使用独立计数和固定响应正文。 */
typedef struct test_http_server_mux_route {
	xatomic32 Headers;
	xatomic32 Body;
	xatomic32 Requests;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		xatomic32 Middleware;
	#endif
	cstr sReply;
} test_http_server_mux_route;



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 组合测试证明 Mux 固定 Router 后会进入该 Router 自己的中间件链。 */
static bool testHttpServerMuxMiddleware(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	test_http_server_mux_route* pRoute =
		(test_http_server_mux_route*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)xrtAtomic32FetchAdd(
		&pRoute->Middleware, 1, XMEMORY_ACQ_REL
	);
	return xrtHttpServerNext(pNext);
}

#endif



/* Header 阶段选择流式正文，以验证 Router 在整个请求期间固定。 */
static xhttpserverbodypolicy testHttpServerMuxHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_mux_route* pRoute =
		(test_http_server_mux_route*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)xrtAtomic32FetchAdd(
		&pRoute->Headers, 1, XMEMORY_ACQ_REL
	);
	return XHTTP_SERVER_BODY_STREAM;
}



/* 流式正文只接受测试使用的四字节 DATA。 */
static bool testHttpServerMuxBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xbytesview Data,
	ptr pData
)
{
	test_http_server_mux_route* pRoute =
		(test_http_server_mux_route*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	testRequire(
		(Data.Size == 4u) &&
		(memcmp(Data.Data, "DATA", 4u) == 0),
		"HTTP server mux streaming body mismatch"
	);
	(void)xrtAtomic32FetchAdd(
		&pRoute->Body, 1, XMEMORY_ACQ_REL
	);
	return true;
}



/* 完整请求直接提交固定正文，不构造额外 Reply 对象。 */
static void testHttpServerMuxRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_mux_route* pRoute =
		(test_http_server_mux_route*)pData;
	size_t iSize = strlen(pRoute->sReply);

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)xrtAtomic32FetchAdd(
		&pRoute->Requests, 1, XMEMORY_ACQ_REL
	);
	testRequire(
		xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			(xbytesview){ (cbytes)pRoute->sReply, iSize }
		) == XNET_RESULT_OK,
		"HTTP server mux response failed"
	);
}



/* 创建一条流式 POST 路由并冻结。 */
static xhttpserverrouter* testHttpServerMuxRuntimeRouter(
	test_http_server_mux_route* pRoute
)
{
	xhttpserverrouteevents Events;
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	xrtHttpServerRouteEventsInit(&Events);
	Events.Headers = testHttpServerMuxHeaders;
	Events.Body = testHttpServerMuxBody;
	Events.Request = testHttpServerMuxRequest;
	Events.Data = pRoute;
	testRequire(pRouter != NULL, "HTTP server mux Router create failed");
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		testRequire(
			xrtHttpServerUse(
				pRouter,
				testHttpServerMuxMiddleware,
				pRoute
			),
			"HTTP server mux middleware setup failed"
		);
	#endif
	testRequire(
		xrtHttpServerRouteEvents(
			pRouter,
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("/upload"),
			&Events
		) && xrtHttpServerRouterFreeze(pRouter),
		"HTTP server mux runtime Router fixture failed"
	);
	return pRouter;
}



/* 在截止时间内等待一个 Worker 回调计数达到目标。 */
static void testHttpServerMuxWait(
	xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(5000000)
	);

	while ( xrtAtomic32Load(
		pValue, XMEMORY_ACQUIRE
	) != iExpected ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 对阻塞测试 Socket 完整发送一段请求。 */
static void testHttpServerMuxSend(
	xnetsocket Socket,
	cstr sData
)
{
	size_t iSize = strlen(sData);
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			xrtNetSocketSend(
				Socket,
				sData + iOffset,
				iSize - iOffset,
				&iSent
			) == XNET_RESULT_OK &&
			(iSent != 0),
			"HTTP server mux test send failed"
		);
		iOffset += iSent;
	}
}



/* 接收直到 Connection: close，并返回零结尾完整响应。 */
static void testHttpServerMuxReceive(
	xnetsocket Socket,
	char* sResponse,
	size_t iCapacity
)
{
	size_t iSize = 0;

	for ( ;; ) {
		size_t iReceived = 0;
		xnetresult Result;

		testRequire(
			iSize < (iCapacity - 1u),
			"HTTP server mux response exceeded test capacity"
		);
		Result = xrtNetSocketRecv(
			Socket,
			sResponse + iSize,
			iCapacity - iSize - 1u,
			&iReceived
		);
		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iReceived != 0),
			"HTTP server mux test receive failed"
		);
		iSize += iReceived;
	}
	sResponse[iSize] = '\0';
}



/* 连接动态端口并返回一个阻塞 TCP Socket。 */
static xnetsocket testHttpServerMuxConnect(
	const xnetaddr* pAddress
)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket, pAddress
		 ) == XNET_RESULT_OK),
		"HTTP server mux test connect failed"
	);
	return Socket;
}



/* 发送完整短请求并收取 Connection: close 响应。 */
static void testHttpServerMuxRoundTrip(
	const xnetaddr* pAddress,
	cstr sRequest,
	char* sResponse,
	size_t iCapacity
)
{
	xnetsocket Socket = testHttpServerMuxConnect(pAddress);

	testHttpServerMuxSend(Socket, sRequest);
	testHttpServerMuxReceive(
		Socket, sResponse, iCapacity
	);
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP server mux test socket close failed"
	);
}



/* 验证请求固定、热替换、未知 Host 和完整关闭生命周期。 */
int main(void)
{
	test_http_server_mux_route One;
	test_http_server_mux_route Two;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverrouter* pOne;
	xhttpserverrouter* pTwo;
	xhttpservermux* pMux;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetsocket Socket;
	xnetaddr Address;
	char Response[4096];
	xdeadline Deadline;

	memset(&One, 0, sizeof(One));
	memset(&Two, 0, sizeof(Two));
	xrtAtomic32Init(&One.Headers, 0);
	xrtAtomic32Init(&One.Body, 0);
	xrtAtomic32Init(&One.Requests, 0);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		xrtAtomic32Init(&One.Middleware, 0);
	#endif
	xrtAtomic32Init(&Two.Headers, 0);
	xrtAtomic32Init(&Two.Body, 0);
	xrtAtomic32Init(&Two.Requests, 0);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		xrtAtomic32Init(&Two.Middleware, 0);
	#endif
	One.sReply = "one";
	Two.sReply = "two";
	pOne = testHttpServerMuxRuntimeRouter(&One);
	pTwo = testHttpServerMuxRuntimeRouter(&Two);
	pMux = xrtHttpServerMuxCreate(NULL);
	testRequire(
		(pMux != NULL) &&
		xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("x.test"),
			pOne
		),
		"HTTP server mux runtime Host setup failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP server mux engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server mux loopback address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 1u;
	pServer = xrtHttpServerMuxStart(
		pEngine, &ServerConfig, pMux, NULL
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server mux runtime start failed"
	);

	Socket = testHttpServerMuxConnect(&Address);
	testHttpServerMuxSend(
		Socket,
		"POST /upload HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n\r\n"
	);
	testHttpServerMuxWait(
		&One.Headers,
		1,
		"HTTP server mux old Header callback did not run"
	);
	testRequire(
		xrtHttpServerMuxHost(
			pMux,
			XRT_STR_LITERAL("X.TEST"),
			pTwo
		),
		"HTTP server mux runtime hot replacement failed"
	);
	xrtHttpServerRouterDestroy(pOne);
	pOne = NULL;
	testHttpServerMuxSend(Socket, "DATA");
	testHttpServerMuxReceive(
		Socket, Response, sizeof(Response)
	);
	testRequire(
		xrtNetSocketClose(Socket) &&
		(strstr(Response, "HTTP/1.1 200 OK") != NULL) &&
		(strstr(Response, "\r\n\r\none") != NULL) &&
		(xrtAtomic32Load(
			&One.Body, XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&One.Requests, XMEMORY_ACQUIRE
		 ) == 1) &&
		#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
			(xrtAtomic32Load(
				&One.Middleware, XMEMORY_ACQUIRE
			 ) == 1) &&
		#endif
		(xrtAtomic32Load(
			&Two.Requests, XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server mux did not pin old Router for the request"
	);

	testHttpServerMuxRoundTrip(
		&Address,
		"POST /upload HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n\r\nDATA",
		Response,
		sizeof(Response)
	);
	testRequire(
		(strstr(Response, "\r\n\r\ntwo") != NULL) &&
		(xrtAtomic32Load(
			&Two.Headers, XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&Two.Body, XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&Two.Requests, XMEMORY_ACQUIRE
		 ) == 1)
		#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
			&& (xrtAtomic32Load(
				&Two.Middleware, XMEMORY_ACQUIRE
			 ) == 1)
		#endif
		,
		"HTTP server mux next request did not use new Router"
	);
	testHttpServerMuxRoundTrip(
		&Address,
		"GET / HTTP/1.1\r\n"
		"Host: missing.test\r\n"
		"Connection: close\r\n\r\n",
		Response,
		sizeof(Response)
	);
	testRequire(
		strstr(
			Response,
			"HTTP/1.1 421 Misdirected Request"
		) != NULL,
		"HTTP server mux unknown Host response mismatch"
	);

	xrtHttpServerRouterDestroy(pTwo);
	xrtHttpServerMuxDestroy(pMux);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server mux drain failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtHttpServerState(pServer) !=
		XHTTP_SERVER_CLOSED ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server mux shutdown timed out"
		);
		xrtThreadYield();
	}
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server mux engine destroy failed"
	);
	puts("[PASS] HTTP server mux runtime");
	return 0;
}
