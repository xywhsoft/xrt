#include "../test.h"



#ifndef TEST_HTTP_SERVER_BACKEND
	#define TEST_HTTP_SERVER_BACKEND XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_BACKEND_NAME
	#define TEST_HTTP_SERVER_BACKEND_NAME "select"
#endif



typedef enum test_http_server_future_case {
	TEST_HTTP_FUTURE_SUCCESS = 0,
	TEST_HTTP_FUTURE_READY,
	TEST_HTTP_FUTURE_FAILED,
	TEST_HTTP_FUTURE_FAILED_TIMEOUT,
	TEST_HTTP_FUTURE_CANCELLED,
	TEST_HTTP_FUTURE_REQUEST_TIMEOUT,
	TEST_HTTP_FUTURE_MANUAL,
	TEST_HTTP_FUTURE_HALF_CLOSE,
	TEST_HTTP_FUTURE_DISCONNECT,
	TEST_HTTP_FUTURE_ABORT
} test_http_server_future_case;



typedef struct test_http_server_future {
	xatomic32 Scenario;
	xatomic32 Requested;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
	xpromise* Promise;
	xfuture* Future;
	xcancel* Cancel;
	xerrkind ErrorKind;
	int32 ErrorCode;
	char ErrorOperation[64];
} test_http_server_future;



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerFutureWait(
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



/* 完整发送一个短 HTTP 请求。 */
static void testHttpServerFutureSend(
	xnetsocket Socket,
	cbytes pData,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		size_t iSent = 0;

		testRequire(
			(xrtNetSocketSend(
				Socket,
				pData + iOffset,
				iSize - iOffset,
				&iSent
			 ) == XNET_RESULT_OK) &&
			(iSent != 0),
			"HTTP server Future request send failed"
		);
		iOffset += iSent;
	}
}



/* 读取到服务端关闭连接，并返回零结尾响应。 */
static size_t testHttpServerFutureReceive(
	xnetsocket Socket,
	char* pOutput,
	size_t iCapacity
)
{
	size_t iOffset = 0;

	while ( iOffset < (iCapacity - 1) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			pOutput + iOffset,
			iCapacity - iOffset - 1,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server Future response receive failed"
		);
		iOffset += iRead;
	}
	pOutput[iOffset] = '\0';
	return iOffset;
}



