#include "../fixtures/http_origin.h"



#ifndef TEST_WS_HTTP_FUTURE_BACKEND
	#define TEST_WS_HTTP_FUTURE_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_WS_HTTP_FUTURE_BACKEND_NAME
	#define TEST_WS_HTTP_FUTURE_BACKEND_NAME "select"
#endif

#ifndef TEST_WS_HTTP_FUTURE_TLS
	#define TEST_WS_HTTP_FUTURE_TLS 0
#endif

#if TEST_WS_HTTP_FUTURE_TLS
	#include "../fixtures/tls_server.h"
#endif



typedef struct test_ws_http_future {
	xnetengine* Engine;
	xhttpserver* Server;
	xhttpclient* Client;
	xatomicptr ServerFuture;
	xatomic32 CancelNext;
	xatomic32 Requests;
	xatomic32 Shutdown;
	xwsconnevents WsEvents;
	char Url[160];
	size_t UrlSize;
	char HttpUrl[160];
	size_t HttpUrlSize;
} test_ws_http_future;



/* 在固定截止时间前等待一个原子计数达到目标值。 */
static void testWsHttpFutureWait(
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



/* 确认 HTTP 适配器按测试模式转移了唯一的底层传输。 */
static bool testWsHttpFutureTransport(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return false;
	}
	#if TEST_WS_HTTP_FUTURE_TLS
		xtlsstream* pStream = xrtWsConnTlsRef(
			pConnection
		);

		if ( pStream == NULL ) {
			return false;
		}
		xrtTlsStreamDestroy(pStream);
		return true;
	#else
		xnetstream* pStream = xrtWsConnTcpRef(
			pConnection
		);

		if ( pStream == NULL ) {
			return false;
		}
		xrtNetStreamDestroy(pStream);
		return true;
	#endif
}



/* 等待 Request Worker 发布当前唯一服务端 Upgrade Future。 */
static xfuture* testWsHttpFutureTakeServer(
	test_ws_http_future* pTest
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);
	xfuture* pFuture;

	do {
		pFuture = (xfuture*)xrtAtomicPtrExchange(
			&pTest->ServerFuture,
			NULL,
			XMEMORY_ACQ_REL
		);
		if ( pFuture == NULL ) {
			testRequire(
				!xrtDeadlineExpired(Deadline),
				"WebSocket server Future was not published"
			);
			xrtThreadYield();
		}
	} while ( pFuture == NULL );
	return pFuture;
}



/* HTTP Request Worker 使用服务端 Future 适配器提交 Upgrade。 */
static void testWsHttpFutureRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_ws_http_future* pTest =
		(test_ws_http_future*)pData;
	xwsserverconfig Config;
	xfuture* pFuture;
	ptr pPrevious;

	(void)pServer;
	testRequire(
		(xrtHttpServerRequestFlags(pRequest) &
		 XHTTP_SERVER_REQUEST_UPGRADE) != 0,
		"WebSocket Future request omitted Upgrade flag"
	);
	xrtWsServerConfigInit(&Config);
	Config.Protocols = XRT_STR_LITERAL(
		"future.v2, future.v1"
	);
	pFuture = xrtWsUpgradeAsync(
		pHttp,
		&Config,
		&pTest->WsEvents,
		pTest
	);
	testRequire(
		pFuture != NULL,
		"WebSocket server Future submission failed"
	);
	if ( xrtAtomic32Exchange(
		&pTest->CancelNext,
		0,
		XMEMORY_ACQ_REL
	) != 0 ) {
		testRequire(
			xrtFutureCancel(pFuture),
			"WebSocket server Future cancel failed"
		);
	}
	pPrevious = xrtAtomicPtrExchange(
		&pTest->ServerFuture,
		pFuture,
		XMEMORY_ACQ_REL
	);
	testRequire(
		pPrevious == NULL,
		"WebSocket server Future slot was still occupied"
	);
	(void)xrtAtomic32FetchAdd(
		&pTest->Requests,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 HTTP Server 已经完成排空。 */
static void testWsHttpFutureShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_ws_http_future* pTest =
		(test_ws_http_future*)pData;

	(void)pServer;
	xrtAtomic32Store(
		&pTest->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 创建带相同子协议策略的客户端配置。 */
static void testWsHttpFutureClientConfig(
	xwsclientconfig* pConfig
)
{
	xrtWsClientConfigInit(pConfig);
	pConfig->Protocols = XRT_STR_LITERAL(
		"future.v1, future.v2"
	);
}



/* 创建当前测试服务器 URL 对应的自定义 GET 请求。 */
static xhttprequest* testWsHttpFutureRequestCreate(
	const test_ws_http_future* pTest
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview) {
			pTest->HttpUrl,
			pTest->HttpUrlSize
		}
	);

	testRequire(
		pRequest != NULL,
		"WebSocket custom request create failed"
	);
	testRequire(
		xrtHttpRequestSetHeader(
			pRequest,
			XRT_STR_LITERAL("X-Future-Test"),
			XRT_STR_LITERAL("request")
		),
		"WebSocket custom request header failed"
	);
	return pRequest;
}



