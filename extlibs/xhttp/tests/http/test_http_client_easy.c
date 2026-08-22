#include "../test.h"
#include "../fixtures/http_origin.h"



/* 便利 callback 用例只跨线程保存已经转移给调用方的响应所有权。 */
typedef struct test_http_easy_result {
	xatomic32 Completed;
	xnetresult Result;
	xhttpresponse* Response;
} test_http_easy_result;



/* 接管 callback 交付的响应并发布终态。 */
static void testHttpEasyDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	test_http_easy_result* pState =
		(test_http_easy_result*)pData;

	(void)pCall;
	pState->Result = pResult->Result;
	pState->Response = pResult->Response;
	xrtAtomic32Store(
		&pState->Completed,
		1,
		XMEMORY_RELEASE
	);
}



/* 在固定截止时间内等待便利 callback 发布结果。 */
static void testHttpEasyWait(const xatomic32* pCompleted)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(10000000));

	while ( xrtAtomic32Load(
		pCompleted,
		XMEMORY_ACQUIRE
	) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP easy callback did not complete"
		);
		xrtThreadYield();
	}
}



/* 构造指向环回 origin 的完整 URL。 */
static xstrview testHttpEasyUrl(
	const testhttporigin* pOrigin,
	cstr sTarget,
	char* sUrl,
	size_t iCapacity
)
{
	int iLength = snprintf(
		sUrl,
		iCapacity,
		"http://127.0.0.1:%u%s",
		(unsigned)testHttpOriginPort(pOrigin),
		sTarget
	);

	testRequire(
		(iLength > 0) && ((size_t)iLength < iCapacity),
		"HTTP easy URL overflowed"
	);
	return (xstrview){ sUrl, (size_t)iLength };
}



/* 执行并核对 GET、POST 或任意方法的 callback 便利入口。 */
static void testHttpEasyCall(
	xnetengine* pEngine,
	xhttpclient* pClient,
	int iKind,
	xstrview Method,
	cstr sTarget,
	xbytesview Body,
	xstrview ContentType
)
{
	static const char Wire[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 2\r\n"
		"Connection: close\r\n"
		"\r\n"
		"OK";
	test_http_easy_result State;
	testhttporigin Origin;
	xhttpcall* pCall;
	char Url[256];
	xstrview UrlView;

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.Completed, 0);
	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	testHttpOriginExpect(
		&Origin,
		Method,
		(xstrview){ sTarget, strlen(sTarget) },
		ContentType,
		Body
	);
	UrlView = testHttpEasyUrl(
		&Origin,
		sTarget,
		Url,
		sizeof(Url)
	);
	if ( iKind == 0 ) {
		pCall = xrtHttpClientGet(
			pClient,
			UrlView,
			NULL,
			testHttpEasyDone,
			&State
		);
	} else if ( iKind == 1 ) {
		pCall = xrtHttpClientPost(
			pClient,
			UrlView,
			Body,
			ContentType,
			NULL,
			testHttpEasyDone,
			&State
		);
	} else {
		pCall = xrtHttpClientSendBytes(
			pClient,
			Method,
			UrlView,
			Body,
			ContentType,
			NULL,
			testHttpEasyDone,
			&State
		);
	}
	testRequire(
		pCall != NULL,
		"HTTP easy callback submission failed"
	);
	testHttpEasyWait(&State.Completed);
	testRequire(
		(State.Result == XNET_RESULT_OK) &&
		(State.Response != NULL) &&
		(xrtHttpResponseStatus(State.Response) == 200) &&
		(xrtHttpResponseBody(State.Response).Size == 2u),
		"HTTP easy callback result mismatch"
	);
	xrtHttpResponseDestroy(State.Response);
	xrtHttpCallDestroy(pCall);
	testHttpOriginStop(&Origin);
}



/* 验证便利入口参数失败保留结构化 Client 或请求错误。 */
static void testHttpEasyArguments(xhttpclient* pClient)
{
	xhttpcall* pCall;
	const xerror* pError;

	pCall = xrtHttpClientGet(
		NULL,
		XRT_STR_LITERAL("http://example.test/"),
		NULL,
		testHttpEasyDone,
		NULL
	);
	pError = xrtGetError();
	testRequire(
		(pCall == NULL) &&
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.http.client") == 0) &&
		(xrtErrorCode(pError) == XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP easy null Client error mismatch"
	);
	xrtClearError();

	pCall = xrtHttpClientGet(
		pClient,
		XRT_STR_LITERAL("http://example.test/"),
		NULL,
		NULL,
		NULL
	);
	testRequire(
		(pCall == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP easy null callback error mismatch"
	);
	xrtClearError();

	pCall = xrtHttpClientSendBytes(
		pClient,
		XRT_STR_LITERAL("bad method"),
		XRT_STR_LITERAL("http://example.test/"),
		XRT_BYTES_LITERAL("x"),
		XRT_STR_LITERAL("text/plain"),
		NULL,
		testHttpEasyDone,
		NULL
	);
	testRequire(
		(pCall == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"HTTP easy invalid method error mismatch"
	);
	xrtClearError();
}



/* 覆盖 callback 便利层的完整线路、参数和生命周期契约。 */
int main(void)
{
	static const char Json[] = "{\"name\":\"xrt\"}";
	static const char Patch[] = "enabled=true";
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xdeadline Deadline;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP easy Engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.Family = XNET_FAMILY_IPV4;
	ClientConfig.Dial.MaxAttempts = 1;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"HTTP easy Client create failed"
	);

	testHttpEasyArguments(pClient);
	testHttpEasyCall(
		pEngine,
		pClient,
		0,
		XRT_STR_LITERAL("GET"),
		"/easy-get",
		(xbytesview){ NULL, 0 },
		(xstrview){ NULL, 0 }
	);
	testHttpEasyCall(
		pEngine,
		pClient,
		1,
		XRT_STR_LITERAL("POST"),
		"/easy-post",
		(xbytesview){ (cbytes)Json, sizeof(Json) - 1u },
		XRT_STR_LITERAL("application/json; charset=utf-8")
	);
	testHttpEasyCall(
		pEngine,
		pClient,
		2,
		XRT_STR_LITERAL("PATCH"),
		"/easy-send",
		(xbytesview){ (cbytes)Patch, sizeof(Patch) - 1u },
		XRT_STR_LITERAL("application/x-www-form-urlencoded")
	);

	xrtHttpClientDestroy(pClient);
	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP easy Engine retained an object"
		);
		xrtThreadYield();
	}
	printf("[PASS] HTTP client callback convenience layer\n");
	return 0;
}


