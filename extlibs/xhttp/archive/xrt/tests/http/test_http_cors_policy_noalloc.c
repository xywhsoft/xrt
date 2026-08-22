#include "../test_allocator.h"



/* CORS 策略判断与原始字段写出不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") }
	};
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	testRequire(testInstallFailAllocator(),
		"CORS policy failure allocator install failed");
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN |
		XHTTP_CORS_POLICY_ANY_METHOD;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		2u,
		&Decision
	) && ((Decision.Flags & XHTTP_CORS_DECISION_PREFLIGHT) != 0) &&
		((Decision.Flags & XHTTP_CORS_DECISION_ALLOW) != 0),
		"CORS policy allocated memory");
	printf("[PASS] http_cors_policy_noalloc\n");
	return 0;
}
