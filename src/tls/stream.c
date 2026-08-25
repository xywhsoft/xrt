#include "../internal/xrt_tls_stream.h"
#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
	#include "../internal/xrt_tls_client.h"
#endif



#if defined(XRT_FEATURE_TLS_STREAM)

#define XRT_TLS_STREAM_DRIVE_ROUNDS 8u



static void __xrtTlsStreamDrive(xtlsstream* pStream);
static void __xrtTlsStreamProgressClose(xtlsstream* pStream);



/* 在所属 Worker 上取得当前回调数据，不额外建立跨线程屏障。 */
static ptr __xrtTlsStreamDataCurrent(const xtlsstream* pStream)
{
	return xrtAtomicPtrLoad(&pStream->Data, XMEMORY_RELAXED);
}



/* 可裁剪地推进 TLS Stream Future 适配层。 */
static void __xrtTlsStreamNotifyFutures(xtlsstream* pStream)
{
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtTlsStreamFutureNotify(pStream);
	#else
		(void)pStream;
	#endif
}



/* 将适配层错误写入统一 TLS 错误域。 */
static void __xrtTlsStreamSetError(
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



/* 保存当前线程错误；第一个根因在对象终态前保持不变。 */
static void __xrtTlsStreamRememberError(
	xtlsstream* pStream,
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		__xrtTlsStreamSetError(
			Kind, Code, sOperation, sMessage, NULL
		);
		pError = xrtTakeError();
	}
	if ( pStream->Error == NULL ) {
		pStream->Error = pError;
	} else {
		xrtErrorFree(pError);
	}
}



/* 将底层传输错误包装为 TLS 适配层原因链。 */
static void __xrtTlsStreamRememberTransport(
	xtlsstream* pStream,
	const xerror* pCause,
	cstr sOperation,
	cstr sMessage
)
{
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;

	if ( pStream->Error != NULL ) {
		return;
	}
	if ( Kind == XERR_NONE ) {
		Kind = XERR_IO;
	}
	__xrtTlsStreamSetError(
		Kind,
		XTLS_ERROR_CLOSED,
		sOperation,
		sMessage,
		pCause
	);
	__xrtTlsStreamRememberError(
		pStream,
		Kind,
		XTLS_ERROR_CLOSED,
		sOperation,
		sMessage
	);
}



/* 增加组合 Stream 引用。 */
XRT_API xtlsstream* xrtTlsStreamRef(xtlsstream* pStream)
{
	if ( pStream == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"retain-tls-stream",
			"TLS stream is null",
			NULL
		);
		return NULL;
	}
	if ( xrtRefRetain(&pStream->References) < 0 ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"retain-tls-stream",
			"TLS stream reference count is exhausted",
			NULL
		);
		return NULL;
	}
	return pStream;
}



/* 释放最后一个引用以及会话、传输和错误根因。 */
XRT_API void xrtTlsStreamDestroy(xtlsstream* pStream)
{
	xnetstream* pTransport;

	if ( pStream == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pStream->References) != 0 ) {
		return;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xrtTlsSessionDestroy(pStream->Session);
	xrtNetStreamDestroy(pTransport);
	xrtErrorFree(pStream->Error);
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinUnit(&pStream->AsyncLock);
	#endif
	xrtFree(pStream);
}



/* 初始化握手与认证关闭默认超时。 */
XRT_API void xrtTlsStreamConfigInit(xtlsstreamconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"init-tls-stream-config",
			"TLS stream config is null",
			NULL
		);
		return;
	}
	pConfig->HandshakeTimeout = XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT;
	pConfig->CloseTimeout = XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT;
	pConfig->AsyncBytesLimit =
		XTLS_STREAM_ASYNC_BYTES_DEFAULT;
	pConfig->AsyncCountLimit =
		XTLS_STREAM_ASYNC_COUNT_DEFAULT;
	pConfig->AsyncBatch =
		XTLS_STREAM_ASYNC_BATCH_DEFAULT;
}



/* 创建尚未绑定 TCP Stream 的组合对象。 */
xtlsstream* __xrtTlsStreamCreate(
	xtlssession* pSession,
	bool bServer,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData
)
{
	xtlsstreamconfig Config;
	xtlsstream* pStream;

	xrtTlsStreamConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if ( (Config.AsyncBytesLimit == 0) ||
			(Config.AsyncCountLimit == 0) ||
			(Config.AsyncBatch == 0) ) {
			__xrtTlsStreamSetError(
				XERR_RANGE,
				XTLS_ERROR_LIMIT,
				"configure-tls-stream",
				"TLS stream asynchronous limits must be nonzero",
				NULL
			);
			return NULL;
		}
	#endif
	pStream = (xtlsstream*)xrtCalloc(1, sizeof(*pStream));
	if ( pStream == NULL ) {
		return NULL;
	}
	pStream->References = 2;
	xrtAtomic32Init(
		&pStream->State,
		bServer ? XTLS_STREAM_HANDSHAKE : XTLS_STREAM_CONNECTING
	);
	xrtAtomic32Init(&pStream->CloseGate, 0);
	xrtAtomic32Init(&pStream->AbortGate, 0);
	xrtAtomic32Init(&pStream->TerminalResult, XNET_RESULT_OK);
	xrtAtomic64Init(&pStream->Available, 0);
	xrtAtomic64Init(&pStream->CipherPending, 0);
	xrtAtomicPtrInit(&pStream->Transport, NULL);
	xrtAtomicPtrInit(&pStream->Data, pData);
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		xrtAtomic64Init(&pStream->AsyncBytes, 0);
		xrtAtomic32Init(&pStream->AsyncCount, 0);
		xrtAtomic32Init(&pStream->AsyncSends, 0);
		xrtAtomic32Init(&pStream->AsyncReads, 0);
		__xrtSpinInit(&pStream->AsyncLock);
	#endif
	pStream->Session = pSession;
	pStream->Config = Config;
	if ( pEvents != NULL ) {
		pStream->Events = *pEvents;
	}
	pStream->Server = bServer;
	pStream->RuntimeHeld = true;
	return pStream;
}



