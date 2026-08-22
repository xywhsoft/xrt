#include "../test.h"



/* 比较借用字符串视图和零结尾字面量。 */
static bool testHttpRequestViewEqual(
	xstrview View,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (View.Size == iSize) &&
		((iSize == 0) ||
			(memcmp(View.Data, sExpected, iSize) == 0));
}



/* 验证 URL、方法、Header 和失败原子修改。 */
static void testHttpRequestMetadata(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/a/b?q=1#local"
		)
	);
	const xurl* pUrl;
	const xhttpfield* pField;
	uint16 iPort;

	testRequire(pRequest != NULL,
		"HTTP client request create failed");
	pUrl = xrtHttpRequestUrl(pRequest);
	testRequire((pUrl != NULL) &&
		testHttpRequestViewEqual(
			xrtHttpRequestMethod(pRequest), "GET"
		) &&
		testHttpRequestViewEqual(
			xrtHttpRequestUrlText(pRequest),
			"https://example.test:8443/a/b?q=1#local"
		) &&
		testHttpRequestViewEqual(pUrl->Host, "example.test") &&
		(pUrl->Port == 8443) &&
		testHttpRequestViewEqual(pUrl->Path, "/a/b") &&
		testHttpRequestViewEqual(pUrl->Query, "q=1"),
		"HTTP client request URL metadata mismatch");

	testRequire(xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("one")
	), "HTTP request first Header add failed");
	testRequire(xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("x-test"),
		XRT_STR_LITERAL("two")
	), "HTTP request duplicate Header add failed");
	testRequire((xrtHttpRequestHeaderCount(pRequest) == 2) &&
		(xrtHttpRequestHeaderData(pRequest) != NULL) &&
		(xrtHttpRequestHeaderData(pRequest) ==
		 xrtHttpRequestHeaderAt(pRequest, 0)) &&
		(xrtHttpRequestHeaderAt(pRequest, 1) != NULL) &&
		(xrtHttpRequestHeaders(pRequest) != NULL) &&
		(xrtHttpHeadersCount(
			xrtHttpRequestHeaders(pRequest)
		) == 2),
		"HTTP request Header count mismatch");
	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("X-Test"),
		XRT_STR_LITERAL("final")
	), "HTTP request Header set failed");
	pField = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("x-test")
	);
	testRequire((xrtHttpRequestHeaderCount(pRequest) == 1) &&
		(pField != NULL) &&
		testHttpRequestViewEqual(pField->Value, "final"),
		"HTTP request Header replacement mismatch");

	testRequire(!xrtHttpRequestSetMethod(
		pRequest, XRT_STR_LITERAL("BAD METHOD")
	), "HTTP request accepted invalid method");
	testRequire(testHttpRequestViewEqual(
		xrtHttpRequestMethod(pRequest), "GET"
	), "invalid method changed HTTP request");
	testRequire((xrtGetError() != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()), "http.request"
		) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
			XHTTP_REQUEST_ERROR_METHOD),
		"invalid method reported wrong error");
	xrtClearError();

	testRequire(!xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("ftp://example.test/file")
	), "HTTP request accepted unsupported scheme");
	testRequire(!xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("https://user@example.test/")
	), "HTTP request accepted URL userinfo");
	testRequire(!xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("https://example.test:65536/")
	) && (xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_REQUEST_ERROR_URL) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(xrtErrorKind(xrtErrorCause(xrtGetError())) == XERR_RANGE),
		"HTTP request accepted a port outside the network range");
	xrtClearError();
	testRequire(!xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("https://example.test:0/")
	) && (xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_REQUEST_ERROR_URL),
		"HTTP request accepted network port zero");
	testRequire(testHttpRequestViewEqual(
		xrtHttpRequestUrlText(pRequest),
		"https://example.test:8443/a/b?q=1#local"
	), "invalid URL changed HTTP request");
	xrtClearError();

	testRequire(xrtHttpRequestSetUrl(
		pRequest,
		XRT_STR_LITERAL("http://example.test:/empty-port")
	), "HTTP request rejected a standard empty port");
	pUrl = xrtHttpRequestUrl(pRequest);
	testRequire((pUrl != NULL) &&
		((pUrl->Flags & XURL_PORT_EMPTY) != 0) &&
		xrtUrlPort(pUrl, &iPort) && (iPort == 80),
		"HTTP request empty port default mismatch");

	testRequire((xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("X-Test")
	) == 1) &&
		(xrtHttpRequestHeaderCount(pRequest) == 0),
		"HTTP request Header remove failed");
	xrtHttpRequestDestroy(pRequest);
}



