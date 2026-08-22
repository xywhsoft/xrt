#include "../test.h"



/* 验证请求准备选项初始化器支持未对齐存储并拒绝回绕地址。 */
static void testHttpPrepareOptionsStorage(void)
{
	uint8 Storage[sizeof(xhttp1requestoptions) + 2u];
	xhttp1requestoptions Options;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttp1RequestOptionsInit(
		(xhttp1requestoptions*)(void*)(Storage + 1u)
	);
	memcpy(&Options, Storage + 1u, sizeof(Options));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Options.TargetForm == XHTTP1_TARGET_AUTO) &&
		(Options.CustomTarget.Data == NULL) &&
		(Options.CustomTarget.Size == 0),
		"HTTP prepare unaligned options initializer mismatch"
	);

	xrtClearError();
	xrtHttp1RequestOptionsInit(
		(xhttp1requestoptions*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP prepare wrapping options initializer mismatch"
	);
	xrtClearError();
}



/* 比较借用文本与字面量。 */
static bool testHttpPrepareText(
	xstrview Text,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (Text.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Text.Data, sExpected, iSize) == 0));
}



/* 比较线路 Header 与字面量。 */
static bool testHttpPrepareHead(
	const xhttp1requestplan* pPlan,
	cstr sExpected
)
{
	xbytesview Head = xrtHttp1RequestPlanHead(pPlan);
	size_t iSize = strlen(sExpected);

	return (Head.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(Head.Data, sExpected, iSize) == 0));
}



/* 验证默认直连计划生成 origin-form、Host 与 HTTPS 端点事实。 */
static void testHttpPrepareDefault(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://[2001:db8::1]/v1?q=1#fragment"
		)
	);
	xhttp1requestplan* pPlan;

	testRequire(pRequest != NULL,
		"HTTP prepare default request create failed");
	xrtClearError();
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtGetError() == NULL) &&
		testHttpPrepareHead(
			pPlan,
			"GET /v1?q=1 HTTP/1.1\r\n"
			"Host: [2001:db8::1]\r\n"
			"\r\n"
		) &&
		testHttpPrepareText(
			xrtHttp1RequestPlanMethod(pPlan),
			"GET"
		) &&
		testHttpPrepareText(
			xrtHttp1RequestPlanUrl(pPlan),
			"https://[2001:db8::1]/v1?q=1#fragment"
		) &&
		testHttpPrepareText(
			xrtHttp1RequestPlanTarget(pPlan),
			"/v1?q=1"
		) &&
		testHttpPrepareText(
			xrtHttp1RequestPlanHost(pPlan),
			"2001:db8::1"
		) &&
		(xrtHttp1RequestPlanPort(pPlan) == 443) &&
		xrtHttp1RequestPlanSecure(pPlan) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_NONE) &&
		!xrtHttp1RequestPlanClose(pPlan),
		"HTTP prepare default plan mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证计划冻结 Header、target、端点和正文引用。 */
static void testHttpPrepareSnapshot(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test:8080/upload")
	);
	xhttp1requestplan* pPlan;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){ (cbytes)"abc", 3 },
			XRT_STR_LITERAL("application/octet-stream")
		),
		"HTTP prepare snapshot request setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		testHttpPrepareHead(
			pPlan,
			"POST /upload HTTP/1.1\r\n"
			"Host: example.test:8080\r\n"
			"Content-Length: 3\r\n"
			"Content-Type: application/octet-stream\r\n"
			"\r\n"
		) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_FIXED) &&
		(xrtHttp1RequestPlanBodyLength(pPlan) == 3),
		"HTTP prepare snapshot initial plan mismatch");
	testRequire(xrtHttpRequestSetMethod(
		pRequest, XRT_STR_LITERAL("PUT")
	) && xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("https://changed.test/other")
	) && xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain")
	), "HTTP prepare source request mutation failed");
	xrtHttpRequestDestroy(pRequest);

	testRequire(testHttpPrepareHead(
		pPlan,
		"POST /upload HTTP/1.1\r\n"
		"Host: example.test:8080\r\n"
		"Content-Length: 3\r\n"
		"Content-Type: application/octet-stream\r\n"
		"\r\n"
	) && testHttpPrepareText(
		xrtHttp1RequestPlanMethod(pPlan), "POST"
	) && testHttpPrepareText(
		xrtHttp1RequestPlanUrl(pPlan),
		"http://example.test:8080/upload"
	), "HTTP prepare plan changed with source request");
	pReader = xrtHttpBodyOpen(
		xrtHttp1RequestPlanBody(pPlan)
	);
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 3, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 3) &&
		(memcmp(Chunk.Data, "abc", 3) == 0),
		"HTTP prepare plan did not retain body");
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttp1RequestPlanDestroy(pPlan);
}



