#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REQUEST_AUTH_DIGEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端请求 Digest 入口。 */
int main(void)
{
	xhttpdigestauth Digest;
	size_t iSize;

	return xrtHttpServerRequestDigestAuth(
		NULL, NULL, 0, &iSize, &Digest
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
