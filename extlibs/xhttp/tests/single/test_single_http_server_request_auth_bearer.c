#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REQUEST_AUTH_BEARER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 Bearer 认证入口。 */
int main(void)
{
	xstrview Token;

	return xrtHttpServerRequestBearerAuth(
		NULL, &Token
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
