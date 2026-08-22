#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_REQUEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头服务端请求只读空值契约。 */
int main(void)
{
	return (xrtHttpServerRequestVersion(NULL) == 0) &&
		(xrtHttpServerRequestHeaderCount(NULL) == 0) &&
		(xrtHttpServerRequestHeaderData(NULL) == NULL) &&
		(xrtHttpServerRequestTrailerCount(NULL) == 0) &&
		(xrtHttpServerRequestTrailerData(NULL) == NULL) &&
		(xrtHttpServerRequestBodyBytes(NULL) == 0) &&
		!xrtHttpServerRequestAcceptsTrailers(NULL) ? 0 : 1;
}

