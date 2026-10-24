#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_DIAL)

#define XRT_TLS_DIAL_GATE_OPEN 0u
#define XRT_TLS_DIAL_GATE_CANCEL 1u
#define XRT_TLS_DIAL_GATE_TIMEOUT 2u
#define XRT_TLS_DIAL_GATE_WINNER 3u
#define XRT_TLS_DIAL_GATE_TERMINAL 4u
#define XRT_TLS_DIAL_OWN_READY 1u
#define XRT_TLS_DIAL_OWN_CLEARED 2u



/* 受管 TLS Dial 在握手成功前独占 Stream 调用方引用。 */
struct xtlsdial {
	volatile int32 References;
	xatomic32 State;
	xatomic32 TerminalGate;
	xatomic32 TimerDone;
	xatomic64 Timer;
	xatomicptr TransportDial;
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	xatomicptr ProxyDial;
#endif
	xatomicptr Stream;
	xnetengine* Engine;
	xtlsstreamevents StreamEvents;
	ptr StreamData;
	xtlsdialproc Done;
	ptr DoneData;
	xerror* Error;
	uint64 Affinity;
	bool RuntimeHeld;
	/* Owned mode has a Worker-confined stage slot, not an atomically borrowed
	 * Stream pointer. Public cancellation only posts to that Worker. */
	const xtlsdialownershipv1* OwnershipPolicy;
	xatomic32 OwnershipActive, OwnershipState, OwnershipStop;
	xatomicptr OwnershipClaim;
	xrt_spinlock OwnershipLock;
	__xrt_net_engine_runtime RuntimeOwner;
	__xrt_net_engine_internal OwnershipStopCommand;
	xnetworker* Worker;
	xtlssession* OwnedSession;
	xtlsstream* OwnedStream;
	xtlsstreamconfig OwnedConfig;
	xerror* OwnershipFinishError;
	bool OwnershipDataHeld, OwnershipEngineHeld, OwnershipStorageHeld;
	bool OwnershipPublished, OwnershipAbandoned, OwnershipTransportDone;
	bool OwnershipOpened, OwnershipFinishing, OwnershipFinished;
};

static const xrtownershipops __xrtTlsDialOwnershipOps;
static void __xrtTlsDialOwnedTryRelease(xtlsdial*);
static void __xrtTlsDialOwnedTransportDone(xtlsdial*, xnetresult, xnetstream*, const xerror*);
static bool __xrtTlsDialOwnedCancel(xtlsdial*);
static void __xrtTlsDialOwnedResources(xtlsdial*);



/* 设置 TLS Dial 当前线程错误。 */
static void __xrtTlsDialSetError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtTlsErrorCause(
		Kind,
		Code,
		sOperation,
		sMessage,
		SIZE_MAX,
		pCause
	);
}



/* 创建一个由 TLS Dial 独占的结构化错误。 */
static xerror* __xrtTlsDialErrorCreate(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtTlsDialSetError(
		Kind, Code, sOperation, sMessage, pCause
	);
	return xrtTakeError();
}



/* 增加 TLS Dial 引用。 */
XRT_API xtlsdial* xrtTlsDialRef(xtlsdial* pDial)
{
	xrtownershipscope m = {0};
	if (!pDial) { __xrtErrorSetInvalidArgument(); return NULL; }
	if (!xrtOwnershipMutationBegin(&m)) return NULL;
	bool held = (!pDial->OwnershipPolicy ||
		!(xrtAtomic32Load(&pDial->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED)) &&
		xrtRefRetain(&pDial->References) >= 0;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!held) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"retain-tls-dial",
			"TLS dial is null or already released",
			NULL
		);
		return NULL;
	}
	return pDial;
}



