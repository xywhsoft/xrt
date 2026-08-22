#include "../internal/xrt_http_client.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BEARER)

/* 把通用响应 challenge 解码回调适配到 Bearer 协议结果。 */
static bool __xrtHttpResponseBearerChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge
)
{
	return xrtHttpBearerChallengeRead(
		Value,
		pOutput,
		iCapacity,
		pSize,
		(xhttpbearerchallenge*)pChallenge
	);
}



/* 查找并解码指定响应字段中的下一份 Bearer challenge。 */
static xhttpnext __xrtHttpResponseBearerChallengeNext(
	const xhttpresponse* pResponse,
	xstrview Name,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpResponseChallengeRead(
		pResponse,
		Name,
		XRT_STR_LITERAL("Bearer"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		sizeof(*pChallenge),
		__xrtHttpResponseBearerChallengeRead,
		"parse-http-response-bearer-challenge",
		"HTTP response Bearer challenge is invalid"
	);
}



/* 迭代并解码源站 Bearer challenge。 */
XRT_API xhttpnext xrtHttpResponseBearerChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpResponseBearerChallengeNext(
		pResponse,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge
	);
}



/* 迭代并解码代理 Bearer challenge。 */
XRT_API xhttpnext xrtHttpResponseProxyBearerChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
)
{
	return __xrtHttpResponseBearerChallengeNext(
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
