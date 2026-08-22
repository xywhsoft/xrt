#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 HMAC-SHA384 与 HMAC-SHA512 入口。 */
int main(void)
{
	uint8 arrMac384[XRT_SHA384_SIZE];
	uint8 arrMac512[XRT_SHA512_SIZE];

	return (!xrtHmacSha384(
			"Jefe", 4, "what do ya want for nothing?", 28, arrMac384
		) || !xrtHmacSha512(
			"Jefe", 4, "what do ya want for nothing?", 28, arrMac512
		) || (arrMac384[0] != 0xAFu) || (arrMac384[47] != 0x49u) ||
		(arrMac512[0] != 0x16u) || (arrMac512[63] != 0x37u)) ? 1 : 0;
}
