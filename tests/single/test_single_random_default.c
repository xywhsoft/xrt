#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供线程本地随机便捷层。 */
int main(void)
{
	xrtRandSeed(42, 54);
	return xrtRand32() == UINT32_C(0xA15C02B7) ? 0 : 1;
}
