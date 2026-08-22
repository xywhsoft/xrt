#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST)

/* 把通用 challenge 解码回调适配到 Digest 结果。 */
static bool __xrtHttpResponseDigestChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge
)
{
	return xrtHttpDigestChallengeRead(
		Value,
		pOutput,
		iCapacity,
		pSize,
		(xhttpdigestchallenge*)pChallenge
	);
}



/* 查找并解码指定响应字段中的下一份 Digest challenge。 */
static xhttpnext __xrtHttpResponseDigestChallengeNext(
	const xhttpresponse* pResponse,
	xstrview Name,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpResponseChallengeRead(
		pResponse,
		Name,
		XRT_STR_LITERAL("Digest"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		sizeof(*pChallenge),
		__xrtHttpResponseDigestChallengeRead,
		"parse-http-response-digest-challenge",
		"HTTP response Digest challenge is invalid"
	);
}



/* 迭代源站 Digest challenge。 */
XRT_API xhttpnext xrtHttpResponseDigestChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpResponseDigestChallengeNext(
		pResponse,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge
	);
}



/* 迭代代理 Digest challenge。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
)
{
	return __xrtHttpResponseDigestChallengeNext(
		pResponse,
		XRT_STR_LITERAL("Proxy-Authenticate"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge
	);
}

#endif