/* 释放最后一个引用及其尚未转移的网络对象。 */
XRT_API void xrtTlsDialDestroy(xtlsdial* pDial)
{
	xnetdial* pTransportDial;
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	xnetproxydial* pProxyDial;
#endif
	xtlsstream* pStream;

	if (!pDial) return;
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	int32 left = xrtRefRelease(&pDial->References);
	if (left < 0 || (!left && pDial->OwnershipPolicy && (pDial->RuntimeHeld ||
		pDial->RuntimeOwner.Data || pDial->RuntimeOwner.Previous ||
		xrtAtomic32Load(&pDial->OwnershipActive, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pDial->OwnershipStop, XMEMORY_ACQUIRE)))) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (left) return;
	if (pDial->OwnershipPolicy) {
		__xrtTlsDialOwnedResources(pDial); xrtErrorFree(pDial->OwnershipFinishError);
		xrtFree(pDial); return;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	pProxyDial = (xnetproxydial*)xrtAtomicPtrLoad(
		&pDial->ProxyDial,
		XMEMORY_ACQUIRE
	);
	if ( pProxyDial != NULL ) {
		xrtNetProxyDialDestroy(pProxyDial);
	} else
#endif
	{
		xrtNetDialDestroy(pTransportDial);
	}
	xrtTlsStreamDestroy(pStream);
	xrtErrorFree(pDial->Error);
	xrtFree(pDial);
}



/* 取消总超时 Timer；终态回调仍释放 Timer 引用。 */
static void __xrtTlsDialCancelTimer(xtlsdial* pDial)
{
	uint64 Id = xrtAtomic64Exchange(
		&pDial->Timer,
		0,
		XMEMORY_ACQ_REL
	);

	if ( (Id != 0) && !xrtAtomic32Load(
		&pDial->TimerDone,
		XMEMORY_ACQUIRE
	) && !__xrtNetEngineTimerCancelLifecycle(
		pDial->Engine,
		Id
	) ) {
		xrtClearError();
	}
}



/* 发布一次不可变终态并转移或释放 TLS Stream 调用方引用。 */
static void __xrtTlsDialFinish(
	xtlsdial* pDial,
	xnetresult Result,
	const xerror* pError
)
{
	uint32 iExpected = xrtAtomic32Load(
		&pDial->TerminalGate,
		XMEMORY_ACQUIRE
	);
	xtlsstream* pStream;

	if ( (iExpected != XRT_TLS_DIAL_GATE_OPEN) &&
		(iExpected != XRT_TLS_DIAL_GATE_CANCEL) &&
		(iExpected != XRT_TLS_DIAL_GATE_TIMEOUT) &&
		(iExpected != XRT_TLS_DIAL_GATE_WINNER) ) {
		return;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(iExpected != XRT_TLS_DIAL_GATE_WINNER) ) {
		return;
	}
	if ( !xrtAtomic32CompareExchange(
		&pDial->TerminalGate,
		&iExpected,
		XRT_TLS_DIAL_GATE_TERMINAL,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( iExpected == XRT_TLS_DIAL_GATE_CANCEL ) {
		Result = XNET_RESULT_CANCELLED;
	} else if ( iExpected == XRT_TLS_DIAL_GATE_TIMEOUT ) {
		Result = XNET_RESULT_TIMEOUT;
	}
	__xrtTlsDialCancelTimer(pDial);
	if (pDial->OwnershipPolicy) {
		pStream = pDial->OwnedStream; pDial->OwnedStream = NULL;
		xtlssession* session = pDial->OwnedSession; pDial->OwnedSession = NULL;
		xrtTlsSessionDestroy(session);
	} else {
		pStream = (xtlsstream*)xrtAtomicPtrExchange(&pDial->Stream, NULL, XMEMORY_ACQ_REL);
	}
	if ( Result == XNET_RESULT_OK ) {
		xrtAtomic32Store(
			&pDial->State,
			XTLS_DIAL_CONNECTED,
			XMEMORY_RELEASE
		);
	} else {
		if ( Result == XNET_RESULT_TIMEOUT ) {
			pDial->Error = __xrtTlsDialErrorCreate(
				XERR_TIMEOUT,
				XTLS_ERROR_HANDSHAKE,
				"dial-tls-stream",
				"TLS dial timed out before the secure stream opened",
				pError
			);
		} else if ( Result == XNET_RESULT_CANCELLED ) {
			pDial->Error = __xrtTlsDialErrorCreate(
				XERR_CANCELLED,
				XTLS_ERROR_CLOSED,
				"dial-tls-stream",
				"TLS dial was cancelled",
				pError
			);
		} else {
			pDial->Error = xrtErrorRef(pError);
			if ( pDial->Error == NULL ) {
				pDial->Error = __xrtTlsDialErrorCreate(
					XERR_IO,
					XTLS_ERROR_CLOSED,
					"dial-tls-stream",
					"TLS dial failed before the secure stream opened",
					NULL
				);
			}
		}
		xrtAtomic32Store(
			&pDial->State,
			Result == XNET_RESULT_CANCELLED ?
				XTLS_DIAL_CANCELLED : XTLS_DIAL_FAILED,
			XMEMORY_RELEASE
		);
	}
	if ( Result != XNET_RESULT_OK ) {
		xrtTlsStreamDestroy(pStream);
		pStream = NULL;
	}
	if ( pDial->Done != NULL ) {
		pDial->Done(
			pDial,
			Result,
			Result == XNET_RESULT_OK ? pStream : NULL,
			pDial->Error,
			pDial->DoneData
		);
	}
	if (pDial->OwnershipPolicy) {
		__xrtTlsDialOwnedTryRelease(pDial);
	} else if ( pDial->RuntimeHeld ) {
		pDial->RuntimeHeld = false;
		xrtTlsDialDestroy(pDial);
	}
}



/* TLS Open 先安装最终事件，再保持与 TCP Dial 一致的 Open-before-Done 顺序。 */
static void __xrtTlsDialStreamOpen(xtlsstream* pStream, ptr pData)
{
	xtlsdial* pDial = (xtlsdial*)pData;
	uint32 iExpected = XRT_TLS_DIAL_GATE_OPEN;

	if ( !xrtAtomic32CompareExchange(
		&pDial->TerminalGate,
		&iExpected,
		XRT_TLS_DIAL_GATE_WINNER,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		(void)__xrtTlsStreamAbortAccepted(pStream);
		return;
	}
	if ( !pDial->OwnershipPolicy && !xrtTlsStreamSetEvents(
		pStream,
		&pDial->StreamEvents,
		pDial->StreamData
	) ) {
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
	if (pDial->OwnershipPolicy) pDial->OwnershipOpened = true;
	xrtAtomic32Store(
		&pDial->State,
		XTLS_DIAL_CONNECTED,
		XMEMORY_RELEASE
	);
	if ( pDial->StreamEvents.Open != NULL ) {
		pDial->StreamEvents.Open(pStream, pDial->StreamData);
	}
	__xrtTlsDialFinish(pDial, XNET_RESULT_OK, NULL);
}



/* 握手前的 TLS Stream 关闭只通过 Dial 完成回调发布。 */
static void __xrtTlsDialStreamClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xtlsdial* pDial = (xtlsdial*)pData;

	(void)pStream;
	__xrtTlsDialFinish(pDial, Result, pError);
}



/* 判断显式取消或全过程超时是否已经赢得终态门。 */
static bool __xrtTlsDialStopping(const xtlsdial* pDial)
{
	uint32 iGate = xrtAtomic32Load(
		&pDial->TerminalGate,
		XMEMORY_ACQUIRE
	);

	return (iGate == XRT_TLS_DIAL_GATE_CANCEL) ||
		(iGate == XRT_TLS_DIAL_GATE_TIMEOUT);
}



#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
static void __xrtTlsDialTransportDoneBody(
	xnetresult Result,
	xnetstream* pTransport,
	const xerror* pError,
	ptr pData
);

/* 代理隧道完成：与 TCP 路径同一张事件表驱动 TLS，仅首参类型不同。 */
static void __xrtTlsDialProxyDone(
	xnetproxydial* pProxyDial,
	xnetresult Result,
	xnetstream* pTransport,
	const xerror* pError,
	ptr pData
)
{
	(void)pProxyDial;
	__xrtTlsDialTransportDoneBody(Result, pTransport, pError, pData);
}
#endif

/* TCP Dial 完成后，失败进入组合终态，成功则把传输引用交给 TLS Stream。 */
static void __xrtTlsDialTransportDoneBody(
	xnetresult Result,
	xnetstream* pTransport,
	const xerror* pError,
	ptr pData
)
{
	xtlsdial* pDial = (xtlsdial*)pData;
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);

	if ( Result != XNET_RESULT_OK ) {
		__xrtTlsStreamTransportFailed(pStream, Result, pError);
		return;
	}
	if ( (pStream == NULL) ||
		(xrtTlsStreamTransport(pStream) != pTransport) ) {
		__xrtTlsDialSetError(
			XERR_INTERNAL,
			XTLS_ERROR_INTERNAL,
			"dial-tls-stream",
			"TCP dial transferred an unexpected TLS transport",
			NULL
		);
		if ( pStream != NULL ) {
			__xrtTlsStreamTransportFailed(
				pStream,
				XNET_RESULT_ERROR,
				xrtGetError()
			);
		} else {
			__xrtTlsDialFinish(
				pDial,
				XNET_RESULT_ERROR,
				xrtGetError()
			);
		}
		(void)xrtNetStreamAbort(pTransport);
		xrtNetStreamDestroy(pTransport);
		return;
	}
	xrtAtomic32Store(
		&pDial->State,
		XTLS_DIAL_HANDSHAKE,
		XMEMORY_RELEASE
	);
	if ( __xrtTlsDialStopping(pDial) ) {
		(void)xrtTlsStreamAbort(pStream);
	}
}

static void __xrtTlsDialTransportDone(
	xnetdial* pTransportDial,
	xnetresult Result,
	xnetstream* pTransport,
	const xerror* pError,
	ptr pData
)
{
	(void)pTransportDial;
	__xrtTlsDialTransportDoneBody(Result, pTransport, pError, pData);
}



/* 取消当前活动的 TCP 或 TLS 阶段；调用方已经赢得终态门。 */
static void __xrtTlsDialCancelStage(xtlsdial* pDial)
{
	xnetdial* pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	xtlsstream* pStream;

#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	{
		xnetproxydial* pProxyDial = (xnetproxydial*)xrtAtomicPtrLoad(
			&pDial->ProxyDial,
			XMEMORY_ACQUIRE
		);

		if ( (pProxyDial != NULL) &&
			xrtNetProxyDialCancel(pProxyDial) ) {
			return;
		}
	}
#endif
	if ( (pTransportDial != NULL) &&
		xrtNetDialCancel(pTransportDial) ) {
		return;
	}
	pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);
	if ( (pStream != NULL) &&
		(xrtTlsStreamTransport(pStream) != NULL) ) {
		(void)xrtTlsStreamAbort(pStream);
	}
}



/* 总超时原子赢得终态门，再取消当前活动阶段。 */
static void __xrtTlsDialTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xtlsdial* pDial = (xtlsdial*)pData;
	uint32 iExpected = XRT_TLS_DIAL_GATE_OPEN;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(&pDial->TimerDone, 1, XMEMORY_RELEASE);
	(void)xrtAtomic64Exchange(
		&pDial->Timer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) &&
		xrtAtomic32CompareExchange(
			&pDial->TerminalGate,
			&iExpected,
			XRT_TLS_DIAL_GATE_TIMEOUT,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
		__xrtTlsDialCancelStage(pDial);
	}
	xrtTlsDialDestroy(pDial);
}



