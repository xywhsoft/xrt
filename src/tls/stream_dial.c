#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_DIAL)

#define XRT_TLS_DIAL_GATE_OPEN 0u
#define XRT_TLS_DIAL_GATE_CANCEL 1u
#define XRT_TLS_DIAL_GATE_TIMEOUT 2u
#define XRT_TLS_DIAL_GATE_WINNER 3u
#define XRT_TLS_DIAL_GATE_TERMINAL 4u



/* 受管 TLS Dial 在握手成功前独占 Stream 调用方引用。 */
struct xtlsdial {
	volatile int32 References;
	xatomic32 State;
	xatomic32 TerminalGate;
	xatomic32 TimerDone;
	xatomic64 Timer;
	xatomicptr TransportDial;
	xatomicptr Stream;
	xnetengine* Engine;
	xtlsstreamevents StreamEvents;
	ptr StreamData;
	xtlsdialproc Done;
	ptr DoneData;
	xerror* Error;
	uint64 Affinity;
	bool RuntimeHeld;
};



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
	if ( (pDial == NULL) ||
		(xrtRefRetain(&pDial->References) < 0) ) {
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
	xtlsstream* pStream;

	if ( (pDial == NULL) ||
		(xrtRefRelease(&pDial->References) != 0) ) {
		return;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);
	xrtNetDialDestroy(pTransportDial);
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
	pStream = (xtlsstream*)xrtAtomicPtrExchange(
		&pDial->Stream,
		NULL,
		XMEMORY_ACQ_REL
	);
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
	if ( pDial->RuntimeHeld ) {
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
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
	if ( !xrtTlsStreamSetEvents(
		pStream,
		&pDial->StreamEvents,
		pDial->StreamData
	) ) {
		(void)xrtTlsStreamAbort(pStream);
		return;
	}
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



/* TCP Dial 完成后，失败进入组合终态，成功则把传输引用交给 TLS Stream。 */
static void __xrtTlsDialTransportDone(
	xnetdial* pTransportDial,
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

	(void)pTransportDial;
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



/* 取消当前活动的 TCP 或 TLS 阶段；调用方已经赢得终态门。 */
static void __xrtTlsDialCancelStage(xtlsdial* pDial)
{
	xnetdial* pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	xtlsstream* pStream;

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
	xtlsdialconfig Config;
	xtlsclientconfig Tls;
	xtlsstreamevents PendingEvents;
	xtlssession* pSession;
	xtlsstream* pStream;
	xtlsdial* pDial;
	xnetdial* pTransportDial;
	xerror* pError;
	uint64 Id;

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
		(void)xrtNetDialCancel(pTransportDial);
	}
	__xrtNetEngineObjectRelease(pEngine);
	return pDial;
}



/* 从任意线程让解析、TCP 或 TLS 握手中当前有效的阶段接受一次取消。 */
XRT_API bool xrtTlsDialCancel(xtlsdial* pDial)
{
	uint32 iExpected = XRT_TLS_DIAL_GATE_OPEN;

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

#endif
