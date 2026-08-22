#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的确定性 P-256 ECDSA 签名入口。 */
int main(void)
{
	uint8 Hash[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Private[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Signature[XRT_ECDSA_P256_SIGNATURE_SIZE];

	Private[sizeof(Private) - 1u] = 1;
	return xrtEcdsaP256Sign(
		XCRYPTO_HASH_SHA256, Hash, Private, Signature
	) ? 0 : 1;
}
