#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端 Basic 认证入口。 */
int main(void)
{
	xhttpbasicauth Basic;
	size_t iSize;

	return xrtHttpServerRequestBasicAuth(
		NULL, NULL, 0, &iSize, &Basic
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
