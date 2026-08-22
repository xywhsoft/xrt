#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 P-256 标量二能派生有效公钥。 */
int main(void)
{
	uint8 Private[XRT_P256_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P256_PUBLIC_SIZE];

	Private[sizeof(Private) - 1u] = 2;
	return (!xrtP256Public(Private, Public) || !xrtP256Valid(Public)) ? 1 : 0;
}
