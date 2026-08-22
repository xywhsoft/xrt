#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_BASIC)

/* 把通用响应 challenge 解码回调适配到 Basic 协议结果。 */
static bool __xrtHttpResponseBasicChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	void* pChallenge
)
{
	return xrtHttpBasicChallengeRead(
		Value,
		pOutput,
		iCapacity,
		pSize,
		(xhttpbasicchallenge*)pChallenge
	);
}



/* 查找并解码指定响应字段中的下一份 Basic challenge。 */
static xhttpnext __xrtHttpResponseBasicChallengeNext(
	const xhttpresponse* pResponse,
	xstrview Name,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
)
{
	return __xrtHttpResponseChallengeRead(
		pResponse,
		Name,
		XRT_STR_LITERAL("Basic"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		sizeof(*pChallenge),
		__xrtHttpResponseBasicChallengeRead,
		"parse-http-response-basic-challenge",
		"HTTP response Basic challenge is invalid"
	);
}



/* 迭代并解码源站 Basic challenge。 */
XRT_API xhttpnext xrtHttpResponseBasicChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
)
{
	return __xrtHttpResponseBasicChallengeNext(
		pResponse,
		XRT_STR_LITERAL("WWW-Authenticate"),
		pCursor,
		pOutput,
		iCapacity,
		pSize,
		pChallenge
	);
}



/* 迭代并解码代理 Basic challenge。 */
XRT_API xhttpnext xrtHttpResponseProxyBasicChallengeNext(
	const xhttpresponse* pResponse,
	xhttpauthcursor* pCursor,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
)
{
	return __xrtHttpResponseBasicChallengeNext(
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