/* 等待两端 Connection 都释放底层传输。 */
static void testWsHttpFutureConnectionsWait(
	xwsconn* pClient,
	xwsconn* pServer,
	cstr sMessage
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( (xrtWsConnState(pClient) !=
		 XWS_CONN_CLOSED) ||
		(xrtWsConnState(pServer) !=
		 XWS_CONN_CLOSED) ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			sMessage
		);
		xrtThreadYield();
	}
}



/* 结束调用方已经取走的客户端和服务端 Connection。 */
static void testWsHttpFutureConnectionsDestroy(
	xwsconn* pClient,
	xwsconn* pServer
)
{
	testRequire(
		(pClient != NULL) &&
		(pServer != NULL),
		"WebSocket Future did not transfer both connections"
	);
	(void)xrtWsConnAbort(pClient);
	(void)xrtWsConnAbort(pServer);
	testWsHttpFutureConnectionsWait(
		pClient,
		pServer,
		"WebSocket Future connections did not close"
	);
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
}



typedef enum test_ws_http_future_mode {
	TEST_WS_HTTP_FUTURE_URL,
	TEST_WS_HTTP_FUTURE_REQUEST,
	TEST_WS_HTTP_FUTURE_SYNC
} test_ws_http_future_mode;



/*
	验证 URL、定制 Request 或同步入口共享相同的结果和所有权契约。
	RetainResult 额外证明结果可以脱离 Future 生命周期继续使用。
*/
static void testWsHttpFutureOpen(
	test_ws_http_future* pTest,
	test_ws_http_future_mode Mode,
	bool RetainResult
)
{
	xwsclientconfig Config;
	xhttprequest* pRequest = NULL;
	xfuture* pClientFuture = NULL;
	xfuture* pServerFuture;
	xwsopenresult* pClientResult;
	xwsopenresult* pServerResult;
	xwsconn* pClient;
	xwsconn* pServer;
	xhttpresponse* pResponse;

	testWsHttpFutureClientConfig(&Config);
	if ( Mode == TEST_WS_HTTP_FUTURE_URL ) {
		pClientFuture = xrtWsConnectAsync(
			pTest->Client,
			(xstrview) {
				pTest->Url,
				pTest->UrlSize
			},
			&Config,
			&pTest->WsEvents,
			pTest
		);
		pClientResult = NULL;
	} else if ( Mode == TEST_WS_HTTP_FUTURE_REQUEST ) {
		pRequest = testWsHttpFutureRequestCreate(
			pTest
		);
		pClientFuture = xrtWsConnectRequestAsync(
			pTest->Client,
			pRequest,
			&Config,
			&pTest->WsEvents,
			pTest
		);
		xrtHttpRequestDestroy(pRequest);
		pClientResult = NULL;
	} else {
		pClientResult = xrtWsConnectSync(
			pTest->Client,
			(xstrview) {
				pTest->Url,
				pTest->UrlSize
			},
			&Config,
			&pTest->WsEvents,
			pTest
		);
	}
	testRequire(
		((Mode == TEST_WS_HTTP_FUTURE_SYNC) &&
		 (pClientResult != NULL)) ||
		((Mode != TEST_WS_HTTP_FUTURE_SYNC) &&
		 (pClientFuture != NULL)),
		"WebSocket client Future submission failed"
	);
	pServerFuture = testWsHttpFutureTakeServer(pTest);
	testRequire(
		(xrtFutureWaitFor(
			pServerFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pServerFuture) ==
		 XFUTURE_RESOLVED),
		"WebSocket server Future did not resolve"
	);
	pServerResult = (xwsopenresult*)xrtFutureValue(
		pServerFuture
	);
	if ( pClientFuture != NULL ) {
		testRequire(
			(xrtFutureWaitFor(
				pClientFuture,
				UINT64_C(10000000)
			 ) == XWAIT_OK) &&
			(xrtFutureState(pClientFuture) ==
			 XFUTURE_RESOLVED),
			"WebSocket client Future did not resolve"
		);
		pClientResult =
			(xwsopenresult*)xrtFutureValue(
				pClientFuture
			);
	}
	testRequire(
		(pClientResult != NULL) &&
		(pServerResult != NULL) &&
		testWsHttpFutureTransport(
			xrtWsOpenResultConnection(pClientResult)
		) &&
		testWsHttpFutureTransport(
			xrtWsOpenResultConnection(pServerResult)
		) &&
		(xrtWsOpenResultResponse(
			pServerResult
		 ) == NULL) &&
		(xrtHttpResponseStatus(
			xrtWsOpenResultResponse(
				pClientResult
			)
		 ) == XHTTP_STATUS_SWITCHING_PROTOCOLS),
		"WebSocket Future result content mismatch"
	);
	if ( RetainResult ) {
		pClientResult = xrtWsOpenResultRef(
			pClientResult
		);
		testRequire(
			pClientResult != NULL,
			"WebSocket result retain failed"
		);
		xrtFutureDestroy(pClientFuture);
		pClientFuture = NULL;
	}
	pClient = xrtWsOpenResultTakeConnection(
		pClientResult
	);
	pServer = xrtWsOpenResultTakeConnection(
		pServerResult
	);
	pResponse = xrtWsOpenResultTakeResponse(
		pClientResult
	);
	testRequire(
		(pResponse != NULL) &&
		(xrtWsOpenResultConnection(
			pClientResult
		 ) == NULL) &&
		(xrtWsOpenResultResponse(
			pClientResult
		 ) == NULL),
		"WebSocket Future Take ownership mismatch"
	);
	xrtFutureDestroy(pClientFuture);
	if ( Mode == TEST_WS_HTTP_FUTURE_SYNC ) {
		xrtWsOpenResultDestroy(pClientResult);
	} else if ( RetainResult ) {
		xrtWsOpenResultDestroy(pClientResult);
	}
	xrtFutureDestroy(pServerFuture);
	xrtHttpResponseDestroy(pResponse);
	testWsHttpFutureConnectionsDestroy(
		pClient,
		pServer
	);
}



