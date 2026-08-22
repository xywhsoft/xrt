#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REPLY_AUTH_BEARER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端 Bearer challenge 入口。 */
int main(void)
{
	xhttpbearerchallenge Challenge = {
		XHTTP_BEARER_HAS_REALM,
		XRT_STR_INIT("api"),
		{ NULL, 0 }, { NULL, 0 }, { NULL, 0 }, { NULL, 0 }
	};

	return xrtHttpReplyAddBearerChallenge(
		NULL, &Challenge
	) ? 1 : 0;
}
