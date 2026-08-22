#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

#define XRT_HTTP_SERVER_DEFAULT_BODY (UINT64_C(4) * 1024u * 1024u)



/* 返回原因链当前最内层可见类别或保守默认值。 */
static xerrkind __xrtHttp1ServerExchangeCauseKind(
	xerrkind Fallback,
	const xerror* pCause
)
{
	const xerror* pCurrent = pCause;
	xerrkind Kind = Fallback;

	while ( pCurrent != NULL ) {
		xerrkind Current = xrtErrorKind(pCurrent);

		if ( Current != XERR_NONE ) {
			Kind = Current;
		}
		pCurrent = xrtErrorCause(pCurrent);
	}
	return Kind;
}



/* 验证 Server Exchange 限额在创建后不会产生无效协议状态。 */
bool __xrtHttp1ServerConfigValid(
	const xhttp1serverconfig* pConfig
)
{
	if ( (pConfig->Head.MaxHead < 4) ||
		(pConfig->Head.MaxStartLine == 0) ||
		(pConfig->Head.MaxFieldLine == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 建立 Server Exchange 错误并固定失败终态。 */
bool __xrtHttp1ServerExchangeFailCause(
	xhttp1serverexchange* pExchange,
	xhttp1servererror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pCauseRef;
	xerror* pError;
	xerrordesc Desc;

	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return false;
	}
	pCauseRef = xrtErrorRef(pCause);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = __xrtHttp1ServerExchangeCauseKind(
		Kind, pCauseRef
	);
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.server.exchange";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCauseRef;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCauseRef);
	if ( pError == NULL ) {
		pError = xrtTakeError();
	}
	pExchange->Error = pError;
	pExchange->State = XRT_HTTP_SERVER_STATE_FAILED;
	pExchange->Paused = false;
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
	}
	return false;
}



/* 建立没有额外原因链的 Server Exchange 错误。 */
bool __xrtHttp1ServerExchangeFail(
	xhttp1serverexchange* pExchange,
	xhttp1servererror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	return __xrtHttp1ServerExchangeFailCause(
		pExchange,
		Code,
		Kind,
		sOperation,
		sMessage,
		NULL
	);
}



/* 累计当前请求已经接受的线缆字节。 */
bool __xrtHttp1ServerExchangeWireAdd(
	xhttp1serverexchange* pExchange,
	size_t iBytes
)
{
	if ( pExchange->WireBytes >
		(UINT64_MAX - (uint64)iBytes) ) {
		return __xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_STATE,
			XERR_RANGE,
			"count-http1-server-input",
			"HTTP server request wire byte count overflowed"
		);
	}
	pExchange->WireBytes += (uint64)iBytes;
	return true;
}



/* 完成请求并同步发布 Complete 事件。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFinish(
	xhttp1serverexchange* pExchange
)
{
	bool bAccepted = true;

	__xrtHttpServerRequestSetFlags(
		pExchange->Request,
		XHTTP_SERVER_REQUEST_COMPLETE
	);
	pExchange->State = XRT_HTTP_SERVER_STATE_COMPLETE;
	pExchange->Paused = false;
	if ( pExchange->Events.Complete != NULL ) {
		xrtClearError();
		pExchange->InCallback = true;
		bAccepted = pExchange->Events.Complete(
			pExchange,
			pExchange->Request,
			pExchange->Events.Data
		);
		pExchange->InCallback = false;
	}
	if ( pExchange->State == XRT_HTTP_SERVER_STATE_FAILED ) {
		return XHTTP1_SERVER_FEED_ERROR;
	}
	if ( !bAccepted ) {
		(void)__xrtHttp1ServerExchangeFailCause(
			pExchange,
			XHTTP1_SERVER_ERROR_COMPLETE_CALLBACK,
			XERR_CANCELLED,
			"complete-http1-server-request",
			"HTTP server request completion callback failed",
			xrtGetError()
		);
		return XHTTP1_SERVER_FEED_ERROR;
	}
	return XHTTP1_SERVER_FEED_COMPLETE;
}



/* 初始化公开网络服务默认限额。 */
XRT_API void xrtHttp1ServerConfigInit(
	xhttp1serverconfig* pConfig
)
{
	xhttp1serverconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttp1LimitsInit(&Config.Head);
	xrtHttp1BodyLimitsInit(&Config.Body);
	Config.Body.MaxBody = XRT_HTTP_SERVER_DEFAULT_BODY;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建无 I/O Server Exchange。 */
XRT_API xhttp1serverexchange* xrtHttp1ServerExchangeCreate(
	const xhttp1serverconfig* pConfig,
	const xhttp1serverevents* pEvents
)
{
	xhttp1serverconfig Config;
	xhttp1serverevents Events;
	xhttp1serverexchange* pExchange;

	xrtHttp1ServerConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !__xrtRangeValid(pEvents, sizeof(Events)) ) {
			__xrtErrorSetInvalidArgument();
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	if ( !__xrtHttp1ServerConfigValid(&Config) ) {
		return NULL;
	}
	pExchange = (xhttp1serverexchange*)xrtCalloc(
		1, sizeof(*pExchange)
	);
	if ( pExchange == NULL ) {
		return NULL;
	}
	pExchange->Config = Config;
	pExchange->Events = Events;
	pExchange->BodyLimit = Config.Body.MaxBody;
	pExchange->State = XRT_HTTP_SERVER_STATE_HEAD;
	return pExchange;
}



/* 销毁 Server Exchange 的请求和全部临时状态。 */
XRT_API void xrtHttp1ServerExchangeDestroy(
	xhttp1serverexchange* pExchange
)
{
	if ( pExchange == NULL ) {
		return;
	}
	xrtHttpServerRequestDestroy(pExchange->Request);
	xrtErrorFree(pExchange->Error);
	xrtFree(pExchange->ParseTrailers);
	xrtFree(pExchange->ParseFields);
	xrtFree(pExchange->HeadBuffer.Data);
	memset(pExchange, 0, sizeof(*pExchange));
	xrtFree(pExchange);
}



/* 暂停尚未完成的当前请求正文。 */
XRT_API bool xrtHttp1ServerExchangePause(
	xhttp1serverexchange* pExchange
)
{
	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pExchange->State != XRT_HTTP_SERVER_STATE_BODY) ||
		(xrtHttp1BodyDone(&pExchange->Body) &&
		 !pExchange->InCallback) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pExchange->Paused = true;
	return true;
}



