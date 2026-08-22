#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端响应 Content-Type 辅助入口。 */
int main(void)
{
	xmediatype Type;

	return xrtHttpResponseContentType(
		NULL,
		&Type
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
