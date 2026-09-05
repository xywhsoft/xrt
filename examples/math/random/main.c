/*
 * 范例：math/random —— 显式 RNG：可复现、无全局状态的随机全家族
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrng / xrtRngSeed        PCG32 状态与播种（种子+流序号）
 *   xrtRng32 / RngReal       32 位整数 / [0,1) 浮点
 *   xrtRngRangeClosed        双闭区间均匀整数（1..6 掷骰）
 *   xrtRngBytes              随机字节填充缓冲
 *   xrtRngShuffle            Fisher-Yates 原地洗牌
 * 模块宏：XRT_MODULE_RANDOM（依赖 MATH）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/random/main.c -lws2_32 -liphlpapi
 * 预期输出（种子固定 → 完全可复现）：
 *   explicit: 4207372542
 *   dice    : 6
 *   real    : 0.219369470187
 *   bytes   : 8641
 *   shuffle : 2 1 6
 *
 * 显式状态的价值：状态是局部变量——
 *   可放进结构体/每会话一个/回放系统存档；同种子必得同序列
 *   （测试快照、游戏录像回放的基础）。
 * 流序号（第二个播种参数）：同种子不同流 = 独立序列，
 *   避免"多个 RNG 播同一种子产生相关输出"的经典坑。
 * 非密码学安全——密钥场景用 random_secure 范例的 SecureRandom。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xrng Rng;                        /* 状态在栈上：零全局依赖 */
	int arrOrder[] = { 1, 2, 3, 4, 5, 6 };
	uint8 arrBytes[8];

	xrtRngSeed(&Rng, 2026, 7);       /* 种子 2026，流 7 */

	/* 整数与浮点：同一状态顺序推进（消耗即前进）。 */
	printf("explicit: %u\n", (unsigned int)xrtRng32(&Rng));
	printf("dice    : %lld\n", (long long)xrtRngRangeClosed(&Rng, 1, 6));
	printf("real    : %.12f\n", xrtRngReal(&Rng));

	/* 字节填充与洗牌：RngBytes 无失败路径（内部检查冗余但保留统一风格）。 */
	if ( !xrtRngBytes(&Rng, arrBytes, sizeof(arrBytes)) ||
		 !xrtRngShuffle(&Rng, arrOrder,
			sizeof(arrOrder) / sizeof(arrOrder[0]), sizeof(arrOrder[0])) ) {
		return 1;
	}
	printf("bytes   : %02x%02x\n",
		(unsigned int)arrBytes[0], (unsigned int)arrBytes[1]);
	printf("shuffle : %d %d %d\n", arrOrder[0], arrOrder[1], arrOrder[2]);
	return 0;
}
