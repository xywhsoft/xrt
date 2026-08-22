#include "../test_allocator.h"



/* 调用方缓冲路径必须零分配，字符串便捷路径必须准确报告 OOM。 */
int main(void)
{
	char arrOutput[33];

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtSecureText(XRT_STR_LITERAL("ab"), arrOutput,
		sizeof(arrOutput), sizeof(arrOutput) - 1u),
		"secure text base path allocated memory");
	testRequire(xrtSecureString(32) == NULL,
		"allocated secure string should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"secure string OOM error mismatch");
	xrtSecureZero(arrOutput, sizeof(arrOutput));
	xrtClearError();
	return 0;
}
