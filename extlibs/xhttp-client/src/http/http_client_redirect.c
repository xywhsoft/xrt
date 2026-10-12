#include "../internal/xrt_http_client_runtime.h"
#include "../internal/xrt_url.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

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



/* 调用无 IO 的重定向策略，并保留待排空响应的 Call 状态。 */
static bool __xrtHttpRedirectBuild(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	const xhttpfield* pLocation
)
{
	xhttpredirecterror Error;
	xhttprequest* pNext = xrtHttpRedirectRequest(
		pCall->Request, xrtHttpResponseStatus(pResponse),
		pLocation->Value, pCall->RedirectConfig.Flags, &Error
	);
	if ( pNext == NULL ) {
		return __xrtHttpRedirectReject(pCall,
			Error == XHTTP_REDIRECT_DOWNGRADE ? XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE :
			Error == XHTTP_REDIRECT_REPLAY ? XHTTP_CLIENT_ERROR_REDIRECT_REPLAY :
			XHTTP_CLIENT_ERROR_REDIRECT);
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
	if ( !xrtHttpRedirectStatus(
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
		return xrtHttpBridgeResponseBufferDeliveredBody(
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
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pCall->Client->Config.Redirect.Flags &
		~(XHTTP_REDIRECT_POST_TO_GET | XHTTP_REDIRECT_FORWARD_CREDENTIALS | XHTTP_REDIRECT_ALLOW_DOWNGRADE)) != 0 ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pOptions->Redirect < XHTTP_REDIRECT_DEFAULT) ||
		(pOptions->Redirect > XHTTP_REDIRECT_ERROR) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
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
		xrtSetErrorKind(XERR_ARGUMENT);
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
