#include "../internal/xrt_http_client_easy.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_EASY_FUTURE)

/* 创建临时请求并把其不可变快照交给现有 Future 执行层。 */
static xfuture* __xrtHttpClientEasyAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	xhttprequest* pRequest;
	xfuture* pFuture;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		true,
		"submit-http-easy-future"
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
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		pOptions
	);
	xrtHttpRequestDestroy(pRequest);
	return pFuture;
}



/* 创建临时请求并在宿主线程复用现有同步等待层。 */
static xhttpresult* __xrtHttpClientEasySync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	xhttprequest* pRequest;
	xhttpresult* pResult;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		true,
		"run-http-easy-sync"
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
	pResult = xrtHttpClientDoSync(
		pClient,
		pRequest,
		pOptions
	);
	xrtHttpRequestDestroy(pRequest);
	return pResult;
}



/* 异步提交无正文 GET。 */
XRT_API xfuture* xrtHttpClientGetAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions
	);
}



/* 异步提交固定正文 POST。 */
XRT_API xfuture* xrtHttpClientPostAsync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 异步提交任意方法的固定正文请求。 */
XRT_API xfuture* xrtHttpClientSendBytesAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 同步执行无正文 GET。 */
XRT_API xhttpresult* xrtHttpClientGetSync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions
	);
}



/* 同步执行固定正文 POST。 */
XRT_API xhttpresult* xrtHttpClientPostSync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 同步执行任意方法的固定正文请求。 */
XRT_API xhttpresult* xrtHttpClientSendBytesSync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions
	);
}

#endif
