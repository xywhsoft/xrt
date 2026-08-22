#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单文件测试只统计报告确实产生了输出。 */
static bool testWrite(xbytesview Data, ptr pUserData)
{
	size_t* pSize = (size_t*)pUserData;

	*pSize += Data.Size;
	return true;
}



/* 验证报告裁剪特性可以独立生成单头文件程序。 */
int main(void)
{
	size_t iSize = 0;
	ptr pMemory = xrtMalloc(16);
	bool bResult = xrtMemDebugReport(XMEMDEBUG_REPORT_JSON, testWrite, &iSize);

	xrtFree(pMemory);
	(void)xrtMemDebugReset();
	return (bResult && (iSize != 0)) ? 0 : 1;
}
