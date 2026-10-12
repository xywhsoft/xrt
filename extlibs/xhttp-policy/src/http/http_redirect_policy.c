#include "../internal/xrt_internal.h"
#include <xrt/http_bridge.h>
#include <xrt/http_redirect_policy.h>

#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)

#define XHTTP_REDIRECT_FLAGS_MASK \
	(XHTTP_REDIRECT_POST_TO_GET | \
	 XHTTP_REDIRECT_FORWARD_CREDENTIALS | \
	 XHTTP_REDIRECT_ALLOW_DOWNGRADE)



/* 初始化通用客户端使用的安全重定向策略。 */
XHTTP_POLICY_API void xrtHttpRedirectConfigInit(
	xhttpredirectconfig* pConfig
)
{
	const xhttpredirectconfig Config = {
		XHTTP_REDIRECT_POST_TO_GET,
		XHTTP_REDIRECT_MAX_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 按 ASCII 大小写不敏感规则比较两个文本视图。 */
static bool __xrtHttpRedirectTextEqual(
	xstrview Left,
	xstrview Right
)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		uint8 iLeft = (uint8)Left.Data[i];
		uint8 iRight = (uint8)Right.Data[i];

		if ( (iLeft >= (uint8)'A') &&
			(iLeft <= (uint8)'Z') ) {
			iLeft = (uint8)(
				iLeft +
				((uint8)'a' - (uint8)'A')
			);
		}
		if ( (iRight >= (uint8)'A') &&
			(iRight <= (uint8)'Z') ) {
			iRight = (uint8)(
				iRight +
				((uint8)'a' - (uint8)'A')
			);
		}
		if ( iLeft != iRight ) {
			return false;
		}
	}
	return true;
}



/* 判断状态码是否具有标准自动重定向语义。 */
XHTTP_POLICY_API bool xrtHttpRedirectStatus(uint16 iStatus)
{
	return (iStatus == 301) ||
		(iStatus == 302) ||
		(iStatus == 303) ||
		(iStatus == 307) ||
		(iStatus == 308);
}



/* 比较会影响凭据转发和连接复用的 HTTP origin。 */
static bool __xrtHttpRedirectSameOrigin(
	const xurl* pLeft,
	const xurl* pRight
)
{
	uint16 iLeftPort;
	uint16 iRightPort;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return false;
	}
	if ( !xrtHttpBridgeUrlPortValue(pLeft, &iLeftPort) ||
		!xrtHttpBridgeUrlPortValue(pRight, &iRightPort) ) {
		return false;
	}
	return (xrtUrlSecure(pLeft) ==
			xrtUrlSecure(pRight)) &&
		(iLeftPort == iRightPort) &&
		__xrtHttpRedirectTextEqual(
			pLeft->Host,
			pRight->Host
		);
}



