#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端通用认证入口。 */
int main(void)
{
	xhttpauth Auth;

	return xrtHttpServerRequestAuth(
		NULL, &Auth
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