/* 释放创建失败对象的运行时和调用方两份初始引用。 */
void __xrtTlsStreamDiscard(xtlsstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	pStream->RuntimeHeld = false;
	xrtTlsStreamDestroy(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 验证调用发生在底层 TCP Stream 的所属 Worker。 */
static bool __xrtTlsStreamWorker(
	xtlsstream* pStream,
	cstr sOperation
)
{
	xnetstream* pTransport;

	if ( pStream == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			sOperation,
			"TLS stream is null",
			NULL
		);
		return false;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( (pTransport == NULL) ||
		!xrtNetWorkerIsCurrent(pTransport->Worker) ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			sOperation,
			"TLS stream operation requires its network worker",
			NULL
		);
		return false;
	}
	return true;
}



/* 取消一个计时器；终态回调仍负责释放其对象引用。 */
static void __xrtTlsStreamCancelTimer(
	xtlsstream* pStream,
	uint64 Id
)
{
	xnetstream* pTransport;

	if ( Id == 0 ) {
		return;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( (pTransport != NULL) &&
		!__xrtNetEngineTimerCancelLifecycle(
			pTransport->Engine,
			Id
		) ) {
		xrtClearError();
	}
}



/* 握手 Timer 终态只在仍未开放时发布超时失败。 */
static void __xrtTlsStreamHandshakeTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pWorker;
	if ( pStream->HandshakeTimer == Id ) {
		pStream->HandshakeTimer = 0;
		if ( (Result == XNET_RESULT_OK) && !pStream->OpenEmitted &&
			(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
			__xrtTlsStreamSetError(
				XERR_TIMEOUT,
				XTLS_ERROR_HANDSHAKE,
				"handshake-tls-stream",
				"TLS handshake timed out",
				NULL
			);
			__xrtTlsStreamRememberError(
				pStream,
				XERR_TIMEOUT,
				XTLS_ERROR_HANDSHAKE,
				"handshake-tls-stream",
				"TLS handshake timed out"
			);
			xrtAtomic32Store(
				&pStream->TerminalResult,
				XNET_RESULT_TIMEOUT,
				XMEMORY_RELEASE
			);
			xrtAtomic32Store(
				&pStream->State,
				XTLS_STREAM_FAILED,
				XMEMORY_RELEASE
			);
			pStream->Failing = true;
			__xrtTlsStreamNotifyFutures(pStream);
			(void)xrtNetStreamAbort((xnetstream*)xrtAtomicPtrLoad(
				&pStream->Transport,
				XMEMORY_ACQUIRE
			));
		}
	}
	xrtTlsStreamDestroy(pStream);
}



/* 认证关闭 Timer 终态把未完成的双向 close_notify 变为明确超时。 */
static void __xrtTlsStreamCloseTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pWorker;
	if ( pStream->CloseTimer == Id ) {
		pStream->CloseTimer = 0;
		if ( (Result == XNET_RESULT_OK) &&
			(xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
			!pStream->CloseEmitted ) {
			__xrtTlsStreamSetError(
				XERR_TIMEOUT,
				XTLS_ERROR_CLOSED,
				"close-tls-stream",
				"TLS authenticated close timed out",
				NULL
			);
			__xrtTlsStreamRememberError(
				pStream,
				XERR_TIMEOUT,
				XTLS_ERROR_CLOSED,
				"close-tls-stream",
				"TLS authenticated close timed out"
			);
			xrtAtomic32Store(
				&pStream->TerminalResult,
				XNET_RESULT_TIMEOUT,
				XMEMORY_RELEASE
			);
			xrtAtomic32Store(
				&pStream->State,
				XTLS_STREAM_FAILED,
				XMEMORY_RELEASE
			);
			pStream->Failing = true;
			__xrtTlsStreamNotifyFutures(pStream);
			(void)xrtNetStreamAbort((xnetstream*)xrtAtomicPtrLoad(
				&pStream->Transport,
				XMEMORY_ACQUIRE
			));
		}
	}
	xrtTlsStreamDestroy(pStream);
}



/* 为握手建立独立超时；调度失败视为连接失败。 */
static bool __xrtTlsStreamStartHandshakeTimer(xtlsstream* pStream)
{
	xnetstream* pTransport;

	if ( (pStream->Config.HandshakeTimeout == 0) ||
		(pStream->HandshakeTimer != 0) ) {
		return true;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xrtTlsStreamRef(pStream);
	pStream->HandshakeTimer = xrtNetEngineAfter(
		pTransport->Engine,
		xrtNetWorkerIndex(pTransport->Worker),
		pStream->Config.HandshakeTimeout,
		__xrtTlsStreamHandshakeTimer,
		pStream
	);
	if ( pStream->HandshakeTimer == 0 ) {
		xrtTlsStreamDestroy(pStream);
		return false;
	}
	return true;
}



/* 首次进入认证关闭或错误告警排空时建立关闭超时。 */
static bool __xrtTlsStreamStartCloseTimer(xtlsstream* pStream)
{
	xnetstream* pTransport;

	if ( (pStream->Config.CloseTimeout == 0) ||
		(pStream->CloseTimer != 0) ) {
		return true;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xrtTlsStreamRef(pStream);
	pStream->CloseTimer = xrtNetEngineAfter(
		pTransport->Engine,
		xrtNetWorkerIndex(pTransport->Worker),
		pStream->Config.CloseTimeout,
		__xrtTlsStreamCloseTimer,
		pStream
	);
	if ( pStream->CloseTimer == 0 ) {
		xrtTlsStreamDestroy(pStream);
		return false;
	}
	return true;
}



/* 同步 TLS 会话尚未进入 TCP 队列的密文快照。 */
static void __xrtTlsStreamCipherPending(xtlsstream* pStream)
{
	xrtAtomic64Store(
		&pStream->CipherPending,
		(uint64)xrtTlsSessionSendSize(pStream->Session),
		XMEMORY_RELEASE
	);
}



/* 将 TLS 密文块链原子转移给 TCP 队列，不复制载荷。 */
static xnetresult __xrtTlsStreamFlush(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xnetbuf* pBuffer = __xrtTlsSessionSendBuffer(pStream->Session);
	xnetresult Result;
	bool bGuard;

	if ( (pTransport == NULL) || (pBuffer == NULL) ) {
		__xrtTlsStreamCipherPending(pStream);
		return XNET_RESULT_ERROR;
	}
	if ( xrtNetBufEmpty(pBuffer) ) {
		__xrtTlsStreamCipherPending(pStream);
		return XNET_RESULT_OK;
	}
	bGuard = !pStream->Driving;
	if ( bGuard ) {
		pStream->Driving = true;
	}
	Result = xrtNetStreamSendBuffer(pTransport, pBuffer);
	if ( Result == XNET_RESULT_OK ) {
		if ( !__xrtTlsSessionSendMoved(pStream->Session) ) {
			Result = XNET_RESULT_ERROR;
		}
	} else if ( Result == XNET_RESULT_ERROR ) {
		const xerror* pCause = xrtGetError();

		__xrtTlsStreamSetError(
			pCause != NULL ? xrtErrorKind(pCause) : XERR_IO,
			XTLS_ERROR_CLOSED,
			"flush-tls-stream",
			"TLS ciphertext could not enter the TCP send queue",
			pCause
		);
	}
	if ( bGuard ) {
		pStream->Driving = false;
	}
	__xrtTlsStreamCipherPending(pStream);
	return Result;
}



/* 把当前 TLS 或适配层错误升级为终态，并尽力发送 fatal Alert。 */
static void __xrtTlsStreamFail(xtlsstream* pStream)
{
	xnetresult Result;

	if ( pStream->Failing ) {
		return;
	}
	__xrtTlsStreamRememberError(
		pStream,
		XERR_PROTOCOL,
		XTLS_ERROR_INTERNAL,
		"drive-tls-stream",
		"TLS stream failed"
	);
	xrtAtomic32Store(
		&pStream->TerminalResult,
		XNET_RESULT_ERROR,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pStream->State,
		XTLS_STREAM_FAILED,
		XMEMORY_RELEASE
	);
	pStream->Failing = true;
	__xrtTlsStreamNotifyFutures(pStream);
	(void)__xrtTlsStreamStartCloseTimer(pStream);
	Result = __xrtTlsStreamFlush(pStream);
	if ( Result == XNET_RESULT_ERROR || Result == XNET_RESULT_CLOSED ) {
		(void)xrtNetStreamAbort((xnetstream*)xrtAtomicPtrLoad(
			&pStream->Transport,
			XMEMORY_ACQUIRE
		));
		return;
	}
	__xrtTlsStreamProgressClose(pStream);
}



/* 发布 TLS Open，并终止握手 Timer。 */
static void __xrtTlsStreamOpen(xtlsstream* pStream)
{
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
		bool bManaged = true;
	#endif

	if ( pStream->OpenEmitted ||
		(xrtTlsSessionState(pStream->Session) != XTLS_STATE_READY) ) {
		return;
	}
	pStream->OpenEmitted = true;
	xrtAtomic32Store(
		&pStream->State,
		XTLS_STREAM_OPEN,
		XMEMORY_RELEASE
	);
	__xrtTlsStreamCancelTimer(pStream, pStream->HandshakeTimer);
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
		if ( pStream->ManagedOpen != NULL ) {
			bManaged = pStream->ManagedOpen(
				pStream,
				pStream->ManagedData
			);
		}
		if ( !bManaged ) {
			__xrtTlsStreamNotifyFutures(pStream);
			return;
		}
	#endif
	if ( pStream->Events.Open != NULL ) {
		pStream->Events.Open(
			pStream,
			__xrtTlsStreamDataCurrent(pStream)
		);
	}
	__xrtTlsStreamNotifyFutures(pStream);
}



/* 发布一次对端认证关闭事件。 */
static void __xrtTlsStreamEnd(xtlsstream* pStream)
{
	if ( pStream->EndEmitted || !pStream->Session->CloseReceived ) {
		return;
	}
	pStream->EndEmitted = true;
	if ( pStream->Events.End != NULL ) {
		pStream->Events.End(
			pStream,
			__xrtTlsStreamDataCurrent(pStream)
		);
	}
	__xrtTlsStreamNotifyFutures(pStream);
}



/* 同步明文并发快照，并对未消费明文施加传输读取背压。 */
static bool __xrtTlsStreamPlain(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	size_t iAvailable = xrtTlsSessionPlainSize(pStream->Session);

	xrtAtomic64Store(
		&pStream->Available,
		(uint64)iAvailable,
		XMEMORY_RELEASE
	);
	if ( iAvailable == 0 ) {
		pStream->ReadNotified = false;
		pStream->ReadMore = false;
		pStream->ReadMoreSize = 0;
		return false;
	}
	if ( pStream->ReadMore ) {
		if ( iAvailable <= pStream->ReadMoreSize ) {
			return false;
		}
		pStream->ReadMore = false;
		pStream->ReadMoreSize = 0;
		pStream->ReadNotified = false;
	}
	if ( !pStream->TransportPaused ) {
		xrtNetStreamPause(pTransport);
		pStream->TransportPaused = true;
	}
	if ( !pStream->ReadNotified && (pStream->Events.Read != NULL) ) {
		const xnetbuf* pBuffer = __xrtTlsSessionPlainBuffer(
			pStream->Session
		);

		pStream->ReadNotified = true;
		pStream->Events.Read(
			pStream,
			pBuffer,
			__xrtTlsStreamDataCurrent(pStream)
		);
		iAvailable = xrtTlsSessionPlainSize(pStream->Session);
		xrtAtomic64Store(
			&pStream->Available,
			(uint64)iAvailable,
			XMEMORY_RELEASE
		);
		if ( iAvailable == 0 ) {
			pStream->ReadNotified = false;
		}
	}
	return iAvailable != 0;
}



/* 从 TCP 接收缓冲向 TLS Feed 推进；正常路径直接移动完整块链。 */
static bool __xrtTlsStreamInput(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xnetbuf* pSource = &pTransport->ReadBuffer;
	const xtlslimits* pLimits = xrtTlsContextLimits(
		pStream->Session->Context
	);
	xtlsresult Result;

	if ( xrtNetBufEmpty(pSource) ) {
		return false;
	}
	Result = xrtTlsSessionFeedBuffer(pStream->Session, pSource);
	if ( Result == XTLS_OK ) {
		__xrtNetStreamReadRefresh(pTransport, false);
		return true;
	}
	if ( Result == XTLS_ERROR ) {
		__xrtTlsStreamFail(pStream);
		return false;
	}
	if ( pLimits != NULL ) {
		size_t iFeed = xrtTlsSessionFeedSize(pStream->Session);

		if ( iFeed < pLimits->FeedLimit ) {
			xnetspan Span;
			size_t iCapacity = pLimits->FeedLimit - iFeed;

			if ( xrtNetBufFront(pSource, &Span) ) {
				if ( Span.Size > iCapacity ) {
					Span.Size = iCapacity;
				}
				Result = xrtTlsSessionFeed(
					pStream->Session,
					Span.Data,
					Span.Size
				);
				if ( Result == XTLS_OK ) {
					(void)xrtNetBufConsume(pSource, Span.Size);
					__xrtNetStreamReadRefresh(pTransport, false);
					return true;
				}
				if ( Result == XTLS_ERROR ) {
					__xrtTlsStreamFail(pStream);
				}
			}
		}
	}
	if ( !pStream->TransportPaused ) {
		xrtNetStreamPause(pTransport);
		pStream->TransportPaused = true;
	}
	return false;
}



/* 明文与 TCP 残留输入解除阻塞后恢复底层读取。 */
static void __xrtTlsStreamResumeInput(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);

	if ( !pStream->TransportPaused ||
		((xrtTlsSessionPlainSize(pStream->Session) != 0) &&
		 !pStream->ReadMore) ||
		!xrtNetBufEmpty(&pTransport->ReadBuffer) ||
		pStream->Session->CloseReceived || pStream->Failing ) {
		return;
	}
	if ( xrtNetStreamResume(pTransport) ) {
		pStream->TransportPaused = false;
	} else {
		xrtClearError();
	}
}



/* 两级发送队列都排空后发布一次应用 Drain 边沿。 */
static void __xrtTlsStreamDrain(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pStream->Sending ) {
		pStream->DriveAgain = true;
		return;
	}
	if ( !pStream->DrainPending ||
		(xrtTlsSessionSendSize(pStream->Session) != 0) ||
		(xrtNetStreamPending(pTransport) != 0) ) {
		return;
	}
	pStream->DrainPending = false;
	if ( pStream->Events.Drain != NULL ) {
		pStream->Events.Drain(
			pStream,
			__xrtTlsStreamDataCurrent(pStream)
		);
	}
}



/* 已经报告过写阻塞时，在传输水位恢复后发布一次 Writable。 */
static void __xrtTlsStreamWritable(xtlsstream* pStream)
{
	const xtlslimits* pLimits;

	if ( pStream->Sending ) {
		pStream->DriveAgain = true;
		return;
	}
	if ( !pStream->WriteBlocked ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ) {
		return;
	}
	pLimits = xrtTlsContextLimits(pStream->Session->Context);
	if ( (pLimits == NULL) ||
		(xrtTlsSessionSendSize(pStream->Session) >= pLimits->SendLimit) ) {
		return;
	}
	pStream->WriteBlocked = false;
	if ( pStream->Events.Writable != NULL ) {
		pStream->Events.Writable(
			pStream,
			__xrtTlsStreamDataCurrent(pStream)
		);
	}
}



/* 满足协议终态后请求 TCP 排空关闭；错误路径先排空 fatal Alert。 */
static void __xrtTlsStreamProgressClose(xtlsstream* pStream)
{
	xnetstream* pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xtlsstate State = xrtTlsSessionState(pStream->Session);

	if ( pStream->Closing || (pTransport == NULL) ) {
		return;
	}
	if ( pStream->Failing ) {
		if ( xrtTlsSessionSendSize(pStream->Session) == 0 ) {
			pStream->Closing = true;
			if ( !xrtNetStreamClose(pTransport) ) {
				xrtClearError();
				(void)xrtNetStreamAbort(pTransport);
			}
		}
		return;
	}
	if ( State == XTLS_STATE_CLOSING ) {
		xrtAtomic32Store(
			&pStream->State,
			XTLS_STREAM_CLOSING,
			XMEMORY_RELEASE
		);
		if ( !__xrtTlsStreamStartCloseTimer(pStream) ) {
			__xrtTlsStreamFail(pStream);
		}
		return;
	}
	if ( State == XTLS_STATE_CLOSED ) {
		xrtAtomic32Store(
			&pStream->State,
			XTLS_STREAM_CLOSING,
			XMEMORY_RELEASE
		);
		pStream->Closing = true;
		if ( !xrtNetStreamClose(pTransport) ) {
			__xrtTlsStreamRememberTransport(
				pStream,
				xrtGetError(),
				"close-tls-transport",
				"TLS transport close request failed"
			);
			xrtAtomic32Store(
				&pStream->TerminalResult,
				XNET_RESULT_ERROR,
				XMEMORY_RELEASE
			);
			xrtAtomic32Store(
				&pStream->State,
				XTLS_STREAM_FAILED,
				XMEMORY_RELEASE
			);
			pStream->Failing = true;
			(void)xrtNetStreamAbort(pTransport);
		}
	}
}



/* 延后继续公平性预算耗尽但仍有进展的协议驱动。 */
static void __xrtTlsStreamDriveTask(xnetworker* pWorker, ptr pData)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pWorker;
	pStream->DrivePosted = false;
	__xrtTlsStreamDrive(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 使用无分配内部命令继续驱动，避免占满公开任务队列。 */
static void __xrtTlsStreamScheduleDrive(xtlsstream* pStream)
{
	xnetstream* pTransport;

	if ( pStream->DrivePosted || pStream->CloseEmitted ) {
		return;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	pStream->DrivePosted = true;
	xrtTlsStreamRef(pStream);
	if ( !__xrtNetEnginePostInternal(
		pTransport->Worker,
		&pStream->DriveCommand,
		__xrtTlsStreamDriveTask,
		pStream
	) ) {
		pStream->DrivePosted = false;
		xrtTlsStreamDestroy(pStream);
	}
}



/* 在公平性轮次内统一推进输入、TLS 状态、输出和应用事件。 */
static void __xrtTlsStreamDrive(xtlsstream* pStream)
{
	uint32 iRound = 0;
	bool bProgress = false;

	if ( pStream->Driving ) {
		pStream->DriveAgain = true;
		return;
	}
	if ( pStream->CloseEmitted || (pStream->Session == NULL) ) {
		return;
	}
	pStream->Driving = true;
	do {
		size_t iFeedBefore = xrtTlsSessionFeedSize(pStream->Session);
		size_t iSendBefore = xrtTlsSessionSendSize(pStream->Session);
		size_t iPlainBefore = xrtTlsSessionPlainSize(pStream->Session);
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			uint64 iResumeBefore = pStream->Server ? 0 :
				__xrtTlsClientResumePublished(
					pStream->Session
				);
		#endif
		xtlsstate StateBefore = xrtTlsSessionState(pStream->Session);
		xtlsresult TlsResult;
		xnetresult NetResult;
		bool bInput;

		pStream->DriveAgain = false;
		if ( pStream->Failing ) {
			NetResult = __xrtTlsStreamFlush(pStream);
			if ( NetResult == XNET_RESULT_ERROR ||
				NetResult == XNET_RESULT_CLOSED ) {
				(void)xrtNetStreamAbort((xnetstream*)xrtAtomicPtrLoad(
					&pStream->Transport,
					XMEMORY_ACQUIRE
				));
			}
			__xrtTlsStreamProgressClose(pStream);
			break;
		}
		TlsResult = pStream->Server ?
			xrtTlsServerDrive(pStream->Session) :
			xrtTlsClientDrive(pStream->Session);
		if ( TlsResult == XTLS_ERROR ) {
			__xrtTlsStreamFail(pStream);
			break;
		}
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			if ( !pStream->Server &&
				(__xrtTlsClientResumePublished(
					pStream->Session
				) != iResumeBefore) &&
				(pStream->Events.Ticket != NULL) ) {
				pStream->Events.Ticket(
					pStream,
					__xrtTlsStreamDataCurrent(pStream)
				);
			}
		#endif
		NetResult = __xrtTlsStreamFlush(pStream);
		if ( NetResult == XNET_RESULT_ERROR ||
			NetResult == XNET_RESULT_CLOSED ) {
			__xrtTlsStreamFail(pStream);
			break;
		}
		__xrtTlsStreamOpen(pStream);
		if ( __xrtTlsStreamPlain(pStream) ) {
			break;
		}
		__xrtTlsStreamEnd(pStream);
		bInput = __xrtTlsStreamInput(pStream);
		bProgress = (TlsResult == XTLS_OK) || bInput ||
			(iFeedBefore != xrtTlsSessionFeedSize(pStream->Session)) ||
			(iSendBefore != xrtTlsSessionSendSize(pStream->Session)) ||
			(iPlainBefore != xrtTlsSessionPlainSize(pStream->Session)) ||
			(StateBefore != xrtTlsSessionState(pStream->Session));
		__xrtTlsStreamProgressClose(pStream);
		if ( pStream->Failing || pStream->Closing ) {
			break;
		}
		iRound++;
	} while ( (pStream->DriveAgain || bProgress) &&
		(iRound < XRT_TLS_STREAM_DRIVE_ROUNDS) );
	pStream->Driving = false;
	__xrtTlsStreamResumeInput(pStream);
	__xrtTlsStreamWritable(pStream);
	__xrtTlsStreamDrain(pStream);
	if ( !pStream->Failing && !pStream->Closing &&
		(iRound == XRT_TLS_STREAM_DRIVE_ROUNDS) && bProgress ) {
		__xrtTlsStreamScheduleDrive(pStream);
	}
	__xrtTlsStreamNotifyFutures(pStream);
}



/* TCP Open 后把 TLS 队列切换到 Worker 缓冲池并开始握手。 */
static void __xrtTlsStreamTransportOpen(
	xnetstream* pTransport,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;
	xnetbufpool* pPool;

	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		(void)xrtNetStreamAbort(pTransport);
		return;
	}
	pPool = xrtNetWorkerBufPool(pTransport->Worker);
	if ( (pPool == NULL) || !__xrtTlsSessionPool(
		pStream->Session,
		pPool
	) || !__xrtTlsStreamStartHandshakeTimer(pStream) ) {
		__xrtTlsStreamFail(pStream);
		return;
	}
	xrtAtomic32Store(
		&pStream->State,
		XTLS_STREAM_HANDSHAKE,
		XMEMORY_RELEASE
	);
	__xrtTlsStreamDrive(pStream);
}



/* TCP Read 只触发组合驱动，输入所有权由统一泵处理。 */
static void __xrtTlsStreamTransportRead(
	xnetstream* pTransport,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pTransport;
	(void)pBuffer;
	__xrtTlsStreamDrive(pStream);
}



/* TCP EOF 必须经过 TLS close_notify 认证，否则报告截断。 */
static void __xrtTlsStreamTransportEnd(
	xnetstream* pTransport,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;
	xtlsresult Result;

	(void)pTransport;
	Result = xrtTlsSessionEof(pStream->Session);
	if ( Result == XTLS_ERROR ) {
		__xrtTlsStreamFail(pStream);
		return;
	}
	__xrtTlsStreamEnd(pStream);
	__xrtTlsStreamProgressClose(pStream);
}



/* TCP 低水位释放 TLS 写阻塞并继续转移密文。 */
static void __xrtTlsStreamTransportLowWater(
	xnetstream* pTransport,
	size_t iQueued,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pTransport;
	(void)iQueued;
	if ( pStream->Driving || pStream->Sending ) {
		pStream->DriveAgain = true;
		return;
	}
	__xrtTlsStreamDrive(pStream);
	__xrtTlsStreamWritable(pStream);
	__xrtTlsStreamDrain(pStream);
}



/* TCP 队列完全排空后推进关闭并发布组合 Drain。 */
static void __xrtTlsStreamTransportDrain(
	xnetstream* pTransport,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pTransport;
	if ( pStream->Driving || pStream->Sending ) {
		pStream->DriveAgain = true;
		return;
	}
	__xrtTlsStreamDrive(pStream);
	__xrtTlsStreamWritable(pStream);
	__xrtTlsStreamDrain(pStream);
	__xrtTlsStreamProgressClose(pStream);
}



/* TCP Close 固化组合终态、发布一次回调并释放运行时引用。 */
static void __xrtTlsStreamTransportClose(
	xnetstream* pTransport,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;
	xnetresult Final = (xnetresult)xrtAtomic32Load(
		&pStream->TerminalResult,
		XMEMORY_ACQUIRE
	);
	xtlsstreamstate State;
	bool bClean = (xrtTlsSessionState(pStream->Session) ==
		XTLS_STATE_CLOSED) && !pStream->Failing &&
		!xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE);

	(void)pTransport;
	if ( pStream->CloseEmitted ) {
		return;
	}
	pStream->CloseEmitted = true;
	__xrtTlsStreamCancelTimer(pStream, pStream->HandshakeTimer);
	__xrtTlsStreamCancelTimer(pStream, pStream->CloseTimer);
	if ( bClean ) {
		Final = XNET_RESULT_OK;
		State = XTLS_STREAM_CLOSED;
	} else {
		if ( Final == XNET_RESULT_OK ) {
			Final = Result == XNET_RESULT_OK ?
				XNET_RESULT_ERROR : Result;
		}
		if ( pStream->Error == NULL ) {
			if ( xrtAtomic32Load(
				&pStream->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
				__xrtTlsStreamSetError(
					XERR_CANCELLED,
					XTLS_ERROR_CLOSED,
					"abort-tls-stream",
					"TLS stream was aborted",
					pError
				);
				__xrtTlsStreamRememberError(
					pStream,
					XERR_CANCELLED,
					XTLS_ERROR_CLOSED,
					"abort-tls-stream",
					"TLS stream was aborted"
				);
				Final = XNET_RESULT_CANCELLED;
			} else {
				__xrtTlsStreamRememberTransport(
					pStream,
					pError,
					"close-tls-transport",
					"TLS transport closed before authenticated shutdown"
				);
			}
		}
		State = XTLS_STREAM_FAILED;
	}
	xrtAtomic32Store(
		&pStream->TerminalResult,
		Final,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pStream->Available,
		(uint64)xrtTlsSessionPlainSize(pStream->Session),
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pStream->CipherPending,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pStream->State,
		State,
		XMEMORY_RELEASE
	);
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
		if ( pStream->ManagedClose != NULL ) {
			pStream->ManagedClose(
				pStream,
				Final,
				pStream->Error,
				pStream->ManagedData
			);
			pStream->ManagedOpen = NULL;
			pStream->ManagedClose = NULL;
			pStream->ManagedData = NULL;
		}
	#endif
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinLock(&pStream->AsyncLock);
		pStream->AsyncClosed = true;
		__xrtSpinUnlock(&pStream->AsyncLock);
	#endif
	if ( pStream->Events.Close != NULL ) {
		pStream->Events.Close(
			pStream,
			Final,
			pStream->Error,
			__xrtTlsStreamDataCurrent(pStream)
		);
	}
	__xrtTlsStreamNotifyFutures(pStream);
	if ( pStream->RuntimeHeld ) {
		pStream->RuntimeHeld = false;
		xrtTlsStreamDestroy(pStream);
	}
}



/* 底层 TCP 事件表只暴露组合状态机需要的方向。 */
static const xnetstreamevents __xrtTlsStreamTransportEvents = {
	__xrtTlsStreamTransportOpen,
	__xrtTlsStreamTransportRead,
	__xrtTlsStreamTransportEnd,
	NULL,
	__xrtTlsStreamTransportLowWater,
	__xrtTlsStreamTransportDrain,
	__xrtTlsStreamTransportClose
};



/* 返回组合层唯一的底层 TCP 事件表。 */
const xnetstreamevents* __xrtTlsStreamTransportEventTable(void)
{
	return &__xrtTlsStreamTransportEvents;
}



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
/* 安装只供受管服务端入口使用的内部生命周期观察器。 */
void __xrtTlsStreamManage(
	xtlsstream* pStream,
	bool (*pOpen)(xtlsstream* pStream, ptr pData),
	void (*pClose)(xtlsstream* pStream,
		xnetresult Result, const xerror* pError, ptr pData),
	ptr pData
)
{
	pStream->ManagedOpen = pOpen;
	pStream->ManagedClose = pClose;
	pStream->ManagedData = pData;
}
#endif



/*
	在所属 Worker 上切换已打开 TLS Stream 的事件和数据。
	调用方负责处理切换前已经留在明文缓冲中的数据。
*/
XRT_API bool xrtTlsStreamSetEvents(
	xtlsstream* pStream,
	const xtlsstreamevents* pEvents,
	ptr pData
)
{
	if ( !__xrtTlsStreamWorker(
		pStream,
		"set-tls-stream-events"
	) ) {
		return false;
	}
	if ( (xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ||
		!pStream->OpenEmitted ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"set-tls-stream-events",
			"TLS stream events require an open stream",
			NULL
		);
		return false;
	}
	if ( pStream->ReadMore ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"set-tls-stream-events",
			"TLS stream has a pending incremental read",
			NULL
		);
		return false;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinLock(&pStream->AsyncLock);
		if ( (pEvents != NULL) && (pEvents->Read != NULL) &&
			(xrtAtomic32Load(
				&pStream->AsyncReads,
				XMEMORY_ACQUIRE
			) != 0) ) {
			__xrtSpinUnlock(&pStream->AsyncLock);
			__xrtTlsStreamSetError(
				XERR_STATE,
				XTLS_ERROR_STATE,
				"set-tls-stream-events",
				"TLS stream has pending asynchronous read operations",
				NULL
			);
			return false;
		}
	#endif
	memset(&pStream->Events, 0, sizeof(pStream->Events));
	if ( pEvents != NULL ) {
		pStream->Events = *pEvents;
	}
	xrtAtomicPtrStore(&pStream->Data, pData, XMEMORY_RELEASE);
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinUnlock(&pStream->AsyncLock);
	#endif
	return true;
}



/* 名称解析和 TCP 拨号失败时复用组合终态发布与引用回收。 */
void __xrtTlsStreamTransportFailed(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError
)
{
	if ( (pStream == NULL) || pStream->CloseEmitted ) {
		return;
	}
	__xrtTlsStreamTransportClose(NULL, Result, pError, pStream);
}



/* 验证 TCP 硬上限可以原子容纳 TLS 整条密文队列。 */
bool __xrtTlsStreamLimits(
	const xnetstreamconfig* pTransport,
	const xtlssession* pSession
)
{
	const xtlslimits* pLimits = xrtTlsContextLimits(
		pSession->Context
	);

	if ( (pLimits == NULL) ||
		(pTransport->WriteLimit < pLimits->SendLimit) ) {
		__xrtTlsStreamSetError(
			XERR_RANGE,
			XTLS_ERROR_LIMIT,
			"configure-tls-stream",
			"TCP write limit is smaller than the TLS send limit",
			NULL
		);
		return false;
	}
	return true;
}



/* 创建客户端 TLS 会话并连接数字地址。 */
XRT_API xtlsstream* xrtTlsStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData
)
{
	xnetstreamconfig Transport;
	xtlssession* pSession;
	xtlsstream* pStream;
	xnetstream* pTcp;
	xerror* pError;

	if ( (pEngine == NULL) || (pRemote == NULL) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"connect-tls-stream",
			"TLS stream engine or remote address is null",
			NULL
		);
		return NULL;
	}
	xrtNetStreamConfigInit(&Transport);
	if ( pTransport != NULL ) {
		Transport = *pTransport;
	}
	if ( !__xrtNetStreamConfigValid(&Transport) ) {
		return NULL;
	}
	pSession = xrtTlsClientCreate(pTls, NULL);
	if ( pSession == NULL ) {
		return NULL;
	}
	if ( !__xrtTlsStreamLimits(&Transport, pSession) ) {
		xrtTlsSessionDestroy(pSession);
		return NULL;
	}
	pStream = __xrtTlsStreamCreate(
		pSession,
		false,
		pConfig,
		pEvents,
		pData
	);
	if ( pStream == NULL ) {
		xrtTlsSessionDestroy(pSession);
		return NULL;
	}
	pTcp = xrtNetStreamConnect(
		pEngine,
		pRemote,
		iAffinity,
		&Transport,
		&__xrtTlsStreamTransportEvents,
		pStream
	);
	if ( pTcp == NULL ) {
		pError = xrtTakeError();
		__xrtTlsStreamDiscard(pStream);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(&pStream->Transport, pTcp, XMEMORY_RELEASE);
	return pStream;
}



/*
	把已连接 TCP Stream 与调用方创建的 TLS Session 组合。
	全部可失败检查都在接管引用前完成。
*/
XRT_API bool xrtTlsStreamAttach(
	xnetstream* pTransport,
	xtlssession* pSession,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
)
{
	xnetbufpool* pPool;
	xtlsstream* pStream;
	xtlsrole Role;

	if ( ppStream != NULL ) {
		*ppStream = NULL;
	}
	if ( (pTransport == NULL) || (pSession == NULL) ||
		(ppStream == NULL) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"attach-tls-stream",
			"TLS transport, session or output is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pTransport->Worker) ||
		(xrtNetStreamState(pTransport) != XNET_STREAM_OPEN) ||
		!pTransport->OpenEmitted ||
		xrtAtomic32Load(&pTransport->ReadEnded, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pTransport->WriteEnded, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pTransport->WriteGate, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pTransport->CloseGate, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pTransport->AbortGate, XMEMORY_ACQUIRE) ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"attach-tls-stream",
			"TLS attach requires a bidirectional open TCP stream on its worker",
			NULL
		);
		return false;
	}
	Role = xrtTlsSessionRole(pSession);
	if ( (Role != XTLS_CLIENT) &&
		(Role != XTLS_SERVER) ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"attach-tls-stream",
			"TLS session role is invalid",
			NULL
		);
		return false;
	}
	if ( xrtTlsSessionState(pSession) != XTLS_STATE_HANDSHAKE ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"attach-tls-stream",
			"TLS attach requires a session in handshake state",
			NULL
		);
		return false;
	}
	if ( !__xrtTlsStreamLimits(&pTransport->Config, pSession) ) {
		return false;
	}
	pPool = xrtNetWorkerBufPool(pTransport->Worker);
	if ( pPool == NULL ) {
		return false;
	}
	pStream = __xrtTlsStreamCreate(
		pSession,
		Role == XTLS_SERVER,
		pConfig,
		pEvents,
		pData
	);
	if ( pStream == NULL ) {
		return false;
	}
	if ( !__xrtTlsSessionPool(pSession, pPool) ) {
		pStream->Session = NULL;
		__xrtTlsStreamDiscard(pStream);
		return false;
	}
	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	pTransport->Events = __xrtTlsStreamTransportEvents;
	xrtAtomicPtrStore(&pTransport->Data, pStream, XMEMORY_RELEASE);
	*ppStream = pStream;
	__xrtTlsStreamTransportOpen(pTransport, pStream);
	return true;
}



