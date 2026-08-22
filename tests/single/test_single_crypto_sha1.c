#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件只启用 SHA-1 时必须得到 RFC 3174 的 abc 摘要。 */
int main(void)
{
	static const uint8 arrExpected[XRT_SHA1_SIZE] = {
		0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A,
		0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C,
		0x9C, 0xD0, 0xD8, 0x9D
	};
	uint8 arrDigest[XRT_SHA1_SIZE];

	return xrtSha1("abc", 3, arrDigest) &&
		xrtConstTimeEqual(arrDigest, arrExpected, sizeof(arrDigest)) ? 0 : 1;
}
