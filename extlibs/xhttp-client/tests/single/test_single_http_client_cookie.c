#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_CLIENT_IMPLEMENTATION
#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_COOKIES
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-client/features.h"
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
#include "../../../../single/extlibs/xhttp-client.h"




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
