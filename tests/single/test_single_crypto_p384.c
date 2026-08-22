#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 P-384 标量二能派生有效公钥。 */
int main(void)
{
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];

	Private[sizeof(Private) - 1u] = 2;
	return (!xrtP384Public(Private, Public) || !xrtP384Valid(Public)) ? 1 : 0;
}
