#include "../internal/xrt_http_sse_client.h"



#if defined(XRT_FEATURE_HTTP_SSE_CLIENT)

/* 创建 SSE Client 层错误，并保留明确的下层原因。 */
static xerror* __xrtHttpSseClientErrorCreate(
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	return pCause != NULL ?
		xrtErrorWrap(
			pCause,
			Kind,
			"xrt.http.sse.client",
			(int32)Code,
			sMessage
		) :
		xrtErrorCreate(
			Kind,
			"xrt.http.sse.client",
			(int32)Code,
			sMessage
		);
}



/* 向当前线程发布一个 SSE Client 参数或同步构造错误。 */
static void __xrtHttpSseClientSetError(
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpSseClientErrorCreate(
		Code, Kind, sMessage, pCause
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



#if defined(XRT_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XRT_FEATURE_HTTP_CLIENT_CACHE)

/* 深复制一个可空字符串视图，并保持空视图规范化。 */
static str __xrtHttpSseClientString(xstrview Value)
{
	str sValue;

	if ( !__xrtRangeValid(Value.Data, Value.Size) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Value.Size == 0 ) {
		return NULL;
	}
	if ( Value.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sValue = (str)xrtMalloc(Value.Size + 1u);
	if ( sValue == NULL ) {
		return NULL;
	}
	memcpy(sValue, Value.Data, Value.Size);
	sValue[Value.Size] = '\0';
	return sValue;
}

#endif



/* 初始化 EventSource 兼容 Parser、无调用超时和自动重连策略。 */
XRT_API void xrtHttpSseClientConfigInit(
	xhttpsseclientconfig* pConfig
)
{
	xhttpsseclientconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpSseParserConfigInit(&Config.Parser);
	xrtHttpCallOptionsInit(&Config.Http);
	Config.Http.Timeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Config.Http.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Config.Http.ResponseBodyLimit = UINT64_MAX;
	Config.Http.Request.TargetForm = XHTTP1_TARGET_ORIGIN;
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		Config.Http.Redirect = XHTTP_REDIRECT_FOLLOW;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_RETRY)
		Config.Http.Retry.Mode = XHTTP_RETRY_DISABLED;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)
		Config.Http.Cache.Mode = XHTTP_CLIENT_CACHE_DISABLED;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_DECOMPRESS)
		Config.Http.Decompress = XHTTP_DECOMPRESS_AUTO;
	#endif
	Config.MaxReconnects = XHTTP_SSE_RECONNECT_MAX_DEFAULT;
	Config.RetryMin = XHTTP_SSE_RETRY_MIN_DEFAULT;
	Config.RetryMax = XHTTP_SSE_RETRY_MAX_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证只由 SSE Client 管理的回调、target 和重连范围。 */
static bool __xrtHttpSseClientConfigValid(
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	if ( (pConfig == NULL) || (pEvents == NULL) ||
		(pEvents->Message == NULL) ||
		(pConfig->RetryMin > pConfig->RetryMax) ||
		(pConfig->Http.Request.TargetForm !=
		 XHTTP1_TARGET_ORIGIN) ||
		(pConfig->Http.Request.CustomTarget.Size != 0) ||
		(pConfig->Http.Events.Informational != NULL) ||
		(pConfig->Http.Events.Headers != NULL) ||
		(pConfig->Http.Events.Body != NULL) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_VALUE,
			"HTTP SSE client config or callbacks are invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpSseParserConfigValid(&pConfig->Parser) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_VALUE,
			"HTTP SSE parser config is invalid",
			NULL
		);
		return false;
	}
	return true;
}



/* 解析可选配置和必需事件表，允许固定公开值位于未对齐存储。 */
static bool __xrtHttpSseClientInputsResolve(
	const xhttpsseclientconfig* pInput,
	const xhttpsseclientevents* pEventInput,
	xhttpsseclientconfig* pConfig,
	xhttpsseclientevents* pEvents
)
{
	xrtHttpSseClientConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !__xrtRangeValid(pInput, sizeof(*pInput)) ) {
			__xrtHttpSseClientSetError(
				XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
				XERR_ARGUMENT,
				"HTTP SSE client config range is invalid",
				NULL
			);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( !__xrtRangeValid(pEventInput, sizeof(*pEventInput)) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client event range is invalid",
			NULL
		);
		return false;
	}
	memcpy(pEvents, pEventInput, sizeof(*pEvents));
	return __xrtHttpSseClientConfigValid(pConfig, pEvents);
}



/* 释放会话深持有的 HTTP 选项对象与文本。 */
static void __xrtHttpSseClientOptionsUnit(
	xhttpsseclient* pClient
)
{
	xrtCancelDestroy(pClient->Cancel);
	pClient->Cancel = NULL;
	pClient->Config.Http.Cancel = NULL;
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pClient->Proxy);
		pClient->Proxy = NULL;
		pClient->Config.Http.Proxy.Proxy = NULL;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_COOKIES)
		xrtFree(pClient->CookiePartition);
		pClient->CookiePartition = NULL;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)
		xrtFree(pClient->CachePartition);
		pClient->CachePartition = NULL;
	#endif
}



