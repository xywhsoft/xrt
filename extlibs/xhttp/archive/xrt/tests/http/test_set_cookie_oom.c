#include "../test_allocator.h"



/* Set-Cookie 分配构建失败必须保留内存不足错误。 */
int main(void)
{
	xsetcookie Cookie;

	memset(&Cookie, 0, sizeof(Cookie));
	Cookie.Name = XRT_STR_LITERAL("sid");
	Cookie.Value = XRT_STR_LITERAL("abc123");
	testRequire(testInstallFailAllocator(),
		"set-cookie OOM allocator install failed");
	testRequire((xrtSetCookieBuild(&Cookie, NULL) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"set-cookie build did not publish OOM");
	printf("[PASS] set_cookie_oom\n");
	return 0;
}
