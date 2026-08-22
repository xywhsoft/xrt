#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REPLY_AUTH_DIGEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 Reply Digest challenge 入口。 */
int main(void)
{
	return xrtHttpReplyAddDigestChallenge(NULL, NULL) ? 1 : 0;
}
