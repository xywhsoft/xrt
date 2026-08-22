#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供确定性的 32 位哈希。 */
int main(void)
{
	return xrtHash32("Hello", 5) == UINT32_C(0x83B10C78) ? 0 : 1;
}
