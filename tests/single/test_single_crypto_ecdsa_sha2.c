#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须支持 P-384 与 SHA-256 的跨宽度 RFC 6979 路径。 */
int main(void)
{
	uint8 Hash[XRT_SHA256_SIZE] = { 0 };
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Signature[XRT_ECDSA_P384_SIGNATURE_SIZE];

	Private[sizeof(Private) - 1u] = 1;
	return xrtP384Public(Private, Public) && xrtEcdsaP384Sign(
		XCRYPTO_HASH_SHA256, Hash, Private, Signature
	) && xrtEcdsaP384Verify(
		Hash, sizeof(Hash), Signature, Public
	) ? 0 : 1;
}