/* 验证全部标准 request-target 形式。 */
static void testHttpPrepareTargets(void)
{
	static const struct {
		xstrview Method;
		xhttp1targetform Form;
		xstrview Custom;
		cstr Expected;
	} Cases[] = {
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_AUTO,
			{ NULL, 0 },
			"/path?q=1"
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_ORIGIN,
			{ NULL, 0 },
			"/path?q=1"
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_ABSOLUTE,
			{ NULL, 0 },
			"https://example.test/path?q=1"
		},
		{
			XRT_STR_INIT("CONNECT"),
			XHTTP1_TARGET_AUTO,
			{ NULL, 0 },
			"example.test:443"
		},
		{
			XRT_STR_INIT("CONNECT"),
			XHTTP1_TARGET_AUTHORITY,
			{ NULL, 0 },
			"example.test:443"
		},
		{
			XRT_STR_INIT("OPTIONS"),
			XHTTP1_TARGET_ASTERISK,
			{ NULL, 0 },
			"*"
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_CUSTOM,
			XRT_STR_INIT("/custom;v=1"),
			"/custom;v=1"
		}
	};
	xhttp1requestoptions Options;
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttprequest* pRequest = xrtHttpRequestCreate(
			Cases[i].Method,
			XRT_STR_LITERAL(
				"https://example.test/path?q=1#fragment"
			)
		);
		xhttp1requestplan* pPlan;

		testRequire(pRequest != NULL,
			"HTTP prepare target request create failed");
		xrtHttp1RequestOptionsInit(&Options);
		Options.TargetForm = Cases[i].Form;
		Options.CustomTarget = Cases[i].Custom;
		pPlan = xrtHttp1RequestPrepare(
			pRequest, &Options
		);
		testRequire((pPlan != NULL) &&
			testHttpPrepareText(
				xrtHttp1RequestPlanTarget(pPlan),
				Cases[i].Expected
			), "HTTP prepare request-target form mismatch");
		xrtHttp1RequestPlanDestroy(pPlan);
		xrtHttpRequestDestroy(pRequest);
	}
}



/* 验证标准目标形式不能绕过方法语义约束。 */
static void testHttpPrepareTargetErrors(void)
{
	static const struct {
		xstrview Method;
		xhttp1targetform Form;
		xstrview Custom;
	} Cases[] = {
		{
			XRT_STR_INIT("CONNECT"),
			XHTTP1_TARGET_ORIGIN,
			{ NULL, 0 }
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_AUTHORITY,
			{ NULL, 0 }
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_ASTERISK,
			{ NULL, 0 }
		},
		{
			XRT_STR_INIT("GET"),
			XHTTP1_TARGET_CUSTOM,
			XRT_STR_INIT("*")
		}
	};
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttprequest* pRequest = xrtHttpRequestCreate(
			Cases[i].Method,
			XRT_STR_LITERAL("https://example.test/path")
		);
		xhttp1requestoptions Options;

		testRequire(pRequest != NULL,
			"HTTP prepare invalid target request create failed");
		xrtHttp1RequestOptionsInit(&Options);
		Options.TargetForm = Cases[i].Form;
		Options.CustomTarget = Cases[i].Custom;
		testRequire((xrtHttp1RequestPrepare(
			pRequest, &Options
		) == NULL) &&
			(xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XHTTP_REQUEST_ERROR_TARGET),
			"HTTP prepare accepted method and target mismatch");
		xrtClearError();
		xrtHttpRequestDestroy(pRequest);
	}
}



