#include "../internal/xrt_http_client_runtime.h"

#include <xrt/http_retry.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

#define XHTTP_RETRY_FLAGS_MASK \
	(XHTTP_RETRY_STATUS | \
	 XHTTP_RETRY_TRANSPORT | \
	 XHTTP_RETRY_RESPECT_AFTER | \
	 XHTTP_RETRY_JITTER)



/* 初始化默认关闭、启用后采用保守完整能力的重试策略。 */
XRT_API void xrtHttpRetryConfigInit(
	xhttpretryconfig* pConfig
)
{
	const xhttpretryconfig Config = {
		XHTTP_RETRY_BASE_DEFAULT,
		XHTTP_RETRY_DELAY_MAX_DEFAULT,
		XHTTP_RETRY_FLAGS_MASK,
		0
	};

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化为继承 Client 且不授权非幂等重放。 */
XRT_API void xrtHttpRetryOptionsInit(
	xhttpretryoptions* pOptions
)
{
	const xhttpretryoptions Options = {
		XHTTP_RETRY_DEFAULT,
		0
	};

	if ( !__xrtRangeValid(pOptions, sizeof(Options)) ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 判断当前请求是否满足方法与正文重放边界。 */
static bool __xrtHttpRetryReplayable(const xhttpcall* pCall)
{
	xhttpbody* pBody;

	if ( !pCall->RetryUnsafe &&
		!xrtHttpMethodIdempotent(
			xrtHttpRequestMethod(pCall->Request)
		) ) {
		return false;
	}
	pBody = xrtHttpRequestBody(pCall->Request);
	return (pBody == NULL) || xrtHttpBodyReplayable(pBody);
}



/* 判断当前调用是否还可以消费一个新的重试名额。 */
static bool __xrtHttpRetryAvailable(const xhttpcall* pCall)
{
	return pCall->RetryEnabled &&
		(pCall->Retries < (size_t)pCall->RetryMax) &&
		__xrtHttpRetryReplayable(pCall);
}



/* 初始化当前 Call 独占的非安全随机状态。 */
static void __xrtHttpRetryRng(xhttpcall* pCall)
{
	if ( pCall->RetryRngReady ) {
		return;
	}
	xrtRngSeed(
		&pCall->RetryRng,
		xrtClock() ^ (uint64)(uintptr_t)pCall,
		(uint64)(uintptr_t)pCall->Request
	);
	pCall->RetryRngReady = true;
}



/* 计算下一次本地退避，并按需应用包含零和上界的 full jitter。 */
static bool __xrtHttpRetryBackoffDelay(
	xhttpcall* pCall,
	uint64* pDelay
)
{
	uint64 iDelay;

	if ( !xrtHttpRetryBackoff(
		pCall->RetryConfig.BaseDelay,
		pCall->RetryConfig.MaxDelay,
		(uint32)pCall->Retries,
		&iDelay
	) ) {
		return false;
	}
	if ( ((pCall->RetryConfig.Flags &
		  XHTTP_RETRY_JITTER) != 0) && (iDelay != 0) ) {
		__xrtHttpRetryRng(pCall);
		iDelay = iDelay == UINT64_MAX ?
			xrtRng64(&pCall->RetryRng) :
			xrtRngBelow64(&pCall->RetryRng, iDelay + 1u);
	}
	*pDelay = iDelay;
	return true;
}



/* 读取有效的服务端等待建议，非法或重复值回退到本地策略。 */
static bool __xrtHttpRetryDelay(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	uint64* pDelay
)
{
	const xhttpheaders* pHeaders;
	xhttpnext Next;
	uint64 iDelay;

	if ( (pCall->RetryConfig.Flags &
		 XHTTP_RETRY_RESPECT_AFTER) != 0 ) {
		pHeaders = xrtHttpResponseHeaders(pResponse);
		Next = xrtHttpRetryAfterFields(
			xrtHttpHeadersData(pHeaders),
			xrtHttpHeadersCount(pHeaders),
			xrtNow(),
			&iDelay
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			*pDelay = iDelay > pCall->RetryConfig.MaxDelay ?
				pCall->RetryConfig.MaxDelay : iDelay;
			return true;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			xrtClearError();
		}
	}
	return __xrtHttpRetryBackoffDelay(pCall, pDelay);
}



/* 信息响应不参与重试决策，保持原有回调次序。 */
static bool __xrtHttpRetryInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RetryNext.Informational == NULL ) {
		return true;
	}
	return pCall->RetryNext.Informational(
		pResponse,
		pCall->RetryNext.Data
	);
}



/* 在正文前决定是否隐藏并排空一个临时失败响应。 */
static bool __xrtHttpRetryHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	pCall->RetryPending = false;
	if ( ((pCall->RetryConfig.Flags & XHTTP_RETRY_STATUS) != 0) &&
		xrtHttpRetryStatusDefault(
			xrtHttpResponseStatus(pResponse)
		) && __xrtHttpRetryAvailable(pCall) ) {
		if ( !__xrtHttpRetryDelay(
			pCall,
			pResponse,
			&pCall->RetryDelay
		) ) {
			return false;
		}
		pCall->RetryPending = true;
		return true;
	}
	if ( pCall->RetryNext.Headers == NULL ) {
		return true;
	}
	return pCall->RetryNext.Headers(
		pResponse,
		pCall->RetryNext.Data
	);
}



