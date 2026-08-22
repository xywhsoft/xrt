#include "../internal/xrt_http_server_runtime.h"



#if defined(XRT_FEATURE_HTTP_SERVER)

/* 验证公开输出不会覆盖 Connection 的引用、锁和协议状态。 */
static bool __xrtHttpConnOutputValid(
	const xhttpconn* pConnection,
	const void* pOutput,
	size_t iSize
)
{
	return __xrtRangeValid(pOutput, iSize) &&
		((pConnection == NULL) || !__xrtRangesOverlap(
			pOutput, iSize, pConnection, sizeof(*pConnection)
		));
}



/* 增加 Connection 引用。 */
XRT_API xhttpconn* xrtHttpConnRef(xhttpconn* pConnection)
{
	if ( (pConnection == NULL) ||
		(xrtRefRetain(&pConnection->References) < 0) ) {
		__xrtHttpServerSetError(
			pConnection == NULL ? XERR_ARGUMENT : XERR_STATE,
			pConnection == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			"retain-http-connection",
			"HTTP connection is null or no longer retainable",
			NULL
		);
		return NULL;
	}
	return pConnection;
}



/* 取得当前 TCP 或 TLS 传输的稳定引用。 */
static ptr __xrtHttpConnTransportRef(xhttpconn* pConnection)
{
	ptr pTransport = NULL;

	if ( pConnection == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport != NULL ) {
		#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				pTransport = xrtTlsStreamRef(
					(xtlsstream*)pTransport
				);
			} else {
				pTransport = xrtNetStreamRef(
					(xnetstream*)pTransport
				);
			}
		#else
			pTransport = xrtNetStreamRef(
				(xnetstream*)pTransport
			);
		#endif
	}
	__xrtSpinUnlock(&pConnection->TransportLock);
	return pTransport;
}



/* 释放与 Connection 类型匹配的稳定传输引用。 */
static void __xrtHttpConnTransportDestroy(
	const xhttpconn* pConnection,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
		} else {
			xrtNetStreamDestroy((xnetstream*)pTransport);
		}
	#else
		(void)pConnection;
		xrtNetStreamDestroy((xnetstream*)pTransport);
	#endif
}



/* 释放 Connection 最终存储。 */
XRT_API void xrtHttpConnDestroy(xhttpconn* pConnection)
{
	ptr pTransport;
	xerror* pError;
	__xrt_http_response_queue* pQueued;

	if ( (pConnection == NULL) ||
		(xrtRefRelease(&pConnection->References) != 0) ) {
		return;
	}
	__xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtSpinUnlock(&pConnection->TransportLock);
	pError = (xerror*)xrtAtomicPtrExchange(
		&pConnection->Error,
		NULL,
		XMEMORY_ACQ_REL
	);
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	__xrtHttpConnTransportDestroy(pConnection, pTransport);
	xrtErrorFree(pError);
	if ( pConnection->AdapterRelease != NULL ) {
		pConnection->AdapterRelease(pConnection->AdapterData);
	}
	xrtHttpServerDestroy(pConnection->Server);
	__xrtSpinUnit(&pConnection->TransportLock);
	memset(pConnection, 0, sizeof(*pConnection));
	xrtFree(pConnection);
}



/* 在 Worker 串行域内安装唯一适配器上下文。 */
bool __xrtHttpConnAdapterSet(
	xhttpconn* pConnection,
	ptr pData,
	void (*pRelease)(ptr pData)
)
{
	if ( (pConnection == NULL) || (pData == NULL) ||
		(pRelease == NULL) ||
		(pConnection->AdapterData != NULL) ) {
		return false;
	}
	pConnection->AdapterData = pData;
	pConnection->AdapterRelease = pRelease;
	return true;
}



/* 在 Worker 串行域内借用适配器上下文。 */
ptr __xrtHttpConnAdapterData(xhttpconn* pConnection)
{
	return pConnection != NULL ?
		pConnection->AdapterData : NULL;
}



/* 在 Worker 串行域内取走适配器上下文。 */
ptr __xrtHttpConnAdapterTake(xhttpconn* pConnection)
{
	ptr pData;

	if ( pConnection == NULL ) {
		return NULL;
	}
	pData = pConnection->AdapterData;
	pConnection->AdapterData = NULL;
	pConnection->AdapterRelease = NULL;
	return pData;
}



/* 取得底层 TCP Stream 稳定引用，防止与 Close 回调交换所有权竞态。 */
xnetstream* __xrtHttpConnTcpRef(xhttpconn* pConnection)
{
	xnetstream* pStream = NULL;
	ptr pTransport;

	if ( pConnection == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( (pTransport != NULL) &&
			(pConnection->TransportKind ==
			 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
			pStream = xrtTlsStreamTransport(
				(xtlsstream*)pTransport
			);
		} else {
			pStream = (xnetstream*)pTransport;
		}
	#else
		pStream = (xnetstream*)pTransport;
	#endif
	if ( pStream != NULL ) {
		pStream = xrtNetStreamRef(pStream);
	}
	__xrtSpinUnlock(&pConnection->TransportLock);
	return pStream;
}



/* 返回 Connection 当前阶段。 */
XRT_API xhttpconnstate xrtHttpConnState(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ?
		(xhttpconnstate)xrtAtomic32Load(
			&pConnection->State,
			XMEMORY_ACQUIRE
		) : XHTTP_CONN_CLOSED;
}



/* 返回借用 Server。 */
XRT_API xhttpserver* xrtHttpConnServer(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ? pConnection->Server : NULL;
}



/* 在所属 Worker 上返回当前借用请求。 */
XRT_API const xhttpserverrequest* xrtHttpConnRequest(
	const xhttpconn* pConnection
)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		return NULL;
	}
	return xrtHttp1ServerExchangeRequest(
		pConnection->Exchange
	);
}



