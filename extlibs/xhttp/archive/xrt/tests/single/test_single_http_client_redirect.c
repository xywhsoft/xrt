#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布的重定向默认策略和公开结果查询。 */
int main(void)
{
	xhttpredirectconfig Redirect;
	xhttpcalloptions Call;

	xrtHttpRedirectConfigInit(&Redirect);
	xrtHttpCallOptionsInit(&Call);
	if ( (Redirect.MaxHops != XHTTP_REDIRECT_MAX_DEFAULT) ||
		(Redirect.Flags != XHTTP_REDIRECT_POST_TO_GET) ||
		(Call.Redirect != XHTTP_REDIRECT_DEFAULT) ||
		(xrtHttpResponseRedirects(NULL) != 0) ) {
		return 1;
	}
	return 0;
}
