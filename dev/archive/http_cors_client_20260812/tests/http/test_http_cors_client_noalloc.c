#include "../test_allocator.h"



/* CORS 客户端规划和响应校验不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Request[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	static const xhttpfield Response[] = {
		{ XRT_STR_INIT("Access-Control-Allow-Origin"), XRT_STR_INIT("*") },
		{ XRT_STR_INIT("Access-Control-Allow-Methods"), XRT_STR_INIT("PATCH") },
		{ XRT_STR_INIT("Access-Control-Allow-Headers"), XRT_STR_INIT("content-type") }
	};
	xhttporigin Origin;
	xhttpcorspreflightplan Plan;
	xhttpcorsclientresult Result;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origin
	), "CORS client noalloc Origin parse failed");
	testRequire(testInstallFailAllocator(),
		"CORS client failure allocator install failed");
	testRequire(xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("PATCH"), Request, 1u, false, &Plan
	) && xrtHttpCorsPreflightCheck(
		204u,
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Request,
		1u,
		false,
		Response,
		3u,
		&Result
	) && ((Result.Flags & XHTTP_CORS_CLIENT_ALLOW) != 0),
		"CORS client protocol allocated memory");
	printf("[PASS] http_cors_client_noalloc\n");
	return 0;
}
