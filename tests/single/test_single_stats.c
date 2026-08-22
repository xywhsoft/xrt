#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件包含可裁剪内存统计实现。 */
int main(void)
{
	xmemstats tStats;
	ptr pMemory;

	xrtMemStatsEnable(true);
	xrtMemStatsReset();
	pMemory = xrtMalloc(32);
	if ( pMemory == NULL ) {
		return 1;
	}
	xrtFree(pMemory);
	xrtMemStatsGet(&tStats);
	if ( (tStats.MallocCalls != 1) || (tStats.FreeCalls != 1) ) {
		return 2;
	}
	return 0;
}
