#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/sha224 —— SHA-224 一次性摘要
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSha224   一次性摘要（28 字节输出）
 * 模块宏：XRT_MODULE_CRYPTO（SHA224 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/sha224/main.c -lws2_32 -liphlpapi
 * 预期输出（"xrt" 标准摘要）：
 *   8290a0e5b2aadf6b5c3d4246e2796916c0d1234bae24e434324e6585
 *
 * SHA-256 的截断变体：空间敏感（指纹/ETag）且不需要
 *   完整 256 位抗碰撞性时选用；与 SHA-512/256 同属
 *   "短摘要"档位。
 */


/* 演示一次计算 SHA-224 摘要。 */
int main(void)
{
	uint8 Digest[XRT_SHA224_SIZE];

	if ( !xrtSha224("xrt", 3, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		printf("%02x", (unsigned)Digest[i]);
	}
	putchar('\n');
	return 0;
}
