#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_REQUEST_AUTH_BASIC)

/* Basic 写出上下文只在同步设置调用期间有效。 */
typedef struct xrt_http_basic_write_context {
	xstrview User;
	xstrview Password;
} xrt_http_basic_write_context;



/* 把 Basic 用户信息转交给协议层安全写出器。 */
static bool __xrtHttpRequestBasicWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xrt_http_basic_write_context* pBasic =
		(const xrt_http_basic_write_context*)pContext;

	return xrtHttpBasicWrite(
		pBasic->User,
		pBasic->Password,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名构建并设置 Basic 凭据。 */
static bool __xrtHttpRequestSetBasicAuth(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview User,
	xstrview Password
)
{
	xrt_http_basic_write_context Context;

	Context.User = User;
	Context.Password = Password;
	return __xrtHttpRequestSetWrittenAuth(
		pRequest,
		Name,
		__xrtHttpRequestBasicWrite,
		&Context
	);
}



/* 设置源站 Basic 凭据。 */
XRT_API bool xrtHttpRequestSetBasicAuth(
	xhttprequest* pRequest,
	xstrview User,
	xstrview Password
)
{
	return __xrtHttpRequestSetBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		User,
		Password
	);
}



/* 设置代理 Basic 凭据。 */
XRT_API bool xrtHttpRequestSetProxyBasicAuth(
	xhttprequest* pRequest,
	xstrview User,
	xstrview Password
)
{
	return __xrtHttpRequestSetBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		User,
		Password
	);
}

#endif
