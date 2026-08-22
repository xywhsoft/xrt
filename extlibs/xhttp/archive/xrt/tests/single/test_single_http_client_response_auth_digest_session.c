#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端响应 Digest 会话入口。 */
int main(void)
{
	xhttpdigestsessioncheck Check;

	return xrtHttpResponseDigestSessionAccept(
		NULL,
		NULL,
		NULL,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
