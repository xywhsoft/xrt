#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/sha512_256 —— SHA-512/256 一次性摘要
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSha512_256   一次性摘要（64 位核心、256 位截断）
 * 模块宏：XRT_MODULE_CRYPTO（SHA512 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sha512_256/main.c -lws2_32 -liphlpapi
 * 预期输出（"abc" 标准摘要）：
 *   53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23
 *
 * 与 SHA-256 同长度但走 SHA-512 核心：64 位平台上更快、
 *   对长度扩展攻击先天免疫——现代"短摘要"首选。
 */


/* 计算并输出一段文本的 SHA-512/256 摘要。 */
int main(void)
{
	static const char Hex[] = "0123456789abcdef";
	uint8 Digest[XRT_SHA512_256_SIZE];

	if ( !xrtSha512_256("abc", 3u, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		putchar(Hex[Digest[i] >> 4u]);
		putchar(Hex[Digest[i] & 0x0Fu]);
	}
	putchar('\n');
	return 0;
}
