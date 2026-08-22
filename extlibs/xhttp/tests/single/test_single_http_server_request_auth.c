#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REQUEST_AUTH
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端通用认证入口。 */
int main(void)
{
	xhttpauth Auth;

	return xrtHttpServerRequestAuth(
		NULL, &Auth
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
