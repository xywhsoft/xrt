#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



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
