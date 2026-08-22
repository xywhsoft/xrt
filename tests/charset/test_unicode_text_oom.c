#include "../test_allocator.h"



/* 验证 Unicode 文本便捷操作完整传播内存不足。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtUtf8Reverse(XRT_STR_LITERAL("text")) == NULL,
		"Unicode reverse should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode reverse OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Filter(XRT_STR_LITERAL("text"),
		XRT_STR_LITERAL("x")) == NULL, "Unicode filter should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode filter OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Substr(XRT_STR_LITERAL("text"), 1, 2) == NULL,
		"Unicode substring should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode substring OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Insert(XRT_STR_LITERAL("text"), 1,
		XRT_STR_LITERAL("x")) == NULL, "Unicode insert should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode insert OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8Remove(XRT_STR_LITERAL("text"), 1, 1) == NULL,
		"Unicode remove should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode remove OOM error mismatch");
	xrtClearError();
	testRequire(xrtUtf8PadLeft(XRT_STR_LITERAL("text"), 8,
		XRT_STR_LITERAL("x")) == NULL, "Unicode pad should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"Unicode pad OOM error mismatch");
	xrtClearError();
	printf("[PASS] unicode-text-oom\n");
	return 0;
}
