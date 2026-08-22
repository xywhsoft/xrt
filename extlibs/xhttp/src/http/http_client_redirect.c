#include "../internal/xrt_http_client_runtime.h"
#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

#define XHTTP_REDIRECT_FLAGS_MASK \
	(XHTTP_REDIRECT_POST_TO_GET | \
	 XHTTP_REDIRECT_FORWARD_CREDENTIALS | \
	 XHTTP_REDIRECT_ALLOW_DOWNGRADE)



/* 初始化通用客户端使用的安全重定向策略。 */
XRT_API void xrtHttpRedirectConfigInit(
	xhttpredirectconfig* pConfig
)
{
	const xhttpredirectconfig Config = {
		XHTTP_REDIRECT_POST_TO_GET,
		XHTTP_REDIRECT_MAX_DEFAULT
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
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
static bool __xrtHttpRedirectStatus(uint16 iStatus)
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
	if ( !__xrtUrlPortValue(pLeft, &iLeftPort) ||
		!__xrtUrlPortValue(pRight, &iRightPort) ) {
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
		__xrtErrorSetSizeOverflow();
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
		__xrtHttpRequestClearTrailers(pRequest);
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



/* 计算本次调用实际使用的重定向模式和上限。 */
static xhttpredirectmode __xrtHttpRedirectMode(
	const xhttpcall* pCall,
	uint32* pMaxHops
)
{
	xhttpredirectmode Mode = pCall->RedirectMode;

	*pMaxHops = pCall->RedirectConfig.MaxHops;
	if ( Mode == XHTTP_REDIRECT_DEFAULT ) {
		Mode = *pMaxHops == 0 ?
			XHTTP_REDIRECT_MANUAL :
			XHTTP_REDIRECT_FOLLOW;
	} else if ( (Mode == XHTTP_REDIRECT_FOLLOW) &&
		(*pMaxHops == 0) ) {
		*pMaxHops = XHTTP_REDIRECT_MAX_DEFAULT;
	}
	return Mode;
}



/* 保存第一个重定向策略错误，供低层 Call 完成入口映射。 */
static bool __xrtHttpRedirectReject(
	xhttpcall* pCall,
	xhttpclienterror Error
)
{
	pCall->RedirectError = Error;
	return false;
}



/* 构造下一跳请求，但直到当前响应排空后才提交。 */
static bool __xrtHttpRedirectBuild(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	const xhttpfield* pLocation
)
{
	const xurl* pOldUrl;
	const xurl* pNewUrl;
	xhttprequest* pNext;
	xhttpbody* pBody;
	xstrview Method;
	str sResolved;
	size_t iResolved;
	uint16 iStatus;
	bool bDropBody;
	bool bHead;
	bool bPost;
	bool bCrossOrigin;

	pOldUrl = xrtHttpRequestUrl(pCall->Request);
	sResolved = __xrtHttpRedirectResolve(
		pOldUrl,
		pLocation->Value,
		&iResolved
	);
	if ( sResolved == NULL ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	pNext = xrtHttpRequestClone(pCall->Request);
	if ( (pNext == NULL) ||
		!xrtHttpRequestSetUrl(
			pNext,
			(xstrview){ sResolved, iResolved }
		) ) {
		xrtHttpRequestDestroy(pNext);
		xrtFree(sResolved);
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	xrtFree(sResolved);
	pNewUrl = xrtHttpRequestUrl(pNext);
	if ( xrtUrlSecure(pOldUrl) &&
		!xrtUrlSecure(pNewUrl) &&
		((pCall->RedirectConfig.Flags &
		  XHTTP_REDIRECT_ALLOW_DOWNGRADE) == 0) ) {
		xrtHttpRequestDestroy(pNext);
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE
		);
	}

	Method = xrtHttpRequestMethod(pCall->Request);
	bHead = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("HEAD")
	);
	bPost = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("POST")
	);
	iStatus = xrtHttpResponseStatus(pResponse);
	bDropBody = (iStatus == 303) ||
		(((iStatus == 301) || (iStatus == 302)) &&
		 bPost &&
		 ((pCall->RedirectConfig.Flags &
		   XHTTP_REDIRECT_POST_TO_GET) != 0));
	pBody = xrtHttpRequestBody(pNext);
	if ( !bDropBody && (pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody) ) {
		xrtHttpRequestDestroy(pNext);
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT_REPLAY
		);
	}
	if ( bDropBody ) {
		if ( !bHead &&
			!xrtHttpRequestSetMethod(
				pNext,
				XRT_STR_LITERAL("GET")
			) ) {
			xrtHttpRequestDestroy(pNext);
			return __xrtHttpRedirectReject(
				pCall,
				XHTTP_CLIENT_ERROR_REDIRECT
			);
		}
		if ( !__xrtHttpRedirectDropBody(pNext) ) {
			xrtHttpRequestDestroy(pNext);
			return __xrtHttpRedirectReject(
				pCall,
				XHTTP_CLIENT_ERROR_REDIRECT
			);
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
		((pCall->RedirectConfig.Flags &
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
	xrtHttpRequestDestroy(pCall->RedirectRequest);
	pCall->RedirectRequest = pNext;
	pCall->RedirectPending = true;
	return true;
}



/* 释放一次 Call 尚未提交的重定向请求。 */
void __xrtHttpRedirectUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	xrtHttpRequestDestroy(pCall->RedirectRequest);
	pCall->RedirectRequest = NULL;
	pCall->RedirectPending = false;
}



/* 信息响应保持低层调用时序，不参与最终响应隐藏规则。 */
static bool __xrtHttpRedirectInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RedirectNext.Informational == NULL ) {
		return true;
	}
	return pCall->RedirectNext.Informational(
		pResponse,
		pCall->RedirectNext.Data
	);
}



/* 在正文到达前决定当前响应是最终结果还是待排空中间跳。 */
static bool __xrtHttpRedirectHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	const xhttpheaders* pHeaders;
	const xhttpfield* pLocation;
	xhttpredirectmode Mode;
	xhttpnext Next;
	uint32 iMaxHops;

	__xrtHttpRedirectUnit(pCall);
	pCall->RedirectError = 0;
	if ( !__xrtHttpRedirectStatus(
		xrtHttpResponseStatus(pResponse)
	) ) {
		goto deliver;
	}
	pLocation = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Location")
	);
	if ( pLocation == NULL ) {
		goto deliver;
	}
	Mode = __xrtHttpRedirectMode(
		pCall,
		&iMaxHops
	);
	if ( Mode == XHTTP_REDIRECT_MANUAL ) {
		goto deliver;
	}
	if ( Mode == XHTTP_REDIRECT_ERROR ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	Next = xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("Location"),
		&pLocation
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	if ( pCall->Redirects >= iMaxHops ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT_LIMIT
		);
	}
	return __xrtHttpRedirectBuild(
		pCall,
		pResponse,
		pLocation
	);

deliver:
	if ( pCall->RedirectNext.Headers == NULL ) {
		return true;
	}
	return pCall->RedirectNext.Headers(
		pResponse,
		pCall->RedirectNext.Data
	);
}



