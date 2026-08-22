#include "../test.h"



typedef struct test_http_server_future_job {
	uint16 Status;
	cstr Body;
	cstr Source;
} test_http_server_future_job;



typedef struct test_http_server_future_sources {
	xtaskpool* Pool;
	test_http_server_future_job Task;
	test_http_server_future_job Coroutine;
	xatomic32 Requests;
	xatomic32 Errors;
	xatomic32 Closed;
	xatomic32 Shutdown;
} test_http_server_future_sources;



/* Future 释放任务结果时销毁其拥有的 Reply。 */
static void testHttpServerFutureSourceReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 在线程池或协程中构造一个拥有型 Reply 任务结果。 */
static xtaskoutcome testHttpServerFutureSourceRun(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	const test_http_server_future_job* pJob =
		(const test_http_server_future_job*)pData;
	xhttpreply* pReply;

	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	if ( xrtCoCurrent() != NULL ) {
		if ( xrtCoSleep(UINT64_C(1000)) ==
			XWAIT_CANCELLED ) {
			return XTASK_CANCELLED;
		}
	} else {
		xrtSleep(10);
	}
	if ( xrtCancelRequested(pCancel) ) {
		return XTASK_CANCELLED;
	}
	pReply = xrtHttpReplyCreate(pJob->Status);
	if ( (pReply == NULL) ||
		!xrtHttpReplySetBytes(
			pReply,
			(xbytesview){
				(cbytes)pJob->Body,
				strlen(pJob->Body)
			},
			XRT_STR_LITERAL("text/plain")
		) ||
		!xrtHttpReplySetHeader(
			pReply,
			XRT_STR_LITERAL("X-Source"),
			(xstrview){
				pJob->Source,
				strlen(pJob->Source)
			}
		) ) {
		xrtHttpReplyDestroy(pReply);
		return XTASK_FAILED;
	}
	pResult->Value = pReply;
	pResult->Destroy =
		testHttpServerFutureSourceReplyFree;
	return XTASK_SUCCESS;
}



/* 根据请求目标产生线程池或协程 Future，并交给同一个 HTTP 桥接 API。 */
static void testHttpServerFutureSourceRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	test_http_server_future_sources* pState =
		(test_http_server_future_sources*)pData;
	xstrview Target =
		xrtHttpServerRequestTarget(pRequest);
	xfuture* pFuture = NULL;
	xcosched* pSched = NULL;
	bool bCoroutine = false;
	bool bRan = true;

	(void)pServer;
	if ( (Target.Size == 5u) &&
		(memcmp(Target.Data, "/task", 5u) == 0) ) {
		pFuture = xrtTaskSubmit(
			pState->Pool,
			testHttpServerFutureSourceRun,
			&pState->Task,
			NULL
		);
	} else if ( (Target.Size == 10u) &&
		(memcmp(
			Target.Data,
			"/coroutine",
			10u
		 ) == 0) ) {
		bCoroutine = true;
		pSched = xrtCoSchedCreate();
		if ( pSched != NULL ) {
			pFuture = xrtTaskCo(
				pSched,
				testHttpServerFutureSourceRun,
				&pState->Coroutine,
				NULL,
				0
			);
			bRan = (pFuture != NULL) &&
				xrtCoSchedRun(pSched);
		}
	}
	if ( pSched != NULL ) {
		testRequire(
			xrtCoSchedDestroy(pSched),
			"HTTP server coroutine scheduler destroy failed"
		);
		testRequire(
			xrtCoThreadDetach(),
			"HTTP server coroutine thread detach failed"
		);
	}
	testRequire(
		(pFuture != NULL) &&
		bRan &&
		xrtHttpConnRespondFuture(
			pConnection,
			pFuture
		),
		bCoroutine ?
			"HTTP server coroutine Future binding failed" :
			"HTTP server task Future binding failed"
	);
	xrtFutureDestroy(pFuture);
	(void)xrtAtomic32FetchAdd(
		&pState->Requests,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录来源组合测试中不应出现的 HTTP 错误。 */
static void testHttpServerFutureSourceError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_future_sources* pState =
		(test_http_server_future_sources*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pState->Errors,
		1,
		XMEMORY_RELEASE
	);
}



/* 记录每个来源请求的连接关闭事件。 */
static void testHttpServerFutureSourceClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	test_http_server_future_sources* pState =
		(test_http_server_future_sources*)pData;

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



/* 发布来源组合测试的 Server 排空终态。 */
static void testHttpServerFutureSourceShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	test_http_server_future_sources* pState =
		(test_http_server_future_sources*)pData;

	testRequire(
		xrtHttpServerState(pServer) ==
		XHTTP_SERVER_CLOSED,
		"HTTP server Future source shutdown mismatch"
	);
	xrtAtomic32Store(
		&pState->Shutdown,
		1,
		XMEMORY_RELEASE
	);
}



/* 在截止时间前等待 Worker 发布指定计数。 */
static void testHttpServerFutureSourceWait(
	const xatomic32* pValue,
	uint32 iExpected,
	cstr sMessage
)
{
	xdeadline Deadline =
		xrtDeadlineAfter(UINT64_C(10000000));

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



/* 完整发送测试请求，避免短写掩盖 Future 来源行为。 */
static void testHttpServerFutureSourceSend(
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
			"HTTP server Future source send failed"
		);
		iOffset += iSent;
	}
}