typedef struct test_ws_http_future_coroutine {
	test_ws_http_future* Test;
	xwaitresult Wait;
	xfuturestate State;
	xwsopenresult* Result;
	bool Entered;
	bool Returned;
} test_ws_http_future_coroutine;



/* 在协程中提交连接并直接等待通用 Future。 */
static ptr testWsHttpFutureCoroutineProc(ptr pData)
{
	test_ws_http_future_coroutine* pState =
		(test_ws_http_future_coroutine*)pData;
	xwsclientconfig Config;
	xfuture* pFuture;

	pState->Entered = true;
	testWsHttpFutureClientConfig(&Config);
	pFuture = xrtWsConnectAsync(
		pState->Test->Client,
		(xstrview) {
			pState->Test->Url,
			pState->Test->UrlSize
		},
		&Config,
		&pState->Test->WsEvents,
		pState->Test
	);
	testRequire(
		pFuture != NULL,
		"WebSocket coroutine Future submission failed"
	);
	pState->Wait = xrtFutureAwaitFor(
		pFuture,
		UINT64_C(10000000)
	);
	pState->State = xrtFutureState(pFuture);
	if ( (pState->Wait == XWAIT_OK) &&
		(pState->State == XFUTURE_RESOLVED) ) {
		pState->Result = xrtWsOpenResultRef(
			(xwsopenresult*)xrtFutureValue(pFuture)
		);
	}
	xrtFutureDestroy(pFuture);
	pState->Returned = true;
	return pState;
}



