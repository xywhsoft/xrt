#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Ed25519 随机密钥对入口。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];

	return xrtEd25519KeyPair(Seed, Public) ? 0 : 1;
}
