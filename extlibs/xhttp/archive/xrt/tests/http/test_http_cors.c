#include "../test.h"



/* 验证 CORS 单字段值严格遵守 Fetch 线路语法。 */
static void testCorsValues(void)
{
	xhttpcorsorigin AllowOrigin;
	xhttporigin Expected;
	xstrview Method;
	uint64 iSeconds;

	testRequire(xrtHttpCorsMethodParse(
		XRT_STR_LITERAL(" \tPATCH\t"), &Method
	) && xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("PATCH")
	), "CORS method parse mismatch");
	testRequire(xrtHttpCorsAllowOriginParse(
		XRT_STR_LITERAL(" * "), &AllowOrigin
	) && ((AllowOrigin.Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0),
		"CORS wildcard origin parse mismatch");
	testRequire(xrtHttpCorsAllowOriginParse(
		XRT_STR_LITERAL("https://app.example:443"),
		&AllowOrigin
	) && xrtHttpOriginParse(
		XRT_STR_LITERAL("https://APP.example"), &Expected
	) && xrtHttpOriginSame(
		&AllowOrigin.Origin, &Expected
	), "CORS serialized origin parse mismatch");
	testRequire(xrtHttpCorsAllowOriginParse(
		XRT_STR_LITERAL("null"), &AllowOrigin
	) && ((AllowOrigin.Origin.Flags &
		XHTTP_ORIGIN_NULL) != 0),
		"CORS null origin parse mismatch");
	testRequire(xrtHttpCorsAllowCredentialsParse(
		XRT_STR_LITERAL(" \ttrue ")
	), "CORS credentials true was rejected");
	testRequire(!xrtHttpCorsAllowCredentialsParse(
		XRT_STR_LITERAL("True")
	), "CORS credentials parser ignored case");
	xrtClearError();
	testRequire(xrtHttpCorsMaxAgeParse(
		XRT_STR_LITERAL("18446744073709551615"), &iSeconds
	) && (iSeconds == UINT64_MAX),
		"CORS maximum Max-Age was rejected");
	iSeconds = 99u;
	testRequire(!xrtHttpCorsMaxAgeParse(
		XRT_STR_LITERAL("18446744073709551616"), &iSeconds
	) && (iSeconds == 0),
		"CORS overflowing Max-Age was accepted");
	xrtClearError();
	testRequire(!xrtHttpCorsAllowOriginParse(
		XRT_STR_LITERAL("https://one.example https://two.example"),
		&AllowOrigin
	), "CORS Allow-Origin accepted an Origin list");
	xrtClearError();
}



/* 验证请求读取区分普通 Origin 请求与完整预检。 */
static void testCorsRequest(void)
{
	static const xhttpfield Preflight[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("Content-Type, X-Trace") },
		{ XRT_STR_INIT("access-control-request-headers"), XRT_STR_INIT("X-Token") }
	};
	static const xhttpfield Simple[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("null") }
	};
	static const xhttpfield HeadersOnly[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("X-Test") }
	};
	static const xhttpfield EmptyHeaders[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT(" , \t,") }
	};
	static const xhttpfield SplitHeaders[] = {
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("") },
		{ XRT_STR_INIT("access-control-request-headers"), XRT_STR_INIT("X-One") }
	};
	static const char* Expected[] = {
		"Content-Type", "X-Trace", "X-Token"
	};
	xhttpcorsrequest Request;
	xhttpcorscursor Cursor;
	xstrview Name;
	size_t i;

	testRequire(xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"),
		Preflight,
		4u,
		&Request
	) && ((Request.Flags & XHTTP_CORS_REQUEST_ORIGIN) != 0) &&
		((Request.Flags & XHTTP_CORS_REQUEST_PREFLIGHT) != 0) &&
		((Request.Flags & XHTTP_CORS_REQUEST_HEADERS) != 0) &&
		xrtHttpMethodEqual(
			Request.RequestMethod, XRT_STR_LITERAL("PATCH")
		) && (Request.HeaderCount == 3u),
		"CORS preflight request read mismatch");
	xrtHttpCorsCursorInit(&Cursor);
	for ( i = 0; i < 3u; i++ ) {
		testRequire(
			(xrtHttpCorsRequestHeaderNext(
				Preflight, 4u, &Cursor, &Name
			 ) == XHTTP_NEXT_ITEM) &&
			(Name.Size == strlen(Expected[i])) &&
			(memcmp(Name.Data, Expected[i], Name.Size) == 0),
			"CORS request header order mismatch"
		);
	}
	testRequire(xrtHttpCorsRequestHeaderNext(
		Preflight, 4u, &Cursor, &Name
	) == XHTTP_NEXT_END,
		"CORS request header cursor did not end");

	testRequire(xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("GET"), Simple, 1u, &Request
	) && (Request.Flags == XHTTP_CORS_REQUEST_ORIGIN) &&
		((Request.Origin.Flags & XHTTP_ORIGIN_NULL) != 0),
		"CORS simple request read mismatch");
	testRequire(xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("GET"), NULL, 0, &Request
	) && (Request.Flags == XHTTP_CORS_REQUEST_NONE),
		"CORS non-CORS request was rejected");
	testRequire(!xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("GET"), Preflight, 4u, &Request
	), "CORS preflight accepted a non-OPTIONS request");
	xrtClearError();
	testRequire(!xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"), HeadersOnly, 2u, &Request
	), "CORS request headers were accepted without request method");
	xrtClearError();
	testRequire(!xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"), EmptyHeaders, 3u, &Request
	), "CORS empty Request-Headers field was accepted");
	xrtClearError();
	xrtHttpCorsCursorInit(&Cursor);
	Name = XRT_STR_LITERAL("sentinel");
	testRequire(
		(xrtHttpCorsRequestHeaderNext(
			EmptyHeaders, 3u, &Cursor, &Name
		) == XHTTP_NEXT_ERROR) &&
		(Name.Data == NULL) && (Name.Size == 0),
		"CORS low-level cursor accepted an empty Request-Headers field"
	);
	xrtClearError();
	xrtHttpCorsCursorInit(&Cursor);
	testRequire(
		(xrtHttpCorsRequestHeaderNext(
			SplitHeaders, 2u, &Cursor, &Name
		) == XHTTP_NEXT_ITEM) &&
		(Name.Size == 5u) &&
		(memcmp(Name.Data, "X-One", Name.Size) == 0),
		"CORS required list rejected empty members around a valid item"
	);
}