/* 验证连接 Future 可直接进入通用协程调度器，不增加专用 Co API。 */
static void testWsHttpFutureCoroutine(
	test_ws_http_future* pTest
)
{
	test_ws_http_future_coroutine State;
	xfuture* pServerFuture;
	xwsopenresult* pServerResult;
	xhttpresponse* pResponse;
	xwsconn* pClient;
	xwsconn* pServer;
	xcosched* pSched;
	xcoro* pCoroutine;

	memset(&State, 0, sizeof(State));
	State.Test = pTest;
	pSched = xrtCoSchedCreate();
	testRequire(
		pSched != NULL,
		"WebSocket coroutine scheduler create failed"
	);
	pCoroutine = xrtCoSpawn(
		pSched,
		testWsHttpFutureCoroutineProc,
		&State,
		NULL
	);
	testRequire(
		(pCoroutine != NULL) &&
		xrtCoSchedRun(pSched),
		"WebSocket coroutine run failed"
	);
	pServerFuture = testWsHttpFutureTakeServer(pTest);
	testRequire(
		State.Entered &&
		State.Returned &&
		(State.Wait == XWAIT_OK) &&
		(State.State == XFUTURE_RESOLVED) &&
		(State.Result != NULL) &&
		(xrtFutureWaitFor(
			pServerFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pServerFuture) ==
		 XFUTURE_RESOLVED),
		"WebSocket coroutine Future terminal mismatch"
	);
	pServerResult = (xwsopenresult*)xrtFutureValue(
		pServerFuture
	);
	pClient = xrtWsOpenResultTakeConnection(
		State.Result
	);
	pServer = xrtWsOpenResultTakeConnection(
		pServerResult
	);
	pResponse = xrtWsOpenResultTakeResponse(
		State.Result
	);
	testRequire(
		(pResponse != NULL) &&
		testWsHttpFutureTransport(pClient) &&
		testWsHttpFutureTransport(pServer),
		"WebSocket coroutine result ownership mismatch"
	);

	testRequire(
		xrtCoDestroy(pCoroutine) &&
		xrtCoSchedDestroy(pSched),
		"WebSocket coroutine scheduler destroy failed"
	);
	xrtWsOpenResultDestroy(State.Result);
	xrtFutureDestroy(pServerFuture);
	xrtHttpResponseDestroy(pResponse);
	testWsHttpFutureConnectionsDestroy(
		pClient,
		pServer
	);
}



/* 丢弃未取走结果必须自动中止两端连接，不能留下孤儿会话。 */
static void testWsHttpFutureUnclaimed(
	test_ws_http_future* pTest
)
{
	xwsclientconfig Config;
	xfuture* pClientFuture;
	xfuture* pServerFuture;
	xwsopenresult* pClientResult;
	xwsopenresult* pServerResult;
	xwsconn* pClient;
	xwsconn* pServer;

	testWsHttpFutureClientConfig(&Config);
	pClientFuture = xrtWsConnectAsync(
		pTest->Client,
		(xstrview) {
			pTest->Url,
			pTest->UrlSize
		},
		&Config,
		&pTest->WsEvents,
		pTest
	);
	testRequire(
		pClientFuture != NULL,
		"WebSocket unclaimed Future submission failed"
	);
	pServerFuture = testWsHttpFutureTakeServer(pTest);
	testRequire(
		(xrtFutureWaitFor(
			pClientFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClientFuture) ==
		 XFUTURE_RESOLVED) &&
		(xrtFutureWaitFor(
			pServerFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pServerFuture) ==
		 XFUTURE_RESOLVED),
		"WebSocket unclaimed Future did not resolve"
	);
	pClientResult = (xwsopenresult*)xrtFutureValue(
		pClientFuture
	);
	pServerResult = (xwsopenresult*)xrtFutureValue(
		pServerFuture
	);
	pClient = xrtWsConnRef(
		xrtWsOpenResultConnection(pClientResult)
	);
	pServer = xrtWsConnRef(
		xrtWsOpenResultConnection(pServerResult)
	);
	testRequire(
		(pClient != NULL) &&
		(pServer != NULL),
		"WebSocket unclaimed observer retain failed"
	);
	xrtFutureDestroy(pClientFuture);
	xrtFutureDestroy(pServerFuture);
	testWsHttpFutureConnectionsWait(
		pClient,
		pServer,
		"WebSocket unclaimed results did not abort sessions"
	);
	xrtWsConnDestroy(pClient);
	xrtWsConnDestroy(pServer);
}



