#include "../internal/xrt_tls_stream.h"
#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
	#include "../internal/xrt_tls_client.h"
#endif



#if defined(XRT_FEATURE_TLS_STREAM)

#define XRT_TLS_STREAM_DRIVE_ROUNDS 8u
#define XRT_TLS_STREAM_OWNERSHIP_READY 1u
#define XRT_TLS_STREAM_OWNERSHIP_CLEARED 2u

static bool __xrtTlsStreamOwnershipCount(const void*, size_t*);
static bool __xrtTlsStreamOwnershipTrace(const void*, xrtownershipvisitor, ptr);
static const xrtownershipops __xrtTlsStreamOwnershipOps = {
	__xrtTlsStreamOwnershipCount, __xrtTlsStreamOwnershipTrace};
static void __xrtTlsStreamOwnedRuntimeRelease(xtlsstream*);
static void __xrtTlsStreamTransportDrop(const void*);



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
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;
	bool bHeld = !(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED) &&
		xrtRefRetain(&pStream->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if ( !bHeld ) {
		__xrtTlsStreamSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"retain-tls-stream",
			"TLS stream is retired or its reference count is exhausted",
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
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	bool bLast = xrtRefRelease(&pStream->References) == 0;
	if (bLast && (pStream->RuntimeOwner.Data || pStream->RuntimeOwner.Previous ||
		xrtAtomic32Load(&pStream->OwnershipActive, XMEMORY_ACQUIRE))) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bLast) return;
	pTransport = (xnetstream*)xrtAtomicPtrLoad(
		&pStream->Transport,
		XMEMORY_ACQUIRE
	);
	xrtTlsSessionDestroy(pStream->Session);
	xrtNetStreamDestroy(pTransport);
	xrtErrorFree(pStream->Error);
	xrtErrorFree(pStream->OwnershipFinishError);
	if (pStream->OwnershipDataHeld)
		pStream->OwnershipPolicy->Drop(xrtAtomicPtrLoad(&pStream->Data, XMEMORY_ACQUIRE));
	if (pStream->OwnershipEngineHeld) __xrtNetEngineObjectRelease(pStream->OwnershipEngine);
	if (pStream->OwnershipStorageHeld) __xrtNetEngineStorageRelease(pStream->OwnershipEngine);
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		__xrtSpinUnit(&pStream->AsyncLock);
	#endif
	xrtFree(pStream);
}




/* 进入一个可能同步触发底层 Close 的组合调用帧。 */
static void __xrtTlsStreamActiveEnter(xtlsstream* pStream)
{
	pStream->ActiveDepth++;
}



/* 立即释放运行时引用，或延迟到最外层组合调用帧退出。 */
static void __xrtTlsStreamReleaseRuntime(xtlsstream* pStream)
{
	if ( !pStream->RuntimeHeld ) {
		return;
	}
	if ( pStream->ActiveDepth != 0 ) {
		pStream->RuntimeReleasePending = true;
		return;
	}
	pStream->RuntimeReleasePending = false;
	if (pStream->OwnershipPolicy) __xrtTlsStreamOwnedRuntimeRelease(pStream);
	else pStream->RuntimeHeld = false;
	xrtTlsStreamDestroy(pStream);
}



