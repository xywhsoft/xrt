#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头客户端响应 Digest info 入口。 */
int main(void)
{
	xhttpdigestinfo Info;
	size_t iSize;

	return xrtHttpResponseDigestInfo(
		NULL, XHTTP_DIGEST_ALGORITHM_SHA256,
		NULL, 0, &iSize, &Info
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
