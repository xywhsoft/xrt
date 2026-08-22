#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 Basic challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddBasicChallenge(
		NULL,
		XRT_STR_LITERAL("api"),
		true
	) ? 1 : 0;
}
