#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REPLY_AUTH_BASIC
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 Basic challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddBasicChallenge(
		NULL,
		XRT_STR_LITERAL("api"),
		true
	) ? 1 : 0;
}