/* 返回借用 Worker。 */
XRT_API xnetworker* xrtHttpConnWorker(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ? pConnection->Worker : NULL;
}



/* 在所属 Worker 上返回借用 TCP Stream。 */
XRT_API xnetstream* xrtHttpConnTcp(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		return NULL;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( (pTransport != NULL) &&
			(pConnection->TransportKind ==
			 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
			return xrtTlsStreamTransport(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return (xnetstream*)pTransport;
}



/* 返回 Connection 是否使用 TLS 传输。 */
XRT_API bool xrtHttpConnSecure(const xhttpconn* pConnection)
{
	return (pConnection != NULL) &&
		(pConnection->TransportKind ==
		 XRT_HTTP_CONN_TRANSPORT_TLS);
}



/* 返回接受当前 Connection 的逻辑端点。 */
XRT_API size_t xrtHttpConnEndpoint(const xhttpconn* pConnection)
{
	return pConnection != NULL ? pConnection->Endpoint : SIZE_MAX;
}



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

/* 在所属 Worker 上返回借用 TLS Stream。 */
XRT_API xtlsstream* xrtHttpConnTls(xhttpconn* pConnection)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(pConnection->TransportKind !=
		 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}

#endif



/* 复制连接本地地址。 */
XRT_API bool xrtHttpConnLocal(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
)
{
	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pAddress, sizeof(*pAddress)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-local",
			"HTTP connection or address output is null",
			NULL
		);
		return false;
	}
	memcpy(pAddress, &pConnection->Local, sizeof(*pAddress));
	return true;
}



/* 复制连接远端地址。 */
XRT_API bool xrtHttpConnRemote(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
)
{
	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pAddress, sizeof(*pAddress)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-remote",
			"HTTP connection or address output is null",
			NULL
		);
		return false;
	}
	memcpy(pAddress, &pConnection->Remote, sizeof(*pAddress));
	return true;
}



/* 返回借用稳定错误。 */
XRT_API const xerror* xrtHttpConnError(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ?
		(const xerror*)xrtAtomicPtrLoad(
			&pConnection->Error,
			XMEMORY_ACQUIRE
		) : NULL;
}



/* 复制 Connection 统计。 */
XRT_API bool xrtHttpConnStats(
	const xhttpconn* pConnection,
	xhttpconnstats* pStats
)
{
	xhttpconnstats Stats;
	ptr pTransport;

	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pStats, sizeof(Stats)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-stats",
			"HTTP connection or stats output is null",
			NULL
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	Stats.State = xrtHttpConnState(pConnection);
	Stats.Requests = xrtAtomic64Load(
		&pConnection->Requests,
		XMEMORY_RELAXED
	);
	Stats.RequestWireBytes = xrtAtomic64Load(
		&pConnection->RequestWireBytes,
		XMEMORY_RELAXED
	);
	Stats.ResponseWireBytes = xrtAtomic64Load(
		&pConnection->ResponseWireBytes,
		XMEMORY_RELAXED
	);
	Stats.RequestBodyPaused = xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.RequestActive = xrtAtomic32Load(
		&pConnection->RequestActive,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.FinalCommitted = xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.Secure = xrtHttpConnSecure(pConnection);
	pTransport = __xrtHttpConnTransportRef(
		(xhttpconn*)pConnection
	);
	if ( pTransport != NULL ) {
		#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
			if ( Stats.Secure ) {
				xtlsstream* pTls = (xtlsstream*)pTransport;

				Stats.BufferedBytes =
					xrtTlsStreamAvailable(pTls);
				Stats.QueuedBytes =
					xrtTlsStreamPending(pTls);
			} else {
				xnetstream* pTcp =
					(xnetstream*)pTransport;

				Stats.BufferedBytes =
					xrtNetStreamAvailable(pTcp);
				Stats.QueuedBytes =
					xrtNetStreamPending(pTcp);
			}
		#else
			Stats.BufferedBytes = xrtNetStreamAvailable(
				(xnetstream*)pTransport
			);
			Stats.QueuedBytes = xrtNetStreamPending(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
	}
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}



/* 在 Headers 回调中把路由正文上限直接应用到唯一 Exchange。 */
XRT_API bool xrtHttpConnSetRequestBodyLimit(
	xhttpconn* pConnection,
	uint64 iMaxBody
)
{
	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"limit-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttpConnState(pConnection) != XHTTP_CONN_REQUEST) ||
		!xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) || (xrtHttpConnRequest(pConnection) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"limit-http-request-body",
			"HTTP request body limit requires its Headers callback",
			NULL
		);
		return false;
	}
	if ( !xrtHttp1ServerExchangeSetBodyLimit(
		pConnection->Exchange, iMaxBody
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(pCause, XERR_STATE),
			XHTTP_SERVER_ERROR_PROTOCOL,
			"limit-http-request-body",
			"HTTP request body limit could not be applied",
			pCause
		);
		return false;
	}
	return true;
}



/* 处理一次 Exchange Feed 结果；定义位于请求输入驱动之后。 */
static bool __xrtHttpConnFeedStatus(
	xhttpconn* pConnection,
	xhttp1serverfeedstatus Status
);



