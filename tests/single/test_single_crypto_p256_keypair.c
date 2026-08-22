#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 P-256 随机密钥对路径。 */
int main(void)
{
	uint8 Private[XRT_P256_PRIVATE_SIZE];
	uint8 Public[XRT_P256_PUBLIC_SIZE];

	return (!xrtP256KeyPair(Private, Public) || !xrtP256Valid(Public)) ? 1 : 0;
}
