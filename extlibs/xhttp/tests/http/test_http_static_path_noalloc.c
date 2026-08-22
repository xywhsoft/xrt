#include "../test_allocator.h"



/* 底层静态路径查询、匹配与写出不得依赖堆分配。 */
int main(void)
{
	xhttpstaticpathconfig Config;
	char Output[64];
	size_t iSize;
	bool bTrailingSlash;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Mount = XRT_STR_LITERAL("/assets");
	testRequire(testInstallFailAllocator(),
		"HTTP static path failure allocator install failed");
	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/%61ssets/css/app.css"),
		&Config,
		NULL,
		0,
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_MATCH,
		"HTTP static path query allocated memory");
	testRequire((iSize == 11u) && !bTrailingSlash,
		"HTTP static path no-allocation query mismatch");
	testRequire(xrtHttpStaticPathWrite(
		XRT_STR_LITERAL("/%61ssets/css/app.css"),
		&Config,
		Output,
		sizeof(Output),
		&iSize,
		&bTrailingSlash
	) == XHTTP_STATIC_PATH_MATCH,
		"HTTP static path write allocated memory");
	testRequire(strcmp(Output, "css/app.css") == 0,
		"HTTP static path no-allocation output mismatch");
	printf("[PASS] http_static_path_noalloc\n");
	return 0;
}