/* 在所属 Worker 上恢复 Exchange，并继续处理已经缓冲的正文。 */
static void __xrtHttpConnResumeRequestBody(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pConnection->RequestBodyResumePosted,
		0,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) ) {
		if ( (xrtHttpConnState(pConnection) ==
			 XHTTP_CONN_BODY) &&
			xrtHttp1ServerExchangePaused(
				pConnection->Exchange
			) ) {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
			if ( !__xrtHttpConnResumeExchange(
				pConnection
			) ) {
				xrtHttpConnDestroy(pConnection);
				return;
			}
			__xrtHttpConnResumeInput(pConnection);
			__xrtHttpConnDriveInput(pConnection);
		} else {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
		}
	}
	xrtHttpConnDestroy(pConnection);
}



/* 合并跨线程恢复请求，并为嵌入命令持有一份 Connection 引用。 */
static bool __xrtHttpConnScheduleRequestBodyResume(
	xhttpconn* pConnection
)
{
	uint32 iExpected = 0;

	if ( !xrtAtomic32CompareExchange(
		&pConnection->RequestBodyResumePosted,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		xrtAtomic32Store(
			&pConnection->RequestBodyResumePosted,
			0,
			XMEMORY_RELEASE
		);
		return false;
	}
	__xrtNetEnginePostInternal(
		pConnection->Worker,
		&pConnection->RequestBodyResumeCommand,
		__xrtHttpConnResumeRequestBody,
		pConnection
	);
	return true;
}



/* 在流式 Body 回调中暂停后续请求正文交付。 */
XRT_API bool xrtHttpConnPauseRequestBody(
	xhttpconn* pConnection
)
{
	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"pause-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttpConnState(pConnection) != XHTTP_CONN_BODY) ||
		!xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"pause-http-request-body",
			"HTTP request body pause requires its streaming Body callback",
			NULL
		);
		return false;
	}
	if ( !xrtHttp1ServerExchangePause(
		pConnection->Exchange
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"pause-http-request-body",
			"HTTP request body is not available for pausing",
			xrtGetError()
		);
		return false;
	}
	xrtAtomic32Store(
		&pConnection->RequestBodyPaused,
		1,
		XMEMORY_RELEASE
	);
	__xrtHttpConnPauseInput(pConnection);
	return true;
}



/* 从任意线程请求恢复流式正文，并唤醒所属 Worker。 */
XRT_API bool xrtHttpConnResumeRequestBody(
	xhttpconn* pConnection
)
{
	xhttpconnstate State;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"resume-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	State = xrtHttpConnState(pConnection);
	if ( (State == XHTTP_CONN_UPGRADED) ||
		(State == XHTTP_CONN_CLOSING) ||
		(State == XHTTP_CONN_CLOSED) ) {
		__xrtHttpServerSetError(
			State == XHTTP_CONN_CLOSED ? XERR_CLOSED : XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"resume-http-request-body",
			"HTTP connection no longer owns a resumable request body",
			NULL
		);
		return false;
	}
	if ( !xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	return __xrtHttpConnScheduleRequestBodyResume(pConnection);
}



/* 返回应用请求正文暂停状态的并发快照。 */
XRT_API bool xrtHttpConnRequestBodyPaused(
	const xhttpconn* pConnection
)
{
	return (pConnection != NULL) &&
		(xrtAtomic32Load(
			&pConnection->RequestBodyPaused,
			XMEMORY_ACQUIRE
		) != 0);
}



/* 拒绝在 HTTP 已经转移传输所有权后继续控制 Connection。 */
static bool __xrtHttpConnClaimClose(
	xhttpconn* pConnection,
	cstr sOperation
)
{
	uint32 iExpected = XRT_HTTP_CONN_GATE_OPEN;

	if ( xrtAtomic32CompareExchange(
		&pConnection->CloseGate,
		&iExpected,
		XRT_HTTP_CONN_GATE_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) || (iExpected == XRT_HTTP_CONN_GATE_CLOSING) ) {
		return true;
	}
	__xrtHttpServerSetError(
		XERR_STATE,
		XHTTP_SERVER_ERROR_STATE,
		sOperation,
		"HTTP connection transport belongs to the upgraded protocol",
		NULL
	);
	return false;
}



/* 从任意线程请求优雅关闭。 */
XRT_API bool xrtHttpConnClose(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"close-http-connection",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnClaimClose(
		pConnection, "close-http-connection"
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSING,
		XMEMORY_RELEASE
	);
	pTransport = __xrtHttpConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		bool bResult;

		#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				bResult = xrtTlsStreamClose(
					(xtlsstream*)pTransport
				);
			} else {
				bResult = xrtNetStreamClose(
					(xnetstream*)pTransport
				);
			}
		#else
			bResult = xrtNetStreamClose(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
		return bResult;
	}
	return true;
}



/* 从任意线程请求异常关闭。 */
XRT_API bool xrtHttpConnAbort(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"abort-http-connection",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnClaimClose(
		pConnection, "abort-http-connection"
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSING,
		XMEMORY_RELEASE
	);
	pTransport = __xrtHttpConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		bool bResult;

		#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				bResult = xrtTlsStreamAbort(
					(xtlsstream*)pTransport
				);
			} else {
				bResult = xrtNetStreamAbort(
					(xnetstream*)pTransport
				);
			}
		#else
			bResult = xrtNetStreamAbort(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
		return bResult;
	}
	return true;
}



/* 取消当前阶段 Timer。 */
void __xrtHttpConnCancelTimer(xhttpconn* pConnection)
{
	uint64 Id = pConnection->Timer;

	pConnection->Timer = 0;
	pConnection->TimerKind = XRT_HTTP_SERVER_TIMER_NONE;
	if ( Id != 0 ) {
		(void)__xrtNetEngineTimerCancelLifecycle(
			pConnection->Server->Engine, Id
		);
	}
}



