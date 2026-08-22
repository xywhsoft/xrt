#include "../test_allocator.h"



/* 验证基础整数路径零分配，便捷路径正确报告 OOM。 */
int main(void)
{
	char sOutput[80];
	size_t iSize;
	uint64 iValue;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtUIntWrite(UINT64_MAX, 10, sOutput,
		sizeof(sOutput), &iSize, 0), "integer write allocated memory");
	testRequire(xrtUIntParse((xstrview){ sOutput, iSize }, 10, 0, &iValue) &&
		(iValue == UINT64_MAX), "integer parse allocated memory");
	testRequire(xrtUIntString(UINT64_MAX, 10, 0) == NULL,
		"allocated unsigned string should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"unsigned string OOM error mismatch");
	xrtClearError();
	testRequire(xrtIntString(INT64_MIN, 10, 0) == NULL,
		"allocated signed string should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"signed string OOM error mismatch");
	xrtClearError();
	printf("[PASS] number-integer-oom\n");
	return 0;
}
