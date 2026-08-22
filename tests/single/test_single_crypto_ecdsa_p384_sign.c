#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的确定性 P-384 ECDSA 签名入口。 */
int main(void)
{
	uint8 Hash[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Signature[XRT_ECDSA_P384_SIGNATURE_SIZE];

	Private[sizeof(Private) - 1u] = 1;
	return xrtEcdsaP384Sign(
		XCRYPTO_HASH_SHA384, Hash, Private, Signature
	) ? 0 : 1;
}