/* 深持有会跨越多次 HTTP Call 使用的取消、代理和分区字段。 */
static bool __xrtHttpSseClientOptionsInit(
	xhttpsseclient* pClient,
	const xhttpsseclientconfig* pConfig
)
{
	pClient->Config = *pConfig;
	pClient->Config.Http.Events.Informational = NULL;
	pClient->Config.Http.Events.Headers = NULL;
	pClient->Config.Http.Events.Body = NULL;
	pClient->Config.Http.Events.Data = NULL;
	pClient->Config.Http.ResponseBodyLimit = UINT64_MAX;
	pClient->Config.Http.Request.TargetForm =
		XHTTP1_TARGET_ORIGIN;
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		pClient->Config.Http.Redirect = XHTTP_REDIRECT_FOLLOW;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_RETRY)
		pClient->Config.Http.Retry.Mode = XHTTP_RETRY_DISABLED;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)
		pClient->Config.Http.Cache.Mode =
			XHTTP_CLIENT_CACHE_DISABLED;
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_DECOMPRESS)
		pClient->Config.Http.Decompress = XHTTP_DECOMPRESS_AUTO;
	#endif
	if ( pConfig->Http.Cancel != NULL ) {
		pClient->Cancel = xrtCancelRef(
			pConfig->Http.Cancel
		);
		if ( pClient->Cancel == NULL ) {
			return false;
		}
		pClient->Config.Http.Cancel = pClient->Cancel;
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		if ( pConfig->Http.Proxy.Proxy != NULL ) {
			pClient->Proxy = xrtNetProxyRetain(
				pConfig->Http.Proxy.Proxy
			);
			if ( pClient->Proxy == NULL ) {
				return false;
			}
			pClient->Config.Http.Proxy.Proxy =
				pClient->Proxy;
		}
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_COOKIES)
		pClient->CookiePartition =
			__xrtHttpSseClientString(
				pConfig->Http.Cookies.PartitionKey
			);
		if ( (pConfig->Http.Cookies.PartitionKey.Size != 0) &&
			(pClient->CookiePartition == NULL) ) {
			return false;
		}
		pClient->Config.Http.Cookies.PartitionKey =
			(xstrview){
				pClient->CookiePartition,
				pConfig->Http.Cookies.PartitionKey.Size
			};
	#endif
	#if defined(XRT_FEATURE_HTTP_CLIENT_CACHE)
		pClient->CachePartition =
			__xrtHttpSseClientString(
				pConfig->Http.Cache.PartitionKey
			);
		if ( (pConfig->Http.Cache.PartitionKey.Size != 0) &&
			(pClient->CachePartition == NULL) ) {
			return false;
		}
		pClient->Config.Http.Cache.PartitionKey =
			(xstrview){
				pClient->CachePartition,
				pConfig->Http.Cache.PartitionKey.Size
			};
	#endif
	return true;
}



/* 验证自定义 EventSource 请求的 GET、无正文和无 fragment 契约。 */
static bool __xrtHttpSseClientRequestValid(
	const xhttprequest* pRequest
)
{
	const xurl* pUrl;

	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE request is null",
			NULL
		);
		return false;
	}
	pUrl = xrtHttpRequestUrl(pRequest);
	if ( !xrtHttpMethodEqual(
		xrtHttpRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) || (xrtHttpRequestBody(pRequest) != NULL) ||
		(pUrl == NULL) ||
		((pUrl->Flags & XURL_HAS_FRAGMENT) != 0) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"HTTP SSE request must be a fragment-free GET without a body",
			NULL
		);
		return false;
	}
	return true;
}



/* 增加 SSE 会话引用并拒绝已经释放的对象。 */
XRT_API xhttpsseclient* xrtHttpSseClientRef(
	xhttpsseclient* pClient
)
{
	if ( (pClient == NULL) ||
		(xrtRefRetain(&pClient->References) < 0) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			pClient == NULL ? XERR_ARGUMENT : XERR_STATE,
			"HTTP SSE client is null or already released",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放最后一个会话引用和全部已经停止的拥有型资源。 */
XRT_API void xrtHttpSseClientDestroy(
	xhttpsseclient* pClient
)
{
	if ( (pClient == NULL) ||
		(xrtRefRelease(&pClient->References) != 0) ) {
		return;
	}
	xrtCancelUnwatch(pClient->CancelWatch);
	__xrtHttpSseClientOptionsUnit(pClient);
	xrtErrorFree(pClient->AttemptError);
	xrtErrorFree(pClient->Error);
	xrtBufferUnit(&pClient->Pending);
	xrtHttpSseParserUnit(&pClient->Parser);
	xrtHttpCallDestroy(pClient->Call);
	xrtHttpRequestDestroy(pClient->Request);
	xrtHttpClientDestroy(pClient->Http);
	__xrtSpinUnit(&pClient->Lock);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}



/* 记录一次 Parser、用户回调或响应分类失败。 */
static bool __xrtHttpSseClientAttemptFail(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( pClient->AttemptError == NULL ) {
		pClient->AttemptError =
			__xrtHttpSseClientErrorCreate(
				Code, Kind, sMessage, pCause
			);
		if ( pClient->AttemptError == NULL ) {
			pClient->AttemptError = xrtErrorRef(
				xrtGetError()
			);
		}
	}
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_FAILED;
	pClient->AttemptReason = Reason;
	if ( pClient->AttemptError != NULL ) {
		xrtSetError(pClient->AttemptError);
	}
	return false;
}



/* 发布一个借用 Parser 缓冲的项目并更新无锁统计。 */
static bool __xrtHttpSseClientItem(
	xhttpsseclient* pClient,
	const xhttpsseitem* pItem
)
{
	bool bAccepted = true;

	xrtClearError();
	if ( pItem->Kind == XHTTP_SSE_ITEM_EVENT ) {
		bAccepted = pClient->Events.Message(
			pClient,
			&pItem->Message,
			pClient->Events.Data
		);
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->Messages,
				1,
				XMEMORY_RELAXED
			);
		}
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_COMMENT ) {
		if ( pClient->Events.Comment != NULL ) {
			bAccepted = pClient->Events.Comment(
				pClient,
				pItem->Comment,
				pClient->Events.Data
			);
		}
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->Comments,
				1,
				XMEMORY_RELAXED
			);
		}
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_RETRY ) {
		if ( pClient->Events.Retry != NULL ) {
			bAccepted = pClient->Events.Retry(
				pClient,
				pItem->Retry,
				pClient->Events.Data
			);
		}
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->RetryUpdates,
				1,
				XMEMORY_RELAXED
			);
		}
	} else {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"HTTP SSE parser returned an unknown item",
			NULL
		);
	}
	if ( !bAccepted ) {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_CALLBACK,
			XHTTP_SSE_CLIENT_ERROR_CALLBACK,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_CANCELLED,
			"HTTP SSE item callback stopped the stream",
			xrtGetError()
		);
	}
	xrtAtomic64Store(
		&pClient->Retry,
		xrtHttpSseParserRetry(&pClient->Parser),
		XMEMORY_RELEASE
	);
	return true;
}



