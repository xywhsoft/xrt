#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/ecdsa_p256 —— ECDSA P-256 签名（DER 输出）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEcdsaP256SignDer   哈希 → DER 编码签名
 *   XRT_ECDSA_P256_DER_MAX_SIZE   签名上限缓冲
 * 模块宏：XRT_MODULE_CRYPTO（ECDSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/ecdsa_p256/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   P-256 DER signature: 71 bytes
 *
 * DER 输出的意义：证书/TLS 层的线上格式就是 DER——
 *   直接可嵌入 X509 签名域（见 x509/signature）。
 *   签的是哈希（先 SHA-256），长度可变（68~72 字节，
 *   前导零压缩），所以带出参长度。
 */


/* 对一段消息生成可直接交给证书或协议层的 P-256 DER 签名。 */
int main(void)
{
	static const char Message[] = "xrt ecdsa";
	uint8 Hash[XRT_SHA256_SIZE];
	uint8 Private[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Der[XRT_ECDSA_P256_DER_MAX_SIZE];
	size_t iSize = 0;

	Private[sizeof(Private) - 1u] = 1;
	if ( !xrtSha256(Message, sizeof(Message) - 1u, Hash) ||
		!xrtEcdsaP256SignDer(
			XCRYPTO_HASH_SHA256,
			Hash, Private, Der, sizeof(Der), &iSize
		) ) {
		return 1;
	}
	printf("P-256 DER signature: %zu bytes\n", iSize);
	xrtSecureZero(Private, sizeof(Private));
	return 0;
}
