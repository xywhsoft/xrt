#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 验证单头客户端响应 Digest 会话入口。 */
int main(void)
{
	xhttpdigestsessioncheck Check;

	return xrtHttpResponseDigestSessionAccept(
		NULL,
		NULL,
		NULL,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		&Check
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
