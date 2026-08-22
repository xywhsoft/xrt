#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 Reply Digest challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddDigestChallenge(NULL, NULL) ? 1 : 0;
}
