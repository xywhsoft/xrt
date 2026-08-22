#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件随机密钥对和双方共享秘密路径。 */
int main(void)
{
	uint8 PrivateA[XRT_X448_PRIVATE_SIZE];
	uint8 PublicA[XRT_X448_PUBLIC_SIZE];
	uint8 PrivateB[XRT_X448_PRIVATE_SIZE];
	uint8 PublicB[XRT_X448_PUBLIC_SIZE];
	uint8 SharedA[XRT_X448_SHARED_SIZE];
	uint8 SharedB[XRT_X448_SHARED_SIZE];

	return (!xrtX448KeyPair(PrivateA, PublicA) ||
		!xrtX448KeyPair(PrivateB, PublicB) ||
		!xrtX448Shared(PrivateA, PublicB, SharedA) ||
		!xrtX448Shared(PrivateB, PublicA, SharedB) ||
		!xrtConstTimeEqual(SharedA, SharedB, sizeof(SharedA))) ? 1 : 0;
}