/* 初始化嵌套 TCP、TLS Stream 和全过程策略。 */
XRT_API void xrtTlsDialConfigInit(xtlsdialconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"init-tls-dial-config",
			"TLS dial config is null",
			NULL
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetDialConfigInit(&pConfig->Transport);
	xrtTlsStreamConfigInit(&pConfig->Stream);
	pConfig->ServerNameFromHost = true;
}



/* 统一构造直接与代理 TLS Dial；公开入口只决定传输建立策略。 */
static xtlsdial* __xrtTlsDialStart(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const void* pProxy,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData,
	xtlsdialproc pDone,
	ptr pDoneData
)
{
	xtlsdialconfig Config;
	xtlsclientconfig Tls;
	xtlsstreamevents PendingEvents;
	xtlssession* pSession;
	xtlsstream* pStream;
	xtlsdial* pDial;
	xnetdial* pTransportDial;
	xerror* pError;
	uint64 Id;

#if !defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	(void)pProxy;
#endif

	if ( (pEngine == NULL) || (pResolver == NULL) ||
		(sHost == NULL) || (sHost[0] == 0) ||
		(iPort == 0) || (pDone == NULL) ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"dial-tls-stream",
			"invalid TLS dial arguments",
			NULL
		);
		return NULL;
	}
	xrtTlsDialConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	xrtTlsClientConfigInit(&Tls);
	if ( pTls != NULL ) {
		Tls = *pTls;
	}
	if ( Config.ServerNameFromHost && (Tls.ServerName.Size == 0) ) {
		Tls.ServerName.Data = sHost;
		Tls.ServerName.Size = strlen(sHost);
	}
	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		pError = xrtTakeError();
		__xrtTlsDialSetError(
			XERR_CLOSED,
			XTLS_ERROR_CLOSED,
			"dial-tls-stream",
			"TLS dial could not hold the network engine",
			pError
		);
		xrtErrorFree(pError);
		return NULL;
	}
	pSession = xrtTlsClientCreate(&Tls, NULL);
	if ( pSession == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	if ( !__xrtTlsStreamLimits(&Config.Transport.Stream, pSession) ) {
		xrtTlsSessionDestroy(pSession);
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	pDial = (xtlsdial*)xrtCalloc(1, sizeof(*pDial));
	if ( pDial == NULL ) {
		xrtTlsSessionDestroy(pSession);
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	pDial->References = 2;
	xrtAtomic32Init(&pDial->State, XTLS_DIAL_RESOLVING);
	xrtAtomic32Init(
		&pDial->TerminalGate,
		XRT_TLS_DIAL_GATE_OPEN
	);
	xrtAtomic32Init(&pDial->TimerDone, 0);
	xrtAtomic64Init(&pDial->Timer, 0);
	xrtAtomicPtrInit(&pDial->TransportDial, NULL);
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	xrtAtomicPtrInit(&pDial->ProxyDial, NULL);
#endif
	xrtAtomicPtrInit(&pDial->Stream, NULL);
	pDial->Engine = pEngine;
	pDial->StreamData = pStreamData;
	pDial->Done = pDone;
	pDial->DoneData = pDoneData;
	pDial->Affinity = Config.Transport.Affinity;
	pDial->RuntimeHeld = true;
	if ( pStreamEvents != NULL ) {
		pDial->StreamEvents = *pStreamEvents;
	}
	memset(&PendingEvents, 0, sizeof(PendingEvents));
	PendingEvents.Open = __xrtTlsDialStreamOpen;
	PendingEvents.Close = __xrtTlsDialStreamClose;
	pStream = __xrtTlsStreamCreate(
		pSession,
		false,
		&Config.Stream,
		&PendingEvents,
		pDial
	);
	if ( pStream == NULL ) {
		xrtTlsSessionDestroy(pSession);
		xrtTlsDialDestroy(pDial);
		xrtTlsDialDestroy(pDial);
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	xrtAtomicPtrStore(&pDial->Stream, pStream, XMEMORY_RELEASE);
	if ( Config.Timeout != 0 ) {
		xrtTlsDialRef(pDial);
		Id = xrtNetEngineAfter(
			pEngine,
			Config.Transport.Affinity,
			Config.Timeout,
			__xrtTlsDialTimer,
			pDial
		);
		if ( Id == 0 ) {
			pError = xrtTakeError();
			xrtTlsDialDestroy(pDial);
			(void)xrtAtomicPtrExchange(
				&pDial->Stream,
				NULL,
				XMEMORY_ACQ_REL
			);
			__xrtTlsStreamDiscard(pStream);
			pDial->RuntimeHeld = false;
			xrtTlsDialDestroy(pDial);
			xrtTlsDialDestroy(pDial);
			__xrtNetEngineObjectRelease(pEngine);
			xrtSetError(pError);
			xrtErrorFree(pError);
			return NULL;
		}
		xrtAtomic64Store(&pDial->Timer, Id, XMEMORY_RELEASE);
		if ( xrtAtomic32Load(&pDial->TimerDone, XMEMORY_ACQUIRE) ) {
			(void)xrtAtomic64Exchange(
				&pDial->Timer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	if ( pProxy != NULL ) {
		xnetproxydialconfig tProxyCfg;
		xnetproxydial* pProxyDial;

		xrtNetProxyDialConfigInit(&tProxyCfg);
		tProxyCfg.Transport = Config.Transport;
		/* The enclosing TLS timer is the sole end-to-end deadline. A zero
		 * proxy timeout preserves Transport's own per-stage limits. */
		tProxyCfg.Timeout = 0;
		pProxyDial = xrtNetProxyDial(
			pEngine,
			pResolver,
			(const xnetproxy*)pProxy,
			sHost,
			iPort,
			&tProxyCfg,
			__xrtTlsStreamTransportEventTable(),
			pStream,
			__xrtTlsDialProxyDone,
			pDial
		);
		if ( pProxyDial == NULL ) {
			pError = xrtTakeError();
			__xrtTlsDialCancelTimer(pDial);
			(void)xrtAtomicPtrExchange(
				&pDial->Stream,
				NULL,
				XMEMORY_ACQ_REL
			);
			__xrtTlsStreamDiscard(pStream);
			pDial->RuntimeHeld = false;
			xrtTlsDialDestroy(pDial);
			xrtTlsDialDestroy(pDial);
			__xrtNetEngineObjectRelease(pEngine);
			xrtSetError(pError);
			xrtErrorFree(pError);
			return NULL;
		}
		xrtAtomicPtrStore(
			&pDial->ProxyDial,
			pProxyDial,
			XMEMORY_RELEASE
		);
		if ( __xrtTlsDialStopping(pDial) ) {
			/* Submission and its Worker may run concurrently.  If CONNECT
			 * already won before the handle was published, proxy cancellation
			 * correctly refuses; the shared stage canceller then falls through
			 * to the attached TLS Stream instead of losing the terminal request. */
			__xrtTlsDialCancelStage(pDial);
		}
		__xrtNetEngineObjectRelease(pEngine);
		return pDial;
	}
#endif
	pTransportDial = xrtNetDial(
		pEngine,
		pResolver,
		sHost,
		iPort,
		&Config.Transport,
		__xrtTlsStreamTransportEventTable(),
		pStream,
		__xrtTlsDialTransportDone,
		pDial
	);
	if ( pTransportDial == NULL ) {
		pError = xrtTakeError();
		__xrtTlsDialCancelTimer(pDial);
		(void)xrtAtomicPtrExchange(
			&pDial->Stream,
			NULL,
			XMEMORY_ACQ_REL
		);
		__xrtTlsStreamDiscard(pStream);
		pDial->RuntimeHeld = false;
		xrtTlsDialDestroy(pDial);
		xrtTlsDialDestroy(pDial);
		__xrtNetEngineObjectRelease(pEngine);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pDial->TransportDial,
		pTransportDial,
		XMEMORY_RELEASE
	);
	if ( __xrtTlsDialStopping(pDial) ) {
		/* Cover the same submit/complete race for the direct transport. */
		__xrtTlsDialCancelStage(pDial);
	}
	__xrtNetEngineObjectRelease(pEngine);
	return pDial;
}



/* 复用 TCP Dial 的解析和地址竞速，TLS 层只负责安全握手发布。 */
XRT_API xtlsdial* xrtTlsDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData,
	xtlsdialproc pDone,
	ptr pDoneData
)
{
	return __xrtTlsDialStart(
		pEngine, pResolver, NULL, sHost, iPort, pTls, pConfig,
		pStreamEvents, pStreamData, pDone, pDoneData
	);
}



#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
/* 建立代理隧道后继续同一 TLS 状态机；不复制握手和终态逻辑。 */
XRT_API xtlsdial* xrtTlsDialProxy(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xnetproxy* pProxy,
	cstr sHost,
	uint16 iPort,
	const xtlsclientconfig* pTls,
	const xtlsdialconfig* pConfig,
	const xtlsstreamevents* pStreamEvents,
	ptr pStreamData,
	xtlsdialproc pDone,
	ptr pDoneData
)
{
	if ( pProxy == NULL ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"dial-tls-proxy",
			"TLS proxy is null",
			NULL
		);
		return NULL;
	}
	return __xrtTlsDialStart(
		pEngine, pResolver, pProxy, sHost, iPort, pTls, pConfig,
		pStreamEvents, pStreamData, pDone, pDoneData
	);
}
#endif



/* 从任意线程让解析、TCP 或 TLS 握手中当前有效的阶段接受一次取消。 */
XRT_API bool xrtTlsDialCancel(xtlsdial* pDial)
{
	uint32 iExpected = XRT_TLS_DIAL_GATE_OPEN;
	if (pDial && pDial->OwnershipPolicy) return __xrtTlsDialOwnedCancel(pDial);

	if ( pDial == NULL ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"cancel-tls-dial",
			"TLS dial is null",
			NULL
		);
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pDial->TerminalGate,
		&iExpected,
		XRT_TLS_DIAL_GATE_CANCEL,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	__xrtTlsDialCancelStage(pDial);
	return true;
}



/* 返回公开 TLS 阶段；解析和连接阶段直接映射 TCP Dial 快照。 */
XRT_API xtlsdialstate xrtTlsDialState(const xtlsdial* pDial)
{
	xtlsdialstate State;
	xnetdial* pTransportDial;
	xnetdialstate TransportState;

	if ( pDial == NULL ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"state-tls-dial",
			"TLS dial is null",
			NULL
		);
		return XTLS_DIAL_FAILED;
	}
	State = (xtlsdialstate)xrtAtomic32Load(
		&pDial->State,
		XMEMORY_ACQUIRE
	);
	if ( (State != XTLS_DIAL_RESOLVING) &&
		(State != XTLS_DIAL_CONNECTING) ) {
		return State;
	}
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	{
		xnetproxydial* pProxyDial = (xnetproxydial*)xrtAtomicPtrLoad(
			&pDial->ProxyDial,
			XMEMORY_ACQUIRE
		);

		if ( pProxyDial != NULL ) {
			xnetproxydialstate ProxyState = xrtNetProxyDialState(pProxyDial);

			if ( ProxyState == XNET_PROXY_DIAL_CONNECTING
				|| ProxyState == XNET_PROXY_DIAL_HANDSHAKE ) {
				return XTLS_DIAL_CONNECTING;
			}
			if ( ProxyState == XNET_PROXY_DIAL_CONNECTED ) {
				return XTLS_DIAL_HANDSHAKE;
			}
			return State;
		}
	}
#endif
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		return State;
	}
	TransportState = xrtNetDialState(pTransportDial);
	if ( TransportState == XNET_DIAL_CONNECTING ) {
		return XTLS_DIAL_CONNECTING;
	}
	if ( TransportState == XNET_DIAL_CONNECTED ) {
		return XTLS_DIAL_HANDSHAKE;
	}
	return State;
}



/* 终态后返回 TLS、TCP 或总超时错误原因链。 */
XRT_API const xerror* xrtTlsDialError(const xtlsdial* pDial)
{
	xtlsdialstate State;

	if ( pDial == NULL ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"error-tls-dial",
			"TLS dial is null",
			NULL
		);
		return NULL;
	}
	State = xrtTlsDialState(pDial);
	return ((State == XTLS_DIAL_FAILED) ||
		(State == XTLS_DIAL_CANCELLED)) ? pDial->Error : NULL;
}



/* 复用底层 TCP Dial 的无锁统计，不复制地址和候选计数。 */
XRT_API bool xrtTlsDialTransportStats(
	const xtlsdial* pDial,
	xnetdialstats* pStats
)
{
	xnetdial* pTransportDial;

	if ( (pDial == NULL) || (pStats == NULL) ) {
		__xrtTlsDialSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"stats-tls-dial",
			"TLS dial or statistics output is null",
			NULL
		);
		return false;
	}
#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
	{
		xnetproxydial* pProxyDial = (xnetproxydial*)xrtAtomicPtrLoad(
			&pDial->ProxyDial,
			XMEMORY_ACQUIRE
		);

		if ( pProxyDial != NULL ) {
			xnetproxydialstats tProxyStats;

			if ( !xrtNetProxyDialStats(pProxyDial, &tProxyStats) ) {
				return false;
			}
			*pStats = tProxyStats.Transport;
			return true;
		}
	}
#endif
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		__xrtTlsDialSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"stats-tls-dial",
			"TLS dial transport is not available",
			NULL
		);
		return false;
	}
	return xrtNetDialStats(pTransportDial, pStats);
}

