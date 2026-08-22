#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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