/* 请求一个来源路径，并核对状态、Header、正文与连接终止。 */
static void testHttpServerFutureSourceCall(
	const xnetaddr* pAddress,
	cstr sPath,
	cstr sStatus,
	cstr sSource,
	cstr sBody
)
{
	char Request[256];
	char Response[2048];
	char Header[128];
	int iRequest;
	int iHeader;
	size_t iOffset = 0;
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
		"HTTP server Future source connect failed"
	);
	iRequest = snprintf(
		Request,
		sizeof(Request),
		"GET %s HTTP/1.1\r\n"
		"Host: server.test\r\n"
		"Connection: close\r\n"
		"\r\n",
		sPath
	);
	testRequire(
		(iRequest > 0) &&
		((size_t)iRequest < sizeof(Request)),
		"HTTP server Future source request overflowed"
	);
	testHttpServerFutureSourceSend(
		Socket,
		(cbytes)Request,
		(size_t)iRequest
	);
	while ( iOffset < (sizeof(Response) - 1u) ) {
		size_t iRead = 0;
		xnetresult Result = xrtNetSocketRecv(
			Socket,
			Response + iOffset,
			sizeof(Response) - iOffset - 1u,
			&iRead
		);

		if ( Result == XNET_RESULT_CLOSED ) {
			break;
		}
		testRequire(
			(Result == XNET_RESULT_OK) &&
			(iRead != 0),
			"HTTP server Future source receive failed"
		);
		iOffset += iRead;
	}
	Response[iOffset] = '\0';
	iHeader = snprintf(
		Header,
		sizeof(Header),
		"\r\nX-Source: %s\r\n",
		sSource
	);
	testRequire(
		(iOffset != 0) &&
		(iHeader > 0) &&
		((size_t)iHeader < sizeof(Header)) &&
		(strstr(Response, sStatus) != NULL) &&
		(strstr(Response, Header) != NULL) &&
		(strstr(Response, sBody) != NULL),
		"HTTP server Future source response mismatch"
	);
	testRequire(
		xrtNetSocketClose(Socket),
		"HTTP server Future source close failed"
	);
}



/* 验证线程池任务与协程任务都直接产生通用 Future，不扩张 HTTP API。 */
int main(void)
{
	test_http_server_future_sources State;
	xtaskpoolconfig PoolConfig;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents Events;
	xhttpserverstats Stats;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xnetaddr Address;

	memset(&State, 0, sizeof(State));
	State.Task.Status = 202;
	State.Task.Body = "task";
	State.Task.Source = "task-pool";
	State.Coroutine.Status = 203;
	State.Coroutine.Body = "coroutine";
	State.Coroutine.Source = "coroutine";
	xrtAtomic32Init(&State.Requests, 0);
	xrtAtomic32Init(&State.Errors, 0);
	xrtAtomic32Init(&State.Closed, 0);
	xrtAtomic32Init(&State.Shutdown, 0);

	memset(&PoolConfig, 0, sizeof(PoolConfig));
	PoolConfig.Threads = 2;
	State.Pool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		State.Pool != NULL,
		"HTTP server Future task pool create failed"
	);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP server Future source engine start failed"
	);
	xrtHttpServerConfigInit(&ServerConfig);
	testRequire(
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		),
		"HTTP server Future source address failed"
	);
	xrtHttpServerEventsInit(&Events);
	Events.Request = testHttpServerFutureSourceRequest;
	Events.Error = testHttpServerFutureSourceError;
	Events.Close = testHttpServerFutureSourceClose;
	Events.Shutdown =
		testHttpServerFutureSourceShutdown;
	Events.Data = &State;
	pServer = xrtHttpServerStart(
		pEngine,
		&ServerConfig,
		&Events
	);
	testRequire(
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address),
		"HTTP server Future source start failed"
	);

	testHttpServerFutureSourceCall(
		&Address,
		"/task",
		"HTTP/1.1 202 Accepted\r\n",
		"task-pool",
		"\r\n\r\ntask"
	);
	testHttpServerFutureSourceCall(
		&Address,
		"/coroutine",
		"HTTP/1.1 203 Non-Authoritative Information\r\n",
		"coroutine",
		"\r\n\r\ncoroutine"
	);
	testHttpServerFutureSourceWait(
		&State.Requests,
		2,
		"HTTP server Future source request count mismatch"
	);
	testHttpServerFutureSourceWait(
		&State.Closed,
		2,
		"HTTP server Future source close count mismatch"
	);
	testRequire(
		xrtHttpServerDrain(pServer),
		"HTTP server Future source drain failed"
	);
	testHttpServerFutureSourceWait(
		&State.Shutdown,
		1,
		"HTTP server Future source shutdown missing"
	);
	testRequire(
		xrtHttpServerStats(pServer, &Stats) &&
		(Stats.Accepted == 2) &&
		(Stats.Requests == 2) &&
		(Stats.Responses == 2) &&
		(Stats.ProtocolErrors == 0) &&
		(Stats.Timeouts == 0) &&
		(Stats.Connections == 0) &&
		(xrtAtomic32Load(
			&State.Errors,
			XMEMORY_ACQUIRE
		 ) == 0),
		"HTTP server Future source statistics mismatch"
	);

	xrtHttpServerDestroy(pServer);
	testRequire(
		xrtTaskPoolDestroy(State.Pool),
		"HTTP server Future task pool destroy failed"
	);
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP server Future source engine destroy failed"
	);
	printf(
		"[PASS] HTTP server task and coroutine Future sources\n"
	);
	return 0;
}
