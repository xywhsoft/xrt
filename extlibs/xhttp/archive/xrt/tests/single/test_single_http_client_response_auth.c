#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头客户端响应认证入口。 */
int main(void)
{
	xhttpauthcursor Cursor;
	xhttpauth Auth;

	xrtHttpAuthCursorInit(&Cursor);
	return xrtHttpResponseChallengeNext(
		NULL, &Cursor, &Auth
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