/* 增量解析一个连续输入，并在暂停点复制尚未解析的当前尾段。 */
static bool __xrtHttpSseClientRead(
	xhttpsseclient* pClient,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed
)
{
	size_t iOffset = 0;

	*pConsumed = 0;
	for ( ;; ) {
		xhttpsseerrorinfo Error;
		xhttpsseitem Item;
		xhttpsseparsestatus Status;
		size_t iRead = 0;
		xbytesview Remaining = iOffset == Input.Size ?
			(xbytesview){ NULL, 0 } :
			(xbytesview){
				Input.Data + iOffset,
				Input.Size - iOffset
			};

		Status = xrtHttpSseParserRead(
			&pClient->Parser,
			Remaining,
			bEnd,
			&iRead,
			&Item,
			&Error
		);
		iOffset += iRead;
		if ( Status == XHTTP_SSE_PARSE_ERROR ) {
			*pConsumed = iOffset;
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_PARSE,
				XHTTP_SSE_CLIENT_ERROR_PARSE,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_PROTOCOL,
				"HTTP SSE event stream parsing failed",
				xrtGetError()
			);
		}
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			if ( !__xrtHttpSseClientItem(
				pClient, &Item
			) ) {
				*pConsumed = iOffset;
				return false;
			}
			if ( xrtAtomic32Load(
				&pClient->CloseGate,
				XMEMORY_ACQUIRE
			) || xrtAtomic32Load(
				&pClient->PauseGate,
				XMEMORY_ACQUIRE
			) ) {
				*pConsumed = iOffset;
				return true;
			}
			continue;
		}
		if ( (iRead == 0) &&
			(iOffset < Input.Size) ) {
			*pConsumed = iOffset;
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE parser made no input progress",
				NULL
			);
		}
		*pConsumed = iOffset;
		return true;
	}
}



/* HTTP 正文到达时直接解析，暂停只复制本次输入尚未解析的后缀。 */
static bool __xrtHttpSseClientBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	size_t iConsumed = 0;

	(void)pCall;
	(void)pResponse;
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( (pClient->Attempt != XRT_HTTP_SSE_ATTEMPT_OPEN) ||
		!__xrtHttpSseClientRead(
			pClient, Data, false, &iConsumed
		) ) {
		return false;
	}
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) && (iConsumed < Data.Size) ) {
		if ( !xrtBufferAssign(
			&pClient->Pending,
			(xbytesview){
				Data.Data + iConsumed,
				Data.Size - iConsumed
			}
		) ) {
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"HTTP SSE paused tail could not be retained",
				xrtGetError()
			);
		}
	}
	return true;
}



/* 最终响应 Header 决定本次尝试是打开、停止还是永久拒绝。 */
static bool __xrtHttpSseClientHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseresponse Result = xrtHttpSseResponseCheck(
		xrtHttpResponseStatus(pResponse),
		xrtHttpResponseHeaders(pResponse)
	);

	(void)pCall;
	xrtAtomic32Store(
		&pClient->Status,
		xrtHttpResponseStatus(pResponse),
		XMEMORY_RELEASE
	);
	if ( Result == XHTTP_SSE_RESPONSE_STOP ) {
		pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_STOP;
		return true;
	}
	if ( Result != XHTTP_SSE_RESPONSE_OPEN ) {
		pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_REJECTED;
		pClient->AttemptReason = XHTTP_SSE_CLOSE_REJECTED;
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_REJECTED,
			XHTTP_SSE_CLIENT_ERROR_RESPONSE,
			XERR_PROTOCOL,
			"HTTP response is not an EventSource stream",
			Result == XHTTP_SSE_RESPONSE_ERROR ?
				xrtGetError() : NULL
		);
	}
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_OPEN;
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_OPEN,
		XMEMORY_RELEASE
	);
	if ( pClient->Events.Open != NULL ) {
		bool bAccepted;

		xrtClearError();
		bAccepted = pClient->Events.Open(
			pClient,
			pResponse,
			pClient->Events.Data
		);
		if ( !bAccepted ) {
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_CALLBACK,
				XHTTP_SSE_CLIENT_ERROR_CALLBACK,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_CANCELLED,
				"HTTP SSE open callback stopped the stream",
				xrtGetError()
			);
		}
	}
	return true;
}



