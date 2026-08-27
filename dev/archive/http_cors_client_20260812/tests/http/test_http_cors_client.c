#include "../test.h"



/* 解析测试请求 Origin。 */
static xhttporigin testCorsClientOrigin(void)
{
	xhttporigin Origin;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"),
		&Origin
	), "CORS client request Origin parse failed");
	return Origin;
}



/* 验证预检触发原因、唯一字段计数和安全值总量。 */
static void testCorsClientPlan(void)
{
	char LongValue[128];
	xhttpfield Large[9];
	static const xhttpfield Safe[] = {
		{ XRT_STR_INIT("Accept"), XRT_STR_INIT("text/plain") }
	};
	static const xhttpfield Unsafe[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("a") },
		{ XRT_STR_INIT("x-trace"), XRT_STR_INIT("b") }
	};
	static const xhttpfield DuplicateType[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("text/plain") },
		{ XRT_STR_INIT("content-type"), XRT_STR_INIT("text/plain") }
	};
	xhttpcorspreflightplan Plan;
	size_t i;

	memset(LongValue, 'a', sizeof(LongValue));
	for ( i = 0; i < 9u; i++ ) {
		Large[i].Name = i == 8u ?
			XRT_STR_LITERAL("Accept-Language") :
			XRT_STR_LITERAL("Accept");
		Large[i].Value = (xstrview){
			LongValue, sizeof(LongValue)
		};
	}

	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("GET"), Safe, 1u, false, &Plan
	) && (Plan.Flags == 0) && (Plan.HeaderCount == 0),
		"CORS safe request unexpectedly required preflight");
	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("PATCH"), Unsafe, 3u, false, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_REQUIRED) != 0) &&
		((Plan.Flags & XHTTP_CORS_PREFLIGHT_METHOD) != 0) &&
		((Plan.Flags & XHTTP_CORS_PREFLIGHT_HEADERS) != 0) &&
		(Plan.HeaderCount == 2u),
		"CORS unsafe request plan mismatch");
	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("GET"), Safe, 1u, true, &Plan
	) && ((Plan.Flags & XHTTP_CORS_PREFLIGHT_FORCED) != 0),
		"CORS forced preflight was not preserved");
	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("POST"), DuplicateType, 2u, false, &Plan
	) && (Plan.HeaderCount == 1u),
		"CORS repeated Content-Type did not become unsafe");
	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("GET"), Large, 9u, false, &Plan
	) && (Plan.HeaderCount == 2u) &&
		((Plan.Flags & XHTTP_CORS_PREFLIGHT_HEADERS) != 0),
		"CORS aggregate overflow did not expose all safe names");
	memset(&Plan, 0xA5, sizeof(Plan));
	testRequire(!xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("bad method"), NULL, 0, false, &Plan
	) && (Plan.Flags == 0) && (Plan.HeaderCount == 0),
		"CORS invalid method published a partial plan");
	xrtClearError();
}



/* 验证实际响应的 Origin、星号和凭据组合。 */
static void testCorsClientActual(void)
{
	static const xhttpfield Wildcard[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") }
	};
	static const xhttpfield Credentialed[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Credentials"), XRT_STR_INIT("true") }
	};
	static const xhttpfield Mismatch[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://other.example") }
	};
	xhttporigin Origin = testCorsClientOrigin();
	xhttpcorsclientresult Result;

	testRequire(xrtHttpCorsClientCheck(
		&Origin, false, Wildcard, 1u, &Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0),
		"CORS non-credentialed wildcard response was rejected");
	testRequire(xrtHttpCorsClientCheck(
		&Origin, true, Wildcard, 1u, &Result
	) && (Result.Reject == XHTTP_CORS_CLIENT_REJECT_ORIGIN),
		"CORS credentialed wildcard response was accepted");
	testRequire(xrtHttpCorsClientCheck(
		&Origin, true, Credentialed, 2u, &Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0),
		"CORS credentialed reflected response was rejected");
	testRequire(xrtHttpCorsClientCheck(
		&Origin, true, Credentialed, 1u, &Result
	) && (Result.Reject ==
		XHTTP_CORS_CLIENT_REJECT_CREDENTIALS),
		"CORS missing credential permission was accepted");
	testRequire(xrtHttpCorsClientCheck(
		&Origin, false, Mismatch, 1u, &Result
	) && (Result.Reject == XHTTP_CORS_CLIENT_REJECT_ORIGIN),
		"CORS mismatched Origin was accepted");
}