/* 离开最外层组合调用帧后完成延迟的运行时引用释放。 */
static void __xrtTlsStreamActiveLeave(xtlsstream* pStream)
{
	pStream->ActiveDepth--;
	if ( (pStream->ActiveDepth == 0) &&
		pStream->RuntimeReleasePending ) {
		__xrtTlsStreamReleaseRuntime(pStream);
	}
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
	xrtAtomicPtrInit(&pStream->OwnershipClaim, NULL);
	xrtAtomic32Init(&pStream->OwnershipState, 0);
	xrtAtomic32Init(&pStream->OwnershipActive, 0);
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
	/* No startup was accepted. Return the caller's Data credit intact. */
	pStream->OwnershipDataHeld = false;
	if (pStream->OwnershipPolicy) {
		if (pStream->RuntimeOwner.Data) __xrtTlsStreamOwnedRuntimeRelease(pStream);
		else pStream->RuntimeHeld = false;
		__xrtTlsStreamOwnershipLeave(pStream);
	} else pStream->RuntimeHeld = false;
	xrtTlsStreamDestroy(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 验证调用发生在底层 TCP Stream 的所属 Worker。 */
static bool __xrtTlsStreamWorkerMode(
	xtlsstream* pStream,
	cstr sOperation,
	bool bSubmission
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
	if (xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED ||
		(bSubmission && xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE))) {
		__xrtErrorSetInvalidState(); return false;
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

static bool __xrtTlsStreamWorker(xtlsstream* pStream, cstr sOperation)
{ return __xrtTlsStreamWorkerMode(pStream, sOperation, true); }



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
	if (!xrtTlsStreamRef(pStream)) return false;
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
	if (pStream->OwnershipPolicy && !__xrtNetEngineRuntimeTimer(pTransport->Worker,
		pStream->HandshakeTimer, xrtTlsStreamRuntimeOwnershipV1Get())) abort();
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
	if (!xrtTlsStreamRef(pStream)) return false;
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
	if (pStream->OwnershipPolicy && !__xrtNetEngineRuntimeTimer(pTransport->Worker,
		pStream->CloseTimer, xrtTlsStreamRuntimeOwnershipV1Get())) abort();
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
	Result = __xrtNetStreamSendBufferAccepted(pTransport, pBuffer);
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
		(uint32)XNET_RESULT_ERROR,
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
	if ( pStream->CloseEmitted ) {
		return;
	}
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
				(uint32)XNET_RESULT_ERROR,
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
	if (!xrtTlsStreamRef(pStream)) return;
	pStream->DrivePosted = true;
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
			if ( pStream->CloseEmitted ) {
				break;
			}
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
		if ( pStream->CloseEmitted ) {
			break;
		}
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
	if ( pStream->CloseEmitted ) {
		return;
	}
	__xrtTlsStreamResumeInput(pStream);
	if ( pStream->CloseEmitted ) {
		return;
	}
	__xrtTlsStreamWritable(pStream);
	if ( pStream->CloseEmitted ) {
		return;
	}
	__xrtTlsStreamDrain(pStream);
	if ( pStream->CloseEmitted ) {
		return;
	}
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

	__xrtTlsStreamActiveEnter(pStream);
	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		(void)xrtNetStreamAbort(pTransport);
		goto cleanup;
	}
	pPool = xrtNetWorkerBufPool(pTransport->Worker);
	if ( (pPool == NULL) || !__xrtTlsSessionPool(
		pStream->Session,
		pPool
	) || !__xrtTlsStreamStartHandshakeTimer(pStream) ) {
		__xrtTlsStreamFail(pStream);
		goto cleanup;
	}
	xrtAtomic32Store(
		&pStream->State,
		XTLS_STREAM_HANDSHAKE,
		XMEMORY_RELEASE
	);
	__xrtTlsStreamDrive(pStream);

cleanup:
	__xrtTlsStreamActiveLeave(pStream);
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
	__xrtTlsStreamActiveEnter(pStream);
	__xrtTlsStreamDrive(pStream);
	__xrtTlsStreamActiveLeave(pStream);
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
	__xrtTlsStreamActiveEnter(pStream);
	Result = xrtTlsSessionEof(pStream->Session);
	if ( Result == XTLS_ERROR ) {
		__xrtTlsStreamFail(pStream);
		goto cleanup;
	}
	__xrtTlsStreamEnd(pStream);
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamProgressClose(pStream);
	}

cleanup:
	__xrtTlsStreamActiveLeave(pStream);
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
	__xrtTlsStreamActiveEnter(pStream);
	if ( pStream->Driving || pStream->Sending ) {
		pStream->DriveAgain = true;
		goto cleanup;
	}
	__xrtTlsStreamDrive(pStream);
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamWritable(pStream);
	}
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamDrain(pStream);
	}

cleanup:
	__xrtTlsStreamActiveLeave(pStream);
}



/* TCP 队列完全排空后推进关闭并发布组合 Drain。 */
static void __xrtTlsStreamTransportDrain(
	xnetstream* pTransport,
	ptr pData
)
{
	xtlsstream* pStream = (xtlsstream*)pData;

	(void)pTransport;
	__xrtTlsStreamActiveEnter(pStream);
	if ( pStream->Driving || pStream->Sending ) {
		pStream->DriveAgain = true;
		goto cleanup;
	}
	__xrtTlsStreamDrive(pStream);
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamWritable(pStream);
	}
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamDrain(pStream);
	}
	if ( !pStream->CloseEmitted ) {
		__xrtTlsStreamProgressClose(pStream);
	}

cleanup:
	__xrtTlsStreamActiveLeave(pStream);
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
	__xrtTlsStreamReleaseRuntime(pStream);
}



/* 底层 TCP 事件表只暴露组合状态机需要的方向。 */
static const xnetstreameventsownershipv1 __xrtTlsStreamTransportPolicy = {
	sizeof(__xrtTlsStreamTransportPolicy),
	{__xrtTlsStreamTransportOpen, __xrtTlsStreamTransportRead,
	 __xrtTlsStreamTransportEnd, NULL, __xrtTlsStreamTransportLowWater,
	 __xrtTlsStreamTransportDrain, __xrtTlsStreamTransportClose},
	__xrtTlsStreamTransportDrop, &__xrtTlsStreamOwnershipOps
};
#define __xrtTlsStreamTransportEvents (__xrtTlsStreamTransportPolicy.Events)
XRT_API const xnetstreameventsownershipv1* xrtTlsStreamTransportOwnershipV1Get(void)
{ return &__xrtTlsStreamTransportPolicy; }



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
	if (pStream && pStream->OwnershipPolicy) {
		__xrtErrorSetInvalidState(); return false;
	}
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



