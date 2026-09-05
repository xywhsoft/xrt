#include <stdio.h>
#include <xrt.h>



/*
 * 范例：crypto/core —— 密码学纪律：常量时间比较 + 安全清零
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtCryptoHashSize    算法元数据（不要求实现编入程序）
 *   xrtConstTimeEqual    常量时间字节比较
 *   xrtSecureZero        不会被优化掉的安全清零
 * 模块宏：XRT_MODULE_CRYPTO
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/core/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   equal: yes
 *
 * 全库密码学两条铁律的入口：比较密文/MAC 必须常量时间
 *   （memcmp 的早退会泄漏前缀匹配长度——时序攻击面）；
 *   密钥材料出作用域前必须 SecureZero（普通 memset 可能
 *   被编译器判死存储删除）。后续每个范例都遵守。
 */


/* 展示摘要元数据、常量时间比较和敏感数据清理。 */
int main(void)
{
	uint8 Left[XRT_SHA256_SIZE] = { 1u, 2u, 3u };
	uint8 Right[XRT_SHA256_SIZE] = { 1u, 2u, 3u };
	bool bEqual;

	/* 算法元数据不要求把对应摘要实现编入程序。 */
	if ( xrtCryptoHashSize(XCRYPTO_HASH_SHA256) != sizeof(Left) ) {
		return 1;
	}

	/* 密钥、标签和摘要等固定长度数据应使用常量时间比较。 */
	bEqual = xrtConstTimeEqual(Left, Right, sizeof(Left));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));

	printf("equal: %s\n", bEqual ? "yes" : "no");
	return bEqual ? 0 : 1;
}
