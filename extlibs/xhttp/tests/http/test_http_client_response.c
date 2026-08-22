#include "../test.h"
#include "../../src/internal/xrt_http_client.h"



/* 比较借用字符串视图和字面量。 */
static bool testHttpResponseViewEqual(
	xstrview View,
	cstr sExpected
)
{
	size_t iSize = strlen(sExpected);

	return (View.Size == iSize) &&
		((iSize == 0) ||
			(memcmp(View.Data, sExpected, iSize) == 0));
}



/* 验证缓冲响应的元数据、正文、trailer 和有效 URL。 */
static void testHttpResponseBuffered(void)
{
	xhttpresponse* pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		201,
		XRT_STR_LITERAL("Created"),
		NULL
	);
	const xhttpfield* pField;
	xbytesview Body;
	str sText;

	testRequire((pResponse != NULL) &&
		(pResponse->Trailers == NULL),
		"HTTP client response eagerly allocated trailers");
	xrtClearError();
	testRequire(
		(xrtHttpResponseTrailerCount(pResponse) == 0) &&
		(xrtHttpResponseTrailerData(pResponse) == NULL) &&
		(xrtHttpResponseTrailerAt(pResponse, 0) == NULL) &&
		(xrtHttpResponseTrailer(
			pResponse, XRT_STR_LITERAL("Digest")
		) == NULL) &&
		(xrtHttpResponseTrailers(pResponse) == NULL) &&
		(xrtGetError() == NULL),
		"empty HTTP response trailers polluted the error state"
	);
	xrtClearError();
	testRequire((xrtHttpResponseTrailer(
		pResponse, XRT_STR_LITERAL("bad name")
	) == NULL) && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"empty HTTP response trailer bypassed name validation"
	);
	xrtClearError();
	testRequire(__xrtHttpResponseAddHeader(
		pResponse,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("a=1")
	) && __xrtHttpResponseAddHeader(
		pResponse,
		XRT_STR_LITERAL("set-cookie"),
		XRT_STR_LITERAL("b=2")
	), "HTTP response duplicate Header add failed");
	testRequire(__xrtHttpResponseAddTrailer(
		pResponse,
		NULL,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("sha-256=:AA==:")
	), "HTTP response trailer add failed");
	testRequire(__xrtHttpResponseAppendBody(
		pResponse,
		(xbytesview){ (cbytes)"hel", 3 }
	) && __xrtHttpResponseAppendBody(
		pResponse,
		(xbytesview){ (cbytes)"lo\0x", 4 }
	) && __xrtHttpResponseAddWireBody(pResponse, 7) &&
		__xrtHttpResponseSetUrl(
			pResponse,
			XRT_STR_LITERAL("https://example.test/final")
		), "HTTP response body or URL append failed");

	testRequire((xrtHttpResponseVersion(pResponse) ==
			XHTTP_VERSION_1_1) &&
		(xrtHttpResponseStatus(pResponse) == 201) &&
		xrtHttpResponseSuccess(pResponse) &&
		testHttpResponseViewEqual(
			xrtHttpResponseReason(pResponse), "Created"
		) &&
		(xrtHttpResponseHeaderCount(pResponse) == 2) &&
		(xrtHttpResponseTrailerCount(pResponse) == 1) &&
		(xrtHttpResponseHeaderData(pResponse) ==
			xrtHttpResponseHeaderAt(pResponse, 0)) &&
		(xrtHttpResponseTrailerData(pResponse) ==
			xrtHttpResponseTrailerAt(pResponse, 0)) &&
		(xrtHttpResponseHeaders(pResponse) != NULL) &&
		(xrtHttpHeadersCount(
			xrtHttpResponseHeaders(pResponse)
		) == 2) &&
		(xrtHttpResponseTrailers(pResponse) != NULL) &&
		(xrtHttpHeadersCount(
			xrtHttpResponseTrailers(pResponse)
		) == 1) &&
		(xrtHttpResponseBodyBytes(pResponse) == 7) &&
		(xrtHttpResponseWireBodyBytes(pResponse) == 7) &&
		testHttpResponseViewEqual(
			xrtHttpResponseUrl(pResponse),
			"https://example.test/final"
		), "HTTP response metadata mismatch");
	pField = xrtHttpResponseHeader(
		pResponse, XRT_STR_LITERAL("SET-COOKIE")
	);
	testRequire((pField != NULL) &&
		testHttpResponseViewEqual(pField->Value, "a=1") &&
		(xrtHttpResponseHeaderAt(pResponse, 1) != NULL) &&
		(xrtHttpResponseTrailer(
			pResponse, XRT_STR_LITERAL("digest")
		) != NULL) &&
		(xrtHttpResponseTrailerAt(pResponse, 0) != NULL),
		"HTTP response Header or trailer lookup mismatch");
	Body = xrtHttpResponseBody(pResponse);
	testRequire((Body.Size == 7) &&
		(memcmp(Body.Data, "hello\0x", 7) == 0) &&
		(pResponse->BodyCapacity == 12),
		"HTTP response buffered body mismatch");
	sText = xrtHttpResponseBodyText(pResponse);
	testRequire((sText != NULL) &&
		(memcmp(sText, "hello\0x\0", 8) == 0),
		"HTTP response body text copy mismatch");
	xrtFree(sText);
	xrtHttpResponseDestroy(pResponse);
	testRequire(
		(xrtHttpResponseHeaderData(NULL) == NULL) &&
		(xrtHttpResponseTrailerData(NULL) == NULL),
		"HTTP response field data accepted null response"
	);
}



