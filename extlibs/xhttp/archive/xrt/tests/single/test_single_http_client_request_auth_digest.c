#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端请求 Digest 设置入口。 */
int main(void)
{
	return xrtHttpRequestSetDigestAuth(NULL, NULL) ? 1 : 0;
}