/* 验证 absolute-form 自动生成匹配 Host 并拒绝显式冲突。 */
static void testHttpPrepareAbsoluteHost(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://Example.test:8080/path")
	);
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan;

	testRequire(pRequest != NULL,
		"HTTP prepare absolute Host request create failed");
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_ABSOLUTE;
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	testRequire((pPlan != NULL) && testHttpPrepareHead(
		pPlan,
		"GET http://Example.test:8080/path HTTP/1.1\r\n"
		"Host: Example.test:8080\r\n"
		"\r\n"
	), "HTTP prepare absolute Host generation mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);

	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Host"),
		XRT_STR_LITERAL("other.test:8080")
	), "HTTP prepare absolute Host override failed");
	testRequire((xrtHttp1RequestPrepare(
		pRequest, &Options
	) == NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XHTTP_REQUEST_ERROR_HOST_HEADER),
		"HTTP prepare accepted conflicting absolute Host");
	xrtClearError();

	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Host"),
		XRT_STR_LITERAL("example.TEST:8080")
	), "HTTP prepare matching absolute Host override failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	testRequire(pPlan != NULL,
		"HTTP prepare rejected matching absolute Host");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证空端口按默认端口执行，并在自动线路文本中规范省略。 */
static void testHttpPrepareEmptyPort(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://Example.test:/path#fragment")
	);
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan;

	testRequire(pRequest != NULL,
		"HTTP prepare empty port request create failed");
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_ABSOLUTE;
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	testRequire((pPlan != NULL) && testHttpPrepareHead(
		pPlan,
		"GET http://Example.test/path HTTP/1.1\r\n"
		"Host: Example.test\r\n"
		"\r\n"
	) && (xrtHttp1RequestPlanPort(pPlan) == 80),
		"HTTP prepare empty port normalization mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);

	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Host"),
		XRT_STR_LITERAL("example.TEST:80")
	), "HTTP prepare empty port Host override failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	testRequire(pPlan != NULL,
		"HTTP prepare rejected equivalent default-port Host");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("http://Example.test:/")
	);
	testRequire(pRequest != NULL,
		"HTTP prepare empty port CONNECT create failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) && testHttpPrepareHead(
		pPlan,
		"CONNECT Example.test:80 HTTP/1.1\r\n"
		"Host: Example.test\r\n"
		"\r\n"
	), "HTTP prepare empty port CONNECT normalization mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证显式 Host、分帧、关闭和 100 Continue 契约。 */
static void testHttpPrepareExplicit(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://127.0.0.1/upload")
	);
	xhttp1requestplan* pPlan;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){ (cbytes)"data", 4 },
			(xstrview){ NULL, 0 }
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Host"),
			XRT_STR_LITERAL("virtual.example:9000")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Transfer-Encoding"),
			XRT_STR_LITERAL("chunked")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("keep-alive, close")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Expect"),
			XRT_STR_LITERAL("100-continue")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Expect"),
			XRT_STR_LITERAL(", 100-CONTINUE,")
		), "HTTP prepare explicit request setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_CHUNKED) &&
		(xrtHttp1RequestPlanBodyLength(pPlan) == 4) &&
		xrtHttp1RequestPlanClose(pPlan) &&
		xrtHttp1RequestPlanExpectContinue(pPlan) &&
		(xrtHttp1RequestPlanPort(pPlan) == 80) &&
		!xrtHttp1RequestPlanSecure(pPlan) &&
		testHttpPrepareHead(
			pPlan,
			"POST /upload HTTP/1.1\r\n"
			"Host: virtual.example:9000\r\n"
			"Transfer-Encoding: chunked\r\n"
			"Connection: keep-alive, close\r\n"
			"Expect: 100-continue\r\n"
			"Expect: , 100-CONTINUE,\r\n"
			"\r\n"
		),
		"HTTP prepare explicit plan mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证用户提供的匹配 Content-Length 保持原值，零正文也不产生歧义。 */
