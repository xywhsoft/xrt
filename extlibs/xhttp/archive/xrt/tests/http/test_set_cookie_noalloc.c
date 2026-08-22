#include "../test_allocator.h"



/* Set-Cookie 解析、属性扫描、日期和结构化写出均不得分配。 */
int main(void)
{
	xstrview Text = XRT_STR_LITERAL(
		"sid=abc; Path=/; Max-Age=60; SameSite=Lax; Secure"
	);
	xsetcookie Cookie;
	xcookieattribute Attribute;
	char Output[128];
	size_t iOffset = 0;
	size_t iSize;
	xtime iTime;

	testRequire(testInstallFailAllocator(),
		"set-cookie failure allocator install failed");
	testRequire(xrtSetCookieParse(Text, &Cookie),
		"set-cookie parse allocated memory");
	testRequire(xrtSetCookieAttributeNext(
		Cookie.RawAttributes, &iOffset, &Attribute
	) == XCOOKIE_ATTRIBUTE_ITEM,
		"set-cookie attribute iteration allocated memory");
	testRequire(xrtCookieDateParse(
		XRT_STR_LITERAL("Wed, 09 Jun 2021 10:18:14 GMT"), &iTime
	), "cookie date parse allocated memory");
	Cookie.RawAttributes = (xstrview){ NULL, 0 };
	testRequire(xrtSetCookieWrite(
		&Cookie, Output, sizeof(Output), &iSize
	), "set-cookie write allocated memory");
	printf("[PASS] set_cookie_noalloc\n");
	return 0;
}
