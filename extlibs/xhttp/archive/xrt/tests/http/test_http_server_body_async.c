#include "../test.h"



#ifndef TEST_HTTP_SERVER_BODY_ASYNC_BACKEND
	#define TEST_HTTP_SERVER_BODY_ASYNC_BACKEND \
		XNET_PORT_SELECT
#endif

#ifndef TEST_HTTP_SERVER_BODY_ASYNC_BACKEND_NAME
	#define TEST_HTTP_SERVER_BODY_ASYNC_BACKEND_NAME \
		"select"
#endif



typedef enum test_http_server_body_async_case {
	TEST_HTTP_BODY_ASYNC_DELAYED = 0,
	TEST_HTTP_BODY_ASYNC_READY,
	TEST_HTTP_BODY_ASYNC_FAILED,
	TEST_HTTP_BODY_ASYNC_WAIT_FAILED,
	TEST_HTTP_BODY_ASYNC_TIMEOUT,
	TEST_HTTP_BODY_ASYNC_DISCONNECT
} test_http_server_body_async_case;



/* 所有场景串行复用同一状态，由原子计数发布 Worker 进度。 */
typedef struct test_http_server_body_async {
	xatomic32 Scenario;
	xatomic32 Ready;
	xatomic32 Emitted;
	xatomic32 Nexts;
	xatomic32 Waits;
	xatomic32 Requested;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 BodyClosed;
	xatomic32 Shutdown;
	xpromise* Promise;
	xcancel* Cancel;
	xerrkind ErrorKind;
	int32 ErrorCode;
	char ErrorOperation[64];
} test_http_server_body_async;



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerBodyAsyncWait(
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
static void testHttpServerBodyAsyncWaitStats(
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
			"HTTP server async body statistics query failed"
		);
		if ( pStats->Connections == 0 ) {
			return;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP server async body statistics did not settle"
		);
		xrtThreadYield();
	}
}



/* 静态正文租约不需要真实回收。 */
static void testHttpServerBodyAsyncReleaseChunk(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 未就绪时返回 AGAIN，就绪后仅发布一次固定正文。 */
static xhttpbodystatus testHttpServerBodyAsyncNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pContext;
	static const unsigned char Body[] = "async-body";
	uint32 iOffset;
	size_t iTake;

	(void)xrtAtomic32FetchAdd(
		&pState->Nexts,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pState->Ready,
		XMEMORY_ACQUIRE
	) == 0 ) {
		return XHTTP_BODY_AGAIN;
	}
	iOffset = xrtAtomic32Load(
		&pState->Emitted,
		XMEMORY_RELAXED
	);
	if ( iOffset == 10 ) {
		return XHTTP_BODY_EOF;
	}
	iTake = 10u - (size_t)iOffset;
	if ( iTake > iMaxBytes ) {
		iTake = iMaxBytes;
	}
	xrtAtomic32Store(
		&pState->Emitted,
		iOffset + (uint32)iTake,
		XMEMORY_RELAXED
	);
	pChunk->Data = Body + iOffset;
	pChunk->Size = iTake;
	pChunk->Release =
		testHttpServerBodyAsyncReleaseChunk;
	return XHTTP_BODY_DATA;
}