static void testHttpPrepareContentLength(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/fixed")
	);
	xhttp1requestplan* pPlan;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestSetBytes(
			pRequest,
			(xbytesview){ (cbytes)"abc", 3 },
			(xstrview){ NULL, 0 }
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("3, 3")
		), "HTTP prepare explicit length setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_FIXED) &&
		(xrtHttp1RequestPlanBodyLength(pPlan) == 3) &&
		testHttpPrepareHead(
			pPlan,
			"POST /fixed HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Content-Length: 3, 3\r\n"
			"\r\n"
		), "HTTP prepare explicit length mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/empty")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		), "HTTP prepare zero length setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBody(pPlan) == NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_FIXED) &&
		(xrtHttp1RequestPlanBodyLength(pPlan) == 0),
		"HTTP prepare zero length contract mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 未知长度正文工厂只用于验证准备阶段的引用和 chunked 选择。 */
typedef struct test_http_prepare_source {
	size_t Destroys;
} test_http_prepare_source;



/* 准备阶段不得提前打开正文 Reader。 */
static bool testHttpPrepareSourceOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	(void)pFactory;
	(void)pOps;
	(void)ppReader;
	testRequire(false,
		"HTTP prepare opened body before transport execution");
	return false;
}



/* 记录未知长度正文工厂的最终销毁。 */
static void testHttpPrepareSourceDestroy(ptr pFactory)
{
	test_http_prepare_source* pSource =
		(test_http_prepare_source*)pFactory;

	pSource->Destroys++;
}



/* 验证未知长度正文自动选择 chunked 并由计划保留。 */
static void testHttpPrepareUnknownBody(void)
{
	static const xhttpbodyops Ops = {
		testHttpPrepareSourceOpen,
		testHttpPrepareSourceDestroy
	};
	test_http_prepare_source Source = { 0 };
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/stream")
	);
	xhttpbody* pBody = xrtHttpBodyCreate(
		&Ops,
		&Source,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_NONE
	);
	xhttp1requestplan* pPlan;

	testRequire((pRequest != NULL) && (pBody != NULL) &&
		xrtHttpRequestSetBody(pRequest, pBody),
		"HTTP prepare unknown body setup failed");
	xrtHttpBodyDestroy(pBody);
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanBodyMode(pPlan) ==
			XHTTP_REQUEST_BODY_CHUNKED) &&
		(xrtHttp1RequestPlanBodyLength(pPlan) ==
			XHTTP_BODY_UNKNOWN) &&
		testHttpPrepareHead(
			pPlan,
			"POST /stream HTTP/1.1\r\n"
			"Host: example.test\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
		) &&
		(Source.Destroys == 0),
		"HTTP prepare unknown body plan mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	testRequire(Source.Destroys == 1,
		"HTTP prepare unknown body lifetime mismatch");
}



/* 验证大 target 和大 Header 不受旧固定容量限制。 */
static void testHttpPrepareDynamic(void)
{
	static char Target[6144];
	static char Value[98304];
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	xhttp1requestoptions Options;
	xhttp1requestplan* pPlan;

	memset(Target, 't', sizeof(Target));
	Target[0] = '/';
	memset(Value, 'v', sizeof(Value));
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("X-Large"),
			(xstrview){ Value, sizeof(Value) }
		), "HTTP prepare dynamic request setup failed");
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = XHTTP1_TARGET_CUSTOM;
	Options.CustomTarget = (xstrview){
		Target, sizeof(Target)
	};
	pPlan = xrtHttp1RequestPrepare(pRequest, &Options);
	testRequire((pPlan != NULL) &&
		(xrtHttp1RequestPlanTarget(pPlan).Size ==
			sizeof(Target)) &&
		(xrtHttp1RequestPlanHead(pPlan).Size >
			(sizeof(Target) + sizeof(Value))),
		"HTTP prepare dynamic data was limited or truncated");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);
}