/* 验证正文引用、复制便利入口和 Clone 独立性。 */
static void testHttpRequestBodyAndClone(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("http://example.test/api")
	);
	xhttprequest* pClone;
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	const xhttpfield* pType;

	testRequire(pRequest != NULL,
		"HTTP request body setup failed");
	testRequire(xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)"{\"ok\":true}", 11 },
		XRT_STR_LITERAL("application/json; charset=utf-8")
	), "HTTP request byte body set failed");
	testRequire((xrtHttpRequestBody(pRequest) != NULL) &&
		(xrtHttpBodyLength(
			xrtHttpRequestBody(pRequest)
		) == 11),
		"HTTP request body metadata mismatch");
	pType = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("Content-Type")
	);
	testRequire((pType != NULL) &&
		testHttpRequestViewEqual(
			pType->Value,
			"application/json; charset=utf-8"
		), "HTTP request Content-Type mismatch");

	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("X-Clone"),
		XRT_STR_LITERAL("original")
	), "HTTP request clone Header setup failed");
	pClone = xrtHttpRequestClone(pRequest);
	testRequire(pClone != NULL,
		"HTTP request clone failed");
	testRequire(xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("X-Clone"),
		XRT_STR_LITERAL("changed")
	), "HTTP request original mutation failed");
	testRequire(testHttpRequestViewEqual(
		xrtHttpRequestHeader(
			pClone, XRT_STR_LITERAL("X-Clone")
		)->Value,
		"original"
	), "HTTP request clone shared mutable Header storage");

	xrtHttpRequestDestroy(pRequest);
	pReader = xrtHttpBodyOpen(xrtHttpRequestBody(pClone));
	testRequire((pReader != NULL) &&
		(xrtHttpBodyNext(
			pReader, 32, &Chunk
		) == XHTTP_BODY_DATA) &&
		(Chunk.Size == 11) &&
		(memcmp(Chunk.Data, "{\"ok\":true}", 11) == 0),
		"HTTP request clone lost body lifetime");
	xrtHttpBodyChunkRelease(&Chunk);
	xrtHttpBodyReaderDestroy(pReader);
	xrtHttpRequestDestroy(pClone);
}



/* 验证请求方法和 URL 不再受旧版内联数组容量限制。 */
static void testHttpRequestDynamicText(void)
{
	char Method[257];
	char Url[6144];
	char* pValue;
	xhttpheadersconfig Config;
	xhttprequest* pRequest;
	size_t i;

	memset(Method, 'M', sizeof(Method) - 1);
	Method[sizeof(Method) - 1] = '\0';
	memcpy(Url, "https://example.test/", 21);
	for ( i = 21; i < sizeof(Url) - 1; i++ ) {
		Url[i] = 'a';
	}
	Url[sizeof(Url) - 1] = '\0';
	pRequest = xrtHttpRequestCreate(
		(xstrview){ Method, sizeof(Method) - 1 },
		(xstrview){ Url, sizeof(Url) - 1 }
	);
	testRequire((pRequest != NULL) &&
		(xrtHttpRequestMethod(pRequest).Size ==
			sizeof(Method) - 1) &&
		(xrtHttpRequestUrlText(pRequest).Size ==
			sizeof(Url) - 1),
		"HTTP request retained fixed text capacity");
	xrtHttpRequestDestroy(pRequest);

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialBytes = 0;
	Config.MaxValue = 96u * 1024u;
	Config.MaxBytes = 128u * 1024u;
	pValue = (char*)xrtMalloc(Config.MaxValue);
	testRequire(pValue != NULL,
		"HTTP request long Header allocation failed");
	memset(pValue, 'v', Config.MaxValue);
	pRequest = xrtHttpRequestCreateWithHeaders(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/"),
		&Config
	);
	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddHeader(
			pRequest,
			XRT_STR_LITERAL("X-Large"),
			(xstrview){ pValue, Config.MaxValue }
		) &&
		(xrtHttpRequestHeader(
			pRequest, XRT_STR_LITERAL("X-Large")
		)->Value.Size == Config.MaxValue),
		"HTTP request imposed a hidden Header value limit");
	xrtHttpRequestDestroy(pRequest);
	xrtFree(pValue);
}



