/*
 * 范例：time/clock —— 单调时钟：整数微秒计时与浮点秒便捷层
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtClock  单调时钟微秒（uint64；只增不减，适合测耗时）
 *   xrtTimer  浮点秒便捷层（兼容旧接口；新代码优先 xrtClock）
 *   xrtSleep  毫秒级睡眠（本例让计时器有东西可测）
 * 模块宏：XRT_MODULE_TIME
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/time/clock/main.c -lws2_32 -liphlpapi
 * 预期输出（实际值 ≥ 10000µs，随调度略有浮动）：
 *   elapsed_us=14695
 *   elapsed_s=0.014727
 *
 * 单调性（monotonic）的意义：系统 NTP 校时、用户改时钟都不影响它——
 *   用 wall-clock（如 time()）测耗时会出现 0 甚至负数，
 *   用单调钟做差永远正确。整数微秒还避免了 double 的精度丢失。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	uint64 iStart = xrtClock();
	double fStart = xrtTimer();

	/* 睡 10ms：两种计时器测同一段间隔。 */
	xrtSleep(10);

	/* 整数差值（新代码推荐）：直接相减即微秒数。 */
	printf("elapsed_us=%llu\n", (unsigned long long)(xrtClock() - iStart));

	/* 浮点秒（旧接口保留）：适合与秒为单位的 API 混用。 */
	printf("elapsed_s=%.6f\n", xrtTimer() - fStart);
	return 0;
}