/* 创建客户端 Session 并复用通用 Attach 完成代理或 STARTTLS 组合。 */
XRT_API bool xrtTlsStreamClient(
	xnetstream* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
)
{
	xtlssession* pSession;

	if ( ppStream != NULL ) {
		*ppStream = NULL;
	}
	if ( (pTransport == NULL) || (ppStream == NULL) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"start-tls-client",
			"TLS client transport or output is null",
			NULL
		);
		return false;
	}
	pSession = xrtTlsClientCreate(pTls, NULL);
	if ( pSession == NULL ) {
		return false;
	}
	if ( !xrtTlsStreamAttach(
		pTransport,
		pSession,
		pConfig,
		pEvents,
		pData,
		ppStream
	) ) {
		xrtTlsSessionDestroy(pSession);
		return false;
	}
	return true;
}



/* 在 Listener Accept 回调中接管尚未发布 Open 的 TCP Stream。 */
XRT_API bool xrtTlsStreamAccept(
	xnetstream* pTransport,
	const xtlsserverconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream
)
{
	xnetbufpool* pPool;
	xtlssession* pSession;
	xtlsstream* pStream;

	if ( ppStream != NULL ) {
		*ppStream = NULL;
	}
	if ( (pTransport == NULL) || (ppStream == NULL) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"accept-tls-stream",
			"TLS stream transport or output is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pTransport->Worker) ||
		(xrtNetStreamState(pTransport) != XNET_STREAM_OPEN) ||
		pTransport->OpenEmitted ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-stream",
			"TLS accept requires an unpublished TCP stream on its worker",
			NULL
		);
		return false;
	}
	pPool = xrtNetWorkerBufPool(pTransport->Worker);
	if ( pPool == NULL ) {
		return false;
	}
	pSession = xrtTlsServerCreate(pTls, pPool);
	if ( pSession == NULL ) {
		return false;
	}
	if ( !__xrtTlsStreamLimits(&pTransport->Config, pSession) ) {
		xrtTlsSessionDestroy(pSession);
		return false;
	}
	pStream = __xrtTlsStreamCreate(
		pSession,
		true,
		pConfig,
		pEvents,
		pData
	);
	if ( pStream == NULL ) {
		xrtTlsSessionDestroy(pSession);
		return false;
	}
	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	pTransport->Events = __xrtTlsStreamTransportEvents;
	xrtAtomicPtrStore(&pTransport->Data, pStream, XMEMORY_RELEASE);
	*ppStream = pStream;
	return true;
}