/* 恢复已经暂停的正文状态机。 */
XRT_API bool xrtHttp1ServerExchangeResume(
	xhttp1serverexchange* pExchange
)
{
	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pExchange->State != XRT_HTTP_SERVER_STATE_BODY) ||
		!pExchange->Paused ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pExchange->Paused = false;
	return true;
}



/* 在第一段正文交付前替换当前请求限额。 */
XRT_API bool xrtHttp1ServerExchangeSetBodyLimit(
	xhttp1serverexchange* pExchange,
	uint64 iMaxBody
)
{
	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pExchange->State != XRT_HTTP_SERVER_STATE_BODY) ||
		pExchange->BodyStarted ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pExchange->BodyLimit = iMaxBody;
	pExchange->Body.Limits.MaxBody = iMaxBody;
	if ( (pExchange->Body.Mode == XHTTP1_BODY_FIXED) &&
		(pExchange->Body.Remaining > iMaxBody) ) {
		return __xrtHttp1ServerExchangeFail(
			pExchange,
			XHTTP1_SERVER_ERROR_BODY_LIMIT,
			XERR_RANGE,
			"limit-http1-server-body",
			"HTTP server request body exceeds its selected limit"
		);
	}
	return true;
}



/* 完成响应后释放当前请求并等待下一条 keep-alive Header。 */
XRT_API bool xrtHttp1ServerExchangeNext(
	xhttp1serverexchange* pExchange
)
{
	if ( pExchange == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pExchange->InCallback ||
		(pExchange->State != XRT_HTTP_SERVER_STATE_COMPLETE) ||
		((xrtHttpServerRequestFlags(pExchange->Request) &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) == 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	xrtHttpServerRequestDestroy(pExchange->Request);
	pExchange->Request = NULL;
	xrtFree(pExchange->ParseTrailers);
	pExchange->ParseTrailers = NULL;
	pExchange->ParseTrailerCapacity = 0;
	memset(&pExchange->Body, 0, sizeof(pExchange->Body));
	pExchange->WireBytes = 0;
	pExchange->BodyLimit = pExchange->Config.Body.MaxBody;
	pExchange->Delimiter = 0;
	pExchange->Paused = false;
	pExchange->BodyStarted = false;
	pExchange->State = XRT_HTTP_SERVER_STATE_HEAD;
	return true;
}



/* 返回当前请求。 */
XRT_API const xhttpserverrequest* xrtHttp1ServerExchangeRequest(
	const xhttp1serverexchange* pExchange
)
{
	return pExchange != NULL ? pExchange->Request : NULL;
}



/* 返回稳定终态错误。 */
XRT_API const xerror* xrtHttp1ServerExchangeError(
	const xhttp1serverexchange* pExchange
)
{
	return pExchange != NULL ? pExchange->Error : NULL;
}



/* 判断当前请求是否完整。 */
XRT_API bool xrtHttp1ServerExchangeComplete(
	const xhttp1serverexchange* pExchange
)
{
	return (pExchange != NULL) &&
		(pExchange->State == XRT_HTTP_SERVER_STATE_COMPLETE);
}



/* 判断正文消费是否暂停。 */
XRT_API bool xrtHttp1ServerExchangePaused(
	const xhttp1serverexchange* pExchange
)
{
	return (pExchange != NULL) && pExchange->Paused;
}



/* 返回当前请求已接受线缆字节数。 */
XRT_API uint64 xrtHttp1ServerExchangeWireBytes(
	const xhttp1serverexchange* pExchange
)
{
	return pExchange != NULL ? pExchange->WireBytes : 0;
}

#endif
