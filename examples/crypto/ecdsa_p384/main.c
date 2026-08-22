#include <stdio.h>

#include <xrt.h>



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
