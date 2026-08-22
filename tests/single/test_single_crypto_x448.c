#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 X448 的确定性原位公钥路径。 */
int main(void)
{
	uint8 Private[XRT_X448_PRIVATE_SIZE] = { 1 };
	uint8 Public[XRT_X448_PUBLIC_SIZE];
	uint8 InPlace[XRT_X448_PRIVATE_SIZE];

	memcpy(InPlace, Private, sizeof(InPlace));
	return (!xrtX448Public(Private, Public) ||
		!xrtX448Public(InPlace, InPlace) ||
		!xrtConstTimeEqual(Public, InPlace, sizeof(Public))) ? 1 : 0;
}