/* 验证响应文本和字段只受显式配置限制，不受旧固定数组容量限制。 */
static void testHttpResponseDynamicStorage(void)
{
	static char Reason[6144];
	static char Value[98304];
	xhttpresponse* pResponse;
	const xhttpfield* pField;

	memset(Reason, 'r', sizeof(Reason));
	memset(Value, 'v', sizeof(Value));
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		299,
		(xstrview){ Reason, sizeof(Reason) },
		NULL
	);
	testRequire(pResponse != NULL,
		"dynamic HTTP response reason create failed");
	testRequire(__xrtHttpResponseAddHeader(
		pResponse,
		XRT_STR_LITERAL("X-Large"),
		(xstrview){ Value, sizeof(Value) }
	), "dynamic HTTP response Header add failed");
	pField = xrtHttpResponseHeader(
		pResponse, XRT_STR_LITERAL("x-large")
	);
	testRequire(
		(xrtHttpResponseReason(pResponse).Size ==
			sizeof(Reason)) &&
		(pField != NULL) &&
		(pField->Value.Size == sizeof(Value)) &&
		(pField->Value.Data[sizeof(Value) - 1] == 'v'),
		"dynamic HTTP response storage was truncated"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 验证按需 trailer 容器继承调用方配置并保持既有内容。 */
static void testHttpResponseTrailerConfig(void)
{
	xhttpheadersconfig Config;
	xhttpresponse* pResponse;

	xrtHttpHeadersConfigInit(&Config);
	Config.InitialFields = 0;
	Config.InitialBytes = 0;
	Config.MaxFields = 1;
	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("OK"),
		NULL
	);
	testRequire((pResponse != NULL) &&
		(pResponse->Trailers == NULL),
		"HTTP response trailer config setup failed");
	testRequire(__xrtHttpResponseAddTrailer(
		pResponse,
		&Config,
		XRT_STR_LITERAL("Digest"),
		XRT_STR_LITERAL("one")
	), "HTTP response first lazy trailer failed");
	xrtClearError();
	testRequire(!__xrtHttpResponseAddTrailer(
		pResponse,
		NULL,
		XRT_STR_LITERAL("X-Extra"),
		XRT_STR_LITERAL("two")
	) && (xrtHttpResponseTrailerCount(pResponse) == 1) &&
		(xrtHttpResponseTrailer(
			pResponse, XRT_STR_LITERAL("Digest")
		) != NULL),
		"HTTP response trailer limit was not preserved"
	);
	xrtHttpResponseDestroy(pResponse);
}