/* 验证 Expect 扩展拒绝与语法错误使用同一稳定代码但保留不同原因链。 */
static void testHttpPrepareExpectErrors(void)
{
	static const struct {
		cstr Value;
		bool Cause;
	} Cases[] = {
		{ "feature=on", false },
		{ "feature =on", true }
	};
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttprequest* pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("http://example.test/")
		);
		const xerror* pError;

		testRequire((pRequest != NULL) &&
			xrtHttpRequestSetBytes(
				pRequest,
				XRT_BYTES_LITERAL("abc"),
				(xstrview){ NULL, 0 }
			) && xrtHttpRequestAddHeader(
				pRequest,
				XRT_STR_LITERAL("Expect"),
				(xstrview){
					Cases[i].Value,
					strlen(Cases[i].Value)
				}
			), "HTTP prepare Expect error setup failed");
		testRequire(xrtHttp1RequestPrepare(
			pRequest, NULL
		) == NULL, "HTTP prepare accepted unusable Expect");
		pError = xrtGetError();
		testRequire((pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
			(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_EXPECT) &&
			((xrtErrorCause(pError) != NULL) == Cases[i].Cause) &&
			(!Cases[i].Cause ||
			 (xrtErrorKind(xrtErrorCause(pError)) == XERR_VALUE)),
			"HTTP prepare Expect error contract mismatch");
		xrtClearError();
		xrtHttpRequestDestroy(pRequest);
	}
}



/* 验证 TE 语法以及逐跳 Connection 声明必须形成完整组合。 */
static void testHttpPrepareTe(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	xhttp1requestplan* pPlan;
	const xerror* pError;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("trailers")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("te"),
			XRT_STR_LITERAL("gzip;q=0.5")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("keep-alive, te")
		), "HTTP prepare TE valid setup failed");
	pPlan = xrtHttp1RequestPrepare(pRequest, NULL);
	testRequire((pPlan != NULL) && testHttpPrepareHead(
		pPlan,
		"GET / HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"TE: trailers\r\n"
		"te: gzip;q=0.5\r\n"
		"Connection: keep-alive, te\r\n"
		"\r\n"
	), "HTTP prepare TE valid plan mismatch");
	xrtHttp1RequestPlanDestroy(pPlan);
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("trailers")
		), "HTTP prepare TE connection setup failed");
	testRequire(xrtHttp1RequestPrepare(
		pRequest, NULL
	) == NULL, "HTTP prepare accepted TE without Connection option");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_TE) &&
		(xrtErrorCause(pError) == NULL),
		"HTTP prepare TE connection error mismatch");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/")
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("TE"),
			XRT_STR_LITERAL("gzip;q=2")
		) && xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("Connection"),
			XRT_STR_LITERAL("TE")
		), "HTTP prepare invalid TE setup failed");
	testRequire(xrtHttp1RequestPrepare(
		pRequest, NULL
	) == NULL, "HTTP prepare accepted malformed TE");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_TE) &&
		(xrtErrorCause(pError) != NULL) &&
		(xrtErrorKind(xrtErrorCause(pError)) == XERR_VALUE),
		"HTTP prepare malformed TE error mismatch");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);
}