/* 失败原子地校验全部明文片段并计算总长度。 */
static bool __xrtTlsStreamValidateSpans(
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pTotal
)
{
	size_t iTotal = 0;

	if ( (pSpans == NULL) && (iCount != 0) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"send-tls-stream",
			"TLS stream send vector is invalid",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (pSpans[i].Data == NULL) && (pSpans[i].Size != 0) ) {
			__xrtTlsStreamSetError(
				XERR_ARGUMENT,
				XTLS_ERROR_ARGUMENT,
				"send-tls-stream",
				"TLS stream send span is invalid",
				NULL
			);
			return false;
		}
		if ( pSpans[i].Size > (SIZE_MAX - iTotal) ) {
			__xrtTlsStreamSetError(
				XERR_RANGE,
				XTLS_ERROR_LIMIT,
				"send-tls-stream",
				"TLS stream send vector size overflows",
				NULL
			);
			return false;
		}
		iTotal += pSpans[i].Size;
	}
	*pTotal = iTotal;
	return true;
}



/* 把一组应用明文片段写入 TLS 记录并统一转移可用密文。 */
static xtlsresult __xrtTlsStreamSendSpans(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pWritten
)
{
	xtlsresult Result = XTLS_OK;
	xnetresult NetResult;
	size_t iTotal = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( !__xrtTlsStreamWorker(pStream, "send-tls-stream") ||
		(pWritten == NULL) ) {
		if ( (pStream != NULL) && (pWritten == NULL) ) {
			__xrtTlsStreamSetError(
				XERR_ARGUMENT,
				XTLS_ERROR_ARGUMENT,
				"send-tls-stream",
				"TLS stream send result is null",
				NULL
			);
		}
		return XTLS_ERROR;
	}
	if ( !__xrtTlsStreamValidateSpans(pSpans, iCount, &iTotal) ) {
		return XTLS_ERROR;
	}
	if ( xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN ) {
		if ( xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED ) {
			return XTLS_CLOSED;
		}
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"send-tls-stream",
			"TLS stream is not open for writing",
			NULL
		);
		return XTLS_ERROR;
	}
	pStream->Sending = true;
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iSpanWritten = 0;

		Result = xrtTlsSessionWrite(
			pStream->Session,
			pSpans[i].Data,
			pSpans[i].Size,
			&iSpanWritten
		);
		*pWritten += iSpanWritten;
		if ( Result == XTLS_ERROR ) {
			pStream->Sending = false;
			__xrtTlsStreamFail(pStream);
			return XTLS_ERROR;
		}
		if ( (Result != XTLS_OK) ||
			(iSpanWritten != pSpans[i].Size) ) {
			break;
		}
	}
	if ( *pWritten < iTotal ) {
		pStream->WriteBlocked = true;
	}
	if ( *pWritten != 0 ) {
		pStream->DrainPending = true;
		if ( Result == XTLS_AGAIN ) {
			Result = XTLS_OK;
		}
	} else if ( Result == XTLS_AGAIN ) {
		pStream->Sending = false;
		pStream->WriteBlocked = true;
		return XTLS_AGAIN;
	}
	NetResult = __xrtTlsStreamFlush(pStream);
	if ( NetResult == XNET_RESULT_ERROR || NetResult == XNET_RESULT_CLOSED ) {
		pStream->Sending = false;
		__xrtTlsStreamFail(pStream);
		return XTLS_ERROR;
	}
	pStream->Sending = false;
	if ( pStream->DriveAgain || pStream->DrainPending ) {
		__xrtTlsStreamScheduleDrive(pStream);
	}
	return Result;
}



