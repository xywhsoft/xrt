#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件异步 HTTP/1 Stream 裁剪依赖完整。 */
int main(void)
{
	#if !defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		return 1;
	#endif
	#if !defined(XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC)
		return 2;
	#endif
	#if !defined(XHTTP_FEATURE_HTTP_BODY_ASYNC)
		return 3;
	#endif
	#if !defined(XRT_FEATURE_FUTURE)
		return 4;
	#endif
	return 0;
}
