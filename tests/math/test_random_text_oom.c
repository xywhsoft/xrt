#include "../test_allocator.h"



/* 验证随机文本基础路径零分配，便捷路径报告 OOM。 */
int main(void)
{
	xrng Rng;
	xrng Before;
	char arrOutput[9];

	xrtRngSeed(&Rng, 1, 2);
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtRngText(&Rng, XRT_STR_LITERAL("ab"), arrOutput,
		sizeof(arrOutput), 8), "random text base path allocated memory");
	Before = Rng;
	testRequire(xrtRngString(&Rng, 8) == NULL,
		"allocated random string should fail");
	testRequire(memcmp(&Rng, &Before, sizeof(Rng)) == 0,
		"failed random string allocation advanced state");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"random string OOM error mismatch");
	xrtClearError();
	printf("[PASS] random-text-oom\n");
	return 0;
}
