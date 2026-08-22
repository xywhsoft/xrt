#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REQUEST_AUTH_BASIC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 Basic 认证入口。 */
int main(void)
{
	xhttpbasicauth Basic;
	size_t iSize;

	return xrtHttpServerRequestBasicAuth(
		NULL, NULL, 0, &iSize, &Basic
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
