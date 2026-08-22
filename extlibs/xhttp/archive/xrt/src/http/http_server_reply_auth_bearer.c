#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BEARER)

/* 把 Bearer challenge 交给协议层的规范写出器。 */
static bool __xrtHttpReplyBearerWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpBearerChallengeWrite(
		(const xhttpbearerchallenge*)pContext,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名事务追加 Bearer challenge。 */
static bool __xrtHttpReplyAddBearerChallenge(
	xhttpreply* pReply,
	xstrview Name,
	const xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpReplyAddWrittenAuth(
		pReply,
		Name,
		__xrtHttpReplyBearerWrite,
		pChallenge
	);
}



/* 追加源站 Bearer challenge。 */
XRT_API bool xrtHttpReplyAddBearerChallenge(
	xhttpreply* pReply,
	const xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpReplyAddBearerChallenge(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pChallenge
	);
}



/* 追加代理 Bearer challenge。 */
XRT_API bool xrtHttpReplyAddProxyBearerChallenge(
	xhttpreply* pReply,
	const xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpReplyAddBearerChallenge(
		pReply,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		pChallenge
	);
}

#endif