/* 验证流式响应只累计字节而不分配正文缓冲。 */
static void testHttpResponseStreamed(void)
{
	xhttpresponse* pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_0,
		200,
		(xstrview){ NULL, 0 },
		NULL
	);

	testRequire(pResponse != NULL,
		"streamed HTTP response create failed");
	__xrtHttpResponseSetFlags(
		pResponse, XHTTP_RESPONSE_STREAMED
	);
	testRequire(__xrtHttpResponseDeliverBody(
		pResponse, UINT64_C(4)
	) && __xrtHttpResponseDeliverBody(
		pResponse, UINT64_C(9)
	), "streamed HTTP response byte delivery failed");
	testRequire(((xrtHttpResponseFlags(pResponse) &
			XHTTP_RESPONSE_STREAMED) != 0) &&
		(xrtHttpResponseBodyBytes(pResponse) == 13) &&
		(xrtHttpResponseBody(pResponse).Data == NULL) &&
		(xrtHttpResponseBody(pResponse).Size == 0) &&
		xrtHttpResponseSuccess(pResponse),
		"streamed HTTP response allocated or reported wrong state");
	testRequire(!__xrtHttpResponseAppendBody(
		pResponse,
		(xbytesview){ (cbytes)"bad", 3 }
	), "streamed HTTP response accepted buffered body");
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
}



/* 验证起始行、借用范围、失败原子性和计数溢出的防御边界。 */
static void testHttpResponseContracts(void)
{
	xhttpresponse* pResponse;
	xbytesview Body;

	testRequire(xrtHttpResponseBodyText(NULL) == NULL,
		"HTTP response text copy accepted null response");
	xrtClearError();
	testRequire(__xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		99,
		XRT_STR_LITERAL("bad"),
		NULL
	) == NULL, "HTTP response accepted two-digit status");
	xrtClearError();
	testRequire(__xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("bad\rreason"),
		NULL
	) == NULL, "HTTP response accepted invalid reason");
	xrtClearError();
	testRequire(__xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		},
		NULL
	) == NULL &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP response accepted a wrapping reason view");
	xrtClearError();

	pResponse = __xrtHttpResponseCreate(
		XHTTP_VERSION_1_1,
		200,
		XRT_STR_LITERAL("OK"),
		NULL
	);
	testRequire(pResponse != NULL,
		"HTTP response overflow setup failed");
	testRequire(__xrtHttpResponseSetUrl(
		pResponse, XRT_STR_LITERAL("https://example.test/original")
	) && __xrtHttpResponseAppendBody(
		pResponse, (xbytesview){ (cbytes)"a", 1u }
	), "HTTP response atomicity setup failed");
	testRequire(!__xrtHttpResponseSetUrl(
		pResponse,
		(xstrview){
			(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		}
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		testHttpResponseViewEqual(
			xrtHttpResponseUrl(pResponse),
			"https://example.test/original"
		), "HTTP response wrapping URL replacement was not atomic");
	xrtClearError();
	testRequire(!__xrtHttpResponseAppendBody(
		pResponse,
		(xbytesview){
			(cbytes)(uintptr_t)(UINTPTR_MAX - 1u), 4u
		}
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtHttpResponseBodyBytes(pResponse) == 1u),
		"HTTP response wrapping body append was not atomic");
	xrtClearError();
	testRequire(__xrtHttpResponseAppendBody(
		pResponse, (xbytesview){ (cbytes)"b", 1u }
	), "HTTP response was unusable after rejecting a body range");
	Body = xrtHttpResponseBody(pResponse);
	testRequire((Body.Size == 2u) &&
		(memcmp(Body.Data, "ab", 2u) == 0) &&
		(xrtHttpResponseBodyBytes(pResponse) == 2u),
		"HTTP response body changed after a rejected range");

	__xrtHttpResponseSetFlags(
		pResponse, XHTTP_RESPONSE_STREAMED
	);
	pResponse->BodyBytes = UINT64_MAX;
	testRequire(!__xrtHttpResponseDeliverBody(
		pResponse, 1
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP response body count overflow succeeded");
	xrtClearError();
	pResponse->BodyBytes = 0;
	pResponse->WireBodyBytes = UINT64_MAX;
	testRequire(!__xrtHttpResponseAddWireBody(
		pResponse, 1
	) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"HTTP response wire count overflow succeeded");
	xrtClearError();
	xrtHttpResponseDestroy(pResponse);
}



/* 运行客户端响应结果层测试。 */
int main(void)
{
	testHttpResponseBuffered();
	testHttpResponseDynamicStorage();
	testHttpResponseTrailerConfig();
	testHttpResponseStreamed();
	testHttpResponseContracts();
	printf("[PASS] HTTP client response\n");
	return 0;
}

