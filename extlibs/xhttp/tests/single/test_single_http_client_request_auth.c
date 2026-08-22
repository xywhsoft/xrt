#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_AUTH
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头通用客户端认证入口。 */
int main(void)
{
	return xrtHttpRequestSetAuth(
		NULL,
		XRT_STR_LITERAL("Basic"),
		XRT_STR_LITERAL("abc==")
	) ? 1 : 0;
}
