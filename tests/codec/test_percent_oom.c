#include "../test_allocator.h"



/* 验证分配型 percent 便捷函数在 OOM 下保持结果参数不变。 */
int main(void)
{
	size_t iSize = 77;

	testRequire(testInstallFailAllocator(),
		"percent failure allocator install failed");
	testRequire(xrtPercentEncodeNew(
		"a b", 3, XRT_STR_LITERAL(""), &iSize
	) == NULL && (iSize == 77),
		"percent allocated encode OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"percent allocated encode OOM error mismatch");
	xrtClearError();
	testRequire(xrtPercentDecodeNew(
		XRT_STR_LITERAL("a%20b"), &iSize
	) == NULL && (iSize == 77),
		"percent allocated decode OOM was not atomic");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"percent allocated decode OOM error mismatch");
	printf("[PASS] codec_percent_oom\n");
	return 0;
}