/* 原子发布唯一 CLOSED 状态、稳定错误和终态回调。 */
static void __xrtHttpSseClientFinish(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError
)
{
	uint32 iExpected = 0;
	bool bRelease = false;

	if ( !xrtAtomic32CompareExchange(
		&pClient->FinishGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( Reason == XHTTP_SSE_CLOSE_CANCELLED ) {
		pClient->Error = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"HTTP SSE session was cancelled",
			pError
		);
	} else if ( pError != NULL ) {
		pClient->Error = xrtErrorRef(pError);
	}
	if ( (pClient->Error == NULL) &&
		(Reason != XHTTP_SSE_CLOSE_USER) &&
		(Reason != XHTTP_SSE_CLOSE_STOP) ) {
		if ( xrtGetError() == NULL ) {
			if ( Reason == XHTTP_SSE_CLOSE_CANCELLED ) {
				__xrtErrorSetCancelled();
			} else {
				__xrtErrorSetInternal();
			}
		}
		pClient->Error = xrtErrorRef(xrtGetError());
	}
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CLOSED,
		XMEMORY_RELEASE
	);
	if ( pClient->Events.Close != NULL ) {
		pClient->Events.Close(
			pClient,
			Reason,
			pClient->Error,
			pClient->Events.Data
		);
	}
	__xrtSpinLock(&pClient->Lock);
	if ( pClient->RuntimeHeld ) {
		pClient->RuntimeHeld = false;
		bRelease = true;
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( bRelease ) {
		xrtHttpSseClientDestroy(pClient);
	}
}



/* 在 Worker 上排空暂停尾段，并仅在仍活动时恢复 HTTP 读取。 */
static void __xrtHttpSseClientResumeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpcall* pCall = NULL;

	(void)pWorker;
	xrtAtomic32Store(
		&pClient->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		xrtHttpSseClientDestroy(pClient);
		return;
	}
	__xrtSpinLock(&pClient->Lock);
	if ( pClient->Call != NULL ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( pCall == NULL ) {
		xrtHttpSseClientDestroy(pClient);
		return;
	}
	xrtAtomic32Store(
		&pClient->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	while ( pClient->Pending.Size != 0 ) {
		size_t iConsumed = 0;

		if ( !__xrtHttpSseClientRead(
			pClient,
			xrtBufferView(&pClient->Pending),
			false,
			&iConsumed
		) ) {
			(void)xrtHttpCallCancel(pCall);
			break;
		}
		if ( (iConsumed == 0) ||
			!xrtBufferRemove(
				&pClient->Pending,
				0,
				iConsumed
			) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE paused tail made no progress",
				xrtGetError()
			);
			(void)xrtHttpCallCancel(pCall);
			break;
		}
		if ( xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	if ( pClient->Pending.Size == 0 ) {
		(void)xrtBufferTrim(&pClient->Pending);
	}
	if ( (pClient->Attempt != XRT_HTTP_SSE_ATTEMPT_FAILED) &&
		!xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) && !xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) ) {
		if ( !xrtHttpCallResume(pCall) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_STATE,
				"HTTP SSE transport could not resume",
				xrtGetError()
			);
			(void)xrtHttpCallCancel(pCall);
		}
	}
	xrtHttpCallDestroy(pCall);
	xrtHttpSseClientDestroy(pClient);
}



/* 在当前 HTTP Worker 上关闭 Parser 与传输输入门。 */
XRT_API bool xrtHttpSseClientPause(
	xhttpsseclient* pClient
)
{
	xhttpcall* pCall = NULL;
	bool bResult;

	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	__xrtSpinLock(&pClient->Lock);
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) && (pClient->Call != NULL) ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( pCall == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client has no active call to pause",
			NULL
		);
		return false;
	}
	bResult = xrtHttpCallPause(pCall);
	if ( bResult ) {
		xrtAtomic32Store(
			&pClient->PauseGate,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtHttpCallDestroy(pCall);
	return bResult;
}



/* 合并一次跨线程恢复，并投递到当前 Call 的稳定 Worker。 */
XRT_API bool xrtHttpSseClientResume(
	xhttpsseclient* pClient
)
{
	xnetworker* pWorker = NULL;
	uint32 iExpected = 0;
	bool bPosted;

	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	__xrtSpinLock(&pClient->Lock);
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) && xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_RELAXED
	) && (pClient->Call != NULL) &&
		xrtAtomic32CompareExchange(
		&pClient->ResumeGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_RELAXED
	) ) {
		pWorker = xrtHttpCallWorker(pClient->Call);
		if ( (pWorker == NULL) ||
			(xrtHttpSseClientRef(pClient) == NULL) ) {
			pWorker = NULL;
			xrtAtomic32Store(
				&pClient->ResumeGate,
				0,
				XMEMORY_RELEASE
			);
		}
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( pWorker == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client is not paused or resume is already pending",
			NULL
		);
		return false;
	}
	bPosted = xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		__xrtHttpSseClientResumeTask,
		pClient
	);
	if ( !bPosted ) {
		xrtAtomic32Store(
			&pClient->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttpSseClientDestroy(pClient);
	}
	return bPosted;
}



