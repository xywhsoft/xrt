#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 P-384 ECDSA DER 便利入口。 */
int main(void)
{
	static const uint8 Der[] = {
		0x30, 0x06, 0x02, 0x01, 0, 0x02, 0x01, 0
	};
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Hash[XRT_P384_PRIVATE_SIZE] = { 0 };

	Private[sizeof(Private) - 1u] = 1;
	return (!xrtP384Public(Private, Public) ||
		xrtEcdsaP384VerifyDer(
			Hash, sizeof(Hash), Der, sizeof(Der), Public
		)) ? 1 : 0;
}
