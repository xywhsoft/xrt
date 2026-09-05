/*
 * 范例：math/random_text —— 可复现随机文本（测试夹具/占位数据）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRngString  显式 RNG 生成 n 字符随机串（默认 URL-safe 字母表）
 * 模块宏：XRT_MODULE_RANDOM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/random_text/main.c -lws2_32 -liphlpapi
 * 预期输出（种子固定 → 恒为此值）：
 *   -Vcy6FYwMRMh5XOsibprEBC3
 *
 * 与 SecureString 的分工：
 *   RngString 可复现——生成测试夹具、演示数据、模糊种子，
 *   相同种子在任何平台得到同一串（快照测试友好）；
 *   凭据类令牌必须用 SecureString（见 random_secure_text）。
 * 自定义字母表用 RngStringFrom（见 thread_random_text 范例）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xrng Rng;
	str sText;

	xrtRngSeed(&Rng, 2026, 7);        /* 与 random 范例同种子：可交叉对照 */

	/* 24 字符，默认 URL-safe 64 字符表（可含 - 和 _）。 */
	sText = xrtRngString(&Rng, 24);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
