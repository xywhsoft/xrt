#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_SET_COOKIE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 Set-Cookie 完整解析路径。 */
int main(void)
{
	xsetcookie Cookie;

	return xrtSetCookieParse(
		XRT_STR_LITERAL("sid=abc; Path=/; Secure; HttpOnly"),
		&Cookie
	) ? 0 : 1;
}
