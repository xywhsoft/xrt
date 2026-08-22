#include "../test_allocator.h"



/* 分配型扩展值 Helper 在 OOM 时不得发布长度或部分结果。 */
int main(void)
{
	size_t iSize = 77u;

	testRequire(testInstallFailAllocator(),
		"HTTP ext-value OOM allocator install failed");
	testRequire((xrtHttpExtValueBuild(
		XRT_STR_LITERAL("UTF-8"), (xstrview){ NULL, 0 },
		(xbytesview){ (const uint8*)"a b", 3u }, &iSize
	) == NULL) && (iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP ext-value build OOM was not atomic");
	return 0;
}

