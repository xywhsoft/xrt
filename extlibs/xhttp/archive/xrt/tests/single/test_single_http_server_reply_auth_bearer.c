#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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
