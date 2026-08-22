#include "../test_allocator.h"



/* 验证基础浮点路径零分配，分配便捷层正确报告 OOM。 */
int main(void)
{
	char sOutput[64];
	size_t iSize;
	double fValue;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtNumWrite(3.141592653589793, sOutput,
		sizeof(sOutput), &iSize, 0),
		"floating-point write allocated memory");
	testRequire(xrtNumParse(
		(xstrview){ sOutput, iSize }, 0, &fValue) &&
		(fValue == 3.141592653589793),
		"floating-point parse allocated memory");
	testRequire(xrtNumString(3.141592653589793, 0) == NULL,
		"allocated floating-point string should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"floating-point string OOM error mismatch");
	xrtClearError();
	printf("[PASS] number-float-oom\n");
	return 0;
}