/* 非 101 HTTP 响应必须保留 WebSocket 握手错误分类。 */
static void testWsHttpFutureRejected(
	test_ws_http_future* pTest
)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	testhttporigin Origin;
	xwsclientconfig Config;
	const xerror* pError;
	xfuture* pFuture;
	char Url[128];
	int iLength;

	testHttpOriginStart(
		&Origin,
		pTest->Engine,
		Wire,
		sizeof(Wire) - 1u
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://127.0.0.1:%u/rejected",
		(unsigned)testHttpOriginPort(&Origin)
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"WebSocket rejected URL overflowed"
	);
	testWsHttpFutureClientConfig(&Config);
	pFuture = xrtWsConnectAsync(
		pTest->Client,
		(xstrview) { Url, (size_t)iLength },
		&Config,
		&pTest->WsEvents,
		pTest
	);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_FAILED),
		"WebSocket rejected Future terminal mismatch"
	);
	pError = xrtFutureError(pFuture);
	testRequire(
		(pError != NULL) &&
		(xrtErrorKind(pError) == XERR_PROTOCOL) &&
		(xrtErrorCode(pError) ==
		 XWS_HANDSHAKE_ERROR_STATUS) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.websocket.handshake"
		 ) == 0),
		"WebSocket rejected Future error mismatch"
	);
	xrtFutureDestroy(pFuture);
	testHttpOriginStop(&Origin);
}



/* 父取消令牌必须推进挂起 HTTP Call 并形成 Future 取消终态。 */
static void testWsHttpFutureParentCancel(
	test_ws_http_future* pTest
)
{
	testhttporigin Origin;
	xwsclientconfig Config;
	xcancel* pCancel;
	xfuture* pFuture;
	char Url[128];
	int iLength;

	testHttpOriginStart(
		&Origin,
		pTest->Engine,
		NULL,
		0
	);
	iLength = snprintf(
		Url,
		sizeof(Url),
		"ws://127.0.0.1:%u/cancel",
		(unsigned)testHttpOriginPort(&Origin)
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"WebSocket cancel URL overflowed"
	);
	testWsHttpFutureClientConfig(&Config);
	pCancel = xrtCancelCreate();
	testRequire(
		pCancel != NULL,
		"WebSocket parent cancel create failed"
	);
	Config.Http.Cancel = pCancel;
	pFuture = xrtWsConnectAsync(
		pTest->Client,
		(xstrview) { Url, (size_t)iLength },
		&Config,
		&pTest->WsEvents,
		pTest
	);
	testRequire(
		pFuture != NULL,
		"WebSocket parent-cancel Future submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"WebSocket parent-cancel request did not reach origin"
	);
	testRequire(
		xrtCancelRequest(pCancel),
		"WebSocket parent cancellation failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pFuture) ==
		 XFUTURE_CANCELLED),
		"WebSocket parent cancellation terminal mismatch"
	);
	xrtFutureDestroy(pFuture);
	xrtCancelDestroy(pCancel);
	testHttpOriginStop(&Origin);
}



