#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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
