#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_COOKIES
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件公开自动 Cookie 默认策略。 */
int main(void)
{
	xhttpcookieoptions Options;

	xrtHttpCookieOptionsInit(&Options);
	if ( Options.Flags != XHTTP_COOKIE_SAME_SITE ) {
		return 1;
	}
	return 0;
}
