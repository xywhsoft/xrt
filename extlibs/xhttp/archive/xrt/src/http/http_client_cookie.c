#include "../internal/xrt_http_client_runtime.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_COOKIES)

#define XRT_HTTP_COOKIE_FLAGS \
	(XHTTP_COOKIE_DISABLED | XHTTP_COOKIE_SAME_SITE | \
	 XHTTP_COOKIE_TOP_LEVEL)



/* 初始化普通同站 HTTP API 调用的 Cookie 策略。 */
XRT_API void xrtHttpCookieOptionsInit(
	xhttpcookieoptions* pOptions
)
{
	const xhttpcookieoptions Options = {
		XHTTP_COOKIE_SAME_SITE,
		{ NULL, 0 }
	};

	if ( !__xrtRangeValid(pOptions, sizeof(Options)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 复制可跨异步和重定向阶段使用的分区键。 */
static bool __xrtHttpCookiePartitionCopy(
	xhttpcall* pCall,
	xstrview PartitionKey
)
{
	if ( (PartitionKey.Data == NULL) &&
		(PartitionKey.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( PartitionKey.Size == 0 ) {
		return true;
	}
	if ( PartitionKey.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	pCall->CookiePartitionKey =
		(str)xrtMalloc(PartitionKey.Size + 1);
	if ( pCall->CookiePartitionKey == NULL ) {
		return false;
	}
	memcpy(
		pCall->CookiePartitionKey,
		PartitionKey.Data,
		PartitionKey.Size
	);
	pCall->CookiePartitionKey[PartitionKey.Size] = '\0';
	pCall->CookiePartitionSize = PartitionKey.Size;
	return true;
}



/* 复制 Call 策略；没有配置 Jar 时保持零成本禁用状态。 */
bool __xrtHttpCookieInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	if ( (pCall == NULL) || (pOptions == NULL) ||
		((pOptions->Cookies.Flags &
		  ~XRT_HTTP_COOKIE_FLAGS) != 0) ) {
		__xrtErrorSetInvalidArgument();
		if ( pCall != NULL ) {
			pCall->CookieError =
				XHTTP_CLIENT_ERROR_COOKIE;
		}
		return false;
	}
	pCall->CookieFlags = pOptions->Cookies.Flags;
	if ( (pCall->Client->Cookies == NULL) ||
		((pCall->CookieFlags &
		  XHTTP_COOKIE_DISABLED) != 0) ) {
		return true;
	}
	if ( !__xrtHttpCookiePartitionCopy(
		pCall,
		pOptions->Cookies.PartitionKey
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	pCall->CookiesEnabled = true;
	return true;
}



/* 释放 Call 拥有的 Cookie 上下文。 */
void __xrtHttpCookieUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	xrtFree(pCall->CookiePartitionKey);
	pCall->CookiePartitionKey = NULL;
	pCall->CookiePartitionSize = 0;
	pCall->CookieAutomatic = false;
}



/* 为当前跳构造精确的 Cookie 请求上下文。 */
static void __xrtHttpCookieRequestContext(
	const xhttpcall* pCall,
	xcookierequestcontext* pContext
)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Flags = XCOOKIE_REQUEST_HTTP_API;
	if ( (pCall->CookieFlags &
		XHTTP_COOKIE_SAME_SITE) != 0 ) {
		pContext->Flags |= XCOOKIE_REQUEST_SAME_SITE;
	}
	if ( (pCall->CookieFlags &
		XHTTP_COOKIE_TOP_LEVEL) != 0 ) {
		pContext->Flags |= XCOOKIE_REQUEST_TOP_LEVEL;
	}
	if ( xrtHttpMethodSafe(
		xrtHttpRequestMethod(pCall->Request)
	) ) {
		pContext->Flags |= XCOOKIE_REQUEST_SAFE_METHOD;
	}
	pContext->URL = xrtHttpRequestUrlText(
		pCall->Request
	);
	pContext->PartitionKey = (xstrview){
		pCall->CookiePartitionKey,
		pCall->CookiePartitionSize
	};
}



/*
	显式 Cookie 由调用方完全控制。
	自动字段在每一跳先删除再按新 URL、方法和 Jar 状态重新生成。
*/
bool __xrtHttpCookiePrepare(xhttpcall* pCall)
{
	xcookierequestcontext Context;
	xhttpheaders* pHeaders;

	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCall->CookieError = 0;
	if ( !pCall->CookiesEnabled ) {
		return true;
	}
	pHeaders = xrtHttpRequestHeaders(pCall->Request);
	if ( pHeaders == NULL ) {
		__xrtErrorSetInvalidArgument();
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	if ( pCall->CookieAutomatic ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Cookie")
		);
		pCall->CookieAutomatic = false;
	}
	if ( xrtHttpHeadersHas(
		pHeaders,
		XRT_STR_LITERAL("Cookie")
	) ) {
		return true;
	}
	__xrtHttpCookieRequestContext(pCall, &Context);
	if ( !xrtCookieJarApply(
		pCall->Client->Cookies,
		&Context,
		pHeaders
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	pCall->CookieAutomatic = true;
	return true;
}



/* 把当前 Jar 错误包装成同步提交阶段的客户端错误。 */
void __xrtHttpCookieSetSubmitError(xhttpcall* pCall)
{
	xerror* pCause;
	xerror* pError;
	xerrkind Kind;

	if ( (pCall == NULL) ||
		(pCall->CookieError == 0) ) {
		return;
	}
	pCause = xrtTakeError();
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY :
			(pCause != NULL ?
				xrtErrorKind(pCause) : XERR_PROTOCOL);
	pError = __xrtHttpClientErrorCreate(
		Kind,
		XHTTP_CLIENT_ERROR_COOKIE,
		"prepare-http-cookies",
		"HTTP Cookie selection failed",
		pCause
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	xrtErrorFree(pCause);
}



/* 信息响应不修改 Jar，只保持下层回调时序。 */
static bool __xrtHttpCookieInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CookieNext.Informational == NULL ) {
		return true;
	}
	return pCall->CookieNext.Informational(
		pResponse,
		pCall->CookieNext.Data
	);
}



/* 在任何最终响应交给重定向或调用方前接收独立 Set-Cookie 字段。 */
static bool __xrtHttpCookieHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xcookiestorecontext Context;

	pCall->CookieError = 0;
	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API;
	if ( (pCall->CookieFlags & XHTTP_COOKIE_SAME_SITE) != 0 ) {
		Context.Flags |= XCOOKIE_STORE_SAME_SITE;
	}
	if ( (pCall->CookieFlags & XHTTP_COOKIE_TOP_LEVEL) != 0 ) {
		Context.Flags |= XCOOKIE_STORE_TOP_LEVEL;
	}
	Context.URL = xrtHttpRequestUrlText(
		pCall->Request
	);
	Context.PartitionKey = (xstrview){
		pCall->CookiePartitionKey,
		pCall->CookiePartitionSize
	};
	if ( !xrtCookieJarStoreHeaders(
		pCall->Client->Cookies,
		&Context,
		xrtHttpResponseHeaders(pResponse),
		NULL
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	if ( pCall->CookieNext.Headers == NULL ) {
		return true;
	}
	return pCall->CookieNext.Headers(
		pResponse,
		pCall->CookieNext.Data
	);
}



/* 保持原始缓冲或流式正文语义。 */
static bool __xrtHttpCookieBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CookieNext.Body == NULL ) {
		return __xrtHttpResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->CookieNext.Body(
		pResponse,
		Data,
		pCall->CookieNext.Data
	);
}



/* 构造一层只拥有事件副本、不拥有用户回调数据的 Cookie 包装器。 */
const xhttp1exchangeevents* __xrtHttpCookieEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->CookiesEnabled ) {
		return pNext;
	}
	pCall->CookieNext = *pNext;
	memset(
		&pCall->CookieEvents,
		0,
		sizeof(pCall->CookieEvents)
	);
	pCall->CookieEvents.Informational =
		__xrtHttpCookieInformational;
	pCall->CookieEvents.Headers =
		__xrtHttpCookieHeaders;
	pCall->CookieEvents.Body =
		__xrtHttpCookieBody;
	pCall->CookieEvents.Data = pCall;
	return &pCall->CookieEvents;
}



/* 把 Cookie 包装器拒绝映射成稳定的高层错误。 */
bool __xrtHttpCookieFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xerrkind Kind;

	if ( (pCall == NULL) ||
		(pCall->CookieError == 0) ) {
		return false;
	}
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY :
			(pCause != NULL ?
				xrtErrorKind(pCause) : XERR_PROTOCOL);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_COOKIE,
		Kind,
		"process-http-cookies",
		"HTTP Cookie processing failed",
		pCause
	);
	return true;
}

#endif
