#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_COOKIE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头 Cookie 请求辅助入口。 */
int main(void)
{
	xcookiepair Cookie;

	return xrtHttpServerRequestCookie(
		NULL,
		XRT_STR_LITERAL("sid"),
		&Cookie
	) == XCOOKIE_NEXT_ERROR ? 0 : 1;
}

