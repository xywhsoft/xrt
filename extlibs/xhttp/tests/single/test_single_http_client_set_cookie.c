#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_SET_COOKIE
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



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

