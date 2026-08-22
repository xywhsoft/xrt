#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 Host authority 校验入口。 */
int main(void)
{
	xhttpauthority Host;

	return xrtHttpHostParse(
		XRT_STR_LITERAL("[::1]:8080"), &Host
	) && (Host.Port == 8080) && xrtHttpHostValid(
		XRT_STR_LITERAL("[::1]:8080")
	) && !xrtHttpHostValid(
		XRT_STR_LITERAL("user@example.test")
	) ? 0 : 1;
}
