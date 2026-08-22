#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_AUTH_DIGEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头客户端请求 Digest 设置入口。 */
int main(void)
{
	return xrtHttpRequestSetDigestAuth(NULL, NULL) ? 1 : 0;
}
