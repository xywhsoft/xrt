#include "../test_allocator.h"



/* 验证编辑距离分配失败和无需分配的阈值快路径。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("abcd"), XRT_STR_LITERAL("abce"),
		XRT_NPOS) == XRT_NPOS, "distance OOM must fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"distance OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Distance(XRT_STR_LITERAL("abcdefgh"), XRT_STR_LITERAL("a"),
		2) == XRT_NPOS, "distance limit fast path mismatch");
	testRequire(xrtGetError() == NULL, "distance limit fast path set an error");
	printf("[PASS] unicode-distance-oom\n");
	return 0;
}
