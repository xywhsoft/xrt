#include "../test.h"



/* 验证 CORS 不安全字节集合与安全方法。 */
static void testCorsSafelistBasics(void)
{
	testRequire(
		xrtHttpCorsRequestByteUnsafe(0x00u) &&
		xrtHttpCorsRequestByteUnsafe((uint8)'"') &&
		xrtHttpCorsRequestByteUnsafe((uint8)'@') &&
		xrtHttpCorsRequestByteUnsafe(0x7Fu) &&
		!xrtHttpCorsRequestByteUnsafe(0x09u) &&
		!xrtHttpCorsRequestByteUnsafe((uint8)'/') &&
		!xrtHttpCorsRequestByteUnsafe(0x80u),
		"CORS unsafe request byte classification mismatch"
	);
	testRequire(
		xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("GET")) &&
		xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("HEAD")) &&
		xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("POST")) &&
		!xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("get")) &&
		!xrtHttpCorsMethodSafelisted(XRT_STR_LITERAL("PUT")),
		"CORS safelisted method classification mismatch"
	);
}



/* 验证五类 safelist 请求字段及其值边界。 */
static void testCorsSafelistHeaders(void)
{
	char LongValue[129];

	memset(LongValue, 'a', sizeof(LongValue));
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Accept"),
		XRT_STR_LITERAL("text/html, application/json;q=0.8")
	), "CORS Accept safelist rejected a valid value");
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("text/(html)")
	), "CORS Accept safelist accepted an unsafe byte");
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Accept-Language"),
		XRT_STR_LITERAL("zh-CN, en;q=0.8")
	), "CORS language safelist rejected a valid value");
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Language"),
		XRT_STR_LITERAL("zh_CN")
	), "CORS language safelist accepted an underscore");
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain;charset=UTF-8")
	) && xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/x-www-form-urlencoded")
	) && xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("multipart/form-data;boundary=abc")
	), "CORS Content-Type safelist rejected a safe MIME type");
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("application/json")
	), "CORS Content-Type safelist accepted JSON");
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain;")
	) && xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain; =")
	) && xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain;bad")
	), "CORS Content-Type essence rejected an ignorable parameter tail");
	xrtClearError();
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("not-a-mime-type")
	) && !xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text /plain")
	) && !xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain /json")
	) && (xrtGetError() == NULL),
		"CORS Content-Type classification polluted the error slot");
	xrtClearError();
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Range"), XRT_STR_LITERAL("bytes=0-")
	) && xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Range"), XRT_STR_LITERAL("bytes=0002-0010")
	) && !xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Range"), XRT_STR_LITERAL("bytes=-500")
	) && !xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Range"), XRT_STR_LITERAL("bytes=10-2")
	) && !xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Range"), XRT_STR_LITERAL("bytes=0-1,3-4")
	), "CORS Range safelist boundary mismatch");
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Accept"),
		(xstrview){ LongValue, sizeof(LongValue) }
	), "CORS safelist accepted a value above 128 bytes");
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL(" text/plain")
	), "CORS safelist accepted an unnormalized value");
	testRequire(xrtHttpCorsRequestHeaderNonWildcard(
		XRT_STR_LITERAL("authorization")
	) && !xrtHttpCorsRequestHeaderNonWildcard(
		XRT_STR_LITERAL("X-Token")
	), "CORS non-wildcard field classification mismatch");
}



