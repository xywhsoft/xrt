#include "../test.h"
#include "../fixtures/http_origin.h"



#if !defined(TEST_HTTP_CLIENT_LIFECYCLE_BACKEND)
	#define TEST_HTTP_CLIENT_LIFECYCLE_BACKEND XNET_PORT_SELECT
	#define TEST_HTTP_CLIENT_LIFECYCLE_BACKEND_NAME "select"
#endif



/* 创建只使用本机 IPv4 的 Client，排除双栈拨号噪声。 */
static xhttpclient* testHttpLifecycleClient(xnetengine* pEngine)
{
	xhttpclientconfig Config;
	xhttpclient* pClient;

	xrtHttpClientConfigInit(&Config);
	Config.Dial.Family = XNET_FAMILY_IPV4;
	Config.Dial.MaxAttempts = 1;
	Config.Timeout = UINT64_C(5000000);
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient != NULL,
		"HTTP lifecycle client create failed"
	);
	return pClient;
}



/* 为指定环回 Origin 创建拥有型 GET 请求。 */
static xhttprequest* testHttpLifecycleRequest(
	const testhttporigin* pOrigin,
	cstr sPath
)
{
	char Url[256];
	int iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u%s",
		(unsigned)testHttpOriginPort(pOrigin),
		sPath
	);
	xhttprequest* pRequest;

	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP lifecycle URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview) { Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP lifecycle request create failed"
	);
	return pRequest;
}



/* 关闭状态测试不会实际提交，因此回调只作为有效参数占位。 */
static void testHttpLifecycleUnusedDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	(void)pCall;
	(void)pResult;
	(void)pData;
}



/* 验证空 Client 的多等待者、独立取消、迟等待和关闭后提交拒绝。 */
static void testHttpLifecycleIdle(xnetengine* pEngine)
{
	xhttpclient* pClient = testHttpLifecycleClient(pEngine);
	xhttpclient* pRetained;
	xhttprequest* pRequest;
	xfuture* pCancelled;
	xfuture* pPending;
	xfuture* pLate;

	testRequire(
		xrtHttpClientState(pClient) == XHTTP_CLIENT_RUNNING,
		"HTTP idle lifecycle initial state mismatch"
	);
	pCancelled = xrtHttpClientWaitAsync(pClient);
	pPending = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pCancelled != NULL) &&
		(pPending != NULL) &&
		(xrtFutureWaitFor(pCancelled, 0) == XWAIT_TIMEOUT) &&
		(xrtFutureWaitFor(pPending, 0) == XWAIT_TIMEOUT),
		"HTTP idle lifecycle waits did not begin pending"
	);
	testRequire(
		xrtFutureCancel(pCancelled) &&
		(xrtFutureWaitFor(
			pCancelled,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pCancelled) == XFUTURE_CANCELLED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_RUNNING) &&
		(xrtFutureWaitFor(pPending, 0) == XWAIT_TIMEOUT),
		"HTTP idle wait cancellation changed Client lifecycle"
	);
	testRequire(
		xrtHttpClientDrain(pClient) &&
		xrtHttpClientDrain(pClient) &&
		(xrtFutureWaitFor(
			pPending,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pPending) == XFUTURE_RESOLVED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_CLOSED),
		"HTTP idle Client did not close after drain"
	);
	testRequire(
		xrtHttpClientAbort(pClient),
		"HTTP closed Client abort was not idempotent"
	);
	pRetained = xrtHttpClientRef(pClient);
	testRequire(
		pRetained == pClient,
		"HTTP closed Client could not be retained"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://127.0.0.1/")
	);
	testRequire(
		pRequest != NULL,
		"HTTP closed Client request create failed"
	);
	testRequire(
		(xrtHttpClientDo(
			pClient,
			pRequest,
			NULL,
			testHttpLifecycleUnusedDone,
			NULL
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_STATE),
		"HTTP closed Client accepted a new Call"
	);
	xrtClearError();
	pLate = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pLate != NULL) &&
		(xrtFutureWaitFor(pLate, 0) == XWAIT_OK) &&
		(xrtFutureState(pLate) == XFUTURE_RESOLVED),
		"HTTP late lifecycle wait was not immediately ready"
	);
	xrtHttpRequestDestroy(pRequest);
	xrtFutureDestroy(pCancelled);
	xrtFutureDestroy(pPending);
	xrtFutureDestroy(pLate);
	xrtHttpClientDestroy(pRetained);
	xrtHttpClientDestroy(pClient);
}



