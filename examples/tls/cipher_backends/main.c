#include <stdio.h>

#include <xrt/tls.h>



/*
 * 范例：tls/cipher_backends —— 套件元数据：可选后端自描述
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtTlsCipherInfo   套件 → {哈希, AEAD 类型, 密钥长度}
 *   xrtTlsCipherName   套件枚举 → 标准名
 * 模块宏：XRT_MODULE_TLS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/cipher_backends/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   TLS_AES_256_GCM_SHA384 hash=2 aead=1 key=32
 *   TLS_CHACHA20_POLY1305_SHA256 hash=1 aead=2 key=32
 *
 * Info 为 NULL 的含义：该套件在当前构建不可用
 *   （SHA-384 / ChaCha20 是可选后端，按模块宏裁剪）——
 *   运行期探测比编译期 #ifdef 更适合做能力降级列表。
 *   aead=1 是 AES-GCM、=2 是 ChaCha20-Poly1305；
 *   key=32 说明两者都是 256 位密钥。
 */


/* 展示 SHA-384 与 ChaCha20-Poly1305 可选后端对应的公开密码套件元数据。 */
int main(void)
{
	static const xtlscipher Ciphers[] = {
		XTLS_AES_256_GCM_SHA384,
		XTLS_CHACHA20_POLY1305_SHA256
	};
	size_t i;

	for ( i = 0; i < (sizeof(Ciphers) / sizeof(Ciphers[0])); ++i ) {
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(
			Ciphers[i]
		);

		if ( pInfo == NULL ) {
			return 1;
		}
		printf(
			"%s hash=%d aead=%d key=%u\n",
			xrtTlsCipherName(Ciphers[i]),
			(int)pInfo->Hash,
			(int)pInfo->Aead,
			(unsigned)pInfo->KeySize
		);
	}
	return 0;
}
