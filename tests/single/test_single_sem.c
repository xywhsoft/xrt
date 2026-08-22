#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的信号量计数。 */
int main(void)
{
	xsem tSem;

	return xrtSemInit(&tSem, 0, 1) && xrtSemPost(&tSem) &&
		(xrtSemTryWait(&tSem) == XWAIT_OK) && xrtSemUnit(&tSem) ? 0 : 1;
}
