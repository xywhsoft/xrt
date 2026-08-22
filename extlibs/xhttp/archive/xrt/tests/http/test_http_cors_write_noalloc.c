#include "../test_allocator.h"



/* CORS 响应字段写出不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") }
	};
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;
	char Output[160];
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"CORS writer failure allocator install failed");
	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN |
		XHTTP_CORS_POLICY_ANY_METHOD;
	testRequire(xrtHttpCorsPolicyCheck(
		&Policy,
		XRT_STR_LITERAL("OPTIONS"),
		Fields,
		2u,
		&Decision
	) && xrtHttpCorsDecisionWrite(
		&Decision, Fields, 2u, Output, sizeof(Output), &iSize
	) && (iSize != 0),
		"CORS writer allocated memory");
	printf("[PASS] http_cors_write_noalloc\n");
	return 0;
}
