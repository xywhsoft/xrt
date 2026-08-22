#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Ed25519 签名入口。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];

	return xrtEd25519Sign(Seed, NULL, 0, Signature) ? 0 : 1;
}
