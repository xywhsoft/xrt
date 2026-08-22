#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供带密钥的一次性哈希。 */
int main(void)
{
	xsipkey Key = xrtSipKey(UINT64_C(0x0706050403020100),
		UINT64_C(0x0F0E0D0C0B0A0908));

	return xrtSipHash(NULL, 0, Key) == UINT64_C(0x726FDB47DD0E0E31) ? 0 : 1;
}
