#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_AUTH_BEARER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头客户端 Bearer 认证入口。 */
int main(void)
{
	return xrtHttpRequestSetBearerAuth(
		NULL,
		XRT_STR_LITERAL("token")
	) ? 1 : 0;
}
