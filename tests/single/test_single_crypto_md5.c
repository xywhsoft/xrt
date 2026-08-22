#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件可独立提供 RFC 1321 的 MD5 互操作入口。 */
int main(void)
{
	static const uint8 Expected[XRT_MD5_SIZE] = {
		0x90, 0x01, 0x50, 0x98, 0x3C, 0xD2, 0x4F, 0xB0,
		0xD6, 0x96, 0x3F, 0x7D, 0x28, 0xE1, 0x7F, 0x72
	};
	uint8 Digest[XRT_MD5_SIZE];

	return (!xrtMd5("abc", 3u, Digest) ||
		!xrtConstTimeEqual(Digest, Expected, sizeof(Digest))) ? 1 : 0;
}
