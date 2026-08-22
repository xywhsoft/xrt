#include "../test.h"



/* 比较字段值与字面量。 */
static bool testHttpRequestTrailerText(
	const xhttpfield* pField,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (pField != NULL) &&
		(pField->Value.Size == iSize) &&
		((iSize == 0) ||
		 (memcmp(pField->Value.Data, sExpected, iSize) == 0));
}



/* 验证 Trailer 容器惰性创建、重复字段、折叠和直接编辑。 */
static void testHttpRequestTrailersFields(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xhttpheaders* pTrailers;

	testRequire((pRequest != NULL) &&
		(xrtHttpRequestTrailers(pRequest) == NULL) &&
		(xrtHttpRequestTrailerCount(pRequest) == 0) &&
		(xrtHttpRequestTrailerData(pRequest) == NULL) &&
		(xrtHttpRequestTrailerAt(pRequest, 0) == NULL),
		"HTTP request Trailer empty state mismatch");
	testRequire(xrtHttpRequestAddTrailer(
		pRequest,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("one")
	) && xrtHttpRequestAddTrailer(
		pRequest,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("two")
	) && (xrtHttpRequestTrailerCount(pRequest) == 2) &&
		testHttpRequestTrailerText(
			xrtHttpRequestTrailer(
				pRequest, XRT_STR_LITERAL("digest")
			),
			"one"
		), "HTTP request Trailer append mismatch");
	testRequire(xrtHttpRequestSetTrailer(
		pRequest,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("final")
	) && (xrtHttpRequestTrailerCount(pRequest) == 1) &&
		testHttpRequestTrailerText(
			xrtHttpRequestTrailerAt(pRequest, 0), "final"
		) && (xrtHttpRequestTrailerData(pRequest) ==
			xrtHttpRequestTrailerAt(pRequest, 0)),
		"HTTP request Trailer set mismatch");
	pTrailers = xrtHttpRequestEditTrailers(pRequest);
	testRequire((pTrailers != NULL) &&
		xrtHttpHeadersAdd(
			pTrailers,
			XRT_STR_LITERAL("X-Meta"),
			XRT_STR_LITERAL("yes")
		) && (xrtHttpRequestTrailerCount(pRequest) == 2),
		"HTTP request Trailer direct edit mismatch");
	testRequire((xrtHttpRequestRemoveTrailer(
		pRequest, XRT_STR_LITERAL("Digest")
	) == 1) && (xrtHttpRequestTrailerCount(pRequest) == 1),
		"HTTP request Trailer remove mismatch");
	xrtHttpRequestDestroy(pRequest);
}



/* 验证 Clone 深拷贝 Trailer 存储并与源请求独立修改。 */
static void testHttpRequestTrailersClone(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xhttprequest* pClone;

	testRequire((pRequest != NULL) &&
		xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("source")
		), "HTTP request Trailer clone setup failed");
	pClone = xrtHttpRequestClone(pRequest);
	testRequire((pClone != NULL) &&
		xrtHttpRequestSetTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("changed")
		) && testHttpRequestTrailerText(
			xrtHttpRequestTrailer(
				pClone, XRT_STR_LITERAL("Digest")
			),
			"source"
		), "HTTP request Trailer clone was not independent");
	xrtHttpRequestDestroy(pRequest);
	testRequire(testHttpRequestTrailerText(
		xrtHttpRequestTrailer(
			pClone, XRT_STR_LITERAL("Digest")
		),
		"source"
	), "HTTP request Trailer clone did not own storage");
	xrtHttpRequestDestroy(pClone);
}



/* 验证空容器查询和空请求参数边界。 */
static void testHttpRequestTrailersArguments(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("https://example.test/")
	);

	testRequire((pRequest != NULL) &&
		(xrtHttpRequestTrailerData(NULL) == NULL) &&
		(xrtHttpRequestRemoveTrailer(
			pRequest, XRT_STR_LITERAL("Missing")
		) == 0) && (xrtHttpRequestTrailer(
			pRequest, XRT_STR_LITERAL("Missing")
		) == NULL),
		"HTTP request Trailer empty lookup mismatch");
	testRequire(!xrtHttpRequestAddTrailer(
		NULL,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("value")
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP request Trailer accepted null request");
	xrtClearError();
	testRequire(!xrtHttpRequestAddTrailer(
		pRequest,
		XRT_STR_LITERAL("Bad Name"),
		XRT_STR_LITERAL("value")
	), "HTTP request Trailer accepted invalid field name");
	xrtClearError();
	xrtHttpRequestDestroy(pRequest);
}



/* 执行客户端请求 Trailer 容器契约回归。 */
int main(void)
{
	testHttpRequestTrailersFields();
	testHttpRequestTrailersClone();
	testHttpRequestTrailersArguments();
	printf("[PASS] HTTP client request Trailers\n");
	return 0;
}


