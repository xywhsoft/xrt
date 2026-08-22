#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 P-384 ECDSA raw 验签入口与错误契约。 */
int main(void)
{
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Hash[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Signature[XRT_ECDSA_P384_SIGNATURE_SIZE] = { 0 };

	Private[sizeof(Private) - 1u] = 1;
	return (!xrtP384Public(Private, Public) ||
		xrtEcdsaP384Verify(Hash, sizeof(Hash), Signature, Public) ||
		(xrtErrorCode(xrtGetError()) != XCRYPTO_ERROR_SIGNATURE)) ? 1 : 0;
}