/* Timer 到期时按明确协议阶段收敛连接。 */
static void __xrtHttpConnTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	uint32 iKind = XRT_HTTP_SERVER_TIMER_NONE;
	uint64 iRemaining = 0;

	(void)pWorker;
	if ( pConnection->Timer == Id ) {
		iKind = pConnection->TimerKind;
		pConnection->Timer = 0;
		pConnection->TimerKind =
			XRT_HTTP_SERVER_TIMER_NONE;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(iKind != XRT_HTTP_SERVER_TIMER_NONE) ) {
		if ( iKind == XRT_HTTP_SERVER_TIMER_WRITE ) {
			iRemaining = xrtDeadlineRemaining(
				pConnection->WriteDeadline
			);
			#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
				if ( (iRemaining == 0) &&
					pConnection->OutputDraining &&
					(pConnection->TransportKind ==
					 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
					xtlsstream* pStream = (xtlsstream*)
						xrtAtomicPtrLoad(
							&pConnection->Transport,
							XMEMORY_ACQUIRE
						);
					size_t iPending =
						xrtTlsStreamPending(pStream);

					/*
					 * TLS 与 TCP 队列的持续下降属于有效写进度；
					 * 完全排空时直接推进响应，避免等待迟到的 Drain。
					 */
					if ( iPending == 0 ) {
						__xrtHttpConnDriveOutput(
							pConnection
						);
						xrtHttpConnDestroy(
							pConnection
						);
						return;
					}
					if ( iPending <
						pConnection->WritePending ) {
						pConnection->WritePending =
							iPending;
						pConnection->WriteDeadline =
							xrtDeadlineAfter(
								pConnection->Server->
									Config.WriteTimeout
							);
						iRemaining =
							pConnection->Server->
								Config.WriteTimeout;
					}
				}
			#endif
		}
		if ( iRemaining != 0 ) {
			if ( !__xrtHttpConnArmTimer(
				pConnection,
				XRT_HTTP_SERVER_TIMER_WRITE,
				iRemaining
			) ) {
				(void)xrtHttpConnAbort(pConnection);
			}
			xrtHttpConnDestroy(pConnection);
			return;
		}
		(void)xrtAtomic64FetchAdd(
			&pConnection->Server->Timeouts,
			1,
			XMEMORY_RELAXED
		);
		if ( iKind == XRT_HTTP_SERVER_TIMER_HEADER ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				408,
				XHTTP_SERVER_ERROR_TIMEOUT_HEADER,
				"read-http-request-header",
				"HTTP request header timed out",
				NULL
			);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_BODY ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				408,
				XHTTP_SERVER_ERROR_TIMEOUT_BODY,
				"read-http-request-body",
				"HTTP request body made no progress before its timeout",
				NULL
			);
		} else if ( iKind ==
			XRT_HTTP_SERVER_TIMER_REQUEST ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				504,
				XHTTP_SERVER_ERROR_TIMEOUT_REQUEST,
				"wait-http-response",
				"HTTP application did not submit a response before its timeout",
				NULL
			);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_IDLE ) {
			(void)xrtHttpConnClose(pConnection);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_WRITE ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_TIMEOUT,
				XHTTP_SERVER_ERROR_TIMEOUT_WRITE,
				"write-http-response",
				"HTTP response made no progress before its timeout",
				NULL
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		}
	}
	xrtHttpConnDestroy(pConnection);
}



/* 为当前阶段安装新 Timer。 */
bool __xrtHttpConnArmTimer(
	xhttpconn* pConnection,
	uint32 iKind,
	uint64 iTimeout
)
{
	uint64 Id;

	__xrtHttpConnCancelTimer(pConnection);
	if ( iTimeout == 0 ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return false;
	}
	Id = xrtNetEngineAfter(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		iTimeout,
		__xrtHttpConnTimer,
		pConnection
	);
	if ( Id == 0 ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		__xrtHttpConnRememberError(
			pConnection,
			XERR_INTERNAL,
			XHTTP_SERVER_ERROR_INTERNAL,
			"schedule-http-connection-timeout",
			"HTTP connection could not schedule its phase timeout",
			pCause
		);
		xrtErrorFree(pCause);
		xrtHttpConnDestroy(pConnection);
		return false;
	}
	pConnection->Timer = Id;
	pConnection->TimerKind = iKind;
	return true;
}



/* 借用当前传输已经解密的连续块链。 */
const xnetbuf* __xrtHttpConnBuffer(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return NULL;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return xrtTlsStreamBuffer(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return xrtNetStreamBuffer((xnetstream*)pTransport);
}



/* 消费当前传输的一段明文输入。 */
bool __xrtHttpConnConsume(
	xhttpconn* pConnection,
	size_t iSize
)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return false;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return xrtTlsStreamConsume(
				(xtlsstream*)pTransport,
				iSize
			);
		}
	#endif
	return xrtNetStreamConsume(
		(xnetstream*)pTransport,
		iSize
	);
}



/* 暂停明文 TCP 输入；TLS 会在留下未消费明文时自动暂停。 */
void __xrtHttpConnPauseInput(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return;
		}
	#endif
	xrtNetStreamPause((xnetstream*)pTransport);
}



/* 恢复明文 TCP 输入；TLS 会在消费完明文后自动恢复。 */
void __xrtHttpConnResumeInput(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XRT_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return;
		}
	#endif
	(void)xrtNetStreamResume((xnetstream*)pTransport);
}



