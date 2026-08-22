#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST)

/* 把 Digest challenge 交给协议层规范写出器。 */
static bool __xrtHttpReplyDigestChallengeWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpDigestChallengeWrite(
		(const xhttpdigestchallenge*)pContext,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名追加一条 Digest challenge。 */
static bool __xrtHttpReplyAddDigestChallenge(
	xhttpreply* pReply,
	xstrview Name,
	const xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpReplyAddWrittenAuth(
		pReply,
		Name,
		__xrtHttpReplyDigestChallengeWrite,
		pChallenge
	);
}



/* 追加源站 Digest challenge。 */
XRT_API bool xrtHttpReplyAddDigestChallenge(
	xhttpreply* pReply,
	const xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpReplyAddDigestChallenge(
		pReply,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pChallenge
	);
}



/* 追加代理 Digest challenge。 */
XRT_API bool xrtHttpReplyAddProxyDigestChallenge(
	xhttpreply* pReply,
	const xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpReplyAddDigestChallenge(
		pReply,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		pChallenge
	);
}

#endif
