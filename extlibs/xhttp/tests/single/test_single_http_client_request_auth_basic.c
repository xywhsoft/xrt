#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_AUTH_BASIC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头客户端 Basic 认证入口。 */
int main(void)
{
	return xrtHttpRequestSetBasicAuth(
		NULL,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("password")
	) ? 1 : 0;
}
