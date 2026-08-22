#include "../test.h"

#include <xrt/http_server_middleware.h>



/* 运行时记录洋葱顺序、完成次数、参数可见性和拥有数据清理。 */
typedef struct test_http_server_middleware_context {
	xatomic32 Completed;
	xatomic32 Released;
	char Order[64];
	size_t OrderSize;
	bool SawParam;
	bool SawResponseAfterNext;
} test_http_server_middleware_context;



/* 在唯一 Worker 上追加一个顺序标记。 */
static void testHttpServerMiddlewareMark(
	test_http_server_middleware_context* pContext,
	char Mark
)
{
	testRequire(
		pContext->OrderSize < sizeof(pContext->Order) - 1u,
		"HTTP server middleware order overflow"
	);
	pContext->Order[pContext->OrderSize++] = Mark;
	pContext->Order[pContext->OrderSize] = '\0';
}



/* 比较原始请求目标是否等于固定路径。 */
static bool testHttpServerMiddlewareTarget(
	const xhttpserverrequest* pRequest,
	cstr sTarget
)
{
	xstrview Target = xrtHttpServerRequestTarget(pRequest);
	size_t iSize = strlen(sTarget);

	return (Target.Size == iSize) &&
		(memcmp(Target.Data, sTarget, iSize) == 0);
}



/* 最外层中间件验证 Next 单次语义，并在最外层退出时发布完成计数。 */
static bool testHttpServerMiddlewareOuter(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	test_http_server_middleware_context* pContext =
		(test_http_server_middleware_context*)pData;
	bool bResult;

	(void)pServer;
	(void)pConnection;
	(void)pParams;
	(void)iParamCount;
	testHttpServerMiddlewareMark(pContext, 'A');
	bResult = xrtHttpServerNext(pNext);
	if ( testHttpServerMiddlewareTarget(
		pRequest, "/double"
	) ) {
		bResult = xrtHttpServerNext(pNext) && bResult;
	}
	testHttpServerMiddlewareMark(pContext, 'F');
	(void)xrtAtomic32FetchAdd(
		&pContext->Completed, 1, XMEMORY_RELEASE
	);
	return bResult;
}



/* 中间层只负责展示标准的前置、Next、后置洋葱顺序。 */
static bool testHttpServerMiddlewareMiddle(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	test_http_server_middleware_context* pContext =
		(test_http_server_middleware_context*)pData;
	bool bResult;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	testHttpServerMiddlewareMark(pContext, 'B');
	bResult = xrtHttpServerNext(pNext);
	testHttpServerMiddlewareMark(pContext, 'E');
	return bResult;
}



/* 最内层可短路、保留空终端供双 Next 测试，或继续到路由。 */
static bool testHttpServerMiddlewareInner(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
)
{
	test_http_server_middleware_context* pContext =
		(test_http_server_middleware_context*)pData;
	bool bResult;

	(void)pServer;
	testHttpServerMiddlewareMark(pContext, 'C');
	if ( testHttpServerMiddlewareTarget(
		pRequest, "/stop"
	) ) {
		bResult = xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_FORBIDDEN,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("stopped")
		) == XNET_RESULT_OK;
	} else if ( testHttpServerMiddlewareTarget(
		pRequest, "/double"
	) ) {
		bResult = true;
	} else {
		if ( (iParamCount == 1u) &&
			(pParams != NULL) &&
			(pParams[0].Value.Size == 2u) &&
			(memcmp(pParams[0].Value.Data, "42", 2u) == 0) ) {
			pContext->SawParam = true;
		}
		bResult = xrtHttpServerNext(pNext);
		if ( testHttpServerMiddlewareTarget(
			pRequest, "/items/42"
		) ) {
			pContext->SawResponseAfterNext =
				xrtHttpConnState(pConnection) ==
				XHTTP_CONN_RESPONSE;
		}
	}
	testHttpServerMiddlewareMark(pContext, 'D');
	return bResult;
}



/* 最终路由直接提交固定正文，并证明中间件位于终端外侧。 */
static void testHttpServerMiddlewareRoute(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	test_http_server_middleware_context* pContext =
		(test_http_server_middleware_context*)pData;

	(void)pServer;
	(void)pRequest;
	testRequire(
		(iParamCount == 1u) && (pParams != NULL),
		"HTTP server middleware route parameters missing"
	);
	testHttpServerMiddlewareMark(pContext, 'R');
	testRequire(
		xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("route")
		) == XNET_RESULT_OK,
		"HTTP server middleware route response failed"
	);
}



/* Router 最后释放拥有型中间件数据时调用一次。 */
static void testHttpServerMiddlewareRelease(ptr pData)
{
	test_http_server_middleware_context* pContext =
		(test_http_server_middleware_context*)pData;

	(void)xrtAtomic32FetchAdd(
		&pContext->Released, 1, XMEMORY_ACQ_REL
	);
}