/* 把 Exchange Header 事件提升到 Connection。 */
static xhttpserverbodypolicy __xrtHttpConnHeaders(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;
	xhttpserverbodypolicy Policy =
		XHTTP_SERVER_BODY_BUFFER;
	bool bHasBody =
		xrtHttpServerRequestBodyMode(pRequest) !=
		XHTTP1_BODY_NONE;

	if ( pServer->Events.Headers != NULL ) {
		Policy = pServer->Events.Headers(
			pServer,
			pConnection,
			pRequest,
			pServer->Events.Data
		);
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* 最终响应已经固定；无正文请求继续完成，上传请求停止在 Header 边界。 */
		return bHasBody ?
			XHTTP_SERVER_BODY_REJECT :
			XHTTP_SERVER_BODY_BUFFER;
	}
	if ( (Policy != XHTTP_SERVER_BODY_BUFFER) &&
		(Policy != XHTTP_SERVER_BODY_STREAM) &&
		(Policy != XHTTP_SERVER_BODY_REJECT) &&
		(Policy != XHTTP_SERVER_BODY_DISCARD) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_CALLBACK,
			"select-http-request-body",
			"HTTP Headers callback returned an invalid body policy",
			NULL
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( (Policy == XHTTP_SERVER_BODY_STREAM) &&
		(pServer->Events.Body == NULL) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_CALLBACK,
			"stream-http-request-body",
			"HTTP streaming body policy requires a Body callback",
			NULL
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( pConnection->ResponseInformation &&
		(pConnection->Response != NULL) ) {
		if ( bHasBody &&
			!xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-information",
				"HTTP request could not pause for its information response",
				xrtGetError()
			);
			return XHTTP_SERVER_BODY_REJECT;
		}
		return Policy;
	}
	if ( !bHasBody ) {
		return Policy;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_BODY,
		XMEMORY_RELEASE
	);
	if ( ((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_EXPECT_CONTINUE) != 0) &&
		(Policy != XHTTP_SERVER_BODY_REJECT) ) {
		if ( !xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-body",
				"HTTP request body could not pause for 100 Continue",
				xrtGetError()
			);
			return XHTTP_SERVER_BODY_REJECT;
		}
		pConnection->AutoContinue = true;
		return Policy;
	}
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_BODY,
		pServer->Config.BodyTimeout
	) ) {
		return XHTTP_SERVER_BODY_REJECT;
	}
	return Policy;
}



/* 把流式正文片段提升到 Connection 并刷新正文空闲时限。 */
static bool __xrtHttpConnBody(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;
	bool bAccepted = true;

	if ( pServer->Events.Body != NULL ) {
		bAccepted = pServer->Events.Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pServer->Events.Data
		);
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* 阻止底层循环继续消费同一输入片段中的正文或流水线后缀。 */
		if ( !xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-after-response",
				"HTTP request could not stop after its early response",
				xrtGetError()
			);
			return false;
		}
		return true;
	}
	return bAccepted;
}



/* 发布完整请求并等待应用提交最终响应。 */
static bool __xrtHttpConnComplete(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;

	(void)pExchange;
	(void)xrtAtomic64FetchAdd(
		&pConnection->Requests,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pServer->Requests,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* Headers 已经提交最终响应时，只完成协议计数，不重复进入应用路由。 */
		return true;
	}
	__xrtHttpConnCancelTimer(pConnection);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_WAITING,
		XMEMORY_RELEASE
	);
	__xrtHttpConnPauseInput(pConnection);
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_REQUEST,
		pServer->Config.RequestTimeout
	) ) {
		return false;
	}
	if ( pServer->Events.Request != NULL ) {
		pServer->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pServer->Events.Data
		);
	} else {
		(void)xrtHttpConnReply(
			pConnection,
			404,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Not Found")
		);
	}
	if ( xrtHttpConnState(pConnection) ==
		XHTTP_CONN_WAITING ) {
		/*
		 * 明文 TCP 在应用异步生成响应时继续有界读取，以便及时观察
		 * EOF、RST；TLS 则由未消费明文自动施加同等有界背压。
		 */
		__xrtHttpConnResumeInput(pConnection);
	}
	return true;
}



/* 返回 Connection 使用的 Exchange 事件。 */
static void __xrtHttpConnExchangeEvents(
	xhttpconn* pConnection,
	xhttp1serverevents* pEvents
)
{
	memset(pEvents, 0, sizeof(*pEvents));
	pEvents->Headers = __xrtHttpConnHeaders;
	pEvents->Body = __xrtHttpConnBody;
	pEvents->Complete = __xrtHttpConnComplete;
	pEvents->Data = pConnection;
}



/* 把 Exchange 终态错误映射为服务端响应状态。 */
static void __xrtHttpConnExchangeFail(xhttpconn* pConnection)
{
	const xerror* pCause = xrtHttp1ServerExchangeError(
		pConnection->Exchange
	);
	int32 iCode = xrtErrorCode(pCause);
	uint16 iStatus = 400;
	xhttpservererror Code = XHTTP_SERVER_ERROR_PROTOCOL;

	if ( iCode == XHTTP1_SERVER_ERROR_EXPECTATION ) {
		iStatus = 417;
	} else if ( iCode == XHTTP1_SERVER_ERROR_BODY_LIMIT ) {
		iStatus = 413;
	} else if (
		((iCode == XHTTP1_SERVER_ERROR_HEAD) ||
		 (iCode == XHTTP1_SERVER_ERROR_TRAILER)) &&
		(xrtErrorIs(pCause, XERR_RANGE) != NULL)
	) {
		iStatus = 431;
	} else if (
		(iCode == XHTTP1_SERVER_ERROR_BODY_STORAGE) ||
		(iCode == XHTTP1_SERVER_ERROR_TRAILER_STORAGE) ||
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ||
		(iCode == XHTTP1_SERVER_ERROR_HEADERS_CALLBACK) ||
		(iCode == XHTTP1_SERVER_ERROR_BODY_CALLBACK) ||
		(iCode == XHTTP1_SERVER_ERROR_COMPLETE_CALLBACK)
	) {
		iStatus = 500;
		Code = XHTTP_SERVER_ERROR_CALLBACK;
	}
	__xrtHttpConnProtocolFail(
		pConnection,
		iStatus,
		Code,
		"parse-http-request",
		"HTTP request could not be completed",
		pCause
	);
}