/* 验证预检方法、字段通配、Authorization 和缓存时间。 */
static void testCorsClientPreflight(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("X-Trace"), XRT_STR_INIT("abc") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type, x-trace") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("600") }
	};
	static const xhttpfield Wildcard[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("*") }
	};
	static const xhttpfield InvalidMaxAge[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Max-Age"), XRT_STR_INIT("forever") }
	};
	static const xhttpfield Authorization[] = {
		{ XRT_STR_INIT("Authorization"), XRT_STR_INIT("Bearer token") }
	};
	static const xhttpfield InvalidUnusedHeaders[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("bad item") }
	};
	xhttporigin Origin = testCorsClientOrigin();
	xhttpcorsclientresult Result;

	testRequire(xrtHttpCorsPreflightCheck(
		204u, &Origin, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, Response, 4u, &Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0) &&
		((Result.Flags & XHTTP_CORS_CLIENT_PREFLIGHT) != 0) &&
		(Result.MaxAge == 600u),
		"CORS explicit preflight response was rejected");
	testRequire(xrtHttpCorsPreflightCheck(
		200u, &Origin, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, Wildcard, 3u, &Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0) &&
		(Result.MaxAge == 5u),
		"CORS non-credentialed wildcard preflight was rejected");
	xrtClearError();
	testRequire(xrtHttpCorsPreflightCheck(
		200u, &Origin, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, InvalidMaxAge, 4u, &Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0) &&
		(Result.MaxAge == 5u) && (xrtGetError() == NULL),
		"CORS invalid Max-Age did not use the clean 5 second default");
	testRequire(xrtHttpCorsPreflightCheck(
		200u, &Origin, XRT_STR_LITERAL("GET"),
		Authorization, 1u, false, Wildcard, 3u, &Result
	) && (Result.Reject == XHTTP_CORS_CLIENT_REJECT_HEADER),
		"CORS wildcard authorized the non-wildcard Authorization field");
	testRequire(xrtHttpCorsPreflightCheck(
		403u, &Origin, XRT_STR_LITERAL("PATCH"),
		Request, 2u, false, Response, 4u, &Result
	) && (Result.Reject == XHTTP_CORS_CLIENT_REJECT_STATUS),
		"CORS non-2xx preflight status was accepted");
	memset(&Result, 0xA5, sizeof(Result));
	testRequire(!xrtHttpCorsPreflightCheck(
		204u, &Origin, XRT_STR_LITERAL("GET"),
		NULL, 0, false,
		InvalidUnusedHeaders, 2u, &Result
	) && (Result.Flags == 0) && (Result.Reject == 0) &&
		(Result.MaxAge == 0),
		"CORS unused malformed allow-header list was accepted");
	xrtClearError();
}



/* 验证规划和校验结果支持未对齐存储。 */
static void testCorsClientUnaligned(void)
{
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") }
	};
	xhttporigin Origin = testCorsClientOrigin();
	uint8 PlanStorage[sizeof(xhttpcorspreflightplan) + 1u];
	uint8 ResultStorage[sizeof(xhttpcorsclientresult) + 1u];
	xhttpcorspreflightplan Plan;
	xhttpcorsclientresult Result;

	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("GET"),
		NULL,
		0,
		false,
		(xhttpcorspreflightplan*)(PlanStorage + 1u)
	), "CORS unaligned preflight plan failed");
	memcpy(&Plan, PlanStorage + 1u, sizeof(Plan));
	testRequire((Plan.Flags == 0) && (Plan.HeaderCount == 0),
		"CORS unaligned preflight plan mismatch");
	testRequire(xrtHttpCorsClientCheck(
		&Origin,
		false,
		Response,
		1u,
		(xhttpcorsclientresult*)(ResultStorage + 1u)
	), "CORS unaligned client result failed");
	memcpy(&Result, ResultStorage + 1u, sizeof(Result));
	testRequire((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0,
		"CORS unaligned client result mismatch");
}



/* 执行 CORS 客户端协议测试。 */
int main(void)
{
	testCorsClientPlan();
	testCorsClientActual();
	testCorsClientPreflight();
	testCorsClientUnaligned();
	printf("[PASS] http_cors_client\n");
	return 0;
}
