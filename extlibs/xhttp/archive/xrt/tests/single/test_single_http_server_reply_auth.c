#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddChallenge(
		NULL,
		XRT_STR_LITERAL("Basic"),
		XRT_STR_LITERAL("abc==")
	) ? 1 : 0;
}