/*
	为本次 AGAIN 创建唯一 Future。
	READY 场景在返回前完成 Future，用于覆盖 waiter 安装竞态。
*/
static xfuture* testHttpServerBodyAsyncWaitSource(
	ptr pContext
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pContext;
	test_http_server_body_async_case Scenario =
		(test_http_server_body_async_case)
		xrtAtomic32Load(
			&pState->Scenario,
			XMEMORY_ACQUIRE
		);
	xfuture* pFuture = NULL;

	if ( Scenario == TEST_HTTP_BODY_ASYNC_WAIT_FAILED ) {
		xerror* pError = xrtErrorCreate(
			XERR_IO,
			"test.http.server.body.wait",
			93,
			"wait creation failed"
		);

		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		(void)xrtAtomic32FetchAdd(
			&pState->Waits,
			1,
			XMEMORY_RELEASE
		);
		return NULL;
	}
	pState->Promise = xrtPromiseCreate(
		&pFuture,
		NULL
	);
	if ( pState->Promise == NULL ) {
		return NULL;
	}
	pState->Cancel = xrtFutureCancelToken(pFuture);
	if ( pState->Cancel == NULL ) {
		xrtPromiseDestroy(pState->Promise);
		pState->Promise = NULL;
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	if ( Scenario == TEST_HTTP_BODY_ASYNC_READY ) {
		xrtAtomic32Store(
			&pState->Ready,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtPromiseResolve(
			pState->Promise,
			NULL
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pState->Waits,
		1,
		XMEMORY_RELEASE
	);
	return pFuture;
}



/* 记录每个 Response Reader 的唯一关闭。 */
static void testHttpServerBodyAsyncCloseSource(
	ptr pContext
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pContext;

	(void)xrtAtomic32FetchAdd(
		&pState->BodyClosed,
		1,
		XMEMORY_RELEASE
	);
}



/* 打开共享状态对应的单次异步 Reader。 */
static bool testHttpServerBodyAsyncOpenSource(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = testHttpServerBodyAsyncNext;
	pOps->Close = testHttpServerBodyAsyncCloseSource;
	pOps->Wait = testHttpServerBodyAsyncWaitSource;
	*ppReader = pFactory;
	return true;
}



/* 为每条请求提交带异步正文的最终响应。 */
static void testHttpServerBodyAsyncRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	static const xhttpbodyops Ops = {
		testHttpServerBodyAsyncOpenSource,
		NULL
	};
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pData;
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops,
		pState,
		10,
		XHTTP_BODY_NONE
	);
	xhttpreply* pReply = xrtHttpReplyCreate(200);

	(void)pServer;
	testRequire(
		xrtHttpServerRequestTarget(pRequest).Size != 0,
		"HTTP server async body target is empty"
	);
	testRequire(
		(pBody != NULL) &&
		(pReply != NULL) &&
		xrtHttpReplySetBody(pReply, pBody) &&
		xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain")
		) &&
		(xrtHttpConnRespond(
			pConnection,
			pReply
		 ) == XNET_RESULT_OK),
		"HTTP server async body response submission failed"
	);
	xrtHttpReplyDestroy(pReply);
	xrtHttpBodyDestroy(pBody);
	(void)xrtAtomic32FetchAdd(
		&pState->Requested,
		1,
		XMEMORY_RELEASE
	);
}



/* 保存最近一次异步正文或写时限错误。 */
static void testHttpServerBodyAsyncError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pData;
	cstr sOperation;
	size_t iSize;

	(void)pServer;
	(void)pConnection;
	testRequire(
		pError != NULL,
		"HTTP server async body error is null"
	);
	pState->ErrorKind = xrtErrorKind(pError);
	pState->ErrorCode = xrtErrorCode(pError);
	sOperation = xrtErrorOperation(pError);
	iSize = strlen(sOperation);
	testRequire(
		iSize < sizeof(pState->ErrorOperation),
		"HTTP server async body operation is too long"
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



/* 记录每条测试连接的唯一关闭终态。 */
static void testHttpServerBodyAsyncClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pData;

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



/* 记录 Server 已经排空并关闭。 */
static void testHttpServerBodyAsyncShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_body_async* pState =
		(test_http_server_body_async*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server async body shutdown state mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 完整发送一条短请求。 */
static void testHttpServerBodyAsyncSend(
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
			"HTTP server async body request send failed"
		);
		iOffset += iSent;
	}
}



/* 连接 Server 并发送带 close 语义的 GET 请求。 */
static xnetsocket testHttpServerBodyAsyncOpen(
	const xnetaddr* pAddress,
	cstr sPath
)
{
	char Request[256];
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
		"HTTP server async body client connect failed"
	);
	iSize = snprintf(
		Request,
		sizeof(Request),
		"GET %s HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		sPath
	);
	testRequire(
		(iSize > 0) &&
		((size_t)iSize < sizeof(Request)),
		"HTTP server async body request format failed"
	);
	testHttpServerBodyAsyncSend(
		Socket,
		(cbytes)Request,
		(size_t)iSize
	);
	return Socket;
}



/* 读取到 Server 关闭连接并返回零结尾响应。 */
static size_t testHttpServerBodyAsyncReceive(
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
			"HTTP server async body response receive failed"
		);
		iOffset += iRead;
	}
	pOutput[iOffset] = '\0';
	return iOffset;
}



