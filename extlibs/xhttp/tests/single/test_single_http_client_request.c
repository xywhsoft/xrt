#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头文件包含并执行客户端请求构建器。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/api")
	);
	const xhttpfield* pType;
	bool bPass;

	if ( pRequest == NULL ) {
		return 1;
	}
	bPass = xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)"{}", 2 },
		XRT_STR_LITERAL("application/json")
	);
	pType = xrtHttpRequestHeader(
		pRequest, XRT_STR_LITERAL("Content-Type")
	);
	bPass = bPass &&
		(xrtHttpBodyLength(
			xrtHttpRequestBody(pRequest)
		) == 2) &&
		(xrtHttpRequestHeaderData(pRequest) != NULL) &&
		(pType != NULL) &&
		(pType->Value.Size == 16);
	xrtHttpRequestDestroy(pRequest);
	return bPass ? 0 : 1;
}