/* Future 最后释放时销毁其拥有的 Reply。 */
static void testHttpServerFutureReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 建立一个由测试线程完成的响应 Future。 */
static void testHttpServerFutureRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_future* pState =
		(test_http_server_future*)pData;
	test_http_server_future_case Scenario =
		(test_http_server_future_case)xrtAtomic32Load(
			&pState->Scenario,
			XMEMORY_ACQUIRE
		);
	xhttpreply* pReply;

	(void)pServer;
	testRequire(
		xrtHttpServerRequestTarget(pRequest).Size != 0,
		"HTTP server Future request target is empty"
	);
	if ( Scenario == TEST_HTTP_FUTURE_SUCCESS ) {
		xstrview Method =
			xrtHttpServerRequestMethod(pRequest);
		xbytesview Body =
			xrtHttpServerRequestBody(pRequest);

		testRequire(
			(Method.Size == 4u) &&
			(memcmp(Method.Data, "POST", 4u) == 0) &&
			(Body.Size == 11u) &&
			(memcmp(
				Body.Data,
				"hello-async",
				11u
			 ) == 0),
			"HTTP server Future POST body mismatch"
		);
	}
	pState->Promise = xrtPromiseCreate(
		&pState->Future,
		NULL
	);
	testRequire(
		(pState->Promise != NULL) &&
		(pState->Future != NULL),
		"HTTP server Future pair creation failed"
	);
	pState->Cancel = xrtFutureCancelToken(
		pState->Future
	);
	testRequire(
		pState->Cancel != NULL,
		"HTTP server Future cancel token creation failed"
	);
	if ( Scenario == TEST_HTTP_FUTURE_READY ) {
		pReply = xrtHttpReplyCreate(201);
		testRequire(
			(pReply != NULL) &&
			xrtHttpReplySetBytes(
				pReply,
				XRT_BYTES_LITERAL("ready"),
				XRT_STR_LITERAL("text/plain")
			) &&
			xrtHttpReplySetHeader(
				pReply,
				XRT_STR_LITERAL("X-Async"),
				XRT_STR_LITERAL("ready")
			) &&
			xrtPromiseResolveOwned(
				pState->Promise,
				pReply,
				testHttpServerFutureReplyFree,
				NULL
			),
			"HTTP server ready Future setup failed"
		);
	}
	testRequire(
		xrtHttpConnRespondFuture(
			pConnection,
			pState->Future
		),
		"HTTP server Future binding failed"
	);
	testRequire(
		!xrtHttpConnRespondFuture(
			pConnection,
			pState->Future
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"HTTP server accepted two response Futures"
	);
	xrtClearError();
	if ( Scenario == TEST_HTTP_FUTURE_MANUAL ) {
		testRequire(
			xrtHttpConnReply(
				pConnection,
				200,
				XRT_STR_LITERAL("text/plain"),
				XRT_BYTES_LITERAL("manual")
			) == XNET_RESULT_OK,
			"HTTP server manual response did not replace Future"
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存 Future 失败映射的唯一结构化错误。 */
static void testHttpServerFutureError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_future* pState =
		(test_http_server_future*)pData;
	cstr sOperation;
	size_t iSize;

	(void)pServer;
	(void)pConnection;
	testRequire(
		pError != NULL,
		"HTTP server Future error is null"
	);
	pState->ErrorKind = xrtErrorKind(pError);
	pState->ErrorCode = xrtErrorCode(pError);
	sOperation = xrtErrorOperation(pError);
	iSize = strlen(sOperation);
	testRequire(
		iSize < sizeof(pState->ErrorOperation),
		"HTTP server Future operation is too long"
	);
	memcpy(
		pState->ErrorOperation,
		sOperation,
		iSize + 1
	);
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 等待并核对最近一次 Future 或请求时限错误。 */
static void testHttpServerFutureRequireError(
	test_http_server_future* pState,
	uint32 iExpected,
	xerrkind Kind,
	int32 iCode,
	cstr sOperation
)
{
	testHttpServerFutureWait(
		&pState->Errors,
		iExpected,
		"HTTP server Future error missing"
	);
	testRequire(
		(pState->ErrorKind == Kind) &&
		(pState->ErrorCode == iCode) &&
		(strcmp(
			pState->ErrorOperation,
			sOperation
		 ) == 0),
		"HTTP server Future error contract mismatch"
	);
}



/* 记录每条 Future 请求的连接终态。 */
static void testHttpServerFutureClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_future* pState =
		(test_http_server_future*)pData;

	(void)pServer;
	(void)pConnection;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Closed,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录 Server 的排空终态。 */
static void testHttpServerFutureShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_future* pState =
		(test_http_server_future*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server Future shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 打开并发送当前 Future 场景请求。 */
static xnetsocket testHttpServerFutureOpen(
	const xnetaddr* pAddress,
	cstr sPath,
	cstr sBody
)
{
	char Request[512];
	int iSize;
	xnetsocket Socket = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_STREAM,
		0
	);

	testRequire(
		(Socket != NULL) &&
		(xrtNetSocketConnect(
			Socket,
			pAddress
		 ) == XNET_RESULT_OK),
		"HTTP server Future client connect failed"
	);
	if ( sBody != NULL ) {
		iSize = snprintf(
			Request,
			sizeof(Request),
			"POST %s HTTP/1.1\r\n"
			"Host: server.test\r\n"
			"Connection: close\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%s",
			sPath,
			strlen(sBody),
			sBody
		);
	} else {
		iSize = snprintf(
			Request,
			sizeof(Request),
			"GET %s HTTP/1.1\r\n"
			"Host: server.test\r\n"
			"Connection: close\r\n"
			"\r\n",
			sPath
		);
	}
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(Request)),
		"HTTP server Future request format failed"
	);
	testHttpServerFutureSend(
		Socket,
		(cbytes)Request,
		(size_t)iSize
	);
	return Socket;
}



/* 释放当前场景由测试持有的 Future 端点。 */
static void testHttpServerFutureRelease(
	test_http_server_future* pState
)
{
	xrtPromiseDestroy(pState->Promise);
	xrtFutureDestroy(pState->Future);
	xrtCancelDestroy(pState->Cancel);
	pState->Promise = NULL;
	pState->Future = NULL;
	pState->Cancel = NULL;
}



int main(void)
{
	test_http_server_future State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;
	xhttpreply* pReply;
	xerror* pFailure;
	char Response[4096];
	uint32 iRequest = 0;
	uint32 iClose = 0;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(
		&State.Scenario,
		TEST_HTTP_FUTURE_SUCCESS
	);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_HTTP_SERVER_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server Future engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server Future address setup failed"
	);
	ServerConfig.RequestTimeout = UINT64_C(1000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerFutureRequest;
	Events.Error = testHttpServerFutureError;
	Events.Close = testHttpServerFutureClose;
	Events.Shutdown = testHttpServerFutureShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server Future start failed"
	);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_SUCCESS,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/success",
		"hello-async"
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server delayed Future request missing"
	);
	pReply = xrtHttpReplyCreate(202);
	testRequire(
		(pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("delayed"),
			XRT_STR_LITERAL("text/plain")
		) &&
		xrtPromiseResolveOwned(
			State.Promise,
			pReply,
			testHttpServerFutureReplyFree,
			NULL
		),
		"HTTP server delayed Future completion failed"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 202 Accepted\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\ndelayed") != NULL),
		"HTTP server delayed Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server delayed Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server delayed Future connection did not close"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_READY,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/ready",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server ready Future request missing"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 201 Created\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\nX-Async: ready\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\nready") != NULL),
		"HTTP server ready Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server ready Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server ready Future connection did not close"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_FAILED,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/failed",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server failed Future request missing"
	);
	pFailure = xrtErrorCreate(
		XERR_IO,
		"test.http.future",
		77,
		"background response failed"
	);
	testRequire(
		(pFailure != NULL) &&
		xrtPromiseReject(State.Promise, pFailure),
		"HTTP server failed Future rejection failed"
	);
	xrtErrorFree(pFailure);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 500 Internal Server Error\r\n"
		 ) != NULL),
		"HTTP server failed Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server failed Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server failed Future connection did not close"
	);
	testHttpServerFutureRequireError(
		&State,
		1,
		XERR_IO,
		XHTTP_SERVER_ERROR_CALLBACK,
		"complete-http-response-future"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_FAILED_TIMEOUT,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/failed-timeout",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server timeout Future request missing"
	);
	pFailure = xrtErrorCreate(
		XERR_TIMEOUT,
		"test.http.future",
		78,
		"background response timed out"
	);
	testRequire(
		(pFailure != NULL) &&
		xrtPromiseReject(State.Promise, pFailure),
		"HTTP server timeout Future rejection failed"
	);
	xrtErrorFree(pFailure);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 504 Gateway Timeout\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\nGateway Timeout"
		 ) != NULL),
		"HTTP server timeout Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server timeout Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server timeout Future connection did not close"
	);
	testHttpServerFutureRequireError(
		&State,
		2,
		XERR_TIMEOUT,
		XHTTP_SERVER_ERROR_CALLBACK,
		"complete-http-response-future"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_CANCELLED,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/cancelled",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server cancelled result request missing"
	);
	testRequire(
		xrtPromiseCancel(State.Promise),
		"HTTP server Future cancellation failed"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 503 Service Unavailable\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\nService Unavailable"
		 ) != NULL),
		"HTTP server cancelled Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server cancelled result client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server cancelled result connection did not close"
	);
	testHttpServerFutureRequireError(
		&State,
		3,
		XERR_CANCELLED,
		XHTTP_SERVER_ERROR_CALLBACK,
		"complete-http-response-future"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_REQUEST_TIMEOUT,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/request-timeout",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server request timeout Future request missing"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 504 Gateway Timeout\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\nGateway Timeout"
		 ) != NULL),
		"HTTP server request timeout response mismatch"
	);
	testRequire(
		xrtCancelRequested(State.Cancel),
		"HTTP server request timeout did not cancel Future"
	);
	testRequire(
		xrtPromiseCancel(State.Promise),
		"HTTP server timed out Future cancellation failed"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server request timeout client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server request timeout connection did not close"
	);
	testHttpServerFutureRequireError(
		&State,
		4,
		XERR_TIMEOUT,
		XHTTP_SERVER_ERROR_TIMEOUT_REQUEST,
		"wait-http-response"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_MANUAL,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/manual",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server manual Future request missing"
	);
	testRequire(
		xrtCancelRequested(State.Cancel),
		"HTTP server manual response did not cancel Future"
	);
	testRequire(
		xrtPromiseCancel(State.Promise),
		"HTTP server manual Future cancellation failed"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 200 OK\r\n"
		 ) != NULL) &&
		(strstr(Response, "\r\n\r\nmanual") != NULL),
		"HTTP server manual Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server manual Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server manual Future connection did not close"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_HALF_CLOSE,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/half-close",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server half-close Future request missing"
	);
	testRequire(
		xrtNetSocketShutdown(
			Client,
			XNET_SHUTDOWN_WRITE
		),
		"HTTP server Future client half-close failed"
	);
	pReply = xrtHttpReplyCreate(200);
	testRequire(
		(pReply != NULL) &&
		xrtHttpReplySetBytes(
			pReply,
			XRT_BYTES_LITERAL("half-close"),
			XRT_STR_LITERAL("text/plain")
		) &&
		xrtPromiseResolveOwned(
			State.Promise,
			pReply,
			testHttpServerFutureReplyFree,
			NULL
		),
		"HTTP server half-close Future completion failed"
	);
	testRequire(
		(testHttpServerFutureReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(
			Response,
			"HTTP/1.1 200 OK\r\n"
		 ) != NULL) &&
		(strstr(
			Response,
			"\r\n\r\nhalf-close"
		 ) != NULL),
		"HTTP server half-close Future response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server half-close Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server half-close Future connection did not close"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_DISCONNECT,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/cancel",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server cancelled Future request missing"
	);
	testRequire(
		xrtNetSocketSet(
			Client,
			XNET_OPTION_LINGER,
			0
		),
		"HTTP server abortive client setup failed"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server cancelled Future client close failed"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server cancelled Future connection did not close"
	);
	testRequire(
		xrtCancelRequested(State.Cancel),
		"HTTP server disconnect did not cancel Future"
	);
	testRequire(
		xrtPromiseCancel(State.Promise),
		"HTTP server disconnected Future cancellation failed"
	);
	testHttpServerFutureRelease(&State);

	xrtAtomic32Store(
		&State.Scenario,
		TEST_HTTP_FUTURE_ABORT,
		XMEMORY_RELEASE
	);
	Client = testHttpServerFutureOpen(
		&Address,
		"/abort",
		NULL
	);
	testHttpServerFutureWait(
		&State.Requested,
		++iRequest,
		"HTTP server abort Future request missing"
	);
	testRequire(
		xrtHttpServerAbort(pServer),
		"HTTP server Future abort failed"
	);
	testHttpServerFutureWait(
		&State.Shutdown,
		1,
		"HTTP server Future shutdown missing"
	);
	testHttpServerFutureWait(
		&State.Closed,
		++iClose,
		"HTTP server aborted Future connection did not close"
	);
	testRequire(
		xrtCancelRequested(State.Cancel),
		"HTTP server abort did not cancel Future"
	);
	testRequire(
		xrtPromiseCancel(State.Promise),
		"HTTP server aborted Future cancellation failed"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server aborted Future client close failed"
	);
	testHttpServerFutureRelease(&State);

	testRequire(
		xrtHttpServerStats(pServer, &Stats),
		"HTTP server Future statistics query failed"
	);
	testRequire(
		(Stats.Accepted == 10) &&
		(Stats.Requests == 10) &&
		(Stats.Responses == 8) &&
		(Stats.ProtocolErrors == 1) &&
		(Stats.Timeouts == 1) &&
		(Stats.Connections == 0),
		"HTTP server Future statistics mismatch"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server Future engine destroy failed"
	);
	printf(
		"[PASS] HTTP server Future response bridge (%s)\n",
		TEST_HTTP_SERVER_BACKEND_NAME
	);
	return 0;
}
