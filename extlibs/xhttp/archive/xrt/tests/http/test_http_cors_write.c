#include "../test.h"



/* 验证预检决策可直接写成完整字段片段。 */
static void testCorsPreflightWrite(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://APP.example:443") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("content-type, X-Token") }
	};
	static const xstrview Methods[] = { XRT_STR_INIT("PUT") };
	static const xstrview Headers[] = {
		XRT_STR_INIT("Content-Type"), XRT_STR_INIT("X-Token")
	};
	static const char Expected[] =
		"Access-Control-Allow-Origin: https://app.example\r\n"
		"Access-Control-Allow-Credentials: true\r\n"
		"Access-Control-Allow-Methods: PUT\r\n"
		"Access-Control-Allow-Headers: content-type, X-Token\r\n"
		"Access-Control-Max-Age: 600\r\n"
		"Vary: Origin, Access-Control-Request-Method, "
		"Access-Control-Request-Headers\r\n";
	xhttporigin Origins[1];
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;
	char Output[384];
	char Before[384];
	size_t iRequired;
	size_t iSize;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origins[0]
	), "CORS writer policy origin parse failed");
	Policy.Origins = Origins;
	Policy.OriginCount = 1u;
	Policy.Methods = Methods;
	Policy.MethodCount = 1u;
	Policy.Headers = Headers;
	Policy.HeaderCount = 2u;
	Policy.MaxAge = 600u;
	Policy.Flags = XHTTP_CORS_POLICY_CREDENTIALS |
		XHTTP_CORS_POLICY_MAX_AGE;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		3u,
		&Decision
	), "CORS writer policy check failed");
	testRequire(xrtHttpCorsDecisionWrite(
		&Decision, Fields, 3u, NULL, 0, &iRequired
	) && (iRequired == (sizeof(Expected) - 1u)),
		"CORS writer size query mismatch");
	testRequire(xrtHttpCorsDecisionWrite(
		&Decision,
		Fields,
		3u,
		Output,
		sizeof(Output),
		&iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Expected, iSize) == 0),
		"CORS preflight field fragment mismatch");

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpCorsDecisionWrite(
		&Decision,
		Fields,
		3u,
		Output,
		iRequired - 1u,
		&iSize
	) && (iSize == iRequired) &&
		(memcmp(Output, Before, sizeof(Output)) == 0),
		"CORS short writer changed output");
	xrtClearError();
}



/* 验证通配、暴露字段和拒绝决策写出。 */
static void testCorsSimpleWrite(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") }
	};
	static const xstrview Expose[] = {
		XRT_STR_INIT("X-RateLimit"), XRT_STR_INIT("X-Trace")
	};
	static const char Expected[] =
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Expose-Headers: X-RateLimit, X-Trace\r\n";
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;
	char Output[160];
	size_t iSize;

	Policy.ExposeHeaders = Expose;
	Policy.ExposeCount = 2u;
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Fields, 1u, &Decision
	) && xrtHttpCorsDecisionWrite(
		&Decision, Fields, 1u, Output, sizeof(Output), &iSize
	) && (iSize == (sizeof(Expected) - 1u)) &&
		(memcmp(Output, Expected, iSize) == 0),
		"CORS simple field fragment mismatch");

	memset(&Policy, 0, sizeof(Policy));
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Fields, 1u, &Decision
	) && (Decision.Reject == XHTTP_CORS_REJECT_ORIGIN) &&
		xrtHttpCorsDecisionWrite(
			&Decision, Fields, 1u, Output, sizeof(Output), &iSize
		) && (iSize == 0),
		"CORS rejected decision emitted fields");
}



/* 执行 CORS 原始字段写出测试。 */
int main(void)
{
	testCorsPreflightWrite();
	testCorsSimpleWrite();
	printf("[PASS] http_cors_write\n");
	return 0;
}