/* 把一段应用明文写入 TLS 记录并立即转移可用密文。 */
XRT_API xtlsresult xrtTlsStreamSend(
	xtlsstream* pStream,
	const void* pData,
	size_t iSize,
	size_t* pWritten
)
{
	xnetspan Span = { (cbytes)pData, iSize };

	return __xrtTlsStreamSendSpans(
		pStream,
		&Span,
		1u,
		pWritten
	);
}



/* 逐片编码应用明文，避免调用方先行拼接连续缓冲。 */
XRT_API xtlsresult xrtTlsStreamSendVec(
	xtlsstream* pStream,
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pWritten
)
{
	return __xrtTlsStreamSendSpans(
		pStream,
		pSpans,
		iCount,
		pWritten
	);
}



/* 计算当前发送密钥把一段明文切成应用记录后的精确线路长度。 */
XRT_API bool xrtTlsStreamSendBound(
	xtlsstream* pStream,
	size_t iPlainSize,
	size_t* pBound
)
{
	size_t iFullRecords;
	size_t iFullSize;
	size_t iRemainder;
	size_t iRemainderSize = 0;
	size_t iBound;

	if ( !__xrtTlsStreamWorker(pStream, "bound-tls-stream-send") ) {
		return false;
	}
	if ( !__xrtRangeValid(pBound, sizeof(iBound)) ||
		__xrtRangesOverlap(
			pBound,
			sizeof(iBound),
			pStream,
			sizeof(*pStream)
		) || __xrtRangesOverlap(
			pBound,
			sizeof(iBound),
			pStream->Session,
			sizeof(*pStream->Session)
		) ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"bound-tls-stream-send",
			"TLS stream send bound output is invalid",
			NULL
		);
		return false;
	}
	if ( xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"bound-tls-stream-send",
			"TLS stream is not open for send sizing",
			NULL
		);
		return false;
	}
	if ( iPlainSize == 0 ) {
		iBound = 0;
		memcpy(pBound, &iBound, sizeof(iBound));
		return true;
	}
	iFullRecords = iPlainSize / XTLS_RECORD_PLAINTEXT_MAX;
	iRemainder = iPlainSize % XTLS_RECORD_PLAINTEXT_MAX;
	iFullSize = __xrtTlsRecordSealSize(
		&pStream->Session->WriteKey,
		XTLS_RECORD_PLAINTEXT_MAX,
		0
	);
	if ( iRemainder != 0 ) {
		iRemainderSize = __xrtTlsRecordSealSize(
			&pStream->Session->WriteKey,
			iRemainder,
			0
		);
	}
	if ( (iFullSize <= XTLS_RECORD_PLAINTEXT_MAX) ||
		((iRemainder != 0) &&
		 (iRemainderSize <= iRemainder)) ) {
		__xrtTlsStreamSetError(
			XERR_INTERNAL,
			XTLS_ERROR_INTERNAL,
			"bound-tls-stream-send",
			"TLS stream send key has no valid protected record size",
			NULL
		);
		return false;
	}
	if ( (iFullRecords > (SIZE_MAX / iFullSize)) ||
		(iRemainderSize >
		 (SIZE_MAX - (iFullRecords * iFullSize))) ) {
		__xrtTlsStreamSetError(
			XERR_RANGE,
			XTLS_ERROR_LIMIT,
			"bound-tls-stream-send",
			"TLS stream send bound is not representable",
			NULL
		);
		return false;
	}
	iBound = (iFullRecords * iFullSize) + iRemainderSize;
	memcpy(pBound, &iBound, sizeof(iBound));
	return true;
}