/*
	按 HTTP 重定向语义解析 Location。
	通用 URL 解析遵循 RFC 3986；HTTP 额外要求 Location 没有 fragment 时，
	继承当前目标的 fragment，显式空 fragment 则阻止继承。
*/
static str __xrtHttpRedirectResolve(
	const xurl* pBase,
	xstrview Location,
	size_t* pSize
)
{
	xurl Reference;
	str sResolved;
	str sExtended;
	size_t iResolved;
	size_t iExtended;

	if ( !xrtUrlParse(Location, &Reference) ) {
		return NULL;
	}
	sResolved = xrtUrlResolveBuild(
		pBase,
		Location,
		&iResolved
	);
	if ( sResolved == NULL ) {
		return NULL;
	}
	if ( ((Reference.Flags & XURL_HAS_FRAGMENT) != 0) ||
		((pBase->Flags & XURL_HAS_FRAGMENT) == 0) ) {
		*pSize = iResolved;
		return sResolved;
	}
	if ( (iResolved > (SIZE_MAX - 2u)) ||
		(pBase->Fragment.Size >
		 (SIZE_MAX - iResolved - 2u)) ) {
		xrtFree(sResolved);
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iExtended = iResolved +
		1u +
		pBase->Fragment.Size;
	sExtended = (str)xrtRealloc(
		sResolved,
		iExtended + 1u
	);
	if ( sExtended == NULL ) {
		xrtFree(sResolved);
		return NULL;
	}
	sExtended[iResolved] = '#';
	if ( pBase->Fragment.Size != 0 ) {
		memcpy(
			sExtended + iResolved + 1u,
			pBase->Fragment.Data,
			pBase->Fragment.Size
		);
	}
	sExtended[iExtended] = 0;
	*pSize = iExtended;
	return sExtended;
}



/* 删除改写为无正文请求后不再成立的正文和表示元数据。 */
static bool __xrtHttpRedirectDropBody(
	xhttprequest* pRequest
)
{
	static const xstrview Fields[] = {
		XRT_STR_INIT("Content-Length"),
		XRT_STR_INIT("Transfer-Encoding"),
		XRT_STR_INIT("Trailer"),
		XRT_STR_INIT("Expect"),
		XRT_STR_INIT("Content-Type"),
		XRT_STR_INIT("Content-Encoding"),
		XRT_STR_INIT("Content-Language"),
		XRT_STR_INIT("Content-Location"),
		XRT_STR_INIT("Content-Disposition"),
		XRT_STR_INIT("Content-Range"),
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest")
	};
	size_t i;

	if ( !xrtHttpRequestSetBody(pRequest, NULL) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		xrtHttpBridgeRequestClearTrailers(pRequest);
	#endif
	for ( i = 0;
		i < (sizeof(Fields) / sizeof(Fields[0]));
		i++ ) {
		(void)xrtHttpRequestRemoveHeader(
			pRequest,
			Fields[i]
		);
	}
	return true;
}



/* 构造下一跳请求，但直到当前响应排空后才提交。 */
XHTTP_POLICY_API xhttprequest* xrtHttpRedirectRequest(
	const xhttprequest* pRequest,
	uint16 iStatus,
	xstrview Location,
	uint32 iFlags,
	xhttpredirecterror* pError
)
{
	const xurl* pOldUrl;
	const xurl* pNewUrl;
	xhttprequest* pNext;
	xhttpredirecterror Error;
	xhttpbody* pBody;
	xstrview Method;
	str sResolved;
	size_t iResolved;
	bool bDropBody;
	bool bHead;
	bool bPost;
	bool bCrossOrigin;

	if ( !xrtMemRangeValid(pError, sizeof(*pError)) ||
		!__xhttpViewValid(Location) ||
		xrtMemRangesOverlap(pError, sizeof(*pError), Location.Data, Location.Size) ||
		((pRequest != NULL) && !xrtHttpBridgeRequestOutputValid(
			pRequest, pError, sizeof(*pError)
		)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	Error = XHTTP_REDIRECT_INVALID;
	memcpy(pError, &Error, sizeof(Error));
	if ( (pRequest == NULL) || !xrtHttpRedirectStatus(iStatus) ||
		(iFlags & ~XHTTP_REDIRECT_FLAGS_MASK) != 0 ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pOldUrl = xrtHttpRequestUrl(pRequest);
	sResolved = __xrtHttpRedirectResolve(
		pOldUrl,
		Location,
		&iResolved
	);
	if ( sResolved == NULL ) {
		Error = XHTTP_REDIRECT_INVALID;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	pNext = xrtHttpRequestClone(pRequest);
	if ( (pNext == NULL) ||
		!xrtHttpRequestSetUrl(
			pNext,
			(xstrview){ sResolved, iResolved }
		) ) {
		xrtHttpRequestDestroy(pNext);
		xrtFree(sResolved);
		Error = XHTTP_REDIRECT_INVALID;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	xrtFree(sResolved);
	pNewUrl = xrtHttpRequestUrl(pNext);
	if ( xrtUrlSecure(pOldUrl) &&
		!xrtUrlSecure(pNewUrl) &&
		((iFlags &
		  XHTTP_REDIRECT_ALLOW_DOWNGRADE) == 0) ) {
		xrtHttpRequestDestroy(pNext);
		Error = XHTTP_REDIRECT_DOWNGRADE;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}

	Method = xrtHttpRequestMethod(pRequest);
	bHead = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("HEAD")
	);
	bPost = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("POST")
	);
	bDropBody = (iStatus == 303) ||
		(((iStatus == 301) || (iStatus == 302)) &&
		 bPost &&
		 ((iFlags &
		   XHTTP_REDIRECT_POST_TO_GET) != 0));
	pBody = xrtHttpRequestBody(pNext);
	if ( !bDropBody && (pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody) ) {
		xrtHttpRequestDestroy(pNext);
		Error = XHTTP_REDIRECT_REPLAY;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	if ( bDropBody ) {
		if ( !bHead &&
			!xrtHttpRequestSetMethod(
				pNext,
				XRT_STR_LITERAL("GET")
			) ) {
			xrtHttpRequestDestroy(pNext);
			Error = XHTTP_REDIRECT_INVALID;
			memcpy(pError, &Error, sizeof(Error));
			return NULL;
		}
		if ( !__xrtHttpRedirectDropBody(pNext) ) {
			xrtHttpRequestDestroy(pNext);
			Error = XHTTP_REDIRECT_INVALID;
			memcpy(pError, &Error, sizeof(Error));
			return NULL;
		}
	}

	(void)xrtHttpRequestRemoveHeader(
		pNext,
		XRT_STR_LITERAL("Host")
	);
	bCrossOrigin = !__xrtHttpRedirectSameOrigin(
		pOldUrl,
		pNewUrl
	);
	if ( bCrossOrigin &&
		((iFlags &
		  XHTTP_REDIRECT_FORWARD_CREDENTIALS) == 0) ) {
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Authorization")
		);
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Proxy-Authorization")
		);
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Cookie")
		);
	}
	Error = XHTTP_REDIRECT_OK;
	memcpy(pError, &Error, sizeof(Error));
	return pNext;
}



#endif
