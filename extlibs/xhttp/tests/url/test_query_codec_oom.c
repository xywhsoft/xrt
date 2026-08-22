#include "../test_allocator.h"



/* 验证分配型 Query 构建在 OOM 下保持长度输出不变。 */
int main(void)
{
	static const xquerypair Pair = {
		XQUERY_HAS_VALUE, XRT_STR_INIT("a b"), XRT_STR_INIT("1")
	};
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(),
		"query codec failure allocator install failed");
	testRequire((xrtQueryBuild(&Pair, 1, &iSize) == NULL) && (iSize == 77),
		"query codec allocated build OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"query codec allocated build OOM error mismatch");
	printf("[PASS] query_codec_oom\n");
	return 0;
}
