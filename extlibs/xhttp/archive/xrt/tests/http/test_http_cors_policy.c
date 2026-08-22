#include "../test.h"



/* 建立测试策略中的显式 Origin。 */
static void parseOrigin(xstrview Text, xhttporigin* pOrigin)
{
	testRequire(xrtHttpOriginParse(Text, pOrigin),
		"CORS policy test origin parse failed");
}



/* 验证显式策略接受完整预检并产生最小响应决策。 */
static void testCorsPolicyAllow(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://APP.example:443") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("content-type, X-Token") }
	};
	static const xstrview Methods[] = {
		XRT_STR_INIT("GET"), XRT_STR_INIT("PUT")
	};
	static const xstrview Headers[] = {
		XRT_STR_INIT("Content-Type"), XRT_STR_INIT("x-token")
	};
	static const xstrview Expose[] = {
		XRT_STR_INIT("X-RateLimit")
	};
	xhttporigin Origins[1];
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	parseOrigin(XRT_STR_LITERAL("https://app.example"), &Origins[0]);
	Policy.Origins = Origins;
	Policy.OriginCount = 1u;
	Policy.Methods = Methods;
	Policy.MethodCount = 2u;
	Policy.Headers = Headers;
	Policy.HeaderCount = 2u;
	Policy.ExposeHeaders = Expose;
	Policy.ExposeCount = 1u;
	Policy.MaxAge = 600u;
	Policy.Flags = XHTTP_CORS_POLICY_CREDENTIALS |
		XHTTP_CORS_POLICY_MAX_AGE;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		3u,
		&Decision
	) && (Decision.Reject == XHTTP_CORS_REJECT_NONE) &&
		((Decision.Flags & XHTTP_CORS_DECISION_ALLOW) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_PREFLIGHT) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_CREDENTIALS) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_ALLOW_HEADERS) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_MAX_AGE) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_VARY_ORIGIN) != 0) &&
		(Decision.HeaderCount == 2u) &&
		(Decision.MaxAge == 600u) &&
		xrtHttpMethodEqual(
			Decision.AllowMethod, XRT_STR_LITERAL("PUT")
		), "CORS explicit policy decision mismatch");
}



/* 验证策略拒绝按 Origin、方法和字段名精确分类。 */
static void testCorsPolicyReject(void)
{
	static const xhttpfield BadOrigin[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://other.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") }
	};
	static const xhttpfield BadMethod[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("DELETE") }
	};
	static const xhttpfield BadHeader[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("Authorization") }
	};
	static const xstrview Methods[] = { XRT_STR_INIT("PUT") };
	static const xstrview Headers[] = { XRT_STR_INIT("Content-Type") };
	xhttporigin Origins[1];
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	parseOrigin(XRT_STR_LITERAL("https://app.example"), &Origins[0]);
	Policy.Origins = Origins;
	Policy.OriginCount = 1u;
	Policy.Methods = Methods;
	Policy.MethodCount = 1u;
	Policy.Headers = Headers;
	Policy.HeaderCount = 1u;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		BadOrigin,
		2u,
		&Decision
	) && (Decision.Reject == XHTTP_CORS_REJECT_ORIGIN) &&
		(Decision.Flags == 0),
		"CORS origin rejection mismatch");
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		BadMethod,
		2u,
		&Decision
	) && (Decision.Reject == XHTTP_CORS_REJECT_METHOD),
		"CORS method rejection mismatch");
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		BadHeader,
		3u,
		&Decision
	) && (Decision.Reject == XHTTP_CORS_REJECT_HEADER),
		"CORS header rejection mismatch");
}



/* 验证通配、凭据、null Origin 和非 CORS 请求边界。 */
static void testCorsPolicyModes(void)
{
	static const xhttpfield Simple[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") }
	};
	static const xhttpfield Null[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("null") }
	};
	static const xstrview Expose[] = {
		XRT_STR_INIT("X-RateLimit"), XRT_STR_INIT("X-Trace")
	};
	xhttporigin Origins[1];
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	Policy.ExposeHeaders = Expose;
	Policy.ExposeCount = 2u;
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Simple, 1u, &Decision
	) && ((Decision.AllowOrigin.Flags &
		XHTTP_CORS_ORIGIN_WILDCARD) != 0) &&
		((Decision.Flags &
		XHTTP_CORS_DECISION_VARY_ORIGIN) == 0) &&
		((Decision.Flags &
		XHTTP_CORS_DECISION_EXPOSE_HEADERS) != 0) &&
		(Decision.ExposeCount == 2u),
		"CORS wildcard policy decision mismatch");
	Policy.Flags |= XHTTP_CORS_POLICY_CREDENTIALS;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Simple, 1u, &Decision
	) && (Decision.AllowOrigin.Flags == 0) &&
		((Decision.Flags &
		XHTTP_CORS_DECISION_VARY_ORIGIN) != 0),
		"CORS credentialed wildcard did not reflect Origin");

	parseOrigin(XRT_STR_LITERAL("null"), &Origins[0]);
	memset(&Policy, 0, sizeof(Policy));
	Policy.Origins = Origins;
	Policy.OriginCount = 1u;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Null, 1u, &Decision
	) && ((Decision.Flags & XHTTP_CORS_DECISION_ALLOW) != 0),
		"CORS explicit null Origin was rejected");
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), NULL, 0, &Decision
	) && (Decision.Flags == 0) &&
		(Decision.Reject == XHTTP_CORS_REJECT_NONE),
		"CORS non-CORS request did not produce an empty decision");
}



/* 验证策略通配只能通过显式标志表达，避免合法但永远不命中的配置。 */
static void testCorsPolicyWildcardConfig(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") }
	};
	static const xstrview Wildcard[] = { XRT_STR_INIT("*") };
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	Policy.Methods = Wildcard;
	Policy.MethodCount = 1u;
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	testRequire(!xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		2u,
		&Decision
	) && (Decision.Flags == 0),
		"CORS policy accepted wildcard method array");
	xrtClearError();

	memset(&Policy, 0, sizeof(Policy));
	Policy.Headers = Wildcard;
	Policy.HeaderCount = 1u;
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	testRequire(!xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		2u,
		&Decision
	) && (Decision.Flags == 0),
		"CORS policy accepted wildcard header array");
	xrtClearError();
}



/* 执行 CORS 数组策略测试。 */
int main(void)
{
	testCorsPolicyAllow();
	testCorsPolicyReject();
	testCorsPolicyModes();
	testCorsPolicyWildcardConfig();
	printf("[PASS] http_cors_policy\n");
	return 0;
}