/* Owned mode uses the same terminal gate and error vocabulary as the legacy
 * entry, but never installs borrowed TCP events or swaps TLS event tables. */
static bool __xrtTlsDialOwnedEnter(xtlsdial* d, bool submit)
{
	xrtownershipscope m = {0};
	if (!d || !xrtOwnershipMutationBegin(&m)) return false;
	bool entered = false;
	uint32 active = xrtAtomic32Load(&d->OwnershipActive, XMEMORY_ACQUIRE);
	if (!(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED) &&
		(!submit || !xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE))) {
		while (active < UINT32_MAX) {
			uint32 expected = active;
			if (xrtAtomic32CompareExchange(&d->OwnershipActive, &expected, active + 1,
				XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) { entered = true; break; }
			active = expected;
		}
	}
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!entered) __xrtErrorSetInvalidState();
	return entered;
}
static void __xrtTlsDialOwnedLeave(xtlsdial* d)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (!xrtAtomic32FetchSub(&d->OwnershipActive, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
}
static xtlsdial* __xrtTlsDialOwnedFrame(ptr data)
{
	xtlsdial* d = xrtTlsDialRef(data);
	if (!d || !__xrtTlsDialOwnedEnter(d, false)) abort();
	return d;
}
static void __xrtTlsDialOwnedFrameEnd(xtlsdial* d)
{ __xrtTlsDialOwnedLeave(d); xrtTlsDialDestroy(d); }
static bool __xrtTlsDialOwnedBusy(const void* data)
{
	const xtlsdial* d = data;
	return d && (xrtAtomic32Load(&d->OwnershipActive, XMEMORY_ACQUIRE) ||
		(d->OwnershipStorageHeld && xrtNetEngineOwnershipQuiescenceV1Get()->Busy(d->Engine)));
}
static void __xrtTlsDialOwnedTryRelease(xtlsdial* d)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	__xrtSpinLock(&d->OwnershipLock);
	bool release = d->RuntimeHeld && (d->OwnershipPublished || d->OwnershipAbandoned) &&
		d->OwnershipTransportDone && xrtAtomic32Load(&d->TimerDone, XMEMORY_ACQUIRE) &&
		!xrtAtomic32Load(&d->OwnershipStop, XMEMORY_ACQUIRE) &&
		xrtAtomic32Load(&d->TerminalGate, XMEMORY_ACQUIRE) == XRT_TLS_DIAL_GATE_TERMINAL;
	if (release) {
		const xnetruntimeownershipv1* policy = NULL;
		if (__xrtNetEngineRuntimeTake(d->Engine, &d->RuntimeOwner, &policy) != d ||
			policy != xrtTlsDialRuntimeOwnershipV1Get() || !d->OwnershipEngineHeld) abort();
		d->RuntimeHeld = false; d->OwnershipEngineHeld = false;
		xrtAtomic32Store(&d->OwnershipState, XRT_TLS_DIAL_OWN_READY, XMEMORY_RELEASE);
	}
	__xrtSpinUnlock(&d->OwnershipLock);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (release) { __xrtNetEngineObjectRelease(d->Engine); xrtTlsDialDestroy(d); }
}
/* The stage slots below are Worker-confined. A cancellation thread cannot
 * race a raw pointer load against handoff/destruction of the creator credit. */
