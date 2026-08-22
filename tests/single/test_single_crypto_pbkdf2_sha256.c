#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 PBKDF2-HMAC-SHA256 组合入口。 */
int main(void)
{
	uint8 arrOutput[32];

	return (!xrtPbkdf2Sha256(
			"password", 8, "salt", 4, 2, arrOutput, sizeof(arrOutput)
		) || (arrOutput[0] != 0xAEu) || (arrOutput[31] != 0x43u)) ? 1 : 0;
}
