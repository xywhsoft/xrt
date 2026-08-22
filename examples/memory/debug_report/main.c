#include <stdio.h>
#include <xrt.h>



/* 把报告片段直接写入标准输出。 */
static bool writeReport(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1, Data.Size, pFile) == Data.Size;
}



/* 展示无文件模块耦合的流式 JSON 报告。 */
int main(void)
{
	ptr pMemory = xrtMalloc(64);
	bool bResult;

	if ( pMemory == NULL ) {
		return 1;
	}
	bResult = xrtMemDebugReport(XMEMDEBUG_REPORT_JSON, writeReport, stdout);
	xrtFree(pMemory);
	if ( !xrtMemDebugReset() ) {
		return 1;
	}
	return bResult ? 0 : 1;
}
