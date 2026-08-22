#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件随机密钥对和双方共享秘密路径。 */
int main(void)
{
	uint8 PrivateA[XRT_X25519_PRIVATE_SIZE];
	uint8 PublicA[XRT_X25519_PUBLIC_SIZE];
	uint8 PrivateB[XRT_X25519_PRIVATE_SIZE];
	uint8 PublicB[XRT_X25519_PUBLIC_SIZE];
	uint8 SharedA[XRT_X25519_SHARED_SIZE];
	uint8 SharedB[XRT_X25519_SHARED_SIZE];

	return (!xrtX25519KeyPair(PrivateA, PublicA) ||
		!xrtX25519KeyPair(PrivateB, PublicB) ||
		!xrtX25519Shared(PrivateA, PublicB, SharedA) ||
		!xrtX25519Shared(PrivateB, PublicA, SharedB) ||
		!xrtConstTimeEqual(SharedA, SharedB, sizeof(SharedA))) ? 1 : 0;
}
