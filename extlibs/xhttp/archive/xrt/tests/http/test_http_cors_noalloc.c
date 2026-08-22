#include "../test_allocator.h"



/* CORS 字段解析、汇总和列表迭代必须完全不依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") },
		{ XRT_STR_INIT("Access-Control-Request-Method"), XRT_STR_INIT("PUT") },
		{ XRT_STR_INIT("Access-Control-Request-Headers"), XRT_STR_INIT("X-One, X-Two") }
	};
	xhttpcorsrequest Request;
	xhttpcorscursor Cursor;
	xstrview Name;

	testRequire(testInstallFailAllocator(),
		"CORS failure allocator install failed");
	testRequire(xrtHttpCorsRequestRead(
		XRT_STR_LITERAL("OPTIONS"), Fields, 3u, &Request
	) && (Request.HeaderCount == 2u),
		"CORS request read allocated memory");
	xrtHttpCorsCursorInit(&Cursor);
	testRequire(xrtHttpCorsRequestHeaderNext(
		Fields, 3u, &Cursor, &Name
	) == XHTTP_NEXT_ITEM,
		"CORS request header iteration allocated memory");
	printf("[PASS] http_cors_noalloc\n");
	return 0;
}