/* 服务端取消 Future 必须关闭当前 HTTP Upgrade 并发布取消终态。 */
static void testWsHttpFutureServerCancel(
	test_ws_http_future* pTest
)
{
	xwsclientconfig Config;
	xfuture* pClientFuture;
	xfuture* pServerFuture;

	testWsHttpFutureClientConfig(&Config);
	xrtAtomic32Store(
		&pTest->CancelNext,
		1,
		XMEMORY_RELEASE
	);
	pClientFuture = xrtWsConnectAsync(
		pTest->Client,
		(xstrview) {
			pTest->Url,
			pTest->UrlSize
		},
		&Config,
		&pTest->WsEvents,
		pTest
	);
	testRequire(
		pClientFuture != NULL,
		"WebSocket server-cancel client submission failed"
	);
	pServerFuture = testWsHttpFutureTakeServer(pTest);
	testRequire(
		(xrtFutureWaitFor(
			pServerFuture,
			UINT64_C(10000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pServerFuture) ==
		 XFUTURE_CANCELLED),
		"WebSocket server Future cancellation mismatch"
	);
	testRequire(
		xrtFutureWaitFor(
			pClientFuture,
			UINT64_C(10000000)
		) == XWAIT_OK,
		"WebSocket cancelled peer did not finish"
	);
	xrtFutureDestroy(pClientFuture);
	xrtFutureDestroy(pServerFuture);
}



typedef struct test_ws_http_sync_worker {
	test_ws_http_future* Test;
	xatomic32 Done;
	xerrkind Kind;
	int32 Code;
} test_ws_http_sync_worker;



/* 在网络 Worker 中验证同步入口拒绝自我阻塞。 */
static void testWsHttpFutureSyncTask(
	xnetworker* pWorker,
	ptr pData
)
{
	test_ws_http_sync_worker* pState =
		(test_ws_http_sync_worker*)pData;
	xwsopenresult* pResult;
	const xerror* pError;

	(void)pWorker;
	pResult = xrtWsConnectSync(
		pState->Test->Client,
		(xstrview) {
			pState->Test->Url,
			pState->Test->UrlSize
		},
		NULL,
		NULL,
		NULL
	);
	pError = xrtGetError();
	pState->Kind = pError != NULL ?
		xrtErrorKind(pError) : XERR_NONE;
	pState->Code = pError != NULL ?
		xrtErrorCode(pError) : 0;
	xrtWsOpenResultDestroy(pResult);
	xrtClearError();
	xrtAtomic32Store(
		&pState->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 网络 Worker 的同步调用必须在提交 HTTP 请求前失败。 */
static void testWsHttpFutureSyncWorker(
	test_ws_http_future* pTest
)
{
	test_ws_http_sync_worker State;
	uint32 iBefore = xrtAtomic32Load(
		&pTest->Requests,
		XMEMORY_ACQUIRE
	);

	memset(&State, 0, sizeof(State));
	State.Test = pTest;
	xrtAtomic32Init(&State.Done, 0);
	testRequire(
		xrtNetEnginePost(
			pTest->Engine,
			0,
			testWsHttpFutureSyncTask,
			&State
		),
		"WebSocket sync Worker task post failed"
	);
	testWsHttpFutureWait(
		&State.Done,
		1,
		"WebSocket sync Worker task did not finish"
	);
	testRequire(
		(State.Kind == XERR_STATE) &&
		(State.Code == XWS_HANDSHAKE_ERROR_UPGRADE) &&
		(xrtAtomic32Load(
			&pTest->Requests,
			XMEMORY_ACQUIRE
		 ) == iBefore),
		"WebSocket sync Worker deadlock guard mismatch"
	);
}



/* 验证结果和入口的空参数边界。 */
static void testWsHttpFutureInvalid(void)
{
	xhttpclient* pWrappingClient =
		(xhttpclient*)(uintptr_t)(UINTPTR_MAX - 1u);
	xhttpconn* pWrappingConnection =
		(xhttpconn*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsserverconfig* pWrappingServerConfig =
		(const xwsserverconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsconnevents* pWrappingEvents =
		(const xwsconnevents*)(uintptr_t)(UINTPTR_MAX - 1u);

	xrtClearError();
	testRequire(
		(xrtWsOpenResultRef(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket result null retain mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultConnection(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket result null connection mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsOpenResultTakeResponse(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket result null response mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnectAsync(
			NULL,
			XRT_STR_LITERAL("ws://127.0.0.1/"),
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket null client Future mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsUpgradeAsync(
			NULL,
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_ARGUMENT),
		"WebSocket null server Future mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnectAsync(
			pWrappingClient,
			XRT_STR_LITERAL("ws://127.0.0.1/"),
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping client Future mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnectSync(
			pWrappingClient,
			XRT_STR_LITERAL("ws://127.0.0.1/"),
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping client sync mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsUpgradeAsync(
			pWrappingConnection,
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping server Future mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsUpgradeAsync(
			NULL,
			pWrappingServerConfig,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping server Future config mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsUpgradeAsync(
			NULL,
			NULL,
			pWrappingEvents,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping server Future events mismatch"
	);
	xrtClearError();
}



/* 验证完整运行时入口不会提前解引用回绕配置、事件或请求。 */
static void testWsHttpFutureRuntimeInvalid(
	test_ws_http_future* pTest
)
{
	const xwsclientconfig* pWrappingConfig =
		(const xwsclientconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xwsconnevents* pWrappingEvents =
		(const xwsconnevents*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xhttprequest* pWrappingRequest =
		(const xhttprequest*)(uintptr_t)(UINTPTR_MAX - 1u);

	xrtClearError();
	testRequire(
		(xrtWsConnectAsync(
			pTest->Client,
			(xstrview) { pTest->Url, pTest->UrlSize },
			pWrappingConfig,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping Future config mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnectAsync(
			pTest->Client,
			(xstrview) { pTest->Url, pTest->UrlSize },
			NULL,
			pWrappingEvents,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping Future events mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtWsConnectRequestAsync(
			pTest->Client,
			pWrappingRequest,
			NULL,
			NULL,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"WebSocket wrapping Future request mismatch"
	);
	xrtClearError();
}



/* 等待全部异步析构退出 Engine。 */
static void testWsHttpFutureEngineDestroy(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(
		UINT64_C(10000000)
	);

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"WebSocket Future retained an Engine object"
		);
		xrtThreadYield();
	}
}



/* 覆盖连接建立 Future、同步桥、取消竞态和所有权回收。 */
int main(void)
{
	#if TEST_WS_HTTP_FUTURE_TLS
		static const xstrview TlsProtocols[] = {
			XRT_STR_INIT("http/1.1")
		};
	#endif
	test_ws_http_future Test;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	#if TEST_WS_HTTP_FUTURE_TLS
		xhttpservertlsconfig ServerTls;
		xtlsverifierconfig VerifierConfig;
		xtlscontext* pTlsContext;
		xtlsidentity* pTlsIdentity;
		xtlsverifier* pTlsVerifier;
	#endif
	xnetaddr Address;
	int iLength;

	testWsHttpFutureInvalid();
	#if TEST_WS_HTTP_FUTURE_TLS
		pTlsContext = testTlsServerContext();
		pTlsIdentity = testTlsServerIdentity();
		testRequire(
			(pTlsContext != NULL) &&
			(pTlsIdentity != NULL),
			"WebSocket Future TLS fixture creation failed"
		);
		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = testTlsServerAccept;
		pTlsVerifier = xrtTlsVerifierCreate(
			&VerifierConfig
		);
		testRequire(
			pTlsVerifier != NULL,
			"WebSocket Future TLS verifier creation failed"
		);
	#endif
	memset(&Test, 0, sizeof(Test));
	xrtAtomicPtrInit(&Test.ServerFuture, NULL);
	xrtAtomic32Init(&Test.CancelNext, 0);
	xrtAtomic32Init(&Test.Requests, 0);
	xrtAtomic32Init(&Test.Shutdown, 0);
	memset(&Test.WsEvents, 0, sizeof(Test.WsEvents));

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_WS_HTTP_FUTURE_BACKEND;
	EngineConfig.Workers = 2;
	Test.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(Test.Engine != NULL) &&
		xrtNetEngineStart(Test.Engine),
		"WebSocket Future engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"WebSocket Future server address failed"
	);
	xrtHttpServerEventsInit(&ServerEvents);
	ServerEvents.Request = testWsHttpFutureRequest;
	ServerEvents.Shutdown = testWsHttpFutureShutdown;
	ServerEvents.Data = &Test;
	#if TEST_WS_HTTP_FUTURE_TLS
		xrtHttpServerTlsConfigInit(&ServerTls);
		ServerTls.Handshake.Context = pTlsContext;
		ServerTls.Handshake.Identity = pTlsIdentity;
		ServerTls.Handshake.Protocols = TlsProtocols;
		ServerTls.Handshake.ProtocolCount =
			sizeof(TlsProtocols) /
			sizeof(TlsProtocols[0]);
		ServerTls.Handshake.RequireProtocol = true;
		Test.Server = xrtHttpServerStartTls(
			Test.Engine,
			&ServerConfig,
			&ServerTls,
			&ServerEvents
		);
	#else
		Test.Server = xrtHttpServerStart(
			Test.Engine,
			&ServerConfig,
			&ServerEvents
		);
	#endif
	testRequire(
		(Test.Server != NULL) &&
		xrtHttpServerLocal(Test.Server, 0, &Address),
		"WebSocket Future server start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	#if TEST_WS_HTTP_FUTURE_TLS
		ClientConfig.TlsContext = pTlsContext;
		ClientConfig.TlsVerifier = pTlsVerifier;
		ClientConfig.SystemTrust = false;
	#endif
	Test.Client = xrtHttpClientCreate(
		Test.Engine,
		&ClientConfig
	);
	testRequire(
		Test.Client != NULL,
		"WebSocket Future client create failed"
	);
	iLength = snprintf(
		Test.Url,
		sizeof(Test.Url),
		#if TEST_WS_HTTP_FUTURE_TLS
			"wss://127.0.0.1:%u/future",
		#else
			"ws://127.0.0.1:%u/future",
		#endif
		(unsigned)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Test.Url)),
		"WebSocket Future URL overflowed"
	);
	Test.UrlSize = (size_t)iLength;
	iLength = snprintf(
		Test.HttpUrl,
		sizeof(Test.HttpUrl),
		#if TEST_WS_HTTP_FUTURE_TLS
			"https://127.0.0.1:%u/future",
		#else
			"http://127.0.0.1:%u/future",
		#endif
		(unsigned)Address.Port
	);
	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Test.HttpUrl)),
		"WebSocket Future HTTP URL overflowed"
	);
	Test.HttpUrlSize = (size_t)iLength;
	testWsHttpFutureRuntimeInvalid(&Test);

	testWsHttpFutureOpen(
		&Test,
		TEST_WS_HTTP_FUTURE_URL,
		true
	);
	testWsHttpFutureOpen(
		&Test,
		TEST_WS_HTTP_FUTURE_REQUEST,
		false
	);
	testWsHttpFutureOpen(
		&Test,
		TEST_WS_HTTP_FUTURE_SYNC,
		false
	);
	testWsHttpFutureCoroutine(&Test);
	testWsHttpFutureUnclaimed(&Test);
	testWsHttpFutureRejected(&Test);
	testWsHttpFutureParentCancel(&Test);
	testWsHttpFutureServerCancel(&Test);
	testWsHttpFutureSyncWorker(&Test);

	testRequire(
		xrtHttpServerDrain(Test.Server),
		"WebSocket Future server drain failed"
	);
	testWsHttpFutureWait(
		&Test.Shutdown,
		1,
		"WebSocket Future server did not drain"
	);
	xrtHttpClientDestroy(Test.Client);
	xrtHttpServerDestroy(Test.Server);
	#if TEST_WS_HTTP_FUTURE_TLS
		xrtTlsVerifierRelease(pTlsVerifier);
		xrtTlsIdentityRelease(pTlsIdentity);
		xrtTlsContextRelease(pTlsContext);
	#endif
	testWsHttpFutureEngineDestroy(Test.Engine);
	printf(
		"[PASS] WebSocket HTTP Future/sync contract (%s)\n",
		TEST_WS_HTTP_FUTURE_BACKEND_NAME
	);
	return 0;
}