/* 中间响应正文只用于完成消息边界和连接复用。 */
static bool __xrtHttpRetryBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RetryPending ) {
		return true;
	}
	if ( pCall->RetryNext.Body == NULL ) {
		return __xrtHttpResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->RetryNext.Body(
		pResponse,
		Data,
		pCall->RetryNext.Data
	);
}



/* 冻结客户端重试配置和调用级显式授权。 */
bool __xrtHttpRetryInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xhttpretrymode Mode;

	if ( (pCall == NULL) || (pOptions == NULL) ||
		((pCall->Client->Config.Retry.Flags &
		  ~XHTTP_RETRY_FLAGS_MASK) != 0) ||
		(pCall->Client->Config.Retry.MaxDelay == 0) ||
		(pCall->Client->Config.Retry.BaseDelay >
		 pCall->Client->Config.Retry.MaxDelay) ||
		((pOptions->Retry.Flags & ~XHTTP_RETRY_UNSAFE) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Mode = pOptions->Retry.Mode;
	if ( (Mode < XHTTP_RETRY_DEFAULT) ||
		(Mode > XHTTP_RETRY_DISABLED) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCall->RetryConfig = pCall->Client->Config.Retry;
	pCall->RetryMax = pCall->RetryConfig.MaxRetries;
	if ( Mode == XHTTP_RETRY_ENABLED ) {
		if ( pCall->RetryMax == 0 ) {
			pCall->RetryMax = XHTTP_RETRY_MAX_DEFAULT;
		}
		pCall->RetryEnabled = true;
	} else if ( Mode == XHTTP_RETRY_DISABLED ) {
		pCall->RetryEnabled = false;
	} else {
		pCall->RetryEnabled = pCall->RetryMax != 0;
	}
	pCall->RetryUnsafe =
		(pOptions->Retry.Flags & XHTTP_RETRY_UNSAFE) != 0;
	return true;
}



/* 建立位于 Cookie 内侧、Cache 外侧的响应隐藏包装器。 */
const xhttp1exchangeevents* __xrtHttpRetryEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	pCall->RetryNext = *pNext;
	memset(&pCall->RetryEvents, 0, sizeof(pCall->RetryEvents));
	pCall->RetryEvents.Informational =
		__xrtHttpRetryInformational;
	pCall->RetryEvents.Headers = __xrtHttpRetryHeaders;
	pCall->RetryEvents.Body = __xrtHttpRetryBody;
	pCall->RetryEvents.Data = pCall;
	return &pCall->RetryEvents;
}



/* 返回当前响应是否已经被重试包装器隐藏。 */
bool __xrtHttpRetryPending(const xhttpcall* pCall)
{
	return (pCall != NULL) && pCall->RetryPending;
}



/* 计算不回绕的单调截止时间。 */
static xdeadline __xrtHttpRetryDeadline(
	uint64 iNow,
	uint64 iDelay
)
{
	return iDelay > (UINT64_MAX - iNow) ?
		UINT64_MAX : iNow + iDelay;
}



/* Timer 到期后创建全新的逐跳 Exchange 并重新进入池与传输。 */
static void __xrtHttpRetryTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xerror* pCause;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->RetryTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->RetryTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		goto done;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"wait-http-retry",
			"HTTP retry wait was cancelled",
			NULL
		);
		goto done;
	}
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			XERR_INTERNAL,
			"wait-http-retry",
			"HTTP retry timer did not reach its deadline",
			NULL
		);
		goto done;
	}
	pCall->Retries++;
	xrtAtomic64Store(
		&pCall->Info.Retries,
		(uint64)pCall->Retries,
		XMEMORY_RELEASE
	);
	/* 记录新尝试的响应线字节起点，避免前一次响应污染 EOF 判定。 */
	pCall->RetryResponseWireStart = xrtAtomic64Load(
		&pCall->Info.ResponseWireBytes,
		XMEMORY_ACQUIRE
	);
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			__xrtHttpClientCauseKind(pCause, XERR_VALUE),
			"prepare-http-retry",
			"HTTP request could not be prepared for retry",
			pCause
		);
		xrtErrorFree(pCause);
		goto done;
	}
	__xrtHttpCallStartHop(pCall);

done:
	xrtHttpCallDestroy(pCall);
}



