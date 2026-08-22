#include <stdio.h>

#include <xrt.h>



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
