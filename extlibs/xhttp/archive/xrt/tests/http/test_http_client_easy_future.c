#include "../test.h"
#include "../fixtures/http_origin.h"



/* 构造指向环回 origin 的完整 URL。 */
static xstrview testHttpEasyFutureUrl(
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
		"HTTP easy Future URL overflowed"
	);
	return (xstrview){ sUrl, (size_t)iLength };
}



/* 验证一行 GET Async 可以取得拥有型 Future 结果。 */
static void testHttpEasyGetAsync(
	xnetengine* pEngine,
	xhttpclient* pClient
)
{
	static const char Wire[] =
		"HTTP/1.1 204 No Content\r\n"
		"Connection: close\r\n"
		"\r\n";
	testhttporigin Origin;
	xfuture* pFuture;
	xhttpresult* pResult;
	char Url[256];

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	testHttpOriginExpect(
		&Origin,
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/easy-async"),
		(xstrview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	pFuture = xrtHttpClientGetAsync(
		pClient,
		testHttpEasyFutureUrl(
			&Origin,
			"/easy-async",
			Url,
			sizeof(Url)
		),
		NULL
	);
	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(10000000)
		) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED),
		"HTTP easy GET Async failed"
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	testRequire(
		(pResult != NULL) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 204),
		"HTTP easy GET Async result mismatch"
	);
	xrtFutureDestroy(pFuture);
	testHttpOriginStop(&Origin);
}



/* 验证一行 POST Sync 复制正文并返回独立拥有型结果。 */
static void testHttpEasyPostSync(
	xnetengine* pEngine,
	xhttpclient* pClient
)
{
	static const char Wire[] =
		"HTTP/1.1 201 Created\r\n"
		"Content-Length: 0\r\n"
		"Connection: close\r\n"
		"\r\n";
	static const char Body[] = "name=xrt&mode=fast";
	testhttporigin Origin;
	xhttpresult* pResult;
	char Url[256];

	testHttpOriginStart(
		&Origin,
		pEngine,
		Wire,
		sizeof(Wire) - 1u
	);
	testHttpOriginExpect(
		&Origin,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/easy-sync"),
		XRT_STR_LITERAL("application/x-www-form-urlencoded"),
		(xbytesview){ (cbytes)Body, sizeof(Body) - 1u }
	);
	pResult = xrtHttpClientPostSync(
		pClient,
		testHttpEasyFutureUrl(
			&Origin,
			"/easy-sync",
			Url,
			sizeof(Url)
		),
		(xbytesview){ (cbytes)Body, sizeof(Body) - 1u },
		XRT_STR_LITERAL("application/x-www-form-urlencoded"),
		NULL
	);
	testRequire(
		(pResult != NULL) &&
		(xrtHttpResponseStatus(
			xrtHttpResultResponse(pResult)
		) == 201),
		"HTTP easy POST Sync result mismatch"
	);
	xrtHttpResultDestroy(pResult);
	testHttpOriginStop(&Origin);
}



/* 覆盖 Future 与同步便利入口的真实线路和参数契约。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpclient* pClient;
	xdeadline Deadline;

	xrtClearError();
	testRequire(
		(xrtHttpClientGetAsync(
			NULL,
			XRT_STR_LITERAL("http://example.test/"),
			NULL
		) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP easy Future null Client error mismatch"
	);
	xrtClearError();

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"HTTP easy Future Engine start failed"
	);
	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Dial.Family = XNET_FAMILY_IPV4;
	ClientConfig.Dial.MaxAttempts = 1;
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	testRequire(
		pClient != NULL,
		"HTTP easy Future Client create failed"
	);

	testHttpEasyGetAsync(pEngine, pClient);
	testHttpEasyPostSync(pEngine, pClient);

	xrtHttpClientDestroy(pClient);
	Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	while ( !xrtNetEngineDestroy(pEngine) ) {
		xrtClearError();
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"HTTP easy Future Engine retained an object"
		);
		xrtThreadYield();
	}
	printf("[PASS] HTTP client Future convenience layer\n");
	return 0;
}
