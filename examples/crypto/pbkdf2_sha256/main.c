#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/pbkdf2_sha256 —— PBKDF2：从口令派生密钥
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPbkdf2Sha256   (口令, salt, 迭代数) → 密钥
 * 模块宏：XRT_MODULE_CRYPTO（PBKDF2 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/pbkdf2_sha256/main.c -lws2_32 -liphlpapi
 * 预期输出（固定 salt + 10 万次迭代）：
 *   ecfde924b9512da31933191fd1d31754e252fce15eae769808c3cdda9702cbd5
 *
 * PBKDF2 vs HKDF：口令熵低，靠工作因子（本例 100000 次
 *   迭代）拉高攻击成本；salt 必须每用户随机并随密文存储。
 *   PBKDF2 是兼容面最广的口令 KDF。
 */


/* 使用显式 salt 和工作因子派生 32 字节密码密钥。 */
int main(void)
{
	uint8 arrKey[32];
	static const uint8 Salt[16] = {
		0x8Du, 0xCBu, 0x89u, 0x2Eu, 0xD0u, 0x31u, 0x42u, 0x49u,
		0xA1u, 0x7Eu, 0x1Fu, 0x5Bu, 0x12u, 0x6Du, 0xE9u, 0x75u
	};

	if ( !xrtPbkdf2Sha256(
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