/* 安排一个不占用连接池配额且不消耗 idle 超时的退避 Timer。 */
bool __xrtHttpRetrySchedule(xhttpcall* pCall)
{
	xerror* pCause;
	uint64 iNow;
	uint64 iIdle;
	uint64 Id;

	if ( (pCall == NULL) || !__xrtHttpRetryAvailable(pCall) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCall->RetryPending = false;
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_QUEUED,
		XMEMORY_RELEASE
	);
	__xrtHttpCallSetPhase(pCall, XHTTP_CALL_PHASE_RETRY);
	iNow = xrtClock();
	if ( pCall->IdleTimeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		iIdle = __xrtHttpRetryDeadline(
			__xrtHttpRetryDeadline(iNow, pCall->RetryDelay),
			pCall->IdleTimeout
		);
		xrtAtomic64Store(
			&pCall->IdleDeadline,
			iIdle,
			XMEMORY_RELEASE
		);
	}
	if ( xrtHttpCallRef(pCall) == NULL ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			XERR_STATE,
			"schedule-http-retry",
			"HTTP retry timer could not retain the call",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	xrtAtomic32Store(
		&pCall->RetryTimerDone,
		0,
		XMEMORY_RELEASE
	);
	Id = xrtNetEngineAfter(
		pCall->Client->Engine,
		pCall->Affinity,
		pCall->RetryDelay,
		__xrtHttpRetryTimer,
		pCall
	);
	if ( Id == 0 ) {
		pCause = xrtTakeError();
		xrtAtomic32Store(
			&pCall->RetryTimerDone,
			1,
			XMEMORY_RELEASE
		);
		xrtHttpCallDestroy(pCall);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			__xrtHttpClientCauseKind(pCause, XERR_MEMORY),
			"schedule-http-retry",
			"HTTP retry deadline could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	xrtAtomic64Store(
		&pCall->RetryTimer,
		Id,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->RetryTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64Exchange(
			&pCall->RetryTimer,
			0,
			XMEMORY_ACQ_REL
		);
	} else if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)__xrtHttpRetryCancel(pCall);
	}
	return true;
}



/* 判断原因链是否表示收到任何响应字节前的意外 EOF。 */
static bool __xrtHttpRetryUnexpectedEof(const xerror* pError)
{
	while ( pError != NULL ) {
		cstr sDomain = xrtErrorDomain(pError);

		if ( (sDomain != NULL) &&
			(strcmp(sDomain, "xrt.http.exchange") == 0) &&
			(xrtErrorCode(pError) ==
			 XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF) ) {
			return true;
		}
		pError = xrtErrorCause(pError);
	}
	return false;
}



/* 筛选可重试的临时传输错误，排除配置、协议、内存和取消失败。 */
static bool __xrtHttpRetryTransient(
	const xhttpcall* pCall,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
)
{
	if ( Code == XHTTP_CLIENT_ERROR_PROTOCOL ) {
		return (xrtAtomic64Load(
			&pCall->Info.ResponseWireBytes,
			XMEMORY_ACQUIRE
		) == pCall->RetryResponseWireStart) &&
			__xrtHttpRetryUnexpectedEof(pCause);
	}
	if ( (Code != XHTTP_CLIENT_ERROR_DIAL) &&
		(Code != XHTTP_CLIENT_ERROR_PROXY) &&
		(Code != XHTTP_CLIENT_ERROR_TLS) &&
		(Code != XHTTP_CLIENT_ERROR_TRANSPORT) ) {
		return false;
	}
	return (Kind == XERR_IO) || (Kind == XERR_AGAIN) ||
		(Kind == XERR_TIMEOUT) || (Kind == XERR_CLOSED);
}



/* 接管满足策略的失败尝试，并在退避前释放连接池配额。 */
bool __xrtHttpRetryFailure(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
)
{
	if ( (Result == XNET_RESULT_CANCELLED) ||
		(Result == XNET_RESULT_TIMEOUT) ||
		((pCall->RetryConfig.Flags & XHTTP_RETRY_TRANSPORT) == 0) ||
		!__xrtHttpRetryAvailable(pCall) ||
		!__xrtHttpRetryTransient(pCall, Code, Kind, pCause) ) {
		return false;
	}
	if ( !__xrtHttpRetryBackoffDelay(
		pCall,
		&pCall->RetryDelay
	) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolFinish(pCall);
	#endif
	(void)__xrtHttpRetrySchedule(pCall);
	return true;
}



/* 从任意线程请求取消当前退避 Timer。 */
bool __xrtHttpRetryCancel(xhttpcall* pCall)
{
	uint64 Id;

	if ( pCall == NULL ) {
		return false;
	}
	Id = xrtAtomic64Exchange(
		&pCall->RetryTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Id == 0) || xrtAtomic32Load(
		&pCall->RetryTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	if ( !xrtNetEngineTimerCancelCurrent(
		pCall->Client->Engine,
		Id
	) && !xrtNetEngineTimerCancel(
		pCall->Client->Engine,
		Id
	) ) {
		xrtClearError();
		return false;
	}
	return true;
}

#endif
