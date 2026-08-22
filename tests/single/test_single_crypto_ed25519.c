#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的 Ed25519 公钥派生入口。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];

	return xrtEd25519Public(Seed, Public) ? 0 : 1;
}
