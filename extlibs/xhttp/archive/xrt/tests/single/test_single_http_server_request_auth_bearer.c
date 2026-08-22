#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 Bearer 认证入口。 */
int main(void)
{
	xstrview Token;

	return xrtHttpServerRequestBearerAuth(
		NULL, &Token
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