/* 处理一次 Exchange Feed 状态。 */
static bool __xrtHttpConnFeedStatus(
	xhttpconn* pConnection,
	xhttp1serverfeedstatus Status
)
{
	if ( Status == XHTTP1_SERVER_FEED_MORE ) {
		return true;
	}
	if ( Status == XHTTP1_SERVER_FEED_PAUSED ) {
		__xrtHttpConnPauseInput(pConnection);
		if ( pConnection->AutoContinue ) {
			pConnection->AutoContinue = false;
			if ( !__xrtHttpConnContinue(pConnection) ) {
				(void)xrtHttpConnAbort(pConnection);
			}
		}
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_COMPLETE ) {
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_REJECTED ) {
		if ( !xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
			const xerror* pError = xrtHttpConnError(
				pConnection
			);
			bool bCallback = (pError != NULL) &&
				(xrtErrorCode(pError) ==
				 XHTTP_SERVER_ERROR_CALLBACK);

			__xrtHttpConnProtocolFail(
				pConnection,
				bCallback ? 500 : 413,
				bCallback ?
					XHTTP_SERVER_ERROR_CALLBACK :
					XHTTP_SERVER_ERROR_PROTOCOL,
				"reject-http-request-body",
				bCallback ?
					"HTTP request callback rejected the request" :
					"HTTP request body was rejected by policy",
				pError
			);
		}
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_ERROR ) {
		__xrtHttpConnExchangeFail(pConnection);
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_CLOSED ) {
		(void)xrtHttpConnClose(pConnection);
		return false;
	}
	return false;
}



/* 恢复协议正文，并在无需新字节时立即发布 Complete 或错误终态。 */
bool __xrtHttpConnResumeExchange(xhttpconn* pConnection)
{
	xhttp1serverfeedstatus Status;
	size_t iAccepted = 0;

	if ( !xrtHttp1ServerExchangePaused(pConnection->Exchange) ) {
		return true;
	}
	if ( !xrtHttp1ServerExchangeResume(pConnection->Exchange) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"resume-http-request-body",
			"HTTP request body state could not resume",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return false;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pConnection->Exchange,
		(xbytesview){ NULL, 0 },
		false,
		&iAccepted
	);
	if ( iAccepted != 0 ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"resume-http-request-body",
			"HTTP request body accepted bytes from empty input",
			NULL
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return false;
	}
	return __xrtHttpConnFeedStatus(pConnection, Status);
}



/* 消费 Stream 当前全部可处理输入，不吞入下一条流水线请求。 */
void __xrtHttpConnDriveInput(xhttpconn* pConnection)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xhttpconnstate State = xrtHttpConnState(pConnection);

	if ( (pTransport == NULL) ||
		(State == XHTTP_CONN_WAITING) ||
		(State == XHTTP_CONN_INFORMATION) ||
		(State == XHTTP_CONN_RESPONSE) ||
		(State == XHTTP_CONN_CLOSING) ||
		(State == XHTTP_CONN_CLOSED) ||
		(pConnection->Response != NULL) ) {
		return;
	}
	for ( ;; ) {
		const xnetbuf* pBuffer =
			__xrtHttpConnBuffer(pConnection);
		xnetspan Span;
		xhttp1serverfeedstatus Status;
		size_t iAccepted = 0;

		if ( (pBuffer == NULL) ||
			!xrtNetBufFront(pBuffer, &Span) ) {
			break;
		}
		if ( !xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) ) {
			xrtAtomic32Store(
				&pConnection->RequestActive,
				1,
				XMEMORY_RELEASE
			);
			if ( pConnection->TimerKind !=
				XRT_HTTP_SERVER_TIMER_HEADER ) {
				if ( !__xrtHttpConnArmTimer(
					pConnection,
					XRT_HTTP_SERVER_TIMER_HEADER,
					pConnection->Server->Config.HeaderTimeout
				) ) {
					(void)xrtHttpConnAbort(pConnection);
					break;
				}
			}
		}
		Status = xrtHttp1ServerExchangeFeed(
			pConnection->Exchange,
			(xbytesview){ Span.Data, Span.Size },
			false,
			&iAccepted
		);
		if ( iAccepted != 0 ) {
			(void)__xrtHttpConnConsume(
				pConnection, iAccepted
			);
			(void)xrtAtomic64FetchAdd(
				&pConnection->RequestWireBytes,
				(uint64)iAccepted,
				XMEMORY_RELAXED
			);
			if ((xrtHttpConnState(pConnection) ==
				 XHTTP_CONN_BODY) &&
				!pConnection->AutoContinue &&
				!__xrtHttpConnArmTimer(
					pConnection,
					XRT_HTTP_SERVER_TIMER_BODY,
					pConnection->Server->Config.BodyTimeout
				) ) {
				(void)xrtHttpConnAbort(pConnection);
				break;
			}
		}
		if ( !__xrtHttpConnFeedStatus(
			pConnection, Status
		) ) {
			break;
		}
		if ( iAccepted == 0 ) {
			break;
		}
	}
}



