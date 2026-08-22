#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布 Server 默认策略和空值查询。 */
int main(void)
{
	xhttpserverconfig Config;
	xhttpserverevents Events;

	xrtHttpServerConfigInit(&Config);
	xrtHttpServerEventsInit(&Events);
	return (Config.WriteSize != 0) &&
		(Config.HeaderTimeout != 0) &&
		(Config.BodyTimeout != 0) &&
		(Config.RequestTimeout != 0) &&
		(Config.IdleTimeout != 0) &&
		(Config.WriteTimeout != 0) &&
		(xrtHttpServerState(NULL) == XHTTP_SERVER_CLOSED) &&
		(xrtHttpConnState(NULL) == XHTTP_CONN_CLOSED) &&
		(xrtHttpConnError(NULL) == NULL) ? 0 : 1;
}