/* 删除 GET EventSource 请求不应携带的旧正文分帧字段。 */
static void __xrtHttpSseClientRequestClean(
	xhttprequest* pRequest
)
{
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Content-Length")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Transfer-Encoding")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Trailer")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Expect")
	);
}



/* 为一次连接尝试克隆请求并应用持久 Last-Event-ID。 */
static xhttprequest* __xrtHttpSseClientAttemptRequest(
	xhttpsseclient* pClient
)
{
	xhttprequest* pRequest = xrtHttpRequestClone(
		pClient->Request
	);

	if ( pRequest == NULL ) {
		return NULL;
	}
	__xrtHttpSseClientRequestClean(pRequest);
	if ( !xrtHttpSseRequestHeaders(
		xrtHttpRequestHeaders(pRequest),
		xrtHttpSseParserLastEventId(&pClient->Parser)
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* HTTP Call 和重连 Timer 回调在后续生命周期实现中定义。 */
static void __xrtHttpSseClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
);

static void __xrtHttpSseClientTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);



/* 启动一条新的 HTTP Call；同步失败不留下半活动句柄。 */
static bool __xrtHttpSseClientStart(
	xhttpsseclient* pClient
)
{
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	xhttpcall* pCall;

	xrtHttpSseParserReconnect(&pClient->Parser);
	xrtBufferClear(&pClient->Pending);
	(void)xrtBufferTrim(&pClient->Pending);
	xrtErrorFree(pClient->AttemptError);
	pClient->AttemptError = NULL;
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_WAITING;
	pClient->AttemptReason = XHTTP_SSE_CLOSE_HTTP;
	xrtAtomic32Store(
		&pClient->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pClient->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	pRequest = __xrtHttpSseClientAttemptRequest(pClient);
	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE reconnect request could not be prepared",
			xrtGetError()
		);
		return false;
	}
	Options = pClient->Config.Http;
	Options.Events.Headers = __xrtHttpSseClientHeaders;
	Options.Events.Body = __xrtHttpSseClientBody;
	Options.Events.Data = pClient;
	__xrtSpinLock(&pClient->Lock);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) || (pClient->Call != NULL) ) {
		__xrtSpinUnlock(&pClient->Lock);
		xrtHttpRequestDestroy(pRequest);
		return false;
	}
	pCall = xrtHttpClientDo(
		pClient->Http,
		pRequest,
		&Options,
		__xrtHttpSseClientDone,
		pClient
	);
	if ( pCall != NULL ) {
		pClient->Call = pCall;
		pClient->Worker = xrtHttpCallWorker(pCall);
		pClient->Engine = xrtNetWorkerEngine(
			pClient->Worker
		);
	}
	__xrtSpinUnlock(&pClient->Lock);
	xrtHttpRequestDestroy(pRequest);
	return pCall != NULL;
}



/* 判断 HTTP 失败是否属于 EventSource 可以重连的暂态网络失败。 */
static bool __xrtHttpSseClientRetryable(
	const xhttpcallresult* pResult
)
{
	if ( (pResult->Result == XNET_RESULT_TIMEOUT) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_DIAL) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_POOL) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_PROXY) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_TRANSPORT) ||
		(pResult->Info.Error ==
		 XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL) ||
		(pResult->Info.Error ==
		 XHTTP_CLIENT_ERROR_TIMEOUT_IDLE) ) {
		return true;
	}
	return false;
}



/* 重连前保留 HTTP 重定向后的最终请求，避免反复访问旧入口。 */
static bool __xrtHttpSseClientRememberRequest(
	xhttpsseclient* pClient,
	xhttpcall* pCall,
	size_t iRedirects
)
{
	xhttprequest* pRequest;

	if ( iRedirects == 0 ) {
		return true;
	}
	pRequest = xrtHttpCallRequestClone(pCall);
	if ( pRequest == NULL ) {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE effective request could not be retained",
			xrtGetError()
		);
	}
	xrtHttpRequestDestroy(pClient->Request);
	pClient->Request = pRequest;
	return true;
}



/* 把服务端毫秒重连值裁剪到本地策略并安全转换为微秒。 */
static uint64 __xrtHttpSseClientDelay(
	const xhttpsseclient* pClient
)
{
	uint64 iDelay = xrtHttpSseParserRetry(
		&pClient->Parser
	);

	if ( iDelay < pClient->Config.RetryMin ) {
		iDelay = pClient->Config.RetryMin;
	}
	if ( iDelay > pClient->Config.RetryMax ) {
		iDelay = pClient->Config.RetryMax;
	}
	return iDelay > (UINT64_MAX / UINT64_C(1000)) ?
		UINT64_MAX : iDelay * UINT64_C(1000);
}