/* 返回待应用消费明文的并发快照。 */
XRT_API size_t xrtTlsStreamAvailable(const xtlsstream* pStream)
{
	return pStream != NULL ? (size_t)xrtAtomic64Load(
		&pStream->Available,
		XMEMORY_ACQUIRE
	) : 0;
}



/* 组合 TLS 暂存密文与底层 TCP 队列的并发快照。 */
XRT_API size_t xrtTlsStreamPending(const xtlsstream* pStream)
{
	xnetstream* pTransport;
	size_t iTls;
	size_t iTcp;

	if ( pStream == NULL ) {
		return 0;
	}
	iTls = (size_t)xrtAtomic64Load(
		&pStream->CipherPending,
		XMEMORY_ACQUIRE
	);
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	iTcp = pTransport != NULL ?
		xrtNetStreamPending(pTransport) : 0;
	if ( iTcp > (SIZE_MAX - iTls) ) {
		return SIZE_MAX;
	}
	return iTls + iTcp;
}



/* 在所属 Worker 上借用会话明文块链。 */
XRT_API const xnetbuf* xrtTlsStreamBuffer(xtlsstream* pStream)
{
	if ( !__xrtTlsStreamWorker(pStream, "buffer-tls-stream") ) {
		return NULL;
	}
	return __xrtTlsSessionPlainBuffer(pStream->Session);
}