static void __xrtTlsDialOwnedCancelStage(xtlsdial* d)
{
	if (!xrtNetWorkerIsCurrent(d->Worker)) abort();
	xnetdial* tcp = xrtAtomicPtrLoad(&d->TransportDial, XMEMORY_ACQUIRE);
	if (tcp && xrtNetDialCancel(tcp)) return;
	if (d->OwnedStream) (void)__xrtTlsStreamAbortAccepted(d->OwnedStream);
}
static void __xrtTlsDialOwnedStopTask(xnetworker* worker, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data); (void)worker;
	__xrtTlsDialOwnedCancelStage(d);
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	xrtAtomic32Store(&d->OwnershipStop, 0, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	__xrtTlsDialOwnedTryRelease(d); __xrtTlsDialOwnedFrameEnd(d);
}
static bool __xrtTlsDialOwnedPostStop(xtlsdial* d)
{
	/* Called under Mutation/Freeze. The embedded command borrows the actual
	 * registered runtime credit, which cannot retire while OwnershipStop is set. */
	__xrtSpinLock(&d->OwnershipLock);
	if (!d->RuntimeHeld || xrtAtomic32Load(&d->OwnershipStop, XMEMORY_ACQUIRE)) {
		__xrtSpinUnlock(&d->OwnershipLock); return true;
	}
	xrtAtomic32Store(&d->OwnershipStop, 1, XMEMORY_RELEASE);
	bool posted = __xrtNetEnginePostInternal(d->Worker, &d->OwnershipStopCommand, __xrtTlsDialOwnedStopTask, d);
	if (!posted) xrtAtomic32Store(&d->OwnershipStop, 0, XMEMORY_RELEASE);
	__xrtSpinUnlock(&d->OwnershipLock); return posted;
}
static bool __xrtTlsDialOwnedCancel(xtlsdial* d)
{
	if (!__xrtTlsDialOwnedEnter(d, true)) return false;
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	uint32 expected = XRT_TLS_DIAL_GATE_OPEN;
	bool accepted = xrtAtomic32CompareExchange(&d->TerminalGate, &expected,
		XRT_TLS_DIAL_GATE_CANCEL, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
	if (accepted) (void)__xrtTlsDialOwnedPostStop(d);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	__xrtTlsDialOwnedLeave(d); return accepted;
}
static void __xrtTlsDialOwnedTimer(xnetworker* worker, uint64 id, xnetresult result, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data); (void)worker; (void)id;
	xrtAtomic32Store(&d->TimerDone, 1, XMEMORY_RELEASE);
	xrtAtomic64Store(&d->Timer, 0, XMEMORY_RELEASE);
	uint32 expected = XRT_TLS_DIAL_GATE_OPEN;
	if (result == XNET_RESULT_OK && xrtAtomic32CompareExchange(&d->TerminalGate, &expected,
		XRT_TLS_DIAL_GATE_TIMEOUT, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) __xrtTlsDialOwnedCancelStage(d);
	__xrtTlsDialOwnedTryRelease(d); __xrtTlsDialOwnedFrameEnd(d);
}
static void __xrtTlsDialOwnedDrop(const void* data) { xrtTlsDialDestroy((xtlsdial*)data); }
static void __xrtTlsDialOwnedTimerDrop(ptr data) { xrtTlsDialDestroy(data); }
XRT_API const xnettimerownershipv1* xrtTlsDialTimerOwnershipV1Get(void)
{
	static const xnettimerownershipv1 p = {sizeof(p), __xrtTlsDialOwnedTimer,
		__xrtTlsDialOwnedTimerDrop, &__xrtTlsDialOwnershipOps}; return &p;
}
static void __xrtTlsDialOwnedOpen(xtlsstream* stream, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data);
	__xrtTlsDialStreamOpen(stream, d); __xrtTlsDialOwnedFrameEnd(d);
}
static void __xrtTlsDialOwnedClose(xtlsstream* stream, xnetresult result, const xerror* error, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data);
	if (d->OwnershipOpened && d->StreamEvents.Close) d->StreamEvents.Close(stream, result, error, d->StreamData);
	if (result == XNET_RESULT_OK && !d->OwnershipOpened) {
		__xrtTlsDialSetError(XERR_CLOSED, XTLS_ERROR_CLOSED, "dial-tls-stream", "TLS stream closed before opening", error);
		__xrtTlsDialFinish(d, XNET_RESULT_ERROR, xrtGetError());
	} else __xrtTlsDialFinish(d, result, error);
	__xrtTlsDialOwnedFrameEnd(d);
}
static void __xrtTlsDialOwnedRead(xtlsstream* stream, const xnetbuf* buffer, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data);
	if (d->OwnershipOpened && d->StreamEvents.Read) d->StreamEvents.Read(stream, buffer, d->StreamData);
	__xrtTlsDialOwnedFrameEnd(d);
}
#define XRT_TLS_DIAL_FORWARD_EVENT(Name) \
static void __xrtTlsDialOwned##Name(xtlsstream* stream, ptr data) \
{ xtlsdial* d = __xrtTlsDialOwnedFrame(data); \
  if (d->OwnershipOpened && d->StreamEvents.Name) d->StreamEvents.Name(stream, d->StreamData); \
  __xrtTlsDialOwnedFrameEnd(d); }
