#define XHTTP_SERVER_IMPLEMENTATION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-server/features.h"
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-server.h"




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
