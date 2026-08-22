#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端 Basic 认证入口。 */
int main(void)
{
	return xrtHttpRequestSetBasicAuth(
		NULL,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("password")
	) ? 1 : 0;
}