/* 在当前 Call Worker 上安排唯一一次重连 Timer。 */
static bool __xrtHttpSseClientReconnect(
	xhttpsseclient* pClient,
	const xerror* pError
)
{
	xhttpsseclient* pTimerHold;
	uint64 iReconnect = xrtAtomic64Load(
		&pClient->Reconnects,
		XMEMORY_ACQUIRE
	);
	uint64 iDelay;
	uint64 Id;

	if ( iReconnect >= (uint64)pClient->Config.MaxReconnects ) {
		xerror* pLimit = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_RANGE,
			"HTTP SSE reconnect limit was reached",
			pError
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_RECONNECT_LIMIT,
			pLimit
		);
		xrtErrorFree(pLimit);
		return false;
	}
	iDelay = __xrtHttpSseClientDelay(pClient);
	pTimerHold = xrtHttpSseClientRef(pClient);
	if ( pTimerHold == NULL ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			xrtGetError()
		);
		return false;
	}
	__xrtSpinLock(&pClient->Lock);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_RELAXED
	) || (pClient->Engine == NULL) ||
		(pClient->Worker == NULL) ||
		(pClient->Timer != 0) ) {
		Id = 0;
	} else {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_SSE_CLIENT_CONNECTING,
			XMEMORY_RELEASE
		);
		Id = xrtNetEngineAfter(
			pClient->Engine,
			xrtNetWorkerIndex(pClient->Worker),
			iDelay,
			__xrtHttpSseClientTimer,
			pTimerHold
		);
	}
	if ( Id != 0 ) {
		pClient->Timer = Id;
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( Id == 0 ) {
		xrtHttpSseClientDestroy(pTimerHold);
		if ( xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) || xrtAtomic32Load(
			&pClient->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			return false;
		}
		xerror* pTimer = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_IO,
			"HTTP SSE reconnect timer could not be scheduled",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pTimer
		);
		xrtErrorFree(pTimer);
		return false;
	}
	iReconnect = xrtAtomic64FetchAdd(
		&pClient->Reconnects,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	if ( pClient->Events.Retrying != NULL ) {
		pClient->Events.Retrying(
			pClient,
			(size_t)iReconnect,
			iDelay / UINT64_C(1000),
			pError,
			pClient->Events.Data
		);
	}
	return true;
}



/* HTTP Call 终态决定永久关闭、正常重连或暂态网络重连。 */
static void __xrtHttpSseClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseclient* pHold = xrtHttpSseClientRef(pClient);
	xhttpsseclosereason Requested;
	bool bClosed;

	if ( pHold == NULL ) {
		return;
	}
	__xrtSpinLock(&pClient->Lock);
	if ( pClient->Call == pCall ) {
		pClient->Call = NULL;
	}
	__xrtSpinUnlock(&pClient->Lock);
	bClosed = xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) != 0;
	Requested = (xhttpsseclosereason)xrtAtomic32Load(
		&pClient->RequestedReason,
		XMEMORY_ACQUIRE
	);
	if ( bClosed ) {
		__xrtHttpSseClientFinish(
			pClient,
			Requested,
			Requested == XHTTP_SSE_CLOSE_CANCELLED ?
				pResult->Error : NULL
		);
	} else if ( pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_FAILED ) {
		__xrtHttpSseClientFinish(
			pClient,
			pClient->AttemptReason,
			pClient->AttemptError
		);
	} else if ( pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_STOP ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_STOP,
			NULL
		);
	} else if ( (pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_OPEN) &&
		(pResult->Result == XNET_RESULT_OK) ) {
		size_t iConsumed = 0;

		if ( xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) || (pClient->Pending.Size != 0) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE call completed with a paused input tail",
				NULL
			);
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		} else if ( !__xrtHttpSseClientRead(
			pClient,
			(xbytesview){ NULL, 0 },
			true,
			&iConsumed
		) ) {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		} else if ( __xrtHttpSseClientRememberRequest(
			pClient, pCall, pResult->Info.Redirects
		) ) {
			(void)__xrtHttpSseClientReconnect(
				pClient, NULL
			);
		} else {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		}
	} else if ( __xrtHttpSseClientRetryable(pResult) ) {
		if ( __xrtHttpSseClientRememberRequest(
			pClient, pCall, pResult->Info.Redirects
		) ) {
			(void)__xrtHttpSseClientReconnect(
				pClient, pResult->Error
			);
		} else {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		}
	} else if ( pResult->Result == XNET_RESULT_CANCELLED ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_CANCELLED,
			pResult->Error
		);
	} else {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_HTTP,
			pResult->Error != NULL ?
				xrtErrorKind(pResult->Error) : XERR_IO,
			"HTTP SSE connection failed permanently",
			pResult->Error
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_HTTP,
			pError
		);
		xrtErrorFree(pError);
	}
	xrtHttpResponseDestroy(pResult->Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpSseClientDestroy(pHold);
}



