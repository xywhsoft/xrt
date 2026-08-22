#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供显式容差数学比较。 */
int main(void)
{
	return xrtMathNear(100.0, 100.05, 0.0, 0.001) ? 0 : 1;
}
