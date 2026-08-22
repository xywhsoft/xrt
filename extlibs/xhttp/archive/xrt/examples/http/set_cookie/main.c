#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 展示严格、安全且可扩展的 Set-Cookie 字段值构建。 */
int main(void)
{
	static const xcookieattribute Extensions[] = {
		{
			XCOOKIE_ATTRIBUTE_HAS_VALUE,
			XRT_STR_INIT("Vendor"),
			XRT_STR_INIT("on")
		}
	};
	xsetcookie Cookie;
	char Buffer[256];
	size_t iSize;

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Flags = XSET_COOKIE_HAS_PATH |
		XSET_COOKIE_HAS_MAX_AGE |
		XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_SECURE |
		XSET_COOKIE_HTTP_ONLY;
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.Value = XRT_STR_LITERAL("abc123");
	Cookie.Path = XRT_STR_LITERAL("/");
	Cookie.MaxAge = 3600;
	Cookie.SameSite = XCOOKIE_SAME_SITE_LAX;
	Cookie.Extensions = Extensions;
	Cookie.ExtensionCount = 1;
	if ( !xrtSetCookieWrite(
		&Cookie, Buffer, sizeof(Buffer), &iSize
	) ) {
		return 1;
	}
	printf("Set-Cookie: %.*s\n", (int)iSize, Buffer);
	return 0;
}