/* 准备下一条串行场景，上一条连接必须已经关闭。 */
static void testHttpServerBodyAsyncBegin(
	test_http_server_body_async* pState,
	test_http_server_body_async_case Scenario
)
{
	testRequire(
		(pState->Promise == NULL) &&
		(pState->Cancel == NULL),
		"HTTP server async body source was not released"
	);
	xrtAtomic32Store(
		&pState->Scenario,
		(uint32)Scenario,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Ready,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Emitted,
		0,
		XMEMORY_RELEASE
	);
}



/* 释放测试方持有的 Promise 与取消令牌。 */
static void testHttpServerBodyAsyncRelease(
	test_http_server_body_async* pState
)
{
	xrtPromiseDestroy(pState->Promise);
	xrtCancelDestroy(pState->Cancel);
	pState->Promise = NULL;
	pState->Cancel = NULL;
}



/* 核对最近一次结构化错误。 */
static void testHttpServerBodyAsyncRequireError(
	test_http_server_body_async* pState,
	uint32 iExpected,
	xerrkind Kind,
	int32 iCode,
	cstr sOperation
)
{
	testHttpServerBodyAsyncWait(
		&pState->Errors,
		iExpected,
		"HTTP server async body error missing"
	);
	testRequire(
		(pState->ErrorKind == Kind) &&
		(pState->ErrorCode == iCode) &&
		(strcmp(
			pState->ErrorOperation,
			sOperation
		 ) == 0),
		"HTTP server async body error contract mismatch"
	);
}



