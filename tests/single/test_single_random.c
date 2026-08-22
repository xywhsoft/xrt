#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供显式 PCG32 随机原语。 */
int main(void)
{
	xrng Rng;

	xrtRngSeed(&Rng, 42, 54);
	return xrtRng32(&Rng) == UINT32_C(0xA15C02B7) ? 0 : 1;
}
