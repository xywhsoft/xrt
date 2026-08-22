#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BEARER)

/* Bearer 写出上下文借用调用方 token。 */
typedef struct xrt_http_bearer_write_context {
	xstrview Token;
} xrt_http_bearer_write_context;



/* 把 Bearer token 转交给协议层写出器。 */
static bool __xrtHttpRequestBearerWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xrt_http_bearer_write_context* pBearer =
		(const xrt_http_bearer_write_context*)pContext;

	return xrtHttpBearerWrite(
		pBearer->Token,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名构建并设置 Bearer 凭据。 */
static bool __xrtHttpRequestSetBearerAuth(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Token
)
{
	xrt_http_bearer_write_context Context;

	Context.Token = Token;
	return __xrtHttpRequestSetWrittenAuth(
		pRequest,
		Name,
		__xrtHttpRequestBearerWrite,
		&Context
	);
}



/* 设置源站 Bearer token。 */
XRT_API bool xrtHttpRequestSetBearerAuth(
	xhttprequest* pRequest,
	xstrview Token
)
{
	return __xrtHttpRequestSetBearerAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		Token
	);
}



/* 设置代理 Bearer token。 */
XRT_API bool xrtHttpRequestSetProxyBearerAuth(
	xhttprequest* pRequest,
	xstrview Token
)
{
	return __xrtHttpRequestSetBearerAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		Token
	);
}

#endif
