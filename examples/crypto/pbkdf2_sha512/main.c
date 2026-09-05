#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/pbkdf2_sha512 —— PBKDF2-HMAC-SHA512（长输出变体）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPbkdf2Sha512   SHA-512 核心、64 字节输出的口令 KDF
 * 模块宏：XRT_MODULE_CRYPTO（PBKDF2 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/pbkdf2_sha512/main.c -lws2_32 -liphlpapi
 * 预期输出（64 字节十六进制）：
 *   50f14d237adc6e315a55ca47808cb32b...
 *
 * 与 SHA256 版取舍：单轮迭代更贵（GPU 攻击成本更高）、
 *   输出更长；FIPS 兼容场景的口令 KDF 顶配。
 */


/* 使用 PBKDF2-HMAC-SHA512 派生 64 字节密码密钥。 */
int main(void)
{
	uint8 arrKey[64];
	static const uint8 Salt[16] = {
		0x28u, 0x98u, 0xF0u, 0x1Cu, 0x3Du, 0x8Eu, 0xC6u, 0x57u,
		0x17u, 0xC9u, 0x80u, 0xE4u, 0xA6u, 0x3Bu, 0x72u, 0x11u
	};

	if ( !xrtPbkdf2Sha512(
			"correct horse battery staple", 28,
			Salt, sizeof(Salt), 100000, arrKey, sizeof(arrKey)
		) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	xrtSecureZero(arrKey, sizeof(arrKey));
	return 0;
}