static bool __xrtTlsStreamOwnedPolicyValid(ptr data, const xtlsstreamownershipv1* policy)
{
	return data && policy && policy->size == sizeof(*policy) && policy->Drop &&
		policy->Ops && policy->Ops->Count && policy->Ops->Trace;
}
static xtlsstream* __xrtTlsStreamCreateOwned(xnetengine* engine,
	xtlssession* session, bool server, const xtlsstreamconfig* config,
	const xtlsstreamevents* events, ptr data, const xtlsstreamownershipv1* policy)
{
	if (!policy) return __xrtTlsStreamCreate(session, server, config, events, data);
	if (!__xrtTlsStreamOwnedPolicyValid(data, policy)) { __xrtErrorSetInvalidArgument(); return NULL; }
	if (!__xrtNetEngineObjectHold(engine)) return NULL;
	if (!__xrtNetEngineStorageRetain(engine)) { __xrtNetEngineObjectRelease(engine); return NULL; }
	xtlsstream* s = __xrtTlsStreamCreate(session, server, config, &policy->Events, data);
	if (!s) { __xrtNetEngineObjectRelease(engine); __xrtNetEngineStorageRelease(engine); return NULL; }
	s->OwnershipPolicy = policy; s->OwnershipEngine = engine;
	s->OwnershipEngineHeld = true; s->OwnershipStorageHeld = true; s->OwnershipDataHeld = true;
	xrtAtomic32Store(&s->OwnershipActive, 1, XMEMORY_RELEASE);
	if (!__xrtNetEngineRuntimeAdopt(engine, &s->RuntimeOwner, s, xrtTlsStreamRuntimeOwnershipV1Get())) {
		s->Session = NULL; /* failed creation preserves the submitter's session */
		__xrtTlsStreamDiscard(s); return NULL;
	}
	return s;
}
static void __xrtTlsStreamConstructed(xtlsstream* s)
{ if (s->OwnershipPolicy) __xrtTlsStreamOwnershipLeave(s); }

/* 创建客户端 TLS 会话并连接数字地址。 */
static xtlsstream* __xrtTlsStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pTransport,
	const xtlsclientconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	const xtlsstreamownershipv1* pPolicy
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
	pStream = __xrtTlsStreamCreateOwned(
		pEngine,
		pSession,
		false,
		pConfig,
		pEvents,
		pData,
		pPolicy
	);
	if ( pStream == NULL ) {
		xrtTlsSessionDestroy(pSession);
		return NULL;
	}
	if (pPolicy) {
		if (!xrtTlsStreamRef(pStream)) { __xrtTlsStreamDiscard(pStream); return NULL; }
		pTcp = xrtNetStreamConnectOwnedV1(pEngine, pRemote, iAffinity, &Transport,
			pStream, xrtTlsStreamTransportOwnershipV1Get());
		if (!pTcp) xrtTlsStreamDestroy(pStream); /* unconsumed TCP Data credit */
	} else pTcp = xrtNetStreamConnect(pEngine, pRemote, iAffinity, &Transport,
		&__xrtTlsStreamTransportEvents, pStream);
	if ( pTcp == NULL ) {
		pError = xrtTakeError();
		__xrtTlsStreamDiscard(pStream);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(&pStream->Transport, pTcp, XMEMORY_RELEASE);
	__xrtTlsStreamConstructed(pStream);
	return pStream;
}

XRT_API xtlsstream* xrtTlsStreamConnect(xnetengine* engine, const xnetaddr* remote,
	uint64 affinity, const xnetstreamconfig* transport, const xtlsclientconfig* tls,
	const xtlsstreamconfig* config, const xtlsstreamevents* events, ptr data)
{ return __xrtTlsStreamConnect(engine, remote, affinity, transport, tls, config, events, data, NULL); }
XRT_API xtlsstream* xrtTlsStreamConnectOwnedV1(xnetengine* engine, const xnetaddr* remote,
	uint64 affinity, const xnetstreamconfig* transport, const xtlsclientconfig* tls,
	const xtlsstreamconfig* config, ptr data, const xtlsstreamownershipv1* policy)
{
	if (!__xrtTlsStreamOwnedPolicyValid(data, policy)) { __xrtErrorSetInvalidArgument(); return NULL; }
	return __xrtTlsStreamConnect(engine, remote, affinity, transport, tls, config, NULL, data, policy);
}



