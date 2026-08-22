#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端响应 Digest challenge 入口。 */
int main(void)
{
	xhttpauthcursor Cursor;
	xhttpdigestchallenge Challenge;
	size_t iSize;

	xrtHttpAuthCursorInit(&Cursor);
	return xrtHttpResponseDigestChallengeNext(
		NULL, &Cursor, NULL, 0, &iSize, &Challenge
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
