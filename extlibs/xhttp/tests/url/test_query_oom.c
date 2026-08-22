#include "../test_allocator.h"



/* 验证 Query 分配型构建在 OOM 下保持长度输出不变。 */
int main(void)
{
	static const xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_INIT("a"), XRT_STR_INIT("1")
	};
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(),
		"query failure allocator install failed");
	testRequire(xrtQueryRawBuild(&Pair, 1, &iSize) == NULL && (iSize == 77),
		"query allocated build OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"query allocated build OOM error mismatch");
	printf("[PASS] query_oom\n");
	return 0;
}
