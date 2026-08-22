#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST_INFO)

/* 把 Digest Authentication-Info 交给协议层规范写出器。 */
static bool __xrtHttpReplyDigestInfoWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpDigestInfoWrite(
		(const xhttpdigestinfo*)pContext,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名原子设置 Digest 认证信息。 */
static bool __xrtHttpReplySetDigestInfo(
	xhttpreply* pReply,
	xstrview Name,
	const xhttpdigestinfo* pInfo
)
{
	return __xrtHttpReplySetWrittenAuth(
		pReply,
		Name,
		__xrtHttpReplyDigestInfoWrite,
		pInfo
	);
}



/* 设置源站 Authentication-Info。 */
XRT_API bool xrtHttpReplySetDigestInfo(
	xhttpreply* pReply,
	const xhttpdigestinfo* pInfo
)
{
	return __xrtHttpReplySetDigestInfo(
		pReply,
		XRT_STR_LITERAL("Authentication-Info"),
		pInfo
	);
}



/* 设置代理 Proxy-Authentication-Info。 */
XRT_API bool xrtHttpReplySetProxyDigestInfo(
	xhttpreply* pReply,
	const xhttpdigestinfo* pInfo
)
{
	return __xrtHttpReplySetDigestInfo(
		pReply,
		XRT_STR_LITERAL("Proxy-Authentication-Info"),
		pInfo
	);
}

#endif
