#include "../test_allocator.h"



/* CORS 客户端预检写出不得依赖堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") }
	};
	xhttporigin Origin;
	char Output[160];
	size_t iSize;

	testRequire(xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Origin
	), "CORS client writer noalloc Origin parse failed");
	testRequire(testInstallFailAllocator(),
		"CORS client writer failure allocator install failed");
	testRequire(xrtHttpCorsPreflightFieldsWrite(
		&Origin,
		XRT_STR_LITERAL("PATCH"),
		Fields,
		1u,
		Output,
		sizeof(Output),
		&iSize
	) && (iSize != 0),
		"CORS client preflight writer allocated memory");
	printf("[PASS] http_cors_client_write_noalloc\n");
	return 0;
}