XRT_TLS_DIAL_FORWARD_EVENT(End)
XRT_TLS_DIAL_FORWARD_EVENT(Writable)
XRT_TLS_DIAL_FORWARD_EVENT(Drain)
XRT_TLS_DIAL_FORWARD_EVENT(Ticket)
#undef XRT_TLS_DIAL_FORWARD_EVENT
XRT_API const xtlsstreamownershipv1* xrtTlsDialStreamOwnershipV1Get(void)
{
	static const xtlsstreamownershipv1 p = {sizeof(p), {__xrtTlsDialOwnedOpen, NULL,
		__xrtTlsDialOwnedEnd, __xrtTlsDialOwnedWritable, __xrtTlsDialOwnedDrain,
		__xrtTlsDialOwnedClose, __xrtTlsDialOwnedTicket}, __xrtTlsDialOwnedDrop, &__xrtTlsDialOwnershipOps}; return &p;
}
XRT_API const xtlsstreamownershipv1* xrtTlsDialStreamPushOwnershipV1Get(void)
{
	static const xtlsstreamownershipv1 p = {sizeof(p), {__xrtTlsDialOwnedOpen, __xrtTlsDialOwnedRead,
		__xrtTlsDialOwnedEnd, __xrtTlsDialOwnedWritable, __xrtTlsDialOwnedDrain,
		__xrtTlsDialOwnedClose, __xrtTlsDialOwnedTicket}, __xrtTlsDialOwnedDrop, &__xrtTlsDialOwnershipOps}; return &p;
}
static void __xrtTlsDialOwnedTransportDone(xtlsdial* d, xnetresult result, xnetstream* tcp, const xerror* error)
{
	if (result != XNET_RESULT_OK || __xrtTlsDialStopping(d)) {
		if (tcp) { (void)xrtNetStreamAbort(tcp); xrtNetStreamDestroy(tcp); }
		__xrtTlsDialFinish(d, result == XNET_RESULT_OK ? XNET_RESULT_CANCELLED : result, error);
	} else {
		xrtAtomic32Store(&d->State, XTLS_DIAL_HANDSHAKE, XMEMORY_RELEASE);
		xtlssession* session = d->OwnedSession; d->OwnedSession = NULL;
		const xtlsstreamownershipv1* policy = d->StreamEvents.Read ?
			xrtTlsDialStreamPushOwnershipV1Get() : xrtTlsDialStreamOwnershipV1Get();
		bool held = xrtTlsDialRef(d) != NULL;
		/* A genuine, correctly typed owned slot is published by Attach before
		 * it may invoke callbacks. Completion can synchronously clear the slot. */
		if (!held || !xrtTlsStreamAttachOwnedV1(tcp, session, &d->OwnedConfig, d, policy, &d->OwnedStream)) {
			xerror* failure = xrtTakeError();
			if (held) xrtTlsDialDestroy(d);
			xrtTlsSessionDestroy(session); (void)xrtNetStreamAbort(tcp); xrtNetStreamDestroy(tcp);
			__xrtTlsDialFinish(d, XNET_RESULT_ERROR, failure); xrtErrorFree(failure);
		} else if (__xrtTlsDialStopping(d) && d->OwnedStream) (void)__xrtTlsStreamAbortAccepted(d->OwnedStream);
	}
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	__xrtSpinLock(&d->OwnershipLock);
	d->OwnershipTransportDone = true;
	__xrtSpinUnlock(&d->OwnershipLock);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	__xrtTlsDialOwnedTryRelease(d);
}
static void __xrtTlsDialOwnedTcpDone(xnetdial* tcp, xnetresult result, xnetstream* stream, const xerror* error, ptr data)
{
	xtlsdial* d = __xrtTlsDialOwnedFrame(data); (void)tcp;
	__xrtTlsDialOwnedTransportDone(d, result, stream, error); __xrtTlsDialOwnedFrameEnd(d);
}
XRT_API const xnetdialownershipv1* xrtTlsDialTransportOwnershipV1Get(void)
{
	static const xnetdialownershipv1 p = {sizeof(p), __xrtTlsDialOwnedTcpDone,
		__xrtTlsDialOwnedDrop, &__xrtTlsDialOwnershipOps}; return &p;
}
static void __xrtTlsDialOwnedRuntimeDrop(ptr data) { xrtTlsDialDestroy(data); }
XRT_API const xnetruntimeownershipv1* xrtTlsDialRuntimeOwnershipV1Get(void)
{
	static const xnetruntimeownershipv1 p = {sizeof(p), &__xrtTlsDialOwnershipOps,
		__xrtTlsDialOwnedRuntimeDrop, NULL, 0}; return &p;
}
/* Detach under Mutation; destructors, configuration Drop and Engine release
 * run outside the global domain. Session always dies before its context. */
