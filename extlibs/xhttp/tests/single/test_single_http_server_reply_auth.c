#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REPLY_AUTH
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddChallenge(
		NULL,
		XRT_STR_LITERAL("Basic"),
		XRT_STR_LITERAL("abc==")
	) ? 1 : 0;
}
