#include <stdio.h>

#include <xrt.h>



/*
 * 范例：crypto/ecdsa_p384 —— ECDSA P-384 签名（DER 输出）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEcdsaP384SignDer   与 P-256 完全对称，尺寸更大
 * 模块宏：XRT_MODULE_CRYPTO（ECDSA 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/crypto/ecdsa_p384/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   P-384 DER signature: 102 bytes
 *
 * P-384 = ~192 位安全强度：与 AES-256 / SHA-384 同档，
 *   政企合规常指定；签名相应更长（~102 字节）。
 */


/* 对一段消息生成可直接交给证书或协议层的 P-384 DER 签名。 */
int main(void)
{
	static const char Message[] = "xrt ecdsa";
	uint8 Hash[XRT_SHA384_SIZE];
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Der[XRT_ECDSA_P384_DER_MAX_SIZE];
	size_t iSize = 0;

	Private[sizeof(Private) - 1u] = 1;
	if ( !xrtSha384(Message, sizeof(Message) - 1u, Hash) ||
		!xrtEcdsaP384SignDer(
			XCRYPTO_HASH_SHA384,
			Hash, Private, Der, sizeof(Der), &iSize
		) ) {
		return 1;
	}
	printf("P-384 DER signature: %zu bytes\n", iSize);
	xrtSecureZero(Private, sizeof(Private));
	return 0;
}
