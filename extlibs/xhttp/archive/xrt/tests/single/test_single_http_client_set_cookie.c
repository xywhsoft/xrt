#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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
