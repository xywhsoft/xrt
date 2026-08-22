#include "../test_allocator.h"



/* 分配构建失败必须返回空指针并保留内存不足错误。 */
int main(void)
{
	static const xcookiepair Pair = {
		XRT_STR_INIT("sid"), XRT_STR_INIT("abc123")
	};

	testRequire(testInstallFailAllocator(),
		"cookie OOM allocator install failed");
	testRequire((xrtCookieBuild(&Pair, 1, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"cookie build did not publish OOM");
	printf("[PASS] cookie_oom\n");
	return 0;
}
