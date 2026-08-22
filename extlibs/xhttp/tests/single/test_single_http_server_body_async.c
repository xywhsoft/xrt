#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头构建包含异步 Server Response 与 TCP 组合层。 */
int main(void)
{
	xhttpserverconfig Config;

	xrtHttpServerConfigInit(&Config);
	return (Config.WriteSize != 0) &&
		(Config.WriteTimeout != 0) &&
		(xrtHttp1ServerResponseWait(NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) ?
		0 : 1;
}
