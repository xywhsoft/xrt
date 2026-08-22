#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头通用客户端认证入口。 */
int main(void)
{
	return xrtHttpRequestSetAuth(
		NULL,
		XRT_STR_LITERAL("Basic"),
		XRT_STR_LITERAL("abc==")
	) ? 1 : 0;
}