/* 验证响应读取覆盖单值字段、重复列表字段和存在位。 */
static void testCorsResponse(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Credentials"), XRT_STR_INIT("true") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("GET, POST") },
		{ XRT_STR_INIT("access-control-allow-methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("Content-Type, X-Token") },
		{ XRT_STR_INIT("Access-Control-Expose-Headers"), XRT_STR_INIT("X-RateLimit") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("86400") }
	};
	static const xhttpfield EmptyList[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("") }
	};
	static const xhttpfield Duplicate[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("access-control-allow-origin"), XRT_STR_INIT("null") }
	};
	static const xhttpfield BadCredentials[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Credentials"), XRT_STR_INIT("TRUE") }
	};
	xhttpcorsresponse Response;
	xhttpcorscursor Cursor;
	xstrview Method;

	testRequire(xrtHttpCorsResponseRead(
		Fields, 7u, &Response
	) && ((Response.Flags &
		XHTTP_CORS_RESPONSE_ALLOW_ORIGIN) != 0) &&
		((Response.Flags &
		XHTTP_CORS_RESPONSE_CREDENTIALS) != 0) &&
		((Response.Flags &
		XHTTP_CORS_RESPONSE_MAX_AGE) != 0) &&
		(Response.MethodCount == 3u) &&
		(Response.HeaderCount == 2u) &&
		(Response.ExposeCount == 1u) &&
		(Response.MaxAge == 86400u),
		"CORS response summary mismatch");
	xrtHttpCorsCursorInit(&Cursor);
	testRequire(
		(xrtHttpCorsAllowMethodNext(
			Fields, 7u, &Cursor, &Method
		 ) == XHTTP_NEXT_ITEM) &&
		xrtHttpMethodEqual(Method, XRT_STR_LITERAL("GET")),
		"CORS allow method cursor mismatch"
	);
	testRequire(xrtHttpCorsResponseRead(
		EmptyList, 2u, &Response
	) && ((Response.AllowOrigin.Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0) &&
		((Response.Flags &
		XHTTP_CORS_RESPONSE_ALLOW_METHODS) != 0) &&
		(Response.MethodCount == 0),
		"CORS empty response list lost field presence");
	testRequire(!xrtHttpCorsResponseRead(
		Duplicate, 2u, &Response
	), "CORS response accepted duplicate Allow-Origin");
	xrtClearError();
	testRequire(!xrtHttpCorsResponseRead(
		BadCredentials, 1u, &Response
	), "CORS response accepted invalid credentials value");
	xrtClearError();
}



/* 验证 CORS 读取支持未对齐描述符和输出存储。 */
static void testCorsUnaligned(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") }
	};
	uint8 FieldStorage[sizeof(Fields) + 2u];
	uint8 OutputStorage[sizeof(xhttpcorsrequest) + 2u];
	xhttpcorsrequest Request;

	memset(FieldStorage, 0xA5, sizeof(FieldStorage));
	memset(OutputStorage, 0xA5, sizeof(OutputStorage));
	memcpy(FieldStorage + 1u, Fields, sizeof(Fields));
	testRequire(xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("GET"),
		(const xhttpfield*)(const void*)(FieldStorage + 1u),
		1u,
		(xhttpcorsrequest*)(void*)(OutputStorage + 1u)
	), "CORS request rejected unaligned storage");
	memcpy(&Request, OutputStorage + 1u, sizeof(Request));
	testRequire(
		(Request.Flags == XHTTP_CORS_REQUEST_ORIGIN) &&
		(FieldStorage[0] == 0xA5) &&
		(FieldStorage[sizeof(FieldStorage) - 1u] == 0xA5) &&
		(OutputStorage[0] == 0xA5) &&
		(OutputStorage[sizeof(OutputStorage) - 1u] == 0xA5),
		"CORS unaligned read corrupted guard bytes"
	);
}



/* 执行 CORS 协议字段与组合读取测试。 */
int main(void)
{
	testCorsValues();
	testCorsRequest();
	testCorsResponse();
	testCorsUnaligned();
	printf("[PASS] http_cors\n");
	return 0;
}
