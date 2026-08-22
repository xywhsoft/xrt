#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件中的自动复位事件。 */
int main(void)
{
	xevent tEvent;

	return xrtEventInit(&tEvent, false, true) &&
		(xrtEventTryWait(&tEvent) == XWAIT_OK) &&
		(xrtEventTryWait(&tEvent) == XWAIT_TIMEOUT) &&
		xrtEventUnit(&tEvent) ? 0 : 1;
}