/*
	把已连接 TCP Stream 与调用方创建的 TLS Session 组合。
	全部可失败检查都在接管引用前完成。
*/
static bool __xrtTlsStreamInstallTransport(xtlsstream* stream, xnetstream* transport)
{
	if (!stream->OwnershipPolicy)
		return xrtNetStreamSetEvents(transport, &__xrtTlsStreamTransportEvents, stream);
	if (!xrtTlsStreamRef(stream)) return false;
	if (xrtNetStreamSetEventsOwnedV1(transport, stream, xrtTlsStreamTransportOwnershipV1Get())) return true;
	xrtTlsStreamDestroy(stream); /* exactly the unconsumed event-owner credit */
	return false;
}
static bool __xrtTlsStreamAttach(
	xnetstream* pTransport,
	xtlssession* pSession,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream,
	const xtlsstreamownershipv1* pPolicy
)
{
	xnetbufpool* pPool;
	xtlsstream* pStream;
	xtlsrole Role;
	xnetbufpool* PreviousPools[4];

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
	pStream = __xrtTlsStreamCreateOwned(
		pTransport->Engine,
		pSession,
		Role == XTLS_SERVER,
		pConfig,
		pEvents,
		pData,
		pPolicy
	);
	if ( pStream == NULL ) {
		return false;
	}
	PreviousPools[0] = pSession->Feed.Pool;
	PreviousPools[1] = pSession->Send.Pool;
	PreviousPools[2] = pSession->Plain.Pool;
	PreviousPools[3] = pSession->Scratch.Pool;
	if ( !__xrtTlsSessionPool(pSession, pPool) ) {
		pStream->Session = NULL;
		__xrtTlsStreamDiscard(pStream);
		return false;
	}
	/* TCP owns replacement ordering, read-Future exclusion and deferred old
	 * callback retirement. Direct field writes leave an old EventOwner behind.
	 * This setter runs no callbacks; failure precedes TCP publication. Restore
	 * the four allocation policies without consuming either caller reference. */
	bool installed = __xrtTlsStreamInstallTransport(pStream, pTransport);
	if ( !installed ) {
		pSession->Feed.Pool = PreviousPools[0];
		pSession->Send.Pool = PreviousPools[1];
		pSession->Plain.Pool = PreviousPools[2];
		pSession->Scratch.Pool = PreviousPools[3];
		pStream->Session = NULL;
		__xrtTlsStreamDiscard(pStream);
		return false;
	}
	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	*ppStream = pStream;
	__xrtTlsStreamConstructed(pStream);
	__xrtTlsStreamTransportOpen(pTransport, pStream);
	return true;
}