/* 对阻塞 Socket 完整发送一个测试请求。 */
static void testHttpServerMiddlewareSend(
	xnetsocket Socket,
	cstr sRequest
)
{
	size_t iSize = strlen(sRequest);
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			xrtNetSocketSend(
				Socket,
				sRequest + iOffset,
				iSize - iOffset,
				&iSent
			) == XNET_RESULT_OK &&
			(iSent != 0),
			"HTTP server middleware send failed"
		);
		iOffset += iSent;
	}
}



/* 接收 Connection: close 的完整响应。 */
static void testHttpServerMiddlewareReceive(
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
			"HTTP server middleware response overflow"
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
			"HTTP server middleware receive failed"
		);
		iSize += iReceived;
	}
	sResponse[iSize] = '\0';
}



/* 执行一次真实 Select TCP 回环并等待最外层中间件退出。 */
static void testHttpServerMiddlewareRoundTrip(
	const xnetaddr* pAddress,
	test_http_server_middleware_context* pContext,
	cstr sRequest,
	cstr sStatus,
	cstr sOrder,
	uint32 iCompleted
)
{
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);
	char Response[4096];
	xdeadline Deadline;

	pContext->OrderSize = 0;
	pContext->Order[0] = '\0';
	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket, pAddress
		 ) == XNET_RESULT_OK),
		"HTTP server middleware connect failed"
	);
	testHttpServerMiddlewareSend(Socket, sRequest);
	testHttpServerMiddlewareReceive(
		Socket, Response, sizeof(Response)
	);
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP server middleware socket close failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(
		&pContext->Completed, XMEMORY_ACQUIRE
	) != iCompleted ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server middleware callback completion timed out"
		);
		xrtThreadYield();
	}
	testRequire(
		(strstr(Response, sStatus) != NULL) &&
		(strcmp(pContext->Order, sOrder) == 0),
		"HTTP server middleware response or order mismatch"
	);
}



/* 验证冻结、洋葱顺序、短路、404 包裹、双 Next 和数据生命周期。 */
int main(void)
{
	test_http_server_middleware_context Context;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverrouter* pRouter;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xdeadline Deadline;

	memset(&Context, 0, sizeof(Context));
	xrtAtomic32Init(&Context.Completed, 0);
	xrtAtomic32Init(&Context.Released, 0);
	pRouter = xrtHttpServerRouterCreate(NULL);
	testRequire(
		(pRouter != NULL) &&
		xrtHttpServerUseOwned(
			pRouter,
			testHttpServerMiddlewareOuter,
			&Context,
			testHttpServerMiddlewareRelease
		) && xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareMiddle,
			&Context
		) && xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareInner,
			&Context
		) && xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/items/{id}"),
			testHttpServerMiddlewareRoute,
			&Context
		) && (xrtHttpServerMiddlewareCount(pRouter) == 3u) &&
		xrtHttpServerRouterFreeze(pRouter),
		"HTTP server middleware Router setup failed"
	);
	testRequire(
		!xrtHttpServerUse(
			pRouter,
			testHttpServerMiddlewareMiddle,
			&Context
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP server middleware accepted registration after freeze"
	);
	xrtClearError();
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP server middleware engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server middleware loopback address failed"
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 1u;
	pServer = xrtHttpServerRouterStart(
		pEngine, &ServerConfig, pRouter, NULL
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server middleware runtime start failed"
	);

	testHttpServerMiddlewareRoundTrip(
		&Address,
		&Context,
		"GET /items/42 HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Connection: close\r\n\r\n",
		"HTTP/1.1 200 OK",
		"ABCRDEF",
		1u
	);
	testRequire(
		Context.SawParam && Context.SawResponseAfterNext,
		"HTTP server middleware parameters or post-response state missing"
	);
	testHttpServerMiddlewareRoundTrip(
		&Address,
		&Context,
		"GET /stop HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Connection: close\r\n\r\n",
		"HTTP/1.1 403 Forbidden",
		"ABCDEF",
		2u
	);
	testHttpServerMiddlewareRoundTrip(
		&Address,
		&Context,
		"POST /missing HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Content-Length: 4\r\n"
		"Connection: close\r\n\r\nDATA",
		"HTTP/1.1 404 Not Found",
		"ABCDEF",
		3u
	);
	testHttpServerMiddlewareRoundTrip(
		&Address,
		&Context,
		"GET /double HTTP/1.1\r\n"
		"Host: x.test\r\n"
		"Connection: close\r\n\r\n",
		"HTTP/1.1 500 Internal Server Error",
		"ABCDEF",
		4u
	);

	xrtHttpServerRouterDestroy(pRouter);
	testRequire(
		xrtAtomic32Load(
			&Context.Released, XMEMORY_ACQUIRE
		) == 0,
		"HTTP server middleware data released while Server retained Router"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server middleware drain failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtHttpServerState(pServer) !=
		XHTTP_SERVER_CLOSED ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server middleware shutdown timed out"
		);
		xrtThreadYield();
	}
	while ( xrtAtomic32Load(
		&Context.Released, XMEMORY_ACQUIRE
	) != 1 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server middleware owned data release timed out"
		);
		xrtThreadYield();
	}
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server middleware engine destroy failed"
	);
	puts("[PASS] HTTP server middleware");
	return 0;
}
