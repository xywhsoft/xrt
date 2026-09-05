/*
 * 范例：math/random_secure —— 密码学安全随机：128 位标识 + 用后清零
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSecureRandom  系统熵源随机字节（Windows BCrypt / getrandom 等）
 *   xrtSecureZero    安全清零（编译器不会优化掉）
 * 模块宏：XRT_MODULE_RANDOM（依赖 CRYPTO 熵源路径）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/random_secure/main.c -lws2_32 -liphlpapi
 * 预期输出（每次运行都不同，长度恒 32 个十六进制字符）：
 *   c1e7c0ddc27a09318b08b284b487def9
 *
 * SecureRandom 与 Rng32 的本质区别：
 *   PCG32 是确定性算法（可复现即意味着可预测）；
 *   SecureRandom 走操作系统熵池——
 *   会话令牌、盐值、密钥材料的唯一正确来源。
 * 失败处理：熵源故障会返回 false（极罕见但必须处理），
 *   静默降级到不安全随机是安全事故的经典起因。
 * 用完 SecureZero：密钥材料在栈上停留越久被转储的风险越大。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	uint8 arrId[16];     /* 128 位：UUID 级别的碰撞空间 */

	/* 熵源调用必须检查失败（与 PCG32 的"永不失败"不同）。 */
	if ( !xrtSecureRandom(arrId, sizeof(arrId)) ) {
		return 1;
	}

	/* 逐字节转十六进制：128 位 → 32 字符。 */
	for ( size_t i = 0; i < sizeof(arrId); i++ ) {
		printf("%02x", (unsigned int)arrId[i]);
	}
	printf("\n");

	/* 密钥纪律：标识虽非密钥，养成用后即清的习惯。 */
	xrtSecureZero(arrId, sizeof(arrId));
	return 0;
}
