#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 P-384 随机密钥对路径。 */
int main(void)
{
	uint8 Private[XRT_P384_PRIVATE_SIZE];
	uint8 Public[XRT_P384_PUBLIC_SIZE];

	return (!xrtP384KeyPair(Private, Public) || !xrtP384Valid(Public)) ? 1 : 0;
}