/* 在保持 TLS 消费契约的前提下按需连续化明文前缀。 */
XRT_API bool xrtTlsStreamPullup(
	xtlsstream* pStream,
	size_t iSize,
	xnetspan* pSpan
)
{
	if ( !__xrtTlsStreamWorker(pStream, "pullup-tls-stream") ) {
		return false;
	}
	return __xrtTlsSessionPlainPullup(
		pStream->Session,
		iSize,
		pSpan
	) == XTLS_OK;
}



/* 允许增量解析器在保留前缀时继续累积受限明文。 */
XRT_API bool xrtTlsStreamReadMore(xtlsstream* pStream)
{
	const xtlslimits* pLimits;
	size_t iAvailable;

	if ( !__xrtTlsStreamWorker(pStream, "read-more-tls-stream") ) {
		return false;
	}
	if ( (xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ||
		!pStream->ReadNotified ||
		(pStream->Events.Read == NULL) ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"read-more-tls-stream",
			"TLS ReadMore requires a published plaintext callback",
			NULL
		);
		return false;
	}
	if ( pStream->ReadMore ) {
		return true;
	}
	iAvailable = xrtTlsSessionPlainSize(pStream->Session);
	pLimits = xrtTlsContextLimits(pStream->Session->Context);
	if ( (iAvailable == 0) || (pLimits == NULL) ||
		(pLimits->PlainLimit < XTLS_RECORD_PLAINTEXT_MAX) ||
		(iAvailable >
		 (pLimits->PlainLimit - XTLS_RECORD_PLAINTEXT_MAX)) ) {
		__xrtTlsStreamSetError(
			XERR_RANGE,
			XTLS_ERROR_LIMIT,
			"read-more-tls-stream",
			"TLS retained plaintext has no full-record growth space",
			NULL
		);
		return false;
	}
	pStream->ReadMore = true;
	pStream->ReadMoreSize = iAvailable;
	if ( pStream->Driving ) {
		pStream->DriveAgain = true;
	} else {
		__xrtTlsStreamDrive(pStream);
	}
	return true;
}



/* 消费明文后继续处理已缓存密文并恢复 TCP 读取。 */
static void __xrtTlsStreamAfterRead(xtlsstream* pStream)
{
	size_t iAvailable = xrtTlsSessionPlainSize(pStream->Session);

	xrtAtomic64Store(
		&pStream->Available,
		(uint64)iAvailable,
		XMEMORY_RELEASE
	);
	if ( iAvailable == 0 ) {
		pStream->ReadNotified = false;
		if ( pStream->Driving ) {
			pStream->DriveAgain = true;
		} else {
			__xrtTlsStreamDrive(pStream);
		}
	}
}



/* 复制并安全消费一段明文。 */
XRT_API xtlsresult xrtTlsStreamRead(
	xtlsstream* pStream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
)
{
	xtlsresult Result;

	if ( !__xrtTlsStreamWorker(pStream, "read-tls-stream") ) {
		if ( pRead != NULL ) {
			*pRead = 0;
		}
		return XTLS_ERROR;
	}
	Result = xrtTlsSessionRead(
		pStream->Session,
		pOutput,
		iCapacity,
		pRead
	);
	if ( Result == XTLS_OK ) {
		__xrtTlsStreamAfterRead(pStream);
	}
	return Result;
}



