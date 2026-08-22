#ifndef XRT_HTTP_CLIENT_EASY_H
#define XRT_HTTP_CLIENT_EASY_H

#include <xrt/http_client_runtime.h>

#if defined(XRT_FEATURE_HTTP_CLIENT_EASY_FUTURE)
	#include <xrt/http_client_future.h>
#endif



#if defined(XRT_FEATURE_HTTP_CLIENT_EASY) && \
	!defined(XRT_FEATURE_HTTP_CLIENT)
	#error "XRT HTTP client convenience support requires HTTP client support"
#endif

#if defined(XRT_FEATURE_HTTP_CLIENT_EASY_FUTURE) && \
	(!defined(XRT_FEATURE_HTTP_CLIENT_EASY) || \
	 !defined(XRT_FEATURE_HTTP_CLIENT_FUTURE))
	#error "XRT HTTP client Future convenience support requires convenience and Future support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_CLIENT_EASY)

/* 构造并提交一条无正文 GET；返回的 Call 所有权与 xrtHttpClientDo 相同。 */
XRT_API xhttpcall* xrtHttpClientGet(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 复制正文并构造一条 POST；ContentType 为空时不生成 Content-Type。 */
XRT_API xhttpcall* xrtHttpClientPost(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 复制正文并以任意合法 HTTP 方法提交，适合 PUT、PATCH 等固定正文请求。 */
XRT_API xhttpcall* xrtHttpClientSendBytes(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);

#endif



#if defined(XRT_FEATURE_HTTP_CLIENT_EASY_FUTURE)

/* 构造并异步提交一条无正文 GET，返回拥有型 Future。 */
XRT_API xfuture* xrtHttpClientGetAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
);



/* 复制正文并异步提交一条 POST，返回拥有型 Future。 */
XRT_API xfuture* xrtHttpClientPostAsync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 复制正文并以任意合法 HTTP 方法异步提交，返回拥有型 Future。 */
XRT_API xfuture* xrtHttpClientSendBytesAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 构造并阻塞执行一条无正文 GET，返回拥有型结果。 */
XRT_API xhttpresult* xrtHttpClientGetSync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
);



/* 复制正文并阻塞执行一条 POST，返回拥有型结果。 */
XRT_API xhttpresult* xrtHttpClientPostSync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 复制正文并以任意合法 HTTP 方法阻塞执行，返回拥有型结果。 */
XRT_API xhttpresult* xrtHttpClientSendBytesSync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);

#endif



XRT_EXTERN_C_END

#endif
