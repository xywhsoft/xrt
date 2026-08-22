#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



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