/* 精确消费一段明文并继续组合驱动。 */
XRT_API bool xrtTlsStreamConsume(xtlsstream* pStream, size_t iSize)
{
	if ( !__xrtTlsStreamWorker(pStream, "consume-tls-stream") ||
		!xrtTlsSessionPlainConsume(pStream->Session, iSize) ) {
		return false;
	}
	__xrtTlsStreamAfterRead(pStream);
	return true;
}



/* 在所属 Worker 上至多一次地开始双向 close_notify。 */
static void __xrtTlsStreamCloseStart(xtlsstream* pStream)
{
	xtlsresult Result;

	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ||
		pStream->CloseEmitted || pStream->CloseStarted ) {
		return;
	}
	pStream->CloseStarted = true;
	if ( xrtTlsSessionState(pStream->Session) == XTLS_STATE_CLOSED ) {
		__xrtTlsStreamProgressClose(pStream);
		return;
	}
	if ( xrtTlsSessionState(pStream->Session) == XTLS_STATE_FAILED ) {
		return;
	}
	if ( xrtTlsSessionState(pStream->Session) != XTLS_STATE_READY &&
		xrtTlsSessionState(pStream->Session) != XTLS_STATE_CLOSING ) {
		__xrtTlsStreamSetError(
			XERR_CANCELLED,
			XTLS_ERROR_CLOSED,
			"close-tls-stream",
			"TLS stream closed before the handshake completed",
			NULL
		);
		__xrtTlsStreamRememberError(
			pStream,
			XERR_CANCELLED,
			XTLS_ERROR_CLOSED,
			"close-tls-stream",
			"TLS stream closed before the handshake completed"
		);
		xrtAtomic32Store(
			&pStream->TerminalResult,
			XNET_RESULT_CANCELLED,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&pStream->State,
			XTLS_STREAM_FAILED,
			XMEMORY_RELEASE
		);
		pStream->Failing = true;
		(void)xrtNetStreamAbort((xnetstream*)xrtAtomicPtrLoad(
			&pStream->Transport,
			XMEMORY_ACQUIRE
		));
		return;
	}
	Result = xrtTlsSessionClose(pStream->Session);
	if ( Result == XTLS_ERROR ) {
		__xrtTlsStreamFail(pStream);
	} else {
		xrtAtomic32Store(
			&pStream->State,
			XTLS_STREAM_CLOSING,
			XMEMORY_RELEASE
		);
		if ( !__xrtTlsStreamStartCloseTimer(pStream) ) {
			__xrtTlsStreamFail(pStream);
		} else {
			__xrtTlsStreamDrive(pStream);
		}
	}
}



/* Close 命令在关闭门之前已经接纳的异步发送全部完成后才启动协议关闭。 */
static void __xrtTlsStreamCloseTask(xnetworker* pWorker, ptr pData)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pWorker;
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if ( xrtAtomic32Load(
			&pStream->AsyncSends,
			XMEMORY_ACQUIRE
		) != 0 ) {
			xrtTlsStreamDestroy(pStream);
			return;
		}
	#endif
	__xrtTlsStreamCloseStart(pStream);
	xrtTlsStreamDestroy(pStream);
}



#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
/* 异步发送队列释放最后一个节点后，在所属 Worker 上继续延迟的认证关闭。 */
void __xrtTlsStreamCloseReady(xtlsstream* pStream)
{
	if ( (pStream == NULL) ||
		!xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		(xrtAtomic32Load(
			&pStream->AsyncSends,
			XMEMORY_ACQUIRE
		) != 0) ) {
		return;
	}
	__xrtTlsStreamCloseStart(pStream);
}
#endif



/* 从任意线程无分配地投递一次认证关闭。 */
XRT_API bool xrtTlsStreamClose(xtlsstream* pStream)
{
	uint32 iExpected = 0;
	xnetstream* pTransport;
	xtlsstreamstate State;

	if ( pStream == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"close-tls-stream",
			"TLS stream is null",
			NULL
		);
		return false;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinLock(&pStream->AsyncLock);
	#endif
	State = xrtTlsStreamState(pStream);
	if ( (State == XTLS_STREAM_CLOSED) ||
		(State == XTLS_STREAM_FAILED) ) {
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pStream->CloseGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return true;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		xrtAtomic32Store(&pStream->CloseGate, 0, XMEMORY_RELEASE);
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return false;
	}
	xrtTlsStreamRef(pStream);
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinUnlock(&pStream->AsyncLock);
	#endif
	if ( !__xrtNetEnginePostInternal(
		pTransport->Worker,
		&pStream->CloseCommand,
		__xrtTlsStreamCloseTask,
		pStream
	) ) {
		xrtAtomic32Store(&pStream->CloseGate, 0, XMEMORY_RELEASE);
		xrtTlsStreamDestroy(pStream);
		__xrtTlsStreamSetError(
			XERR_CLOSED,
			XTLS_ERROR_CLOSED,
			"close-tls-stream",
			"network worker shutdown is sealed",
			NULL
		);
		return false;
	}
	return true;
}



/* 从任意线程升级为立即异常关闭。 */
XRT_API bool xrtTlsStreamAbort(xtlsstream* pStream)
{
	uint32 iExpected = 0;
	uint32 iState;
	xnetstream* pTransport;

	if ( pStream == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"abort-tls-stream",
			"TLS stream is null",
			NULL
		);
		return false;
	}
	if ( (xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED) ||
		xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		return true;
	}
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return false;
	}
	if ( xrtNetStreamState(pTransport) == XNET_STREAM_CLOSED ) {
		return true;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinLock(&pStream->AsyncLock);
	#endif
	if ( (xrtTlsStreamState(pStream) == XTLS_STREAM_CLOSED) ||
		xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pStream->AbortGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return true;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinUnlock(&pStream->AsyncLock);
	#endif
	if ( !xrtNetStreamAbort(pTransport) ) {
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinLock(&pStream->AsyncLock);
		#endif
		xrtAtomic32Store(&pStream->AbortGate, 0, XMEMORY_RELEASE);
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return false;
	}
	iExpected = XNET_RESULT_OK;
	(void)xrtAtomic32CompareExchange(
		&pStream->TerminalResult,
		&iExpected,
		XNET_RESULT_CANCELLED,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
	iState = xrtAtomic32Load(&pStream->State, XMEMORY_ACQUIRE);
	while ( (iState != XTLS_STREAM_CLOSED) &&
		(iState != XTLS_STREAM_FAILED) &&
		!xrtAtomic32CompareExchange(
			&pStream->State,
			&iState,
			XTLS_STREAM_CLOSING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
	}
	return true;
}



/* 返回组合状态的并发快照。 */
XRT_API xtlsstreamstate xrtTlsStreamState(const xtlsstream* pStream)
{
	if ( pStream == NULL ) {
		__xrtTlsStreamSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"state-tls-stream",
			"TLS stream is null",
			NULL
		);
		return XTLS_STREAM_FAILED;
	}
	return (xtlsstreamstate)xrtAtomic32Load(
		&pStream->State,
		XMEMORY_ACQUIRE
	);
}



/* 借用底层 TCP Stream。 */
XRT_API xnetstream* xrtTlsStreamTransport(const xtlsstream* pStream)
{
	return pStream != NULL ? (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	) : NULL;
}



/* 在所属 Worker 上借用 TLS 协议会话。 */
XRT_API xtlssession* xrtTlsStreamSession(xtlsstream* pStream)
{
	if ( !__xrtTlsStreamWorker(pStream, "session-tls-stream") ) {
		return NULL;
	}
	return pStream->Session;
}



/* 返回创建时绑定的用户数据。 */
XRT_API ptr xrtTlsStreamData(const xtlsstream* pStream)
{
	return pStream != NULL ? xrtAtomicPtrLoad(
		&pStream->Data,
		XMEMORY_ACQUIRE
	) : NULL;
}



/* 借用已经固化的 TLS 或传输根因。 */
XRT_API const xerror* xrtTlsStreamError(const xtlsstream* pStream)
{
	if ( (pStream == NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
		return NULL;
	}
	return pStream->Error;
}

#endif
