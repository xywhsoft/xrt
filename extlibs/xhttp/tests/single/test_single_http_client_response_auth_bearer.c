#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_BEARER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



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
