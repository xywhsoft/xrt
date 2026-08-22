#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头 Content-Type 请求辅助入口。 */
int main(void)
{
	xmediatype Type;

	return xrtHttpServerRequestContentType(
		NULL,
		&Type
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
