/*
 * 范例：memory/stats —— 进程级内存统计：池化率与后端调用量
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMemStatsEnable  开关统计采集（常驻服务按需开启窗口）
 *   xrtMemStatsReset   清零计数器（测量窗口起点）
 *   xrtMemStatsGet     一次取回统计结构
 *   xmemstats          计数器集合（调用数/字节数/峰值等）
 * 模块宏：XRT_MODULE_MEMORY_STATS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/memory/stats/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   malloc_calls=2 pooled=1 direct=1 backing=3
 *
 * 读数解释：
 *   malloc_calls=2  业务侧 xrtMalloc 共 2 次（48B + 4096B）；
 *   pooled=1        48B 命中小块尺寸类缓存（零系统调用）；
 *   direct=1        4096B 超过池阈值直通后端；
 *   backing=3       实际打到系统分配器的次数——
 *                   直通 1 次 + 池内部为尺寸类预热的分配。
 * 统计 vs 调试堆：stats 只计数（几乎零开销，生产可常开）；
 *   memory_debug 记录每笔明细（定位泄漏用，开销大）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xmemstats tStats;
	ptr pSmall;
	ptr pLarge;

	/* 打开采集并清零：接下来的计数全部属于本范例工作负载。 */
	xrtMemStatsEnable(true);
	xrtMemStatsReset();

	/* 一次小块（走池）+ 一次大块（直通）。 */
	pSmall = xrtMalloc(48);
	pLarge = xrtMalloc(4096);
	if ( (pSmall == NULL) || (pLarge == NULL) ) {
		xrtFree(pSmall);
		xrtFree(pLarge);
		return 1;
	}
	xrtFree(pSmall);
	xrtFree(pLarge);

	/* 读取窗口内计数（字段语义见文件头读数解释）。 */
	xrtMemStatsGet(&tStats);
	printf("malloc_calls=%llu pooled=%llu direct=%llu backing=%llu\n",
		(unsigned long long)tStats.MallocCalls,
		(unsigned long long)tStats.PooledAllocCalls,
		(unsigned long long)tStats.DirectAllocCalls,
		(unsigned long long)tStats.BackingAllocCalls);

	/* 用完关闭，回到零开销模式。 */
	xrtMemStatsEnable(false);
	return 0;
}
