#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须提供可创建、唤醒、等待和销毁的 select 端口。 */
int main(void)
{
	xnetportconfig Config;
	xnetportevent Event;
	xnetport* pPort;
	size_t iCount = 0;

	xrtNetPortConfigInit(&Config);
	Config.Backend = XNET_PORT_SELECT;
	pPort = xrtNetPortCreate(&Config);
	if ( (pPort == NULL) || !xrtNetPortWake(pPort) ||
		 (xrtNetPortWait(pPort, &Event, 1,
		xrtDeadlineAfter(1000000), &iCount) != XNET_RESULT_OK) ||
		 (iCount != 1) || (Event.Type != XNET_PORT_EVENT_WAKE) ) {
		if ( pPort != NULL ) { (void)xrtNetPortDestroy(pPort); }
		return 1;
	}
	return xrtNetPortDestroy(pPort) ? 0 : 1;
}
