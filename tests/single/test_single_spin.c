#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供可裁剪的自旋锁。 */
int main(void)
{
	xspinlock Spin = XRT_SPIN_INIT;

	if ( !xrtSpinLock(&Spin) ) {
		return 1;
	}
	if ( !xrtSpinUnlock(&Spin) ) {
		return 2;
	}
	return xrtSpinUnit(&Spin) ? 0 : 3;
}
