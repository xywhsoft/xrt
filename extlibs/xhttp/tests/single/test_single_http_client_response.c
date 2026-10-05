#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 验证单头响应只读 API 与空指针销毁契约。 */
int main(void)
{
	xrtHttpResponseDestroy(NULL);
	return (xrtHttpResponseStatus(NULL) == 0) &&
		(xrtHttpResponseHeaderCount(NULL) == 0) &&
		(xrtHttpResponseBodyBytes(NULL) == 0) &&
		!xrtHttpResponseSuccess(NULL) ? 0 : 1;
}

