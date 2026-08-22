#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 X25519 的确定性原位公钥路径。 */
int main(void)
{
	uint8 Private[XRT_X25519_PRIVATE_SIZE] = { 1 };
	uint8 Public[XRT_X25519_PUBLIC_SIZE];
	uint8 InPlace[XRT_X25519_PRIVATE_SIZE];

	memcpy(InPlace, Private, sizeof(InPlace));
	return (!xrtX25519Public(Private, Public) ||
		!xrtX25519Public(InPlace, InPlace) ||
		!xrtConstTimeEqual(Public, InPlace, sizeof(Public))) ? 1 : 0;
}