XRT_API bool xrtTlsStreamAttach(xnetstream* transport, xtlssession* session,
	const xtlsstreamconfig* config, const xtlsstreamevents* events, ptr data, xtlsstream** stream)
{ return __xrtTlsStreamAttach(transport, session, config, events, data, stream, NULL); }
XRT_API bool xrtTlsStreamAttachOwnedV1(xnetstream* transport, xtlssession* session,
	const xtlsstreamconfig* config, ptr data, const xtlsstreamownershipv1* policy, xtlsstream** stream)
{
	if (stream) *stream = NULL;
	if (!__xrtTlsStreamOwnedPolicyValid(data, policy)) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtTlsStreamAttach(transport, session, config, NULL, data, stream, policy);
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
static bool __xrtTlsStreamAccept(
	xnetstream* pTransport,
	const xtlsserverconfig* pTls,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData,
	xtlsstream** ppStream,
	const xtlsstreamownershipv1* pPolicy
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
	pStream = __xrtTlsStreamCreateOwned(
		pTransport->Engine,
		pSession,
		true,
		pConfig,
		pEvents,
		pData,
		pPolicy
	);
	if ( pStream == NULL ) {
		xrtTlsSessionDestroy(pSession);
		return false;
	}
	bool installed = __xrtTlsStreamInstallTransport(pStream, pTransport);
	if ( !installed ) {
		__xrtTlsStreamDiscard(pStream);
		return false;
	}
	xrtAtomicPtrStore(
		&pStream->Transport,
		pTransport,
		XMEMORY_RELEASE
	);
	*ppStream = pStream;
	__xrtTlsStreamConstructed(pStream);
	return true;
}

XRT_API bool xrtTlsStreamAccept(xnetstream* transport, const xtlsserverconfig* tls,
	const xtlsstreamconfig* config, const xtlsstreamevents* events, ptr data, xtlsstream** stream)
{ return __xrtTlsStreamAccept(transport, tls, config, events, data, stream, NULL); }
XRT_API bool xrtTlsStreamAcceptOwnedV1(xnetstream* transport, const xtlsserverconfig* tls,
	const xtlsstreamconfig* config, ptr data, const xtlsstreamownershipv1* policy, xtlsstream** stream)
{
	if (stream) *stream = NULL;
	if (!__xrtTlsStreamOwnedPolicyValid(data, policy)) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtTlsStreamAccept(transport, tls, config, NULL, data, stream, policy);
}
XRT_API bool xrtTlsStreamClientOwnedV1(xnetstream* transport, const xtlsclientconfig* tls,
	const xtlsstreamconfig* config, ptr data, const xtlsstreamownershipv1* policy, xtlsstream** stream)
{
	if (stream) *stream = NULL;
	if (!transport || !stream || !__xrtTlsStreamOwnedPolicyValid(data, policy)) { __xrtErrorSetInvalidArgument(); return false; }
	xtlssession* session = xrtTlsClientCreate(tls, NULL);
	if (!session) return false;
	if (!xrtTlsStreamAttachOwnedV1(transport, session, config, data, policy, stream)) {
		xrtTlsSessionDestroy(session); return false;
	}
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
	size_t* pWritten,
	bool bSubmission
)
{
	xtlsresult Result = XTLS_OK;
	xnetresult NetResult;
	size_t iTotal = 0;

	if ( pWritten != NULL ) {
		*pWritten = 0;
	}
	if ( !__xrtTlsStreamWorkerMode(pStream, "send-tls-stream", bSubmission) ||
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
	pStream->Sending = false;
	if ( pStream->CloseEmitted ) {
		return *pWritten != 0 ? Result : XTLS_CLOSED;
	}
	if ( NetResult == XNET_RESULT_ERROR || NetResult == XNET_RESULT_CLOSED ) {
		__xrtTlsStreamFail(pStream);
		return XTLS_ERROR;
	}
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
		pWritten,
		true
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
		pWritten,
		true
	);
}

#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
xtlsresult __xrtTlsStreamSendAccepted(xtlsstream* stream, const void* data, size_t size, size_t* written)
{
	xnetspan span = {(cbytes)data, size};
	return __xrtTlsStreamSendSpans(stream, &span, 1, written, false);
}
#endif



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
static xtlsresult __xrtTlsStreamReadImpl(
	xtlsstream* pStream,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead,
	bool bSubmission
)
{
	xtlsresult Result;

	if ( !__xrtTlsStreamWorkerMode(pStream, "read-tls-stream", bSubmission) ) {
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

XRT_API xtlsresult xrtTlsStreamRead(xtlsstream* stream, void* output, size_t capacity, size_t* read)
{ return __xrtTlsStreamReadImpl(stream, output, capacity, read, true); }
#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
xtlsresult __xrtTlsStreamReadAccepted(xtlsstream* stream, void* output, size_t capacity, size_t* read)
{ return __xrtTlsStreamReadImpl(stream, output, capacity, read, false); }
#endif



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
static bool __xrtTlsStreamClose(xtlsstream* pStream)
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
	if (!xrtTlsStreamRef(pStream)) {
		xrtAtomic32Store(&pStream->CloseGate, 0, XMEMORY_RELEASE);
		#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
			__xrtSpinUnlock(&pStream->AsyncLock);
		#endif
		return false;
	}
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
static bool __xrtTlsStreamAbort(xtlsstream* pStream)
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



XRT_API bool xrtTlsStreamClose(xtlsstream* s)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return false;
	bool result = __xrtTlsStreamClose(s); __xrtTlsStreamOwnershipLeave(s); return result;
}
XRT_API bool xrtTlsStreamAbort(xtlsstream* s)
{
	if (!__xrtTlsStreamOwnershipEnter(s, true)) return false;
	bool result = __xrtTlsStreamAbort(s); __xrtTlsStreamOwnershipLeave(s); return result;
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

/* Frozen graph of an explicitly owned session. Legacy callback borrowing is
 * deliberately opaque; a count alone never certifies that graph. */
bool __xrtTlsStreamOwnershipEnter(xtlsstream* pStream, bool bSubmit)
{
	if (!pStream) { __xrtErrorSetInvalidArgument(); return false; }
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	uint32 active = xrtAtomic32Load(&pStream->OwnershipActive, XMEMORY_ACQUIRE);
	bool entered = false;
	if (!(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED) &&
		(!bSubmit || !xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE))) {
		while (active < UINT32_MAX) {
			uint32 expected = active;
			if (xrtAtomic32CompareExchange(&pStream->OwnershipActive, &expected, active + 1,
				XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) { entered = true; break; }
			active = expected;
		}
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!entered) __xrtErrorSetInvalidState();
	return entered;
}
void __xrtTlsStreamOwnershipLeave(xtlsstream* pStream)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!xrtAtomic32FetchSub(&pStream->OwnershipActive, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
static bool __xrtTlsStreamOwnershipBusy(const void* pData)
{
	const xtlsstream* s = pData;
	if (!s) return false;
	if (xrtAtomic32Load(&s->OwnershipActive, XMEMORY_ACQUIRE)) return true;
	return s->OwnershipEngine && xrtNetEngineOwnershipQuiescenceV1Get()->Busy(s->OwnershipEngine);
}
static bool __xrtTlsStreamOwnershipStopped(const void* pData)
{
	const xtlsstream* s = pData;
	if (!s || __xrtTlsStreamOwnershipBusy(s) || s->RuntimeHeld || s->OwnershipEngineHeld ||
		!s->CloseEmitted || s->ActiveDepth || s->HandshakeTimer || s->CloseTimer || s->DrivePosted ||
		s->Driving || s->Sending || s->DriveCommand.Task || s->CloseCommand.Task) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if (xrtAtomic32Load(&s->AsyncCount, XMEMORY_ACQUIRE) || s->AsyncPosted ||
			s->AsyncRunning || s->AsyncCommand.Task) return false;
	#endif
	xnetstream* tcp = xrtAtomicPtrLoad(&s->Transport, XMEMORY_ACQUIRE);
	return !tcp || (xrtNetStreamState(tcp) == XNET_STREAM_CLOSED &&
		!xrtNetStreamOwnershipQuiescenceV1Get()->Busy(tcp));
}
static bool __xrtTlsStreamOwnershipReady(const void* pData)
{
	const xtlsstream* s = pData;
	return s && s->OwnershipBuffersSettled && __xrtTlsStreamOwnershipStopped(s);
}
static bool __xrtTlsStreamValidateBufferEdge(xrtownershipref ref, ptr context)
{ (void)context; return ref.Data && ref.Ops && ref.Ops->Count && ref.Ops->Trace; }
static bool __xrtTlsStreamOwnershipCount(const void* pData, size_t* pCount)
{
	const xtlsstream* s = pData;
	if (!s || !pCount || __xrtTlsStreamOwnershipBusy(s) ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED) ||
		!s->OwnershipPolicy || !s->OwnershipStorageHeld || !s->OwnershipEngine ||
		!s->OwnershipDataHeld || !xrtAtomicPtrLoad(&s->Data, XMEMORY_ACQUIRE) || !s->Session ||
		s->ActiveDepth || s->Driving || s->Sending) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
		if (s->ManagedOpen || s->ManagedClose || s->ManagedData) return false;
	#endif
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if (!__xrtTlsStreamAsyncOwnershipWalk(s, NULL, NULL)) return false;
	#endif
	if (s->RuntimeHeld) {
		if (!s->OwnershipEngineHeld || s->RuntimeOwner.Data != s || !s->RuntimeOwner.Previous ||
			s->RuntimeOwner.Engine != s->OwnershipEngine ||
			s->RuntimeOwner.Policy != xrtTlsStreamRuntimeOwnershipV1Get()) return false;
	} else if (s->OwnershipEngineHeld || s->RuntimeOwner.Data || s->RuntimeOwner.Previous ||
		!(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_READY)) return false;
	xnetstream* tcp = xrtAtomicPtrLoad(&s->Transport, XMEMORY_ACQUIRE);
	if (!tcp) return false;
	if (xrtNetStreamState(tcp) != XNET_STREAM_CLOSED &&
		(!tcp->EventOwner || tcp->EventOwner->Data != s ||
		 tcp->EventOwner->Policy != xrtTlsStreamTransportOwnershipV1Get())) return false;
	if (!xrtTlsSessionBufferOwnershipTrace(s->Session, __xrtTlsStreamValidateBufferEdge, NULL)) return false;
	int32 refs = __xrtAtomicRefLoad(&s->References);
	if (refs <= 0) return false;
	*pCount = (size_t)refs; return true;
}
static bool __xrtTlsStreamOwnershipTrace(const void* pData, xrtownershipvisitor visit, ptr context)
{
	const xtlsstream* s = pData; size_t count;
	if (!visit || !__xrtTlsStreamOwnershipCount(s, &count)) return false;
	/* Storage and operational ownership are TWO actual Engine references. */
	if (!visit(xrtNetEngineOwnership(s->OwnershipEngine), context) ||
		(s->OwnershipEngineHeld && !visit(xrtNetEngineOwnership(s->OwnershipEngine), context)) ||
		!visit(xrtNetStreamOwnership(xrtAtomicPtrLoad(&s->Transport, XMEMORY_ACQUIRE)), context) ||
		!visit((xrtownershipref){xrtAtomicPtrLoad(&s->Data, XMEMORY_ACQUIRE), s->OwnershipPolicy->Ops}, context) ||
		!xrtTlsSessionBufferOwnershipTrace(s->Session, visit, context)) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if (!__xrtTlsStreamAsyncOwnershipWalk(s, visit, context)) return false;
	#endif
	return !s->Error || visit(xrtErrorOwnership(s->Error), context);
}
XRT_API xrtownershipref xrtTlsStreamOwnership(const xtlsstream* pStream)
{ return (xrtownershipref){pStream, pStream ? &__xrtTlsStreamOwnershipOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtTlsStreamOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 policy = {sizeof(policy), &__xrtTlsStreamOwnershipOps, __xrtTlsStreamOwnershipBusy};
	return &policy;
}
static void __xrtTlsStreamRuntimeDrop(ptr pData) { xrtTlsStreamDestroy(pData); }
static void __xrtTlsStreamTransportDrop(const void* pData) { xrtTlsStreamDestroy((xtlsstream*)pData); }
XRT_API const xnetruntimeownershipv1* xrtTlsStreamRuntimeOwnershipV1Get(void)
{
	static const xnettimerproc timers[] = {__xrtTlsStreamHandshakeTimer, __xrtTlsStreamCloseTimer};
	static const xnetruntimeownershipv1 policy = {sizeof(policy), &__xrtTlsStreamOwnershipOps, __xrtTlsStreamRuntimeDrop, timers, 2};
	return &policy;
}
static void __xrtTlsStreamOwnedRuntimeRelease(xtlsstream* s)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	const xnetruntimeownershipv1* policy = NULL;
	if (!s->RuntimeHeld || !s->OwnershipEngineHeld ||
		__xrtNetEngineRuntimeTake(s->OwnershipEngine, &s->RuntimeOwner, &policy) != s ||
		policy != xrtTlsStreamRuntimeOwnershipV1Get()) abort();
	s->RuntimeHeld = false; s->OwnershipEngineHeld = false;
	xrtAtomic32Store(&s->OwnershipState, XRT_TLS_STREAM_OWNERSHIP_READY, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	__xrtNetEngineObjectRelease(s->OwnershipEngine);
}
static bool __xrtTlsStreamOwnershipHold(const void* pData)
{
	xtlsstream* s = (xtlsstream*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool held = !(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED) && xrtRefRetain(&s->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return held;
}
static void __xrtTlsStreamOwnershipDrop(const void* pData) { xrtTlsStreamDestroy((xtlsstream*)pData); }
static bool __xrtTlsStreamOwnershipClaim(const void* pData, const void* token)
{
	xtlsstream* s = (xtlsstream*)pData; size_t count; ptr old = xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!token || (old && old != token) || !__xrtTlsStreamOwnershipCount(s, &count)) return false;
	xrtAtomicPtrStore(&s->OwnershipClaim, (ptr)token, XMEMORY_RELEASE); return true;
}
static void __xrtTlsStreamOwnershipRestore(const void* pData, const void* token)
{
	xtlsstream* s = (xtlsstream*)pData;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED)) abort();
	xrtAtomicPtrStore(&s->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static xrtownershipprepareresult __xrtTlsStreamOwnershipPrepare(const void* pData, const void* token)
{
	xtlsstream* s = (xtlsstream*)pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED)) abort();
	xrtownershipprepareresult result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtTlsStreamOwnershipReady(s)) result = XRT_OWNERSHIP_PREPARE_READY;
	else if (__xrtTlsStreamOwnershipStopped(s)) {
		/* A closed TLS session can still own unread Plain/Feed/Scratch
		 * blocks. Keeping those leases until Clear deadlocks allocator and
		 * Engine preparation. Detach under the stable claimed graph, then
		 * release OUTSIDE Freeze; READY reports this actual edge change. */
		xnetbuf buffers[4] = {s->Session->Feed, s->Session->Send, s->Session->Plain, s->Session->Scratch};
		s->Session->Feed = (xnetbuf){0}; s->Session->Send = (xnetbuf){0};
		s->Session->Plain = (xnetbuf){0}; s->Session->Scratch = (xnetbuf){0};
		s->Session->Record = (xtlssessionrecord){0};
		s->Session->RecordPending = false; s->Session->RecordSize = 0;
		xrtAtomic64Store(&s->Available, 0, XMEMORY_RELEASE);
		xrtAtomic64Store(&s->CipherPending, 0, XMEMORY_RELEASE);
		xrtAtomic32Store(&s->OwnershipActive, 1, XMEMORY_RELEASE);
		if (!xrtOwnershipScopeEnd(&Freeze)) abort();
		for (size_t i = 0; i < 4; ++i) xrtNetBufClear(&buffers[i]);
		xrtownershipscope Mutation = {0};
		if (!xrtOwnershipMutationBegin(&Mutation)) abort();
		s->OwnershipBuffersSettled = true;
		if (xrtAtomic32FetchSub(&s->OwnershipActive, 1, XMEMORY_ACQ_REL) != 1) abort();
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		return XRT_OWNERSHIP_PREPARE_READY;
	}
	else if (!__xrtTlsStreamOwnershipBusy(s)) {
		xnetstream* tcp = xrtAtomicPtrLoad(&s->Transport, XMEMORY_ACQUIRE);
		/* The TCP participant may already have been claimed by this SAME
		 * graph. Its native lifecycle post borrows its real runtime slot. */
		if (tcp && tcp->RuntimeHeld && !__xrtNetStreamOwnershipStop(tcp)) result = XRT_OWNERSHIP_PREPARE_FAILED;
	}
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return result;
}
static void __xrtTlsStreamOwnershipClear(const void* pData, const void* token)
{
	xtlsstream* s = (xtlsstream*)pData;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		!__xrtTlsStreamOwnershipReady(s) ||
		(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED)) abort();
	xrtAtomic32Store(&s->OwnershipState, XRT_TLS_STREAM_OWNERSHIP_READY | XRT_TLS_STREAM_OWNERSHIP_CLEARED, XMEMORY_RELEASE);
}
static bool __xrtTlsStreamOwnershipFinish(const void* pData, const void* token)
{
	xtlsstream* s = (xtlsstream*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	if (!token || xrtAtomicPtrLoad(&s->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		!(xrtAtomic32Load(&s->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_STREAM_OWNERSHIP_CLEARED)) abort();
	if (s->OwnershipFinishing) { if (!xrtOwnershipScopeEnd(&Mutation)) abort(); return false; }
	if (s->OwnershipFinished) {
		xerror* failure = s->OwnershipFinishError;
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		if (failure && !xrtGetError()) xrtSetError(failure);
		return failure == NULL;
	}
	s->OwnershipFinishing = true;
	xtlssession* session = s->Session; s->Session = NULL;
	xnetstream* tcp = xrtAtomicPtrExchange(&s->Transport, NULL, XMEMORY_ACQ_REL);
	ptr data = s->OwnershipDataHeld ? xrtAtomicPtrExchange(&s->Data, NULL, XMEMORY_ACQ_REL) : NULL;
	s->OwnershipDataHeld = false;
	xerror* error = s->Error; s->Error = NULL;
	xnetengine* engine = s->OwnershipEngine;
	bool storage = s->OwnershipStorageHeld, operational = s->OwnershipEngineHeld;
	s->OwnershipStorageHeld = false; s->OwnershipEngineHeld = false; s->OwnershipEngine = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* Every graph target remains pinned. Release the Session before its
	 * configuration owner, with no Freeze/Mutation across any destructor. */
	xerror* prior = xrtTakeError();
	xrtTlsSessionDestroy(session); xrtNetStreamDestroy(tcp); xrtErrorFree(error);
	if (data) s->OwnershipPolicy->Drop(data);
	if (operational) __xrtNetEngineObjectRelease(engine);
	if (storage) __xrtNetEngineStorageRelease(engine);
	xerror* failure = xrtTakeError();
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	s->OwnershipFinishError = failure; s->OwnershipFinishing = false; s->OwnershipFinished = true;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (prior) xrtSetErrorTake(prior);
	else if (failure) xrtSetError(failure);
	return failure == NULL;
}
XRT_API const xrtownershipadapterv1* xrtTlsStreamOwnershipAdapterV1(xrtownershipref ref,
	const xtlsstreamownershipv1* const* policies, size_t policyCount,
	const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xrtTlsStreamOwnershipHold, __xrtTlsStreamOwnershipDrop,
		__xrtTlsStreamOwnershipClaim, __xrtTlsStreamOwnershipRestore, NULL, __xrtTlsStreamOwnershipClear, __xrtTlsStreamOwnershipFinish};
	static const xrtownershippreparationv1 prepare = {sizeof(prepare), &adapter, __xrtTlsStreamOwnershipReady, __xrtTlsStreamOwnershipPrepare};
	if (!preparation || ref.Ops != &__xrtTlsStreamOwnershipOps || !ref.Data || (policyCount && !policies) || __xrtTlsStreamOwnershipBusy(ref.Data)) return NULL;
	const xtlsstream* s = ref.Data; bool known = false;
	for (size_t i = 0; i < policyCount; ++i) if (policies[i] == s->OwnershipPolicy) { known = true; break; }
	if (!known || !s->OwnershipPolicy || s->OwnershipPolicy->size != sizeof(*s->OwnershipPolicy) ||
		!s->OwnershipPolicy->Drop || !s->OwnershipPolicy->Ops || !s->OwnershipPolicy->Ops->Count || !s->OwnershipPolicy->Ops->Trace) return NULL;
	size_t count; if (!__xrtTlsStreamOwnershipCount(s, &count)) return NULL;
	*preparation = &prepare; return &adapter;
}

#endif