/* 验证非法视图、URL 原因链和 Clone 配置保持明确。 */
static void testHttpRequestContracts(void)
{
	xhttpheadersconfig Config;
	xhttprequest* pRequest;
	xhttprequest* pClone;
	const xerror* pError;

	pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/original")
	);
	testRequire(pRequest != NULL,
		"HTTP request contract fixture failed");
	testRequire(!xrtHttpRequestSetMethod(
		pRequest, (xstrview){ NULL, 1 }
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		testHttpRequestViewEqual(
			xrtHttpRequestMethod(pRequest), "GET"
		), "HTTP request invalid method view changed state");
	xrtClearError();

	testRequire(!xrtHttpRequestSetUrl(
		pRequest, (xstrview){ NULL, 1 }
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		testHttpRequestViewEqual(
			xrtHttpRequestUrlText(pRequest),
			"https://example.test/original"
		), "HTTP request invalid URL view changed state");
	xrtClearError();

	testRequire(!xrtHttpRequestSetUrl(
		pRequest, (xstrview){ NULL, 0 }
	), "HTTP request accepted an empty URL");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_URL),
		"HTTP request empty URL used the wrong error");
	xrtClearError();

	testRequire(!xrtHttpRequestSetUrl(
		pRequest, XRT_STR_LITERAL("http://[::1")
	), "HTTP request accepted a malformed URL");
	pError = xrtGetError();
	testRequire((pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "http.request") == 0) &&
		(xrtErrorCode(pError) == XHTTP_REQUEST_ERROR_URL) &&
		(xrtErrorCause(pError) != NULL) &&
		testHttpRequestViewEqual(
			xrtHttpRequestUrlText(pRequest),
			"https://example.test/original"
		), "HTTP request malformed URL lost its cause or state");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);

	testRequire((xrtHttpRequestClone(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request Clone accepted a null request");
	xrtClearError();

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialFields = 0;
	Config.InitialBytes = 0;
	Config.MaxFields = 1;
	pRequest = xrtHttpRequestCreateWithHeaders(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/limited"),
		&Config
	);
	testRequire((pRequest != NULL) && xrtHttpRequestAddHeader(
		pRequest,
		XRT_STR_LITERAL("X-One"),
		XRT_STR_LITERAL("1")
	), "HTTP request limited Header fixture failed");
	pClone = xrtHttpRequestClone(pRequest);
	testRequire((pClone != NULL) && !xrtHttpRequestAddHeader(
		pClone,
		XRT_STR_LITERAL("X-Two"),
		XRT_STR_LITERAL("2")
	) && (xrtHttpRequestHeaderCount(pClone) == 1),
		"HTTP request Clone lost Header limits");
	xrtClearError();
	xrtHttpRequestDestroy(pClone);
	xrtHttpRequestDestroy(pRequest);
}



/* 运行客户端请求构建器契约测试。 */
int main(void)
{
	testHttpRequestMetadata();
	testHttpRequestBodyAndClone();
	testHttpRequestDynamicText();
	testHttpRequestContracts();
	printf("[PASS] HTTP client request\n");
	return 0;
}
