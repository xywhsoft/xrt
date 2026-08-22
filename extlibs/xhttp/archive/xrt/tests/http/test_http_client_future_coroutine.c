#include "../test.h"
#include "../fixtures/http_origin.h"



/* 保存一次 HTTP Future 在调度协程中的等待结果和拥有型响应结果。 */
typedef struct test_http_future_coroutine {
	xhttpclient* Client;
	xhttprequest* Request;
	xwaitresult Wait;
	xfuturestate State;
	xhttpresult* Result;
	bool Entered;
	bool Returned;
} test_http_future_coroutine;



/* 在协程中提交 HTTP 调用，并通过通用 Future await 契约等待网络 Worker。 */
static ptr testHttpFutureCoroutineProc(ptr pData)
{
	test_http_future_coroutine* pState =
		(test_http_future_coroutine*)pData;
	xfuture* pFuture;

	pState->Entered = true;
	pFuture = xrtHttpClientDoAsync(
		pState->Client,
		pState->Request,
		NULL
	);
	testRequire(
		pFuture != NULL,
		"HTTP coroutine Future submission failed"
	);
	pState->Wait = xrtFutureAwaitFor(
		pFuture,
		UINT64_C(10000000)
	);
	pState->State = xrtFutureState(pFuture);
	if ( (pState->Wait == XWAIT_OK) &&
		(pState->State == XFUTURE_RESOLVED) ) {
		pState->Result = xrtHttpResultRef(
			(xhttpresult*)xrtFutureValue(pFuture)
		);
	}
	xrtFutureDestroy(pFuture);
	pState->Returned = true;
	return pState;
}



/* 创建只使用 IPv4 的测试 Client，排除双栈竞速对协程契约的干扰。 */
static xhttpclient* testHttpFutureCoroutineClient(
	xnetengine* pEngine
)
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
		"HTTP coroutine Client create failed"
	);
	return pClient;
}



/* 为本地 origin 创建一条拥有型 GET 请求。 */
static xhttprequest* testHttpFutureCoroutineRequest(
	const testhttporigin* pOrigin
)
{
	char Url[256];
	int iLength = snprintf(
		Url,
		sizeof(Url),
		"http://127.0.0.1:%u/coroutine",
		(unsigned)testHttpOriginPort(pOrigin)
	);
	xhttprequest* pRequest;

	testRequire(
		(iLength > 0) &&
		((size_t)iLength < sizeof(Url)),
		"HTTP coroutine URL overflowed"
	);
	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		(xstrview){ Url, (size_t)iLength }
	);
	testRequire(
		pRequest != NULL,
		"HTTP coroutine Request create failed"
	);
	return pRequest;
}



/* 排空异步析构后销毁测试 Engine。 */
static void testHttpFutureCoroutineEngineDestroy(
	xnetengine* pEngine
)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP coroutine retained an Engine object"
		);
		xrtThreadYield();
	}
}



/* 验证 HTTP Future 可直接进入通用协程调度器，不增加专用 Co API。 */
int main(void)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	test_http_future_coroutine State;
	testhttporigin Origin;
	xnetengineconfig Config;
	xnetengine* pEngine;
	xhttpresponse* pResponse;
	xcosched* pSched;
	xcoro* pCoroutine;

	memset(&State, 0, sizeof(State));
	xrtNetEngineConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	Config.Workers = 2;
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(
		(pEngine != NULL) &&
		xrtNetEngineStart(pEngine),
		"HTTP coroutine Engine start failed"
	);
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	State.Client = testHttpFutureCoroutineClient(pEngine);
	State.Request = testHttpFutureCoroutineRequest(&Origin);

	pSched = xrtCoSchedCreate();
	testRequire(
		pSched != NULL,
		"HTTP coroutine scheduler create failed"
	);
	pCoroutine = xrtCoSpawn(
		pSched,
		testHttpFutureCoroutineProc,
		&State,
		NULL
	);
	testRequire(
		pCoroutine != NULL,
		"HTTP coroutine spawn failed"
	);
	testRequire(
		xrtCoSchedRun(pSched),
		"HTTP coroutine scheduler run failed"
	);
	testRequire(
		State.Entered &&
		State.Returned &&
		(State.Wait == XWAIT_OK) &&
		(State.State == XFUTURE_RESOLVED) &&
		(State.Result != NULL),
		"HTTP coroutine Future terminal mismatch"
	);
	pResponse = xrtHttpResultTakeResponse(State.Result);
	testRequire(
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == 200) &&
		(xrtHttpResponseBody(pResponse).Size == 2u) &&
		(memcmp(
			xrtHttpResponseBody(pResponse).Data,
			"OK",
			2u
		) == 0),
		"HTTP coroutine response mismatch"
	);

	testRequire(
		xrtCoDestroy(pCoroutine),
		"HTTP coroutine destroy failed"
	);
	testRequire(
		xrtCoSchedDestroy(pSched),
		"HTTP coroutine scheduler destroy failed"
	);
	xrtHttpResultDestroy(State.Result);
	xrtHttpResponseDestroy(pResponse);
	xrtHttpRequestDestroy(State.Request);
	xrtHttpClientDestroy(State.Client);
	testHttpOriginStop(&Origin);
	testHttpFutureCoroutineEngineDestroy(pEngine);
	printf("[PASS] high-level HTTP Future coroutine contract\n");
	return 0;
}
