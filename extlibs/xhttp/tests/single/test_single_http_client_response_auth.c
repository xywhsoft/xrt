#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



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