/* Timer 到期后在同一 Engine 上开始下一次独立 HTTP Call。 */
static void __xrtHttpSseClientTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseclosereason Requested;

	(void)pWorker;
	__xrtSpinLock(&pClient->Lock);
	if ( pClient->Timer == Id ) {
		pClient->Timer = 0;
	}
	__xrtSpinUnlock(&pClient->Lock);
	Requested = (xhttpsseclosereason)xrtAtomic32Load(
		&pClient->RequestedReason,
		XMEMORY_ACQUIRE
	);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) || (Result == XNET_RESULT_CANCELLED) ) {
		__xrtHttpSseClientFinish(
			pClient,
			Requested,
			NULL
		);
		goto Complete;
	}
	if ( Result != XNET_RESULT_OK ) {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_IO,
			"HTTP SSE reconnect timer failed",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pError
		);
		xrtErrorFree(pError);
		goto Complete;
	}
	if ( !__xrtHttpSseClientStart(pClient) ) {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE reconnect call could not be submitted",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pError
		);
		xrtErrorFree(pError);
	}

Complete:
	xrtHttpSseClientDestroy(pClient);
}



/* 在 Timer 所属 Worker 上无分配地撤销重连并释放命令持有的引用。 */
static void __xrtHttpSseClientCloseTimerTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xnetengine* pEngine = NULL;
	uint64 Id = 0;

	__xrtSpinLock(&pClient->Lock);
	if ( pClient->Worker == pWorker ) {
		pEngine = pClient->Engine;
		Id = pClient->Timer;
	}
	__xrtSpinUnlock(&pClient->Lock);
	if ( (Id != 0) && (pEngine != NULL) ) {
		(void)__xrtNetEngineTimerCancelCurrent(
			pEngine, Id
		);
	}
	if ( !xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpSseClientFinish(
			pClient,
			(xhttpsseclosereason)xrtAtomic32Load(
				&pClient->RequestedReason,
				XMEMORY_ACQUIRE
			),
			NULL
		);
	}
	xrtHttpSseClientDestroy(pClient);
}



/* 关闭当前 Call 或 Timer，并让其所属 Worker 发布终态。 */
static bool __xrtHttpSseClientCloseReason(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason
)
{
	xhttpcall* pCall = NULL;
	xnetworker* pWorker = NULL;
	uint64 Id = 0;
	uint32 iExpected = 0;
	bool bConstructing;

	if ( !xrtAtomic32CompareExchange(
		&pClient->CloseGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pClient->RequestedReason,
		(uint32)Reason,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtSpinLock(&pClient->Lock);
	if ( pClient->Call != NULL ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	Id = pClient->Timer;
	pWorker = pClient->Worker;
	bConstructing = pClient->Constructing;
	__xrtSpinUnlock(&pClient->Lock);
	if ( pCall != NULL ) {
		(void)xrtHttpCallCancel(pCall);
		xrtHttpCallDestroy(pCall);
	} else if ( (Id != 0) && (pWorker != NULL) ) {
		if ( xrtHttpSseClientRef(pClient) != NULL ) {
			__xrtNetEnginePostInternal(
				pWorker,
				&pClient->CloseCommand,
				__xrtHttpSseClientCloseTimerTask,
				pClient
			);
		} else {
			(void)__xrtNetEngineTimerCancelLifecycle(
				xrtNetWorkerEngine(pWorker), Id
			);
		}
	} else if ( !bConstructing ) {
		__xrtHttpSseClientFinish(pClient, Reason, NULL);
	}
	return true;
}



/* 外部取消令牌把整个 EventSource 会话永久关闭。 */
static void __xrtHttpSseClientCancel(ptr pData)
{
	(void)__xrtHttpSseClientCloseReason(
		(xhttpsseclient*)pData,
		XHTTP_SSE_CLOSE_CANCELLED
	);
}



/* 从任意线程主动关闭整个订阅会话。 */
XRT_API bool xrtHttpSseClientClose(
	xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpSseClientCloseReason(
		pClient,
		XHTTP_SSE_CLOSE_USER
	) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client is already closing or closed",
			NULL
		);
		return false;
	}
	return true;
}



/* 构造失败时保留当前线程错误，并回收用户与运行时两份初始引用。 */
static xhttpsseclient* __xrtHttpSseClientCreateFail(
	xhttpsseclient* pClient,
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	if ( pClient != NULL ) {
		__xrtSpinLock(&pClient->Lock);
		pClient->Constructing = false;
		pClient->RuntimeHeld = false;
		__xrtSpinUnlock(&pClient->Lock);
		xrtHttpSseClientDestroy(pClient);
		xrtHttpSseClientDestroy(pClient);
	}
	__xrtHttpSseClientSetError(
		Code,
		Kind,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return NULL;
}



/* 创建拥有请求模板、Parser 和自动重连运行时引用的 SSE 会话。 */
static xhttpsseclient* __xrtHttpSseClientCreate(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseclient* pClient;

	if ( pHttp == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP client is null",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpSseClientInputsResolve(
		pConfig, pEvents, &Config, &Events
	) || !__xrtHttpSseClientRequestValid(pRequest) ) {
		return NULL;
	}
	pClient = (xhttpsseclient*)xrtMalloc(
		sizeof(*pClient)
	);
	if ( pClient == NULL ) {
		return __xrtHttpSseClientCreateFail(
			NULL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_MEMORY,
			"HTTP SSE client could not be allocated"
		);
	}
	memset(pClient, 0, sizeof(*pClient));
	pClient->References = 2;
	pClient->RuntimeHeld = true;
	pClient->Constructing = true;
	pClient->AttemptReason = XHTTP_SSE_CLOSE_HTTP;
	pClient->Events = Events;
	__xrtSpinInit(&pClient->Lock);
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CONNECTING,
		XMEMORY_RELAXED
	);
	if ( !xrtBufferInit(&pClient->Pending) ||
		!xrtHttpSseParserInit(
			&pClient->Parser,
			&Config.Parser
		) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_MEMORY,
			"HTTP SSE client state could not be initialized"
		);
	}
	xrtAtomic64Store(
		&pClient->Retry,
		xrtHttpSseParserRetry(&pClient->Parser),
		XMEMORY_RELAXED
	);
	pClient->Http = xrtHttpClientRef(pHttp);
	if ( pClient->Http == NULL ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_STATE,
			"HTTP client could not be retained"
		);
	}
	pClient->Request = xrtHttpRequestClone(pRequest);
	if ( pClient->Request == NULL ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"HTTP SSE request could not be retained"
		);
	}
	if ( !__xrtHttpSseClientOptionsInit(
		pClient, &Config
	) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_MEMORY,
			"HTTP SSE call options could not be retained"
		);
	}
	if ( pClient->Cancel != NULL ) {
		pClient->CancelWatch = xrtCancelWatch(
			pClient->Cancel,
			__xrtHttpSseClientCancel,
			pClient
		);
		if ( pClient->CancelWatch == NULL ) {
			return __xrtHttpSseClientCreateFail(
				pClient,
				XHTTP_SSE_CLIENT_ERROR_CONFIG,
				XERR_MEMORY,
				"HTTP SSE cancellation watch could not be installed"
			);
		}
	}
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"HTTP SSE connection was cancelled before submission"
		);
	}
	if ( !__xrtHttpSseClientStart(pClient) ) {
		if ( xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) ) {
			return __xrtHttpSseClientCreateFail(
				pClient,
				XHTTP_SSE_CLIENT_ERROR_CANCELLED,
				XERR_CANCELLED,
				"HTTP SSE connection was cancelled during submission"
			);
		}
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_HTTP,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_IO,
			"HTTP SSE connection could not be submitted"
		);
	}
	__xrtSpinLock(&pClient->Lock);
	pClient->Constructing = false;
	__xrtSpinUnlock(&pClient->Lock);
	return pClient;
}



