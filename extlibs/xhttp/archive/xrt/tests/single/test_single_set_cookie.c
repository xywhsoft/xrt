#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Set-Cookie 宽松解析和结构化构建。 */
int main(void)
{
	xsetcookie Cookie;
	xsetcookie Parsed;
	char Text[128];
	size_t iSize;

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.Value = XRT_STR_LITERAL("abc");
	Cookie.Path = XRT_STR_LITERAL("/");
	Cookie.SameSite = XCOOKIE_SAME_SITE_LAX;
	Cookie.Flags = XSET_COOKIE_HAS_PATH | XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_SECURE | XSET_COOKIE_HTTP_ONLY;
	if ( !xrtSetCookieWrite(
		&Cookie, Text, sizeof(Text), &iSize
	) || !xrtSetCookieParse(
		(xstrview){ Text, iSize }, &Parsed
	) || (Parsed.Name.Size != 3) ||
		((Parsed.Flags & XSET_COOKIE_HTTP_ONLY) == 0) ) {
		return 1;
	}
	return 0;
}
