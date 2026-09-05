/*
 * 范例：math/thread_random —— 线程级便捷随机：免状态、免锁的日常路径
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRandSeed          重置当前线程 RNG（种子+流序号）
 *   xrtRand32 / RandReal 32 位整数 / [0,1) 浮点
 *   xrtRandRangeClosed   双闭区间整数
 * 模块宏：XRT_MODULE_RANDOM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/thread_random/main.c -lws2_32 -liphlpapi
 * 预期输出（与 random 范例同种子 → 序列完全一致）：
 *   value: 4207372542
 *   dice : 6
 *   real : 0.219369470187
 *
 * 线程级实现：状态存线程局部存储——多线程各自独立流，
 *   无锁竞争（这组 API 也正是 FastRand* 家族的底层：
 *   xrtFastRand32 = xrtRand32 的别名，见 random.h）。
 * 三条路径怎么选：
 *   显式 xrng（random 范例）——需要复现/多流/嵌入对象时；
 *   线程级 Rand*（本例）——日常一次性取数最省事；
 *   SecureRandom——凡凭据必用安全源。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xrtRandSeed(2026, 7);             /* 重置本线程状态 → 序列可复现 */
	printf("value: %u\n", (unsigned int)xrtRand32());
	printf("dice : %lld\n", (long long)xrtRandRangeClosed(1, 6));
	printf("real : %.12f\n", xrtRandReal());
	return 0;
}