/* 使用 GET URL 建立自动重连的 EventSource 会话。 */
XRT_API xhttpsseclient* xrtHttpSseConnect(
	xhttpclient* pHttp,
	xstrview Url,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		Url
	);
	xhttpsseclient* pClient;

	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_VALUE,
			"HTTP SSE URL is invalid",
			xrtGetError()
		);
		return NULL;
	}
	pClient = __xrtHttpSseClientCreate(
		pHttp,
		pRequest,
		pConfig,
		pEvents
	);
	xrtHttpRequestDestroy(pRequest);
	return pClient;
}



/* 克隆 GET 请求模板，并由会话管理 SSE 专用 Header 与重连状态。 */
XRT_API xhttpsseclient* xrtHttpSseConnectRequest(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	return __xrtHttpSseClientCreate(
		pHttp,
		pRequest,
		pConfig,
		pEvents
	);
}



/* 返回 EventSource 三态的并发快照。 */
XRT_API xhttpsseclientstate xrtHttpSseClientState(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return XHTTP_SSE_CLIENT_CLOSED;
	}
	return (xhttpsseclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回当前 Parser 与底层 HTTP 输入是否处于暂停状态。 */
XRT_API bool xrtHttpSseClientPaused(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	return xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
}



/* 复制不暴露 Parser 借用内存的会话统计快照。 */
XRT_API bool xrtHttpSseClientInfo(
	const xhttpsseclient* pClient,
	xhttpsseclientinfo* pInfo
)
{
	xhttpsseclientinfo Info;

	if ( (pClient == NULL) ||
		!__xrtRangeValid(pInfo, sizeof(*pInfo)) ||
		__xrtRangesOverlap(
			pClient, sizeof(*pClient), pInfo, sizeof(*pInfo)
		) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client or info output range is invalid",
			NULL
		);
		return false;
	}
	memset(&Info, 0, sizeof(Info));
	Info.State = (xhttpsseclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	Info.Status = (uint16)xrtAtomic32Load(
		&pClient->Status,
		XMEMORY_ACQUIRE
	);
	Info.Retry = xrtAtomic64Load(
		&pClient->Retry,
		XMEMORY_ACQUIRE
	);
	Info.Messages = xrtAtomic64Load(
		&pClient->Messages,
		XMEMORY_ACQUIRE
	);
	Info.Comments = xrtAtomic64Load(
		&pClient->Comments,
		XMEMORY_ACQUIRE
	);
	Info.RetryUpdates = xrtAtomic64Load(
		&pClient->RetryUpdates,
		XMEMORY_ACQUIRE
	);
	Info.Reconnects = (size_t)xrtAtomic64Load(
		&pClient->Reconnects,
		XMEMORY_ACQUIRE
	);
	Info.Paused = xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}



/* CLOSED 后返回由会话拥有的稳定终态错误。 */
XRT_API const xerror* xrtHttpSseClientError(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return NULL;
	}
	if ( !xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client terminal state is not published",
			NULL
		);
		return NULL;
	}
	return pClient->Error;
}

#endif