/* 中间跳正文只为连接复用而排空，最终响应仍按调用方选择流式交付。 */
static bool __xrtHttpRedirectBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RedirectPending ) {
		return true;
	}
	if ( pCall->RedirectNext.Body == NULL ) {
		return __xrtHttpResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->RedirectNext.Body(
		pResponse,
		Data,
		pCall->RedirectNext.Data
	);
}



/* 建立调用拥有的请求快照和稳定策略副本。 */
bool __xrtHttpRedirectInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	if ( (pCall == NULL) || (pCall->Request == NULL) ||
		(pOptions == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pCall->Client->Config.Redirect.Flags &
		~XHTTP_REDIRECT_FLAGS_MASK) != 0 ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pOptions->Redirect < XHTTP_REDIRECT_DEFAULT) ||
		(pOptions->Redirect > XHTTP_REDIRECT_ERROR) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCall->RedirectConfig =
		pCall->Client->Config.Redirect;
	pCall->RedirectMode = pOptions->Redirect;
	return true;
}



/* 返回带当前 Call 数据的事件包装器。 */
const xhttp1exchangeevents* __xrtHttpRedirectEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	pCall->RedirectNext = *pNext;
	memset(
		&pCall->RedirectEvents,
		0,
		sizeof(pCall->RedirectEvents)
	);
	pCall->RedirectEvents.Informational =
		__xrtHttpRedirectInformational;
	pCall->RedirectEvents.Headers =
		__xrtHttpRedirectHeaders;
	pCall->RedirectEvents.Body =
		__xrtHttpRedirectBody;
	pCall->RedirectEvents.Data = pCall;
	return &pCall->RedirectEvents;
}



/* 把包装器拒绝映射成稳定的高层重定向错误。 */
bool __xrtHttpRedirectFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xhttpclienterror Error = pCall->RedirectError;
	xerrkind Kind;
	cstr sOperation;
	cstr sMessage;

	if ( Error == 0 ) {
		return false;
	}
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY : XERR_PROTOCOL;
	sOperation = "follow-http-redirect";
	sMessage = "HTTP redirect is invalid";
	if ( Error == XHTTP_CLIENT_ERROR_REDIRECT_LIMIT ) {
		Kind = XERR_RANGE;
		sMessage = "HTTP redirect limit was reached";
	} else if (
		Error == XHTTP_CLIENT_ERROR_REDIRECT_REPLAY
	) {
		Kind = XERR_STATE;
		sMessage =
			"HTTP redirect requires a replayable request body";
	} else if (
		Error == XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE
	) {
		Kind = XERR_PERMISSION;
		sMessage =
			"HTTPS redirect to HTTP is disabled";
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		Error,
		Kind,
		sOperation,
		sMessage,
		pCause
	);
	return true;
}



/* 提交下一跳请求并建立新的低层 Exchange。 */
bool __xrtHttpRedirectAdvance(xhttpcall* pCall)
{
	xhttprequest* pOld;

	if ( (pCall == NULL) ||
		!pCall->RedirectPending ||
		(pCall->RedirectRequest == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pOld = pCall->Request;
	pCall->Request = pCall->RedirectRequest;
	pCall->RedirectRequest = NULL;
	(void)xrtSpinUnlock(&pCall->Lock);
	pCall->RedirectPending = false;
	pCall->Redirects++;
	xrtAtomic64Store(
		&pCall->Info.Redirects,
		(uint64)pCall->Redirects,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtAtomic64FetchAdd(
			&pCall->Client->RedirectsFollowed,
			1,
			XMEMORY_RELAXED
		);
	#endif
	xrtHttpRequestDestroy(pOld);
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			if ( pCall->CookieError != 0 ) {
				return false;
			}
		#endif
		pCall->RedirectError =
			XHTTP_CLIENT_ERROR_REDIRECT;
		return false;
	}
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_QUEUED,
		XMEMORY_RELEASE
	);
	return true;
}

#endif

