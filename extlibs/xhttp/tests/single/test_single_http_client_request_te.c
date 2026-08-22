#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_REQUEST_TE
	#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_TE
#endif
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 HTTP/1 响应 Trailer 一步式声明。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("http://example.test/events")
	);
	bool bResult;

	if ( pRequest == NULL ) {
		return 1;
	}
	bResult = xrtHttp1RequestAcceptTrailers(pRequest) &&
		(xrtHttpRequestHeaderCount(pRequest) == 2u);
	xrtHttpRequestDestroy(pRequest);
	return bResult ? 0 : 1;
}


