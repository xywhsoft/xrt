#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 PBKDF2-HMAC-SHA384/512 组合入口。 */
int main(void)
{
	uint8 arrSha384[48];
	uint8 arrSha512[64];

	return (!xrtPbkdf2Sha384(
			"password", 8, "salt", 4, 1, arrSha384, sizeof(arrSha384)
		) || !xrtPbkdf2Sha512(
			"password", 8, "salt", 4, 1, arrSha512, sizeof(arrSha512)
		) || (arrSha384[0] != 0xC0u) || (arrSha384[47] != 0xE4u) ||
		(arrSha512[0] != 0x86u) || (arrSha512[63] != 0xCEu)) ? 1 : 0;
}
