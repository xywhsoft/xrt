#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 Reply Digest info 入口。 */
int main(void)
{
	return xrtHttpReplySetDigestInfo(NULL, NULL) ? 1 : 0;
}
