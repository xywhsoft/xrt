#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_RESPONSE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端响应空值查询契约。 */
int main(void)
{
	return !xrtHttp1ServerResponseComplete(NULL) &&
		!xrtHttp1ServerResponseClose(NULL) &&
		!xrtHttp1ServerResponseTunnel(NULL) &&
		!xrtHttp1ServerResponseInformational(NULL) &&
		(xrtHttp1ServerResponseWireBytes(NULL) == 0) ? 0 : 1;
}