/* 覆盖异步正文恢复、失败、超时和断线取消。 */
int main(void)
{
	test_http_server_body_async State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;
	xnetsocket Client;
	xerror* pFailure;
	char Response[4096];
	uint32 iRequest = 0;
	uint32 iWait = 0;
	uint32 iClose = 0;
	uint32 iBodyClose = 0;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(
		&State.Scenario,
		TEST_HTTP_BODY_ASYNC_DELAYED
	);
	xrtAtomic32Init(&State.Ready, 0);
	xrtAtomic32Init(&State.Emitted, 0);
	xrtAtomic32Init(&State.Nexts, 0);
	xrtAtomic32Init(&State.Waits, 0);
	xrtAtomic32Init(&State.Requested, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.BodyClosed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend =
		TEST_HTTP_SERVER_BODY_ASYNC_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server async body engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server async body address setup failed"
	);
	ServerConfig.WriteSize = 3;
	ServerConfig.WriteTimeout = UINT64_C(500000);
	ServerConfig.RequestTimeout = UINT64_C(3000000);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerBodyAsyncRequest;
	Events.Error = testHttpServerBodyAsyncError;
	Events.Close = testHttpServerBodyAsyncClose;
	Events.Shutdown = testHttpServerBodyAsyncShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server async body start failed"
	);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_DELAYED
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/delayed"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server delayed body request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server delayed body wait missing"
	);
	testRequire(
		xrtAtomic32Load(
			&State.Nexts,
			XMEMORY_ACQUIRE
		) == 1,
		"HTTP server re-entered delayed body source"
	);
	xrtSleep(20);
	testRequire(
		xrtAtomic32Load(
			&State.Nexts,
			XMEMORY_ACQUIRE
		) == 1,
		"HTTP server spun while body was not ready"
	);
	xrtAtomic32Store(
		&State.Ready,
		1,
		XMEMORY_RELEASE
	);
	testRequire(
		xrtPromiseResolve(State.Promise, NULL),
		"HTTP server delayed body completion failed"
	);
	testRequire(
		(testHttpServerBodyAsyncReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(Response, "HTTP/1.1 200 OK\r\n") != NULL) &&
		(strstr(Response, "\r\n\r\nasync-body") != NULL),
		"HTTP server delayed body response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server delayed body client close failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server delayed body connection did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server delayed body Reader did not close"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_READY
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/ready"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server ready body request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server ready body wait missing"
	);
	testRequire(
		(testHttpServerBodyAsyncReceive(
			Client,
			Response,
			sizeof(Response)
		 ) != 0) &&
		(strstr(Response, "\r\n\r\nasync-body") != NULL),
		"HTTP server ready body response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server ready body client close failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server ready body connection did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server ready body Reader did not close"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_FAILED
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server failed body request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server failed body wait missing"
	);
	pFailure = xrtErrorCreate(
		XERR_IO,
		"test.http.server.body",
		92,
		"body readiness failed"
	);
	testRequire(
		(pFailure != NULL) &&
		xrtPromiseReject(State.Promise, pFailure),
		"HTTP server body Future rejection failed"
	);
	xrtErrorFree(pFailure);
	testHttpServerBodyAsyncRequireError(
		&State,
		1,
		XERR_IO,
		XHTTP_SERVER_ERROR_RESPONSE,
		"wait-http-response-body"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server failed body connection did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server failed body Reader did not close"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server failed body client close failed"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_WAIT_FAILED
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/wait-failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server body wait failure request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server body wait creation was not attempted"
	);
	testHttpServerBodyAsyncRequireError(
		&State,
		2,
		XERR_IO,
		XHTTP_SERVER_ERROR_RESPONSE,
		"read-http-response-body"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server body wait failure did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server failed wait Reader did not close"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server body wait failure client close failed"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_TIMEOUT
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/timeout"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server timed body request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server timed body wait missing"
	);
	testHttpServerBodyAsyncRequireError(
		&State,
		3,
		XERR_TIMEOUT,
		XHTTP_SERVER_ERROR_TIMEOUT_WRITE,
		"write-http-response"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server timed body connection did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server timed body Reader did not close"
	);
	testRequire(
		xrtCancelRequested(State.Cancel) &&
		xrtPromiseCancel(State.Promise),
		"HTTP server timed body Future was not cancelled"
	);
	testRequire(
		xrtNetSocketClose(Client),
		"HTTP server timed body client close failed"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncBegin(
		&State,
		TEST_HTTP_BODY_ASYNC_DISCONNECT
	);
	Client = testHttpServerBodyAsyncOpen(
		&Address,
		"/disconnect"
	);
	testHttpServerBodyAsyncWait(
		&State.Requested,
		++iRequest,
		"HTTP server disconnected body request missing"
	);
	testHttpServerBodyAsyncWait(
		&State.Waits,
		++iWait,
		"HTTP server disconnected body wait missing"
	);
	testRequire(
		xrtNetSocketSet(
			Client,
			XNET_OPTION_LINGER,
			0
		) &&
		xrtNetSocketClose(Client),
		"HTTP server async body RST failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Closed,
		++iClose,
		"HTTP server disconnected body did not close"
	);
	testHttpServerBodyAsyncWait(
		&State.BodyClosed,
		++iBodyClose,
		"HTTP server disconnected body Reader did not close"
	);
	testRequire(
		xrtCancelRequested(State.Cancel) &&
		xrtPromiseCancel(State.Promise),
		"HTTP server disconnect did not cancel body Future"
	);
	testHttpServerBodyAsyncRelease(&State);

	testHttpServerBodyAsyncWaitStats(pServer, &Stats);
	if ( (Stats.Accepted != 6) ||
		(Stats.Requests != 6) ||
		(Stats.Responses != 2) ||
		(Stats.Timeouts != 1) ||
		(Stats.Connections != 0) ) {
		fprintf(
			stderr,
			"[async-body-stats] accepted=%llu requests=%llu "
			"responses=%llu timeouts=%llu connections=%llu\n",
			(unsigned long long)Stats.Accepted,
			(unsigned long long)Stats.Requests,
			(unsigned long long)Stats.Responses,
			(unsigned long long)Stats.Timeouts,
			(unsigned long long)Stats.Connections
		);
	}
	testRequire(
		(Stats.Accepted == 6) &&
		(Stats.Requests == 6) &&
		(Stats.Responses == 2) &&
		(Stats.Timeouts == 1) &&
		(Stats.Connections == 0),
		"HTTP server async body statistics mismatch"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server async body drain failed"
	);
	testHttpServerBodyAsyncWait(
		&State.Shutdown,
		1,
		"HTTP server async body shutdown missing"
	);
	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server async body engine destroy failed"
	);
	printf(
		"[PASS] HTTP server async body (%s)\n",
		TEST_HTTP_SERVER_BODY_ASYNC_BACKEND_NAME
	);
	return 0;
}