/* 验证 Drain 只阻止新提交，不取消已经进入响应阶段的 Call。 */
static void testHttpLifecycleDrain(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xfuture* pCall;
	xfuture* pClosed;
	xhttpresult* pResult;

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	testHttpOriginSplitResponse(
		&Origin,
		sizeof(Wire) - 3u,
		UINT64_C(500000)
	);
	pClient = testHttpLifecycleClient(pEngine);
	pRequest = testHttpLifecycleRequest(&Origin, "/drain");
	pCall = xrtHttpClientDoAsync(pClient, pRequest, NULL);
	testRequire(
		pCall != NULL,
		"HTTP drain Call submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"HTTP drain request did not reach Origin"
	);
	pClosed = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pClosed != NULL) &&
		xrtHttpClientDrain(pClient) &&
		(xrtHttpClientState(pClient) ==
		 XHTTP_CLIENT_DRAINING) &&
		(xrtFutureWaitFor(pClosed, 0) == XWAIT_TIMEOUT),
		"HTTP drain completed before its active Call"
	);
	testRequire(
		(xrtHttpClientDoAsync(pClient, pRequest, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_CLOSED) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_STATE),
		"HTTP draining Client accepted a new Call"
	);
	xrtClearError();
	testRequire(
		(xrtFutureWaitFor(
			pCall,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pCall) == XFUTURE_RESOLVED),
		"HTTP draining Call did not complete naturally"
	);
	pResult = (xhttpresult*)xrtFutureValue(pCall);
	testRequire(
		(pResult != NULL) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		 ) == 200) &&
		(xrtFutureWaitFor(
			pClosed,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_CLOSED),
		"HTTP Client did not close after graceful Call completion"
	);
	xrtFutureDestroy(pCall);
	xrtFutureDestroy(pClosed);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



/* Abort 回调记录取消终态，并验证回调返回前 Client 等待不能完成。 */
typedef struct test_http_lifecycle_abort {
	xhttpclient* Client;
	xfuture* CallbackWait;
	xatomic32 Called;
} test_http_lifecycle_abort;



