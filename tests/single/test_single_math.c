#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <float.h>



/* 单头文件必须独立提供显式容差数学比较。 */
int main(void)
{
	if ( !xrtMathNear(100.0, 100.05, 0.0, 0.001) ) {
		return 1;
	}
	if ( xrtMathNear(DBL_MAX, -DBL_MAX, 0.0, 1.5) ) {
		return 2;
	}
	return xrtMathNear(DBL_MAX, -DBL_MAX, 0.0, 2.0) ? 0 : 3;
}