/* 验证 safelist 请求字段总量上限为 1024 字节。 */
static void testCorsSafelistAggregate(void)
{
	char Value[128];
	xhttpfield Fields[9];
	static const xhttpfield DuplicateType[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("text/plain") },
		{ XRT_STR_INIT("content-type"), XRT_STR_INIT("text/plain") }
	};
	static const xhttpfield DuplicateAccept[] = {
		{ XRT_STR_INIT("Accept"), XRT_STR_INIT("text/plain") },
		{ XRT_STR_INIT("accept"), XRT_STR_INIT("application/json") }
	};
	static const xhttpfield DuplicateRange[] = {
		{ XRT_STR_INIT("Range"), XRT_STR_INIT("bytes=0-9") },
		{ XRT_STR_INIT("range"), XRT_STR_INIT("bytes=20-29") }
	};
	size_t i;

	memset(Value, 'a', sizeof(Value));
	for ( i = 0; i < 9u; i++ ) {
		Fields[i].Name = XRT_STR_LITERAL("Accept");
		Fields[i].Value = (xstrview){ Value, sizeof(Value) };
	}
	testRequire(xrtHttpCorsRequestFieldsSafelisted(
		Fields, 8u
	), "CORS safelist rejected exactly 1024 value bytes");
	testRequire(!xrtHttpCorsRequestFieldsSafelisted(
		Fields, 9u
	), "CORS safelist accepted more than 1024 value bytes");
	testRequire(!xrtHttpCorsRequestFieldsSafelisted(
		DuplicateType, 2u
	), "CORS safelist accepted combined Content-Type values");
	testRequire(xrtHttpCorsRequestFieldsSafelisted(
		DuplicateAccept, 2u
	), "CORS safelist rejected safe combined Accept values");
	testRequire(!xrtHttpCorsRequestFieldsSafelisted(
		DuplicateRange, 2u
	), "CORS safelist accepted repeated Range values");
}



/* 验证响应字段默认暴露、显式暴露、星号和 Cookie 例外。 */
static void testCorsExposedHeaders(void)
{
	static const xhttpfield Explicit[] = {
		{ XRT_STR_INIT("Access-Control-Expose-Headers"), XRT_STR_INIT("X-Trace, X-Rate") }
	};
	static const xhttpfield Wildcard[] = {
		{ XRT_STR_INIT("Access-Control-Expose-Headers"), XRT_STR_INIT("*") }
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("Access-Control-Expose-Headers"), XRT_STR_INIT("X-Trace") },
		{ XRT_STR_INIT("access-control-expose-headers"), XRT_STR_INIT("bad name") }
	};

	testRequire(xrtHttpCorsResponseHeaderSafelisted(
		XRT_STR_LITERAL("content-length")
	) && !xrtHttpCorsResponseHeaderSafelisted(
		XRT_STR_LITERAL("X-Trace")
	), "CORS response safelist mismatch");
	testRequire(xrtHttpCorsResponseHeaderForbidden(
		XRT_STR_LITERAL("set-cookie")
	) && xrtHttpCorsResponseHeaderForbidden(
		XRT_STR_LITERAL("Set-Cookie2")
	), "CORS forbidden response field mismatch");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Explicit, 1u, XRT_STR_LITERAL("X-TRACE"), false
	) == XHTTP_NEXT_ITEM,
		"CORS explicit exposed field was hidden");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Explicit, 1u, XRT_STR_LITERAL("X-Other"), false
	) == XHTTP_NEXT_END,
		"CORS unlisted response field was exposed");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Wildcard, 1u, XRT_STR_LITERAL("X-Other"), false
	) == XHTTP_NEXT_ITEM,
		"CORS non-credentialed wildcard did not expose a field");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Wildcard, 1u, XRT_STR_LITERAL("X-Other"), true
	) == XHTTP_NEXT_END,
		"CORS credentialed wildcard exposed a field");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Wildcard, 1u, XRT_STR_LITERAL("Set-Cookie"), false
	) == XHTTP_NEXT_END,
		"CORS wildcard exposed Set-Cookie");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Invalid, 2u, XRT_STR_LITERAL("X-Trace"), false
	) == XHTTP_NEXT_ERROR,
		"CORS exposure published before full list validation");
	xrtClearError();
}



/* 执行 Fetch CORS safelist 测试。 */
int main(void)
{
	testCorsSafelistBasics();
	testCorsSafelistHeaders();
	testCorsSafelistAggregate();
	testCorsExposedHeaders();
	printf("[PASS] http_cors_safelist\n");
	return 0;
}
