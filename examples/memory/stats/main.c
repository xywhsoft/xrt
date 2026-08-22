#include <stdio.h>

#include <xrt.h>



/* 演示在一段工作负载周围采集内存统计。 */
int main(void)
{
	xmemstats tStats;
	ptr pSmall;
	ptr pLarge;

	xrtMemStatsEnable(true);
	xrtMemStatsReset();
	pSmall = xrtMalloc(48);
	pLarge = xrtMalloc(4096);
	if ( (pSmall == NULL) || (pLarge == NULL) ) {
		xrtFree(pSmall);
		xrtFree(pLarge);
		return 1;
	}
	xrtFree(pSmall);
	xrtFree(pLarge);

	xrtMemStatsGet(&tStats);
	printf("malloc_calls=%llu pooled=%llu direct=%llu backing=%llu\n",
		(unsigned long long)tStats.MallocCalls,
		(unsigned long long)tStats.PooledAllocCalls,
		(unsigned long long)tStats.DirectAllocCalls,
		(unsigned long long)tStats.BackingAllocCalls);
	xrtMemStatsEnable(false);
	return 0;
}
