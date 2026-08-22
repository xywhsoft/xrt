#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供确定性的 64 位哈希。 */
int main(void)
{
	return xrtHash64("Hello", 5) == UINT64_C(0x341C16AEF48B463D) ? 0 : 1;
}
