#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头服务端请求 Digest 入口。 */
int main(void)
{
	xhttpdigestauth Digest;
	size_t iSize;

	return xrtHttpServerRequestDigestAuth(
		NULL, NULL, 0, &iSize, &Digest
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