/* 接收 Abort 产生的唯一取消终态。 */
static void testHttpLifecycleAbortDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_lifecycle_abort* pState =
		(test_http_lifecycle_abort*)pData;

	testRequire(
		(pCall != NULL) &&
		(pResult->Result == XNET_RESULT_CANCELLED) &&
		(pResult->Response == NULL) &&
		(pResult->Error != NULL) &&
		(xrtErrorKind(pResult->Error) == XERR_CANCELLED) &&
		(xrtHttpClientState(pState->Client) ==
		 XHTTP_CLIENT_ABORTING),
		"HTTP abort callback terminal result mismatch"
	);
	pState->CallbackWait = xrtHttpClientWaitAsync(
		pState->Client
	);
	testRequire(
		(pState->CallbackWait != NULL) &&
		(xrtFutureWaitFor(
			pState->CallbackWait,
			0
		 ) == XWAIT_TIMEOUT),
		"HTTP Client wait completed inside Call callback"
	);
	xrtAtomic32Store(
		&pState->Called,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证 Abort 可以终止活动传输并等待回调完整返回。 */
static void testHttpLifecycleAbort(xnetengine* pEngine)
{
	test_http_lifecycle_abort State;
	testhttporigin Origin;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpcall* pCall;
	xfuture* pClosed;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Called, 0);
	testHttpOriginStart(&Origin, pEngine, NULL, 0);
	pClient = testHttpLifecycleClient(pEngine);
	State.Client = pClient;
	pRequest = testHttpLifecycleRequest(&Origin, "/abort");
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		NULL,
		testHttpLifecycleAbortDone,
		&State
	);
	testRequire(
		pCall != NULL,
		"HTTP abort Call submission failed"
	);
	testHttpOriginWait(
		&Origin.Requests,
		1,
		"HTTP abort request did not reach Origin"
	);
	pClosed = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pClosed != NULL) &&
		xrtHttpClientAbort(pClient),
		"HTTP Client abort failed"
	);
	testRequire(
		(xrtFutureWaitFor(
			pClosed,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_CLOSED) &&
		(xrtAtomic32Load(
			&State.Called,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(State.CallbackWait != NULL) &&
		(xrtFutureWaitFor(State.CallbackWait, 0) == XWAIT_OK) &&
		(xrtFutureState(State.CallbackWait) == XFUTURE_RESOLVED) &&
		(xrtHttpCallState(pCall) == XHTTP_CALL_CANCELLED),
		"HTTP Client abort did not reach a complete closed state"
	);
	xrtFutureDestroy(State.CallbackWait);
	xrtFutureDestroy(pClosed);
	xrtHttpCallDestroy(pCall);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}



#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)

/* Worker 屏障让空闲传输关闭与 Timer 取消回调保持在队列中。 */
typedef struct test_http_lifecycle_barrier {
	xatomic32 Started;
	xatomic32 Release;
	xatomic32 Done;
} test_http_lifecycle_barrier;



/* 暂停唯一 Worker，直到宿主线程完成关闭终态检查。 */
static void testHttpLifecycleBarrier(
	xnetworker* pWorker,
	ptr pData
)
{
	test_http_lifecycle_barrier* pBarrier =
		(test_http_lifecycle_barrier*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pBarrier->Started,
		1,
		XMEMORY_RELEASE
	);
	while ( xrtAtomic32Load(
		&pBarrier->Release,
		XMEMORY_ACQUIRE
	) == 0 ) {
		xrtThreadYield();
	}
	xrtAtomic32Store(
		&pBarrier->Done,
		1,
		XMEMORY_RELEASE
	);
}



/* 验证池传输和 Timer 回调都退出后才发布 CLOSED。 */
static void testHttpLifecyclePool(xnetengine* pEngine)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"OK";
	test_http_lifecycle_barrier Barrier;
	testhttporigin Origin;
	xhttpclientconfig Config;
	xhttpclientstats Stats;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpresult* pResult;
	xfuture* pClosed;

	memset(&Barrier, 0, sizeof(Barrier));
	xrtAtomic32Init(&Barrier.Started, 0);
	xrtAtomic32Init(&Barrier.Release, 0);
	xrtAtomic32Init(&Barrier.Done, 0);
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	xrtHttpClientConfigInit(&Config);
	Config.Dial.Family = XNET_FAMILY_IPV4;
	Config.Dial.MaxAttempts = 1;
	Config.Pool.IdleTimeout = UINT64_C(10000000);
	pClient = xrtHttpClientCreate(pEngine, &Config);
	testRequire(
		pClient != NULL,
		"HTTP lifecycle pool Client create failed"
	);
	pRequest = testHttpLifecycleRequest(&Origin, "/pool");
	pResult = xrtHttpClientDoSync(pClient, pRequest, NULL);
	testRequire(
		(pResult != NULL) &&
		xrtHttpClientStats(pClient, &Stats) &&
		(Stats.IdleConnections == 1),
		"HTTP lifecycle pool did not retain one idle connection"
	);
	xrtHttpResultDestroy(pResult);
	testRequire(
		xrtNetEnginePost(
			pEngine,
			0,
			testHttpLifecycleBarrier,
			&Barrier
		),
		"HTTP lifecycle pool barrier post failed"
	);
	testHttpOriginWait(
		&Barrier.Started,
		1,
		"HTTP lifecycle pool barrier did not start"
	);
	pClosed = xrtHttpClientWaitAsync(pClient);
	testRequire(
		(pClosed != NULL) &&
		xrtHttpClientDrain(pClient) &&
		(xrtHttpClientState(pClient) ==
		 XHTTP_CLIENT_DRAINING) &&
		(xrtFutureWaitFor(pClosed, 0) == XWAIT_TIMEOUT),
		"HTTP pool Client closed before transport and Timer callbacks"
	);
	xrtAtomic32Store(
		&Barrier.Release,
		1,
		XMEMORY_RELEASE
	);
	testHttpOriginWait(
		&Barrier.Done,
		1,
		"HTTP lifecycle pool barrier did not finish"
	);
	testRequire(
		(xrtFutureWaitFor(
			pClosed,
			UINT64_C(5000000)
		 ) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED) &&
		(xrtHttpClientState(pClient) == XHTTP_CLIENT_CLOSED) &&
		xrtHttpClientStats(pClient, &Stats) &&
		(Stats.ActiveConnections == 0) &&
		(Stats.IdleConnections == 0) &&
		(Stats.ClosingConnections == 0) &&
		(Stats.WaitingCalls == 0),
		"HTTP pool Client did not fully close after Worker release"
	);
	xrtFutureDestroy(pClosed);
	xrtHttpRequestDestroy(pRequest);
	xrtHttpClientDestroy(pClient);
	testHttpOriginStop(&Origin);
}

#endif



/* 覆盖 Client 生命周期状态、关闭等待、平滑排空和异常终止。 */
int main(void)
{
	xnetengineconfig Config;
	xnetengine* pEngine;

	testRequire(
		!xrtHttpClientDrain(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP null Client drain error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtHttpClientAbort(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP null Client abort error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtHttpClientWaitAsync(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_CLIENT_ERROR_ARGUMENT) &&
		(strcmp(
			xrtErrorOperation(xrtGetError()),
			"wait-http-client"
		 ) == 0),
		"HTTP null Client wait error mismatch"
	);
	xrtClearError();
	xrtNetEngineConfigInit(&Config);
	Config.Backend = TEST_HTTP_CLIENT_LIFECYCLE_BACKEND;
	Config.Workers = 1;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP lifecycle engine start failed"
	);
	testHttpLifecycleIdle(pEngine);
	testHttpLifecycleDrain(pEngine);
	testHttpLifecycleAbort(pEngine);
	#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)
		testHttpLifecyclePool(pEngine);
	#endif
	testRequire(
		xrtNetEngineDestroy(pEngine),
		"HTTP lifecycle retained Engine ownership"
	);
	printf(
		"[PASS] HTTP Client lifecycle (%s)\n",
		TEST_HTTP_CLIENT_LIFECYCLE_BACKEND_NAME
	);
	return 0;
}