static void __xrtTlsDialOwnedResources(xtlsdial* d)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	xnetdial* tcp = xrtAtomicPtrExchange(&d->TransportDial, NULL, XMEMORY_ACQ_REL);
	xtlssession* session = d->OwnedSession; d->OwnedSession = NULL;
	xtlsstream* stream = d->OwnedStream; d->OwnedStream = NULL;
	xerror* error = d->Error; d->Error = NULL;
	ptr data = d->OwnershipDataHeld ? d->DoneData : NULL;
	d->OwnershipDataHeld = false; d->DoneData = NULL; d->StreamData = NULL;
	xnetengine* engine = d->Engine; bool operational = d->OwnershipEngineHeld, storage = d->OwnershipStorageHeld;
	d->Engine = NULL; d->OwnershipEngineHeld = false; d->OwnershipStorageHeld = false;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	xrtTlsSessionDestroy(session); xrtTlsStreamDestroy(stream); xrtNetDialDestroy(tcp); xrtErrorFree(error);
	if (data) d->OwnershipPolicy->Drop(data);
	if (operational) __xrtNetEngineObjectRelease(engine);
	if (storage) __xrtNetEngineStorageRelease(engine);
}
XRT_API xtlsdial* xrtTlsDialOwnedV1(xnetengine* engine, xnetresolver* resolver, cstr host, uint16 port,
	const xtlsclientconfig* tls, const xtlsdialconfig* config, ptr data, const xtlsdialownershipv1* policy)
{
	if (!engine || !resolver || !host || !host[0] || !port || !data || !policy || policy->size != sizeof(*policy) ||
		!policy->Done || !policy->Drop || !policy->Ops || !policy->Ops->Count || !policy->Ops->Trace) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	xtlsdialconfig cfg; xrtTlsDialConfigInit(&cfg); if (config) cfg = *config;
	/* The pinned language ABI uses an unsigned duration; native uses signed
	 * milliseconds. Reject negative nonzero values without signedness tricks. */
	if ((cfg.Timeout != 0 && cfg.Timeout <= 0) || !xrtNetEngineWorkerCount(engine) || !xrtNetDialConfigValid(&cfg.Transport)) {
		__xrtErrorSetInvalidArgument(); return NULL;
	}
	if (!__xrtNetEngineObjectHold(engine)) return NULL;
	if (!__xrtNetEngineStorageRetain(engine)) { __xrtNetEngineObjectRelease(engine); return NULL; }
	xtlsdial* d = xrtCalloc(1, sizeof(*d));
	if (!d) { __xrtNetEngineObjectRelease(engine); __xrtNetEngineStorageRelease(engine); return NULL; }
	d->References = 2; d->OwnershipPolicy = policy; d->Engine = engine;
	d->OwnershipEngineHeld = true; d->OwnershipStorageHeld = true; d->RuntimeHeld = true;
	d->OwnershipDataHeld = true; d->StreamData = data; d->DoneData = data; d->Done = policy->Done;
	d->StreamEvents = policy->StreamEvents; d->OwnedConfig = cfg.Stream; d->Affinity = cfg.Transport.Affinity;
	xrtAtomic32Init(&d->OwnershipActive, 1); xrtAtomic32Init(&d->OwnershipState, 0);
	xrtAtomic32Init(&d->OwnershipStop, 0); xrtAtomicPtrInit(&d->OwnershipClaim, NULL);
	xrtAtomic32Init(&d->State, XTLS_DIAL_RESOLVING); xrtAtomic32Init(&d->TerminalGate, XRT_TLS_DIAL_GATE_OPEN);
	xrtAtomic32Init(&d->TimerDone, cfg.Timeout ? 0 : 1); xrtAtomic64Init(&d->Timer, 0);
	xrtAtomicPtrInit(&d->TransportDial, NULL); xrtAtomicPtrInit(&d->Stream, NULL);
	#if defined(XRT_FEATURE_TLS_STREAM_DIAL_PROXY)
		xrtAtomicPtrInit(&d->ProxyDial, NULL);
	#endif
	d->Worker = xrtNetEngineWorker(engine, (uint32)(d->Affinity % xrtNetEngineWorkerCount(engine)));
	if (!__xrtNetEngineRuntimeAdopt(engine, &d->RuntimeOwner, d, xrtTlsDialRuntimeOwnershipV1Get())) {
		d->RuntimeHeld = false; d->OwnershipDataHeld = false; __xrtTlsDialOwnedLeave(d);
		xrtTlsDialDestroy(d); xrtTlsDialDestroy(d); return NULL;
	}
	xtlsclientconfig client; xrtTlsClientConfigInit(&client); if (tls) client = *tls;
	if (cfg.ServerNameFromHost && !client.ServerName.Size) { client.ServerName.Data = host; client.ServerName.Size = strlen(host); }
	d->OwnedSession = xrtTlsClientCreate(&client, NULL);
	bool ready = d->OwnedSession && __xrtTlsStreamLimits(&cfg.Transport.Stream, d->OwnedSession);
	if (ready && cfg.Timeout) {
		bool held = xrtTlsDialRef(d) != NULL;
		/* The pinned wait ABI declares an absolute xdeadline; native Schedule
		 * accepts a relative duration. Use the declared contract, never a cast
		 * or an assumption that the two numeric representations are equivalent. */
		#if defined(XRT_DEADLINE_NEVER)
			uint64 id = held ? xrtNetEngineScheduleOwnedV1(engine, d->Affinity,
				xrtDeadlineAfter(cfg.Timeout), d, xrtTlsDialTimerOwnershipV1Get()) : 0;
		#else
			uint64 id = held ? xrtNetEngineScheduleOwnedV1(engine, d->Affinity,
				cfg.Timeout, d, xrtTlsDialTimerOwnershipV1Get()) : 0;
		#endif
		if (!id) { if (held) xrtTlsDialDestroy(d); ready = false; xrtAtomic32Store(&d->TimerDone, 1, XMEMORY_RELEASE); }
		else { xrtAtomic64Store(&d->Timer, id, XMEMORY_RELEASE);
			if (xrtAtomic32Load(&d->TimerDone, XMEMORY_ACQUIRE)) xrtAtomic64Store(&d->Timer, 0, XMEMORY_RELEASE); }
	} else if (!ready) xrtAtomic32Store(&d->TimerDone, 1, XMEMORY_RELEASE);
	xnetdial* tcp = NULL;
	if (ready) {
		bool held = xrtTlsDialRef(d) != NULL;
		tcp = held ? xrtNetDialOwnedV1(engine, resolver, host, port, &cfg.Transport, NULL, NULL, d, xrtTlsDialTransportOwnershipV1Get()) : NULL;
		if (!tcp && held) xrtTlsDialDestroy(d);
	}
	if (!tcp) {
		xerror* failure = xrtTakeError(); __xrtTlsDialCancelTimer(d);
		xtlssession* session = d->OwnedSession; d->OwnedSession = NULL; xrtTlsSessionDestroy(session);
		xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
		__xrtSpinLock(&d->OwnershipLock);
		d->OwnershipDataHeld = false; d->OwnershipAbandoned = true; d->OwnershipTransportDone = true;
		xrtAtomic32Store(&d->TerminalGate, XRT_TLS_DIAL_GATE_TERMINAL, XMEMORY_RELEASE);
		xrtAtomic32Store(&d->State, XTLS_DIAL_FAILED, XMEMORY_RELEASE);
		__xrtSpinUnlock(&d->OwnershipLock);
		if (!xrtOwnershipScopeEnd(&m)) abort();
		__xrtTlsDialOwnedTryRelease(d); __xrtTlsDialOwnedLeave(d); xrtTlsDialDestroy(d);
		xrtClearError(); xrtSetErrorTake(failure); return NULL;
	}
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	__xrtSpinLock(&d->OwnershipLock);
	xrtAtomicPtrStore(&d->TransportDial, tcp, XMEMORY_RELEASE); d->OwnershipPublished = true;
	__xrtSpinUnlock(&d->OwnershipLock);
	if (__xrtTlsDialStopping(d)) (void)__xrtTlsDialOwnedPostStop(d);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	__xrtTlsDialOwnedTryRelease(d); __xrtTlsDialOwnedLeave(d); return d;
}
static bool __xrtTlsDialOwnedCount(const void* data, size_t* count)
{
	const xtlsdial* d = data;
	if (!d || !count || !d->OwnershipPolicy || __xrtTlsDialOwnedBusy(d) ||
		(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED) ||
		!d->OwnershipStorageHeld || (!d->OwnershipPublished && !d->OwnershipAbandoned) ||
		(!d->OwnershipAbandoned && (!d->OwnershipDataHeld || !d->DoneData))) return false;
	if (d->RuntimeHeld) {
		if (!d->OwnershipEngineHeld || d->RuntimeOwner.Data != d || !d->RuntimeOwner.Previous ||
			d->RuntimeOwner.Engine != d->Engine || d->RuntimeOwner.Policy != xrtTlsDialRuntimeOwnershipV1Get()) return false;
	} else if (d->OwnershipEngineHeld || d->RuntimeOwner.Data || d->RuntimeOwner.Previous ||
		!(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_READY)) return false;
	int32 refs = __xrtAtomicRefLoad(&d->References); if (refs <= 0) return false;
	*count = (size_t)refs; return true;
}
static bool __xrtTlsDialOwnedTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const xtlsdial* d = data; size_t count;
	if (!visit || !__xrtTlsDialOwnedCount(d, &count)) return false;
	if (!visit(xrtNetEngineOwnership(d->Engine), context) ||
		(d->OwnershipEngineHeld && !visit(xrtNetEngineOwnership(d->Engine), context)) ||
		(d->OwnershipDataHeld && !visit((xrtownershipref){d->DoneData, d->OwnershipPolicy->Ops}, context))) return false;
	xnetdial* tcp = xrtAtomicPtrLoad(&d->TransportDial, XMEMORY_ACQUIRE);
	return (!tcp || visit(xrtNetDialOwnership(tcp), context)) &&
		(!d->OwnedStream || visit(xrtTlsStreamOwnership(d->OwnedStream), context)) &&
		(!d->OwnedSession || xrtTlsSessionBufferOwnershipTrace(d->OwnedSession, visit, context)) &&
		(!d->Error || visit(xrtErrorOwnership(d->Error), context));
}
static const xrtownershipops __xrtTlsDialOwnershipOps = {__xrtTlsDialOwnedCount, __xrtTlsDialOwnedTrace};
XRT_API xrtownershipref xrtTlsDialOwnership(const xtlsdial* d)
{ return (xrtownershipref){d, d ? &__xrtTlsDialOwnershipOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtTlsDialOwnershipQuiescenceV1Get(void)
{ static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtTlsDialOwnershipOps, __xrtTlsDialOwnedBusy}; return &p; }
static bool __xrtTlsDialOwnedHold(const void* data) { return xrtTlsDialRef((xtlsdial*)data) != NULL; }
static bool __xrtTlsDialOwnedReady(const void* data)
{
	const xtlsdial* d = data; size_t count;
	return __xrtTlsDialOwnedCount(d, &count) && !d->RuntimeHeld && !d->OwnershipEngineHeld &&
		d->OwnershipTransportDone && !d->OwnedStream && !d->OwnedSession &&
		xrtAtomic32Load(&d->TimerDone, XMEMORY_ACQUIRE) && !xrtAtomic32Load(&d->OwnershipStop, XMEMORY_ACQUIRE) &&
		xrtAtomic32Load(&d->TerminalGate, XMEMORY_ACQUIRE) == XRT_TLS_DIAL_GATE_TERMINAL;
}
static bool __xrtTlsDialOwnedClaim(const void* data, const void* token)
{
	xtlsdial* d = (xtlsdial*)data; size_t count; ptr old = xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!token || (old && old != token) || !__xrtTlsDialOwnedCount(d, &count)) return false;
	xrtAtomicPtrStore(&d->OwnershipClaim, (ptr)token, XMEMORY_RELEASE); return true;
}
static void __xrtTlsDialOwnedRestore(const void* data, const void* token)
{
	xtlsdial* d = (xtlsdial*)data;
	if (!token || xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED)) abort();
	xrtAtomicPtrStore(&d->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static xrtownershipprepareresult __xrtTlsDialOwnedPrepare(const void* data, const void* token)
{
	xtlsdial* d = (xtlsdial*)data; xrtownershipscope f = {0};
	if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED)) abort();
	xrtownershipprepareresult result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtTlsDialOwnedReady(d)) result = XRT_OWNERSHIP_PREPARE_READY;
	else if (!__xrtTlsDialOwnedBusy(d)) {
		uint32 expected = XRT_TLS_DIAL_GATE_OPEN;
		(void)xrtAtomic32CompareExchange(&d->TerminalGate, &expected, XRT_TLS_DIAL_GATE_CANCEL, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
		if (!__xrtTlsDialOwnedPostStop(d)) result = XRT_OWNERSHIP_PREPARE_FAILED;
	}
	if (!xrtOwnershipScopeEnd(&f)) abort();
	return result;
}
static void __xrtTlsDialOwnedClear(const void* data, const void* token)
{
	xtlsdial* d = (xtlsdial*)data;
	if (!token || xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE) != token || !__xrtTlsDialOwnedReady(d)) abort();
	xrtAtomic32Store(&d->OwnershipState, XRT_TLS_DIAL_OWN_READY | XRT_TLS_DIAL_OWN_CLEARED, XMEMORY_RELEASE);
}
static bool __xrtTlsDialOwnedFinish(const void* data, const void* token)
{
	xtlsdial* d = (xtlsdial*)data; xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) return false;
	if (!token || xrtAtomicPtrLoad(&d->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		!(xrtAtomic32Load(&d->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_DIAL_OWN_CLEARED)) abort();
	if (d->OwnershipFinishing) { if (!xrtOwnershipScopeEnd(&m)) abort(); return false; }
	if (d->OwnershipFinished) {
		xerror* failure = d->OwnershipFinishError; if (!xrtOwnershipScopeEnd(&m)) abort();
		if (failure && !xrtGetError()) xrtSetError(failure);
		return failure == NULL;
	}
	d->OwnershipFinishing = true; if (!xrtOwnershipScopeEnd(&m)) abort();
	xerror* prior = xrtTakeError(); __xrtTlsDialOwnedResources(d); xerror* failure = xrtTakeError();
	if (!xrtOwnershipMutationBegin(&m)) abort();
	d->OwnershipFinishError = failure; d->OwnershipFinishing = false; d->OwnershipFinished = true;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (prior) xrtSetErrorTake(prior); else if (failure) xrtSetError(failure);
	return failure == NULL;
}
XRT_API const xrtownershipadapterv1* xrtTlsDialOwnershipAdapterV1(xrtownershipref ref,
	const xtlsdialownershipv1* const* policies, size_t policyCount, const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 a = {sizeof(a), __xrtTlsDialOwnedHold, __xrtTlsDialOwnedDrop,
		__xrtTlsDialOwnedClaim, __xrtTlsDialOwnedRestore, NULL, __xrtTlsDialOwnedClear, __xrtTlsDialOwnedFinish};
	static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtTlsDialOwnedReady, __xrtTlsDialOwnedPrepare};
	if (!preparation || ref.Ops != &__xrtTlsDialOwnershipOps || !ref.Data || (policyCount && !policies) || __xrtTlsDialOwnedBusy(ref.Data)) return NULL;
	const xtlsdial* d = ref.Data; bool known = false;
	for (size_t i = 0; i < policyCount; ++i) if (policies[i] == d->OwnershipPolicy) { known = true; break; }
	if (!known || !d->OwnershipPolicy || d->OwnershipPolicy->size != sizeof(*d->OwnershipPolicy) ||
		!d->OwnershipPolicy->Done || !d->OwnershipPolicy->Drop || !d->OwnershipPolicy->Ops ||
		!d->OwnershipPolicy->Ops->Count || !d->OwnershipPolicy->Ops->Trace) return NULL;
	size_t count; if (!__xrtTlsDialOwnedCount(d, &count)) return NULL;
	*preparation = &p; return &a;
}

#endif