/* 处理可靠 TCP 读端结束。 */
static void __xrtHttpConnEndInput(xhttpconn* pConnection)
{
	xhttp1serverfeedstatus Status;
	size_t iAccepted = 0;

	pConnection->InputEnded = true;
	pConnection->ForceClose = true;
	__xrtHttpConnDriveInput(pConnection);
	if ( (pConnection->Response != NULL) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_WAITING) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_CLOSED) ) {
		return;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pConnection->Exchange,
		(xbytesview){ NULL, 0 },
		true,
		&iAccepted
	);
	(void)__xrtHttpConnFeedStatus(pConnection, Status);
}



/* 传输打开后启动 Header 总时限并发布 Open。 */
static void __xrtHttpConnOpened(xhttpconn* pConnection)
{
	xhttpserver* pServer = pConnection->Server;

	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_HEADER,
		pServer->Config.HeaderTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( pServer->Events.Open != NULL ) {
		pServer->Events.Open(
			pServer,
			pConnection,
			pServer->Events.Data
		);
	}
}



/* TCP 打开事件进入统一 HTTP Open 路径。 */
static void __xrtHttpConnOpen(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnOpened((xhttpconn*)pData);
}



/* Stream 可读时推进请求。 */
static void __xrtHttpConnRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpConnDriveInput((xhttpconn*)pData);
}



/* TCP 对端结束写方向时完成 Exchange EOF。 */
static void __xrtHttpConnEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnEndInput((xhttpconn*)pData);
}



/* TCP 写预算恢复时继续当前响应。 */
static void __xrtHttpConnWritable(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtHttpConnDriveOutput((xhttpconn*)pData);
}



/* TCP 发送队列排空时继续或完成当前响应。 */
static void __xrtHttpConnDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnDriveOutput((xhttpconn*)pData);
}



/* 传输终态回收协议对象并发布唯一 Connection Close。 */
static void __xrtHttpConnClosed(
	xhttpconn* pConnection,
	ptr pTransport,
	xnetresult Result,
	const xerror* pCause
)
{
	xhttpserver* pServer = pConnection->Server;
	ptr pOwned;
	const xerror* pError;
	__xrt_http_response_queue* pQueued;
	bool bHadError = xrtHttpConnError(pConnection) != NULL;
	bool bConnectionFinished = false;
	bool bReleaseRuntime;
	#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
		bool bUpgradePending;
	#endif

	if ( pCause != NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			xrtErrorKind(pCause),
			xrtHttpConnSecure(pConnection) ?
				XHTTP_SERVER_ERROR_TLS :
				XHTTP_SERVER_ERROR_CONNECTION,
			"run-http-connection",
			xrtHttpConnSecure(pConnection) ?
				"HTTPS TLS connection failed" :
			"HTTP TCP connection failed",
			pCause
		);
		if ( !bHadError &&
			(xrtErrorKind(pCause) == XERR_TIMEOUT) ) {
			(void)xrtAtomic64FetchAdd(
				&pServer->Timeouts,
				1,
				XMEMORY_RELAXED
			);
		}
	}
	__xrtHttpConnCancelTimer(pConnection);
	xrtAtomic32Store(
		&pConnection->RequestBodyPaused,
		0,
		XMEMORY_RELEASE
	);
	#if defined(XRT_FEATURE_HTTP_SERVER_FUTURE)
		__xrtHttpConnFutureDetach(pConnection, true);
	#endif
	#if defined(XRT_FEATURE_HTTP_SERVER_BODY_ASYNC)
		__xrtHttpConnBodyStop(pConnection, true);
	#endif
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtSpinLock(&pConnection->TransportLock);
	pOwned = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtSpinUnlock(&pConnection->TransportLock);
	pError = xrtHttpConnError(pConnection);
	#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
		bUpgradePending = __xrtHttpConnUpgradeFail(
			pConnection,
			Result,
			pError != NULL ? pError : pCause
		);
	#endif
	bReleaseRuntime = pConnection->RuntimeHeld
		#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
			&& !bUpgradePending
		#endif
	;
	if ( bReleaseRuntime && pConnection->Counted ) {
		pConnection->Counted = false;
		bConnectionFinished = true;
		__xrtHttpServerConnectionClosed(pServer);
	}
	if (
		#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
			!bUpgradePending &&
		#endif
		(pError != NULL) ) {
		__xrtHttpConnEmitError(pConnection);
	}
	if (
		#if defined(XRT_FEATURE_HTTP_SERVER_UPGRADE)
			!bUpgradePending &&
		#endif
		!pConnection->CloseNotified &&
		(pServer->Events.Close != NULL) ) {
		pConnection->CloseNotified = true;
		pServer->Events.Close(
			pServer,
			pConnection,
			Result,
			pError,
			pServer->Events.Data
		);
	}
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	pConnection->Response = NULL;
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	pConnection->ResponseTail = NULL;
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	pConnection->Exchange = NULL;
	__xrtHttpConnTransportDestroy(
		pConnection,
		pOwned != NULL ? pOwned : pTransport
	);
	__xrtHttpServerRemoveConnection(
		pServer, pConnection
	);
	if ( bReleaseRuntime ) {
		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
	}
	if ( bConnectionFinished ) {
		__xrtHttpServerConnectionFinished(pServer);
	}
}



/* TCP 终态进入统一 Connection 回收路径。 */
static void __xrtHttpConnCloseEvent(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pCause,
	ptr pData
)
{
	__xrtHttpConnClosed(
		(xhttpconn*)pData,
		pStream,
		Result,
		pCause
	);
}



