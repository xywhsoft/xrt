#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_SET_COOKIE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头客户端响应 Set-Cookie 辅助入口。 */
int main(void)
{
	xsetcookie Cookie;
	size_t iIndex = 0;

	return xrtHttpResponseSetCookieNext(
		NULL,
		&iIndex,
		&Cookie
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}

