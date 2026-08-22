#include "../internal/xrt_http_client_easy.h"
#include "../internal/xrt_http_client_runtime.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_EASY)

/* 发布便利入口的统一参数错误，保持高层 Client 错误域不变。 */
bool __xrtHttpClientEasyCheck(
	xhttpclient* pClient,
	bool bCompletion,
	cstr sOperation
)
{
	if ( (pClient != NULL) && bCompletion ) {
		return true;
	}
	__xrtHttpClientSetError(
		XERR_ARGUMENT,
		XHTTP_CLIENT_ERROR_ARGUMENT,
		sOperation,
		"HTTP client and required completion callback must not be null",
		NULL
	);
	return false;
}



/* 复用公开请求构建器创建一次提交快照，不引入第二套正文表示。 */
xhttprequest* __xrtHttpClientEasyRequest(
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(Method, Url);

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( (pBody != NULL) && !xrtHttpRequestSetBytes(
		pRequest,
		*pBody,
		ContentType
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 创建临时请求并把其不可变快照交给现有 callback 执行层。 */
static xhttpcall* __xrtHttpClientEasyDo(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	xhttprequest* pRequest;
	xhttpcall* pCall;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		pDone != NULL,
		"submit-http-easy"
	) ) {
		return NULL;
	}
	pRequest = __xrtHttpClientEasyRequest(
		Method,
		Url,
		pBody,
		ContentType
	);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		pOptions,
		pDone,
		pData
	);
	xrtHttpRequestDestroy(pRequest);
	return pCall;
}



/* 提交无正文 GET。 */
XRT_API xhttpcall* xrtHttpClientGet(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions,
		pDone,
		pData
	);
}



/* 提交拥有固定字节正文的 POST。 */
XRT_API xhttpcall* xrtHttpClientPost(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions,
		pDone,
		pData
	);
}



/* 提交拥有固定字节正文的任意方法请求。 */
XRT_API xhttpcall* xrtHttpClientSendBytes(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions,
		pDone,
		pData
	);
}

#endif
