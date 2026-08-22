#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端 Bearer 认证入口。 */
int main(void)
{
	return xrtHttpRequestSetBearerAuth(
		NULL,
		XRT_STR_LITERAL("token")
	) ? 1 : 0;
}
