#include "../test_allocator.h"



/* CORS safelist 与响应字段暴露判断必须完全不依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Access-Control-Expose-Headers"), XRT_STR_INIT("X-Trace") }
	};

	testRequire(testInstallFailAllocator(),
		"CORS safelist failure allocator install failed");
	testRequire(xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("text/plain;charset=UTF-8")
	), "CORS request safelist allocated memory");
	xrtClearError();
	testRequire(!xrtHttpCorsRequestHeaderSafelisted(
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("not-a-mime-type")
	) && (xrtGetError() == NULL),
		"CORS MIME essence classification allocated or set an error");
	testRequire(xrtHttpCorsResponseHeaderExposed(
		Fields, 1u, XRT_STR_LITERAL("X-Trace"), false
	) == XHTTP_NEXT_ITEM,
		"CORS response exposure check allocated memory");
	printf("[PASS] http_cors_safelist_noalloc\n");
	return 0;
}
