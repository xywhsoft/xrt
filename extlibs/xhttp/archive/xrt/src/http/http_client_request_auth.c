#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_AUTH)

/* 通用认证写出上下文只借用本次调用的两个视图。 */
typedef struct xrt_http_auth_write_context {
	xstrview Scheme;
	xstrview Data;
} xrt_http_auth_write_context;



/* 把通用认证上下文转交给协议层写出器。 */
static bool __xrtHttpRequestAuthWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xrt_http_auth_write_context* pAuth =
		(const xrt_http_auth_write_context*)pContext;

	return xrtHttpAuthWrite(
		pAuth->Scheme,
		pAuth->Data,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 请求字段消费上下文只在同步认证构建期间有效。 */
typedef struct xrt_http_auth_consume_context {
	xhttprequest* Request;
	xstrview Name;
} xrt_http_auth_consume_context;



/* 把临时认证值复制进请求拥有的 Header 容器。 */
static bool __xrtHttpRequestAuthConsume(
	void* pContext,
	xstrview Value
)
{
	xrt_http_auth_consume_context* pAuth =
		(xrt_http_auth_consume_context*)pContext;

	return xrtHttpRequestSetHeader(
		pAuth->Request,
		pAuth->Name,
		Value
	);
}



/* 通过协议层共享事务写出、设置并清零认证字段。 */
bool __xrtHttpRequestSetWrittenAuth(
	xhttprequest* pRequest,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
)
{
	xrt_http_auth_consume_context Consume;

	if ( (pRequest == NULL) || (pWrite == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Consume.Request = pRequest;
	Consume.Name = Name;
	return __xrtHttpAuthWriteTemporary(
		pWrite,
		pContext,
		__xrtHttpRequestAuthConsume,
		&Consume
	);
}



/* 使用指定字段名设置通用认证值。 */
static bool __xrtHttpRequestSetAuth(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Scheme,
	xstrview Data
)
{
	xrt_http_auth_write_context Context;

	Context.Scheme = Scheme;
	Context.Data = Data;
	return __xrtHttpRequestSetWrittenAuth(
		pRequest,
		Name,
		__xrtHttpRequestAuthWrite,
		&Context
	);
}



/* 设置源站 Authorization。 */
XRT_API bool xrtHttpRequestSetAuth(
	xhttprequest* pRequest,
	xstrview Scheme,
	xstrview Data
)
{
	return __xrtHttpRequestSetAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		Scheme,
		Data
	);
}



/* 设置代理 Proxy-Authorization。 */
XRT_API bool xrtHttpRequestSetProxyAuth(
	xhttprequest* pRequest,
	xstrview Scheme,
	xstrview Data
)
{
	return __xrtHttpRequestSetAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		Scheme,
		Data
	);
}



/* 清除全部源站认证字段。 */
XRT_API size_t xrtHttpRequestClearAuth(xhttprequest* pRequest)
{
	return xrtHttpRequestRemoveHeader(
		pRequest,
		XRT_STR_LITERAL("Authorization")
	);
}



/* 清除全部代理认证字段。 */
XRT_API size_t xrtHttpRequestClearProxyAuth(xhttprequest* pRequest)
{
	return xrtHttpRequestRemoveHeader(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization")
	);
}

#endif