/* 返回静态 TCP Stream 事件表。 */
const xnetstreamevents* __xrtHttpConnStreamEvents(void)
{
	static const xnetstreamevents Events = {
		__xrtHttpConnOpen,
		__xrtHttpConnRead,
		__xrtHttpConnEnd,
		NULL,
		__xrtHttpConnWritable,
		__xrtHttpConnDrain,
		__xrtHttpConnCloseEvent
	};

	return &Events;
}



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

/* TLS 握手完成后进入统一 HTTP Open 路径。 */
static void __xrtHttpConnTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnOpened((xhttpconn*)pData);
}



/* TLS 明文可读时推进请求。 */
static void __xrtHttpConnTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpConnDriveInput((xhttpconn*)pData);
}



/* TLS 对端认证关闭写方向时完成 Exchange EOF。 */
static void __xrtHttpConnTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnEndInput((xhttpconn*)pData);
}



/* TLS 明文写预算恢复时继续当前响应。 */
static void __xrtHttpConnTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pStream;
	pConnection->OutputQueued = false;
	__xrtHttpConnDriveOutput(pConnection);
}



/* TLS 密文完全排空时继续关闭或防御性恢复输出。 */
static void __xrtHttpConnTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pStream;
	pConnection->OutputQueued = false;
	__xrtHttpConnDriveOutput(pConnection);
}



/* TLS 终态进入统一 Connection 回收路径。 */
static void __xrtHttpConnTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pCause,
	ptr pData
)
{
	__xrtHttpConnClosed(
		(xhttpconn*)pData,
		pStream,
		Result,
		pCause
	);
}



/* 返回静态 TLS Stream 事件表。 */
static const xtlsstreamevents* __xrtHttpConnTlsEvents(void)
{
	static const xtlsstreamevents Events = {
		__xrtHttpConnTlsOpen,
		__xrtHttpConnTlsRead,
		__xrtHttpConnTlsEnd,
		__xrtHttpConnTlsWritable,
		__xrtHttpConnTlsDrain,
		__xrtHttpConnTlsClose
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			, NULL
		#endif
	};

	return &Events;
}

#endif



/* 创建尚未绑定传输的 Worker 归属 HTTP Connection。 */
static xhttpconn* __xrtHttpConnCreateBase(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream,
	uint32 iTransportKind
)
{
	xhttp1serverevents ExchangeEvents;
	xhttpconn* pConnection;

	pConnection = (xhttpconn*)xrtCalloc(
		1, sizeof(*pConnection)
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	pConnection->References = 1;
	xrtAtomic32Init(
		&pConnection->State, XHTTP_CONN_REQUEST
	);
	xrtAtomic32Init(
		&pConnection->CloseGate,
		XRT_HTTP_CONN_GATE_OPEN
	);
	xrtAtomic32Init(&pConnection->FinalGate, 0);
	xrtAtomic32Init(&pConnection->RequestActive, 0);
	xrtAtomic32Init(&pConnection->RequestBodyPaused, 0);
	xrtAtomic32Init(
		&pConnection->RequestBodyResumePosted, 0
	);
	xrtAtomic64Init(&pConnection->Requests, 0);
	xrtAtomic64Init(&pConnection->RequestWireBytes, 0);
	xrtAtomic64Init(&pConnection->ResponseWireBytes, 0);
	xrtAtomicPtrInit(&pConnection->Error, NULL);
	xrtAtomicPtrInit(&pConnection->Transport, NULL);
	__xrtSpinInit(&pConnection->TransportLock);
	pConnection->Server = xrtHttpServerRef(pServer);
	pConnection->Worker = xrtNetStreamWorker(pStream);
	pConnection->Endpoint = iEndpoint;
	pConnection->TransportKind = iTransportKind;
	pConnection->RuntimeHeld = true;
	if ( (pConnection->Server == NULL) ||
		!xrtNetStreamLocal(pStream, &pConnection->Local) ||
		!xrtNetStreamRemote(pStream, &pConnection->Remote) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	__xrtHttpConnExchangeEvents(
		pConnection, &ExchangeEvents
	);
	pConnection->Exchange =
		xrtHttp1ServerExchangeCreate(
			&pServer->Config.Http1,
			&ExchangeEvents
		);
	if ( pConnection->Exchange == NULL ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return pConnection;
}



/* 创建并接管一个已接受的明文 TCP Stream。 */
xhttpconn* __xrtHttpConnCreateTcp(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
)
{
	xhttpconn* pConnection = __xrtHttpConnCreateBase(
		pServer,
		iEndpoint,
		pStream,
		XRT_HTTP_CONN_TRANSPORT_TCP
	);

	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !xrtNetStreamSetEvents(
		pStream,
		__xrtHttpConnStreamEvents(),
		pConnection
	) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pConnection->Transport,
		pStream,
		XMEMORY_RELEASE
	);
	return pConnection;
}



#if defined(XRT_FEATURE_HTTP_SERVER_TLS)

/* 创建并接管一个已接受的 TLS Stream。 */
xhttpconn* __xrtHttpConnCreateTls(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
)
{
	xhttpconn* pConnection = __xrtHttpConnCreateBase(
		pServer,
		iEndpoint,
		pStream,
		XRT_HTTP_CONN_TRANSPORT_TLS
	);
	xtlsstream* pTls = NULL;

	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !xrtTlsStreamAccept(
		pStream,
		&pServer->Tls,
		&pServer->TlsStream,
		__xrtHttpConnTlsEvents(),
		pConnection,
		&pTls
	) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pConnection->Transport,
		pTls,
		XMEMORY_RELEASE
	);
	return pConnection;
}

#endif

#endif