/* 验证歧义或无法执行的用户元数据在网络前被拒绝。 */
static void testHttpPrepareInvalid(void)
{
	static const struct {
		cstr Name1;
		cstr Value1;
		cstr Name2;
		cstr Value2;
	} Cases[] = {
		{ "Host", "a.test", "Host", "b.test" },
		{ "Host", "user@host", NULL, NULL },
		{ "Content-Length", "3", "Content-Length", "3" },
		{ "Content-Length", "3", "Transfer-Encoding", "chunked" },
		{ "Content-Length", "4", NULL, NULL },
		{ "Transfer-Encoding", "gzip, chunked", NULL, NULL },
		{ "Transfer-Encoding", "chunked", "Transfer-Encoding", "chunked" },
		{ "Trailer", "Digest", NULL, NULL },
		{ "Connection", "close keep-alive", NULL, NULL }
	};
	size_t i;

	for ( i = 0; i < (sizeof(Cases) / sizeof(Cases[0])); i++ ) {
		xhttprequest* pRequest = xrtHttpRequestCreate(
			XRT_STR_LITERAL("POST"),
			XRT_STR_LITERAL("http://example.test/")
		);
		xhttp1requestplan* pInvalidPlan;

		testRequire((pRequest != NULL) &&
			xrtHttpRequestSetBytes(
				pRequest,
				(xbytesview){ (cbytes)"abc", 3 },
				(xstrview){ NULL, 0 }
			) && xrtHttpRequestAddHeader(
				pRequest,
				(xstrview){
					Cases[i].Name1,
					strlen(Cases[i].Name1)
				},
				(xstrview){
					Cases[i].Value1,
					strlen(Cases[i].Value1)
				}
			), "HTTP prepare invalid case setup failed");
		if ( Cases[i].Name2 != NULL ) {
			testRequire(xrtHttpRequestAddHeader(
				pRequest,
				(xstrview){
					Cases[i].Name2,
					strlen(Cases[i].Name2)
				},
				(xstrview){
					Cases[i].Value2,
					strlen(Cases[i].Value2)
				}
			), "HTTP prepare invalid duplicate setup failed");
		}
		pInvalidPlan = xrtHttp1RequestPrepare(pRequest, NULL);
		if ( pInvalidPlan != NULL ) {
			fprintf(
				stderr,
				"[INFO] accepted invalid HTTP metadata case %u: %s\n",
				(unsigned)i,
				Cases[i].Name1
			);
			xrtHttp1RequestPlanDestroy(pInvalidPlan);
		}
		testRequire(pInvalidPlan == NULL,
			"HTTP prepare accepted invalid metadata");
		xrtClearError();
		xrtHttpRequestDestroy(pRequest);
	}

	{
		xhttprequest* pTrace = xrtHttpRequestCreate(
			XRT_STR_LITERAL("TRACE"),
			XRT_STR_LITERAL("http://example.test/")
		);

		testRequire((pTrace != NULL) &&
			xrtHttpRequestSetBytes(
				pTrace,
				(xbytesview){ (cbytes)"x", 1 },
				(xstrview){ NULL, 0 }
			) && (xrtHttp1RequestPrepare(
				pTrace, NULL
			) == NULL), "HTTP prepare accepted TRACE body");
		xrtClearError();
		xrtHttpRequestDestroy(pTrace);
	}

	{
		xhttprequest* pCustom = xrtHttpRequestCreate(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("http://example.test/")
		);
		xhttp1requestoptions Options;

		xrtHttp1RequestOptionsInit(&Options);
		Options.TargetForm = XHTTP1_TARGET_CUSTOM;
		Options.CustomTarget = XRT_STR_LITERAL("/bad#fragment");
		testRequire((pCustom != NULL) &&
			(xrtHttp1RequestPrepare(
				pCustom, &Options
			) == NULL) &&
			(xrtGetError() != NULL) &&
			(strcmp(
				xrtErrorDomain(xrtGetError()),
				"http.request"
			) == 0) &&
			(xrtErrorCode(xrtGetError()) ==
				XHTTP_REQUEST_ERROR_TARGET),
			"HTTP prepare custom target error mismatch");
		xrtClearError();
		xrtHttpRequestDestroy(pCustom);
	}
}



/* 运行 HTTP/1 客户端请求准备契约测试。 */
int main(void)
{
	testHttpPrepareOptionsStorage();
	testHttpPrepareDefault();
	testHttpPrepareSnapshot();
	testHttpPrepareTargets();
	testHttpPrepareTargetErrors();
	testHttpPrepareAbsoluteHost();
	testHttpPrepareEmptyPort();
	testHttpPrepareExplicit();
	testHttpPrepareContentLength();
	testHttpPrepareUnknownBody();
	testHttpPrepareDynamic();
	testHttpPrepareExpectErrors();
	testHttpPrepareTe();
	testHttpPrepareInvalid();
	printf("[PASS] HTTP client prepare\n");
	return 0;
}

