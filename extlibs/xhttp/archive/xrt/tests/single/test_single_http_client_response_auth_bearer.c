#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端响应 Bearer challenge 入口。 */
int main(void)
{
	xhttpauthcursor Cursor;
	xhttpbearerchallenge Challenge;
	size_t iSize;

	xrtHttpAuthCursorInit(&Cursor);
	return xrtHttpResponseBearerChallengeNext(
		NULL, &Cursor, NULL, 0, &iSize, &Challenge
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
