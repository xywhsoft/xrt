#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)

#define XRT_TLS_LISTENER_QUEUE_DEFAULT 1024u
#define XRT_TLS_LISTENER_HANDSHAKE_DEFAULT 1024u



/* 构造 TLS Listener 域错误，并保留可选的底层原因链。 */
static xerror* __xrtTlsListenerErrorCreate(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.tls";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	return xrtErrorBuild(&Desc);
}



/* 设置 TLS Listener 域错误，分配失败时保留当前更具体的错误。 */
static void __xrtTlsListenerSetError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtTlsListenerErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 初始化适合通用服务端的有界监听、握手和完成队列。 */
XRT_API void xrtTlsListenerConfigInit(xtlslistenerconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"init-tls-listener-config",
			"TLS listener config is null",
			NULL
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetListenConfigInit(&pConfig->Listen);
	xrtTlsServerConfigInit(&pConfig->Tls);
	xrtTlsStreamConfigInit(&pConfig->Stream);
	pConfig->AcceptQueueLimit = XRT_TLS_LISTENER_QUEUE_DEFAULT;
	pConfig->HandshakeLimit = XRT_TLS_LISTENER_HANDSHAKE_DEFAULT;
}



/* 验证不依赖 Worker 资源的 Listener、TLS 和组合流配置边界。 */
static bool __xrtTlsListenerConfigValid(
	const xtlslistenerconfig* pConfig
)
{
	if ( (pConfig == NULL) ||
		(pConfig->AcceptQueueLimit == 0) ||
		(pConfig->HandshakeLimit == 0) ||
		((pConfig->Tls.Identity == NULL) &&
		 (pConfig->Tls.Select == NULL)) ||
		((pConfig->Tls.Protocols == NULL) &&
		 (pConfig->Tls.ProtocolCount != 0)) ||
		(pConfig->Tls.ProtocolCount >
		 (SIZE_MAX / sizeof(xstrview))) ||
		!__xrtNetListenConfigValid(&pConfig->Listen) ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"configure-tls-listener",
			"invalid TLS listener configuration",
			NULL
		);
		return false;
	}
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		if ( (pConfig->Stream.AsyncBytesLimit == 0) ||
			(pConfig->Stream.AsyncCountLimit == 0) ||
			(pConfig->Stream.AsyncBatch == 0) ) {
			__xrtTlsListenerSetError(
				XERR_RANGE,
				XTLS_ERROR_LIMIT,
				"configure-tls-listener",
				"TLS listener asynchronous limits must be nonzero",
				NULL
			);
			return false;
		}
	#endif
	for ( size_t i = 0; i < pConfig->Tls.ProtocolCount; i++ ) {
		const xstrview* pProtocol = &pConfig->Tls.Protocols[i];

		if ( (pProtocol->Data == NULL) || (pProtocol->Size == 0) ||
			(pProtocol->Size > UINT8_MAX) ) {
			__xrtTlsListenerSetError(
				XERR_VALUE,
				XTLS_ERROR_EXTENSION,
				"configure-tls-listener",
				"TLS listener ALPN protocol is invalid",
				NULL
			);
			return false;
		}
	}
	return true;
}



/* 计算 Listener、ALPN 视图和协议字符串的一次分配布局。 */
static bool __xrtTlsListenerLayout(
	const xtlslistenerconfig* pConfig,
	size_t* pViewsOffset,
	size_t* pDataOffset,
	size_t* pTotal
)
{
	size_t iAlign = _Alignof(xstrview);
	size_t iViews;
	size_t iBytes = 0;
	size_t iViewsOffset;
	size_t iDataOffset;

	for ( size_t i = 0; i < pConfig->Tls.ProtocolCount; i++ ) {
		if ( pConfig->Tls.Protocols[i].Size > (SIZE_MAX - iBytes) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBytes += pConfig->Tls.Protocols[i].Size;
	}
	iViews = pConfig->Tls.ProtocolCount * sizeof(xstrview);
	if ( sizeof(xtlslistener) > (SIZE_MAX - (iAlign - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iViewsOffset = (sizeof(xtlslistener) + (iAlign - 1u)) &
		~(iAlign - 1u);
	if ( iViews > (SIZE_MAX - iViewsOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iDataOffset = iViewsOffset + iViews;
	if ( iBytes > (SIZE_MAX - iDataOffset) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pViewsOffset = iViewsOffset;
	*pDataOffset = iDataOffset;
	*pTotal = iDataOffset + iBytes;
	return true;
}



/* 深复制 ALPN，并为共享 TLS Context 和静态 Identity 持有引用。 */
static bool __xrtTlsListenerTlsCopy(
	xtlslistener* pListener,
	const xtlsserverconfig* pTls,
	size_t iViewsOffset,
	size_t iDataOffset
)
{
	xstrview* pViews = (xstrview*)((bytes)pListener + iViewsOffset);
	bytes pData = (bytes)pListener + iDataOffset;

	pListener->Tls = *pTls;
	pListener->Tls.Context = NULL;
	pListener->Tls.Identity = NULL;
	pListener->Tls.Protocols = NULL;
	if ( pTls->Context != NULL ) {
		pListener->Tls.Context = xrtTlsContextRetain(pTls->Context);
		if ( pListener->Tls.Context == NULL ) {
			return false;
		}
	}
	if ( pTls->Identity != NULL ) {
		pListener->Tls.Identity = xrtTlsIdentityRetain(
			pTls->Identity
		);
		if ( pListener->Tls.Identity == NULL ) {
			xrtTlsContextRelease((xtlscontext*)pListener->Tls.Context);
			pListener->Tls.Context = NULL;
			return false;
		}
	}
	for ( size_t i = 0; i < pTls->ProtocolCount; i++ ) {
		pViews[i].Data = (cstr)pData;
		pViews[i].Size = pTls->Protocols[i].Size;
		memcpy(pData, pTls->Protocols[i].Data, pViews[i].Size);
		pData += pViews[i].Size;
	}
	pListener->Tls.Protocols = pViews;
	return true;
}



/* 释放 Listener 持有的 TLS 配置引用。 */
static void __xrtTlsListenerTlsUnit(xtlslistener* pListener)
{
	xrtTlsIdentityRelease((xtlsidentity*)pListener->Tls.Identity);
	xrtTlsContextRelease((xtlscontext*)pListener->Tls.Context);
	pListener->Tls.Identity = NULL;
	pListener->Tls.Context = NULL;
}



/* 增加 Listener 引用。 */
XRT_API xtlslistener* xrtTlsListenerRef(xtlslistener* pListener)
{
	if ( (pListener == NULL) ||
		(xrtRefRetain(&pListener->References) < 0) ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"retain-tls-listener",
			"TLS listener reference is invalid",
			NULL
		);
		return NULL;
	}
	return pListener;
}



/* 释放最后一个 Listener 引用以及不可变 TLS 配置快照。 */
XRT_API void xrtTlsListenerDestroy(xtlslistener* pListener)
{
	if ( (pListener == NULL) ||
		(xrtRefRelease(&pListener->References) != 0) ) {
		return;
	}
	__xrtTlsListenerTlsUnit(pListener);
	xrtErrorFree(pListener->StartupError);
	__xrtSpinUnit(&pListener->Lock);
	xrtFree(pListener);
}



/* 无锁更新并发握手峰值，统计允许近似但不允许回退。 */
static void __xrtTlsListenerPeak(
	xtlslistener* pListener,
	uint32 iActive
)
{
	uint32 iPeak = xrtAtomic32Load(
		&pListener->PeakHandshakes,
		XMEMORY_RELAXED
	);

	while ( (iActive > iPeak) &&
		!xrtAtomic32CompareExchange(
			&pListener->PeakHandshakes,
			&iPeak,
			iActive,
			XMEMORY_RELAXED,
			XMEMORY_RELAXED
		) ) {
	}
}



/* 在 TLS 会话分配前原子预留一个并发握手名额。 */
static bool __xrtTlsListenerHandshakeReserve(
	xtlslistener* pListener
)
{
	uint32 iActive = xrtAtomic32Load(
		&pListener->ActiveHandshakes,
		XMEMORY_ACQUIRE
	);

	while ( (iActive < pListener->HandshakeLimit) &&
		!xrtAtomic32CompareExchange(
			&pListener->ActiveHandshakes,
			&iActive,
			iActive + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
	}
	if ( iActive >= pListener->HandshakeLimit ) {
		return false;
	}
	(void)xrtAtomic64FetchAdd(
		&pListener->Handshakes,
		1,
		XMEMORY_RELAXED
	);
	__xrtTlsListenerPeak(pListener, iActive + 1u);
	return true;
}



/* 调用方持有 Listener.Lock 时，从活动握手链移除节点。 */
static void __xrtTlsListenerHandshakeUnlink(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pNode
)
{
	if ( pNode->HandshakePrevious != NULL ) {
		pNode->HandshakePrevious->HandshakeNext =
			pNode->HandshakeNext;
	} else if ( pListener->HandshakeHead == pNode ) {
		pListener->HandshakeHead = pNode->HandshakeNext;
	}
	if ( pNode->HandshakeNext != NULL ) {
		pNode->HandshakeNext->HandshakePrevious =
			pNode->HandshakePrevious;
	}
	pNode->HandshakePrevious = NULL;
	pNode->HandshakeNext = NULL;
}



/* 归还一次握手预留并从活动链摘除；返回值表示本次完成了归还。 */
static bool __xrtTlsListenerHandshakeRelease(
	__xrt_tls_listener_stream* pNode
)
{
	xtlslistener* pListener = pNode->Listener;
	bool bReleased = false;

	__xrtSpinLock(&pListener->Lock);
	if ( pNode->HandshakePending ) {
		pNode->HandshakePending = false;
		__xrtTlsListenerHandshakeUnlink(pListener, pNode);
		bReleased = true;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( bReleased ) {
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveHandshakes,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return bReleased;
}



/* 调用方持有 Lock 时取走一个排队节点。 */
__xrt_tls_listener_stream* __xrtTlsListenerTakeQueued(
	xtlslistener* pListener
)
{
	__xrt_tls_listener_stream* pNode = pListener->AcceptHead;

	if ( pNode == NULL ) {
		return NULL;
	}
	pListener->AcceptHead = pNode->Next;
	if ( pListener->AcceptHead == NULL ) {
		pListener->AcceptTail = NULL;
	}
	pNode->Next = NULL;
	pNode->Queued = false;
	(void)xrtAtomic32FetchSub(
		&pListener->QueuedAccepts,
		1,
		XMEMORY_ACQ_REL
	);
	return pNode;
}



/* 把握手完成节点放入有界 FIFO，并更新队列峰值。 */
static bool __xrtTlsListenerQueue(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pNode
)
{
	uint32 iQueued;
	bool bQueued = false;

	__xrtSpinLock(&pListener->Lock);
	iQueued = xrtAtomic32Load(
		&pListener->QueuedAccepts,
		XMEMORY_RELAXED
	);
	if ( (xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN) &&
		(iQueued < pListener->AcceptQueueLimit) ) {
		pNode->Next = NULL;
		pNode->Queued = true;
		if ( pListener->AcceptTail != NULL ) {
			pListener->AcceptTail->Next = pNode;
		} else {
			pListener->AcceptHead = pNode;
		}
		pListener->AcceptTail = pNode;
		iQueued++;
		xrtAtomic32Store(
			&pListener->QueuedAccepts,
			iQueued,
			XMEMORY_RELEASE
		);
		if ( iQueued > xrtAtomic32Load(
			&pListener->PeakQueuedAccepts,
			XMEMORY_RELAXED
		) ) {
			xrtAtomic32Store(
				&pListener->PeakQueuedAccepts,
				iQueued,
				XMEMORY_RELAXED
			);
		}
		bQueued = true;
	}
	__xrtSpinUnlock(&pListener->Lock);
	return bQueued;
}



/* 从队列中移除意外提前关闭的节点；调用方持有 Lock。 */
static bool __xrtTlsListenerQueueRemove(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pNode
)
{
	__xrt_tls_listener_stream** ppCurrent = &pListener->AcceptHead;
	__xrt_tls_listener_stream* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pNode) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pNode->Next;
	if ( pListener->AcceptTail == pNode ) {
		pListener->AcceptTail = pPrevious;
	}
	pNode->Next = NULL;
	pNode->Queued = false;
	(void)xrtAtomic32FetchSub(
		&pListener->QueuedAccepts,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 握手完成时在线性化点把连接交给 push、Future 或有界队列。 */
static bool __xrtTlsListenerStreamOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	__xrt_tls_listener_stream* pNode =
		(__xrt_tls_listener_stream*)pData;
	xtlslistener* pListener = xrtTlsListenerRef(pNode->Listener);
	xtlsstream* pGuard = NULL;
	bool bAccepted = false;

	if ( pListener == NULL ) {
		(void)xrtTlsStreamAbort(pStream);
		return false;
	}
	__xrtTlsListenerHandshakeRelease(pNode);
	if ( xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN ) {
		if ( pListener->Events.Accept != NULL ) {
			pGuard = xrtTlsStreamRef(pStream);
			if ( pGuard != NULL ) {
				__xrtSpinLock(&pListener->Lock);
				pNode->CallerHeld = false;
				__xrtSpinUnlock(&pListener->Lock);
				bAccepted = pListener->Events.Accept(
					pListener,
					pStream,
					pListener->Data
				);
				if ( !bAccepted ) {
					(void)xrtTlsStreamAbort(pStream);
					xrtTlsStreamDestroy(pStream);
				}
				xrtTlsStreamDestroy(pGuard);
			}
		} else {
			#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
				bAccepted = __xrtTlsListenerFutureAccept(
					pListener,
					pNode
				);
			#endif
			if ( !bAccepted ) {
				bAccepted = __xrtTlsListenerQueue(
					pListener,
					pNode
				);
			}
		}
	}
	(void)xrtAtomic64FetchAdd(
		bAccepted ? &pListener->Accepted : &pListener->Rejected,
		1,
		XMEMORY_RELAXED
	);
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		if ( bAccepted && (pListener->Events.Accept == NULL) ) {
			__xrtTlsListenerFutureNotify(pListener);
		}
	#endif
	if ( !bAccepted && (pGuard == NULL) ) {
		(void)xrtTlsStreamAbort(pStream);
	}
	xrtTlsListenerDestroy(pListener);
	return bAccepted;
}



/* TLS Stream 终态释放受管节点、队列所有权和 Listener 引用。 */
static void __xrtTlsListenerStreamClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrt_tls_listener_stream* pNode =
		(__xrt_tls_listener_stream*)pData;
	xtlslistener* pListener = pNode->Listener;
	bool bHandshake;
	bool bCallerHeld;

	(void)Result;
	bHandshake = __xrtTlsListenerHandshakeRelease(pNode);
	__xrtSpinLock(&pListener->Lock);
	if ( pNode->Queued ) {
		(void)__xrtTlsListenerQueueRemove(pListener, pNode);
	}
	bCallerHeld = pNode->CallerHeld;
	pNode->CallerHeld = false;
	__xrtSpinUnlock(&pListener->Lock);
	if ( bHandshake ) {
		(void)xrtAtomic64FetchAdd(
			&pListener->Rejected,
			1,
			XMEMORY_RELAXED
		);
		if ( xrtTlsListenerState(pListener) ==
			XTLS_LISTENER_OPEN ) {
			(void)xrtAtomic64FetchAdd(
				&pListener->HandshakeErrors,
				1,
				XMEMORY_RELAXED
			);
			if ( pListener->Events.HandshakeError != NULL ) {
				pListener->Events.HandshakeError(
					pListener,
					pError,
					pListener->Data
				);
			}
		}
	}
	if ( bCallerHeld ) {
		xrtTlsStreamDestroy(pStream);
	}
	pNode->Stream = NULL;
	xrtTlsListenerDestroy(pListener);
	xrtFree(pNode);
}



/* TCP Accept 回调内预留握手预算并升级尚未发布的 Stream。 */
static bool __xrtTlsListenerAcceptTransport(
	xnetlistener* pNative,
	xnetstream* pTransport,
	ptr pData
)
{
	xtlslistener* pListener = (xtlslistener*)pData;
	__xrt_tls_listener_stream* pNode;
	xtlsstream* pStream = NULL;
	const xerror* pError;

	(void)pNative;
	if ( (xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN) ||
		!__xrtTlsListenerHandshakeReserve(pListener) ) {
		(void)xrtAtomic64FetchAdd(
			&pListener->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	pNode = (__xrt_tls_listener_stream*)xrtCalloc(
		1,
		sizeof(*pNode)
	);
	if ( pNode == NULL ) {
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveHandshakes,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchAdd(
			&pListener->HandshakeErrors,
			1,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pListener->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	pNode->Listener = xrtTlsListenerRef(pListener);
	if ( pNode->Listener == NULL ) {
		xrtFree(pNode);
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveHandshakes,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchAdd(
			&pListener->HandshakeErrors,
			1,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pListener->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	pNode->CallerHeld = true;
	pNode->HandshakePending = true;
	if ( !xrtTlsStreamAccept(
		pTransport,
		&pListener->Tls,
		&pListener->Stream,
		&pListener->StreamEvents,
		pListener->Data,
		&pStream
	) ) {
		pError = xrtGetError();
		__xrtTlsListenerHandshakeRelease(pNode);
		(void)xrtAtomic64FetchAdd(
			&pListener->HandshakeErrors,
			1,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pListener->Rejected,
			1,
			XMEMORY_RELAXED
		);
		if ( pListener->Events.HandshakeError != NULL ) {
			pListener->Events.HandshakeError(
				pListener,
				pError,
				pListener->Data
			);
		}
		xrtTlsListenerDestroy(pNode->Listener);
		xrtFree(pNode);
		return false;
	}
	pNode->Stream = pStream;
	__xrtTlsStreamManage(
		pStream,
		__xrtTlsListenerStreamOpen,
		__xrtTlsListenerStreamClose,
		pNode
	);
	__xrtSpinLock(&pListener->Lock);
	if ( xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN ) {
		pNode->HandshakeNext = pListener->HandshakeHead;
		if ( pListener->HandshakeHead != NULL ) {
			pListener->HandshakeHead->HandshakePrevious = pNode;
		}
		pListener->HandshakeHead = pNode;
	} else {
		pNode->CloseMarked = true;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pNode->CloseMarked ) {
		(void)xrtTlsStreamAbort(pStream);
	}
	return true;
}



/* 底层监听错误直接映射到 TLS Listener，但不把单连接错误升级为全局关闭。 */
static void __xrtTlsListenerNativeError(
	xnetlistener* pNative,
	const xerror* pError,
	ptr pData
)
{
	xtlslistener* pListener = (xtlslistener*)pData;
	xerror* pStartup = NULL;

	(void)pNative;
	__xrtSpinLock(&pListener->Lock);
	if ( !pListener->StartDone &&
		(pListener->StartupError == NULL) &&
		(pError != NULL) ) {
		pStartup = xrtErrorRef(pError);
		pListener->StartupError = pStartup;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pListener->Events.Error != NULL ) {
		pListener->Events.Error(
			pListener,
			pError,
			pListener->Data
		);
	}
}



/* 丢弃全部尚未交付的连接，节点由各自 Close 回调最终释放。 */
static void __xrtTlsListenerDiscardQueued(xtlslistener* pListener)
{
	__xrt_tls_listener_stream* pHead;

	__xrtSpinLock(&pListener->Lock);
	pHead = pListener->AcceptHead;
	pListener->AcceptHead = NULL;
	pListener->AcceptTail = NULL;
	xrtAtomic32Store(
		&pListener->QueuedAccepts,
		0,
		XMEMORY_RELEASE
	);
	for ( __xrt_tls_listener_stream* pNode = pHead;
		pNode != NULL;
		pNode = pNode->Next ) {
		pNode->Queued = false;
	}
	__xrtSpinUnlock(&pListener->Lock);
	while ( pHead != NULL ) {
		__xrt_tls_listener_stream* pNext = pHead->Next;

		pHead->Next = NULL;
		(void)xrtTlsStreamAbort(pHead->Stream);
		pHead = pNext;
	}
}



/* 分批保留并终止全部尚未发布的握手，避免在 Listener 锁内调用 Stream。 */
static void __xrtTlsListenerAbortHandshakes(xtlslistener* pListener)
{
	#define XRT_TLS_LISTENER_ABORT_BATCH 32u
	xtlsstream* Streams[XRT_TLS_LISTENER_ABORT_BATCH];
	size_t iCount;

	for ( ;; ) {
		iCount = 0;
		__xrtSpinLock(&pListener->Lock);
		for ( __xrt_tls_listener_stream* pNode =
			pListener->HandshakeHead;
			(pNode != NULL) &&
			(iCount < XRT_TLS_LISTENER_ABORT_BATCH);
			pNode = pNode->HandshakeNext ) {
			if ( pNode->CloseMarked ) {
				continue;
			}
			pNode->CloseMarked = true;
			Streams[iCount] = xrtTlsStreamRef(pNode->Stream);
			if ( Streams[iCount] != NULL ) {
				iCount++;
			}
		}
		__xrtSpinUnlock(&pListener->Lock);
		if ( iCount == 0 ) {
			break;
		}
		for ( size_t i = 0; i < iCount; i++ ) {
			(void)xrtTlsStreamAbort(Streams[i]);
			xrtTlsStreamDestroy(Streams[i]);
		}
	}
	#undef XRT_TLS_LISTENER_ABORT_BATCH
}



/* 底层 Listener 终态发布 TLS Listener 关闭并释放运行时引用。 */
static void __xrtTlsListenerNativeClose(
	xnetlistener* pNative,
	ptr pData
)
{
	xtlslistener* pListener = (xtlslistener*)pData;
	bool bOwned = false;
	bool bPublished;

	__xrtSpinLock(&pListener->Lock);
	pListener->Closed = true;
	bPublished = pListener->Published;
	if ( pListener->Native == pNative ) {
		pListener->Native = NULL;
		bOwned = true;
	}
	xrtAtomic32Store(
		&pListener->State,
		XTLS_LISTENER_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtSpinUnlock(&pListener->Lock);
	__xrtTlsListenerAbortHandshakes(pListener);
	__xrtTlsListenerDiscardQueued(pListener);
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		__xrtTlsListenerFutureNotify(pListener);
	#endif
	if ( bPublished && (pListener->Events.Close != NULL) ) {
		pListener->Events.Close(pListener, pListener->Data);
	}
	if ( bOwned ) {
		xrtNetListenerDestroy(pNative);
	}
	if ( pListener->RuntimeHeld ) {
		pListener->RuntimeHeld = false;
		xrtTlsListenerDestroy(pListener);
	}
}



/* 返回底层 Listener 使用的静态事件表。 */
static const xnetlistenerevents* __xrtTlsListenerNativeEvents(void)
{
	static const xnetlistenerevents Events = {
		__xrtTlsListenerAcceptTransport,
		__xrtTlsListenerNativeError,
		__xrtTlsListenerNativeClose
	};

	return &Events;
}



/* 创建不可变 TLS 配置快照并同步绑定底层 TCP Listener。 */
XRT_API xtlslistener* xrtTlsListenerStart(
	xnetengine* pEngine,
	const xtlslistenerconfig* pConfig,
	const xtlslistenerevents* pEvents,
	const xtlsstreamevents* pStreamEvents,
	ptr pData
)
{
	xtlslistener* pListener;
	xnetlistener* pNative;
	xerror* pError = NULL;
	size_t iViewsOffset;
	size_t iDataOffset;
	size_t iTotal;
	bool bClosed;

	if ( (pEngine == NULL) ||
		!__xrtTlsListenerConfigValid(pConfig) ||
		!__xrtTlsListenerLayout(
			pConfig,
			&iViewsOffset,
			&iDataOffset,
			&iTotal
		) ) {
		return NULL;
	}
	pListener = (xtlslistener*)xrtCalloc(1, iTotal);
	if ( pListener == NULL ) {
		return NULL;
	}
	pListener->References = 2;
	xrtAtomic32Init(&pListener->State, XTLS_LISTENER_OPEN);
	xrtAtomic64Init(&pListener->Handshakes, 0);
	xrtAtomic64Init(&pListener->Accepted, 0);
	xrtAtomic64Init(&pListener->Rejected, 0);
	xrtAtomic64Init(&pListener->HandshakeErrors, 0);
	xrtAtomic32Init(&pListener->ActiveHandshakes, 0);
	xrtAtomic32Init(&pListener->PeakHandshakes, 0);
	xrtAtomic32Init(&pListener->QueuedAccepts, 0);
	xrtAtomic32Init(&pListener->PeakQueuedAccepts, 0);
	xrtAtomic32Init(&pListener->AcceptWaiters, 0);
	__xrtSpinInit(&pListener->Lock);
	pListener->Engine = pEngine;
	pListener->Stream = pConfig->Stream;
	pListener->Data = pData;
	pListener->AcceptQueueLimit = pConfig->AcceptQueueLimit;
	pListener->HandshakeLimit = pConfig->HandshakeLimit;
	pListener->RuntimeHeld = true;
	if ( pEvents != NULL ) {
		pListener->Events = *pEvents;
	}
	if ( pStreamEvents != NULL ) {
		pListener->StreamEvents = *pStreamEvents;
	}
	if ( !__xrtTlsListenerTlsCopy(
		pListener,
		&pConfig->Tls,
		iViewsOffset,
		iDataOffset
	) ) {
		pListener->RuntimeHeld = false;
		xrtTlsListenerDestroy(pListener);
		xrtTlsListenerDestroy(pListener);
		return NULL;
	}
	pNative = xrtNetListen(
		pEngine,
		&pConfig->Listen,
		__xrtTlsListenerNativeEvents(),
		NULL,
		pListener
	);
	if ( pNative == NULL ) {
		pListener->RuntimeHeld = false;
		xrtTlsListenerDestroy(pListener);
		xrtTlsListenerDestroy(pListener);
		return NULL;
	}
	__xrtSpinLock(&pListener->Lock);
	pListener->StartDone = true;
	bClosed = pListener->Closed;
	if ( !bClosed ) {
		pListener->Native = pNative;
		pListener->Published = true;
	} else {
		pError = xrtErrorRef(pListener->StartupError);
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( bClosed ) {
		xrtNetListenerDestroy(pNative);
		xrtTlsListenerDestroy(pListener);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtTlsListenerSetError(
				XERR_CLOSED,
				XTLS_ERROR_CLOSED,
				"start-tls-listener",
				"TLS listener closed while starting",
				NULL
			);
		}
		return NULL;
	}
	return pListener;
}



/* pull 模式下非阻塞转移一个完成握手的 Stream 引用。 */
XRT_API xtlsstream* xrtTlsListenerAccept(xtlslistener* pListener)
{
	__xrt_tls_listener_stream* pNode = NULL;
	xtlsstream* pStream = NULL;
	bool bWaiting = false;

	if ( pListener == NULL ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"accept-tls-listener",
			"TLS listener is null",
			NULL
		);
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtTlsListenerSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-listener",
			"TLS listener is configured for push callbacks",
			NULL
		);
		return NULL;
	}
	__xrtSpinLock(&pListener->Lock);
	if ( xrtAtomic32Load(
		&pListener->AcceptWaiters,
		XMEMORY_ACQUIRE
	) != 0 ) {
		bWaiting = true;
	} else if ( xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN ) {
		pNode = __xrtTlsListenerTakeQueued(pListener);
		if ( pNode != NULL ) {
			pNode->CallerHeld = false;
			pStream = pNode->Stream;
		}
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( bWaiting ) {
		__xrtTlsListenerSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-listener",
			"TLS listener already has an asynchronous accept consumer",
			NULL
		);
	}
	return pStream;
}



/* 原子关闭底层 Listener，并让完成队列和 Future 观察同一终态。 */
XRT_API bool xrtTlsListenerClose(xtlslistener* pListener)
{
	uint32 iExpected = XTLS_LISTENER_OPEN;
	xnetlistener* pNative = NULL;

	if ( pListener == NULL ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"close-tls-listener",
			"TLS listener is null",
			NULL
		);
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pListener->State,
		&iExpected,
		XTLS_LISTENER_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return iExpected != XTLS_LISTENER_OPEN;
	}
	__xrtTlsListenerAbortHandshakes(pListener);
	__xrtTlsListenerDiscardQueued(pListener);
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		__xrtTlsListenerFutureNotify(pListener);
	#endif
	__xrtSpinLock(&pListener->Lock);
	if ( pListener->Native != NULL ) {
		pNative = xrtNetListenerRef(pListener->Native);
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pNative != NULL ) {
		(void)xrtNetListenerClose(pNative);
		xrtNetListenerDestroy(pNative);
	}
	return true;
}



/* 返回 Listener 生命周期的原子快照。 */
XRT_API xtlslistenerstate xrtTlsListenerState(
	const xtlslistener* pListener
)
{
	return pListener != NULL ?
		(xtlslistenerstate)xrtAtomic32Load(
			&pListener->State,
			XMEMORY_ACQUIRE
		) : XTLS_LISTENER_CLOSED;
}



/* 借助底层 Listener 复制动态绑定后的本地地址。 */
XRT_API bool xrtTlsListenerLocal(
	xtlslistener* pListener,
	xnetaddr* pAddress
)
{
	xnetlistener* pNative = NULL;
	bool bResult = false;

	if ( (pListener == NULL) || (pAddress == NULL) ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"local-tls-listener",
			"TLS listener or address output is null",
			NULL
		);
		return false;
	}
	__xrtSpinLock(&pListener->Lock);
	if ( pListener->Native != NULL ) {
		pNative = xrtNetListenerRef(pListener->Native);
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pNative != NULL ) {
		bResult = xrtNetListenerLocal(pNative, pAddress);
		xrtNetListenerDestroy(pNative);
	}
	return bResult;
}



/* 返回创建时保存的用户数据。 */
XRT_API ptr xrtTlsListenerData(const xtlslistener* pListener)
{
	return pListener != NULL ? pListener->Data : NULL;
}



/* 复制 Listener 的并发统计快照。 */
XRT_API bool xrtTlsListenerStats(
	const xtlslistener* pListener,
	xtlslistenerstats* pStats
)
{
	xtlslistenerstats Stats;

	if ( (pListener == NULL) || (pStats == NULL) ) {
		__xrtTlsListenerSetError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			"stats-tls-listener",
			"TLS listener or stats output is null",
			NULL
		);
		return false;
	}
	Stats.State = xrtTlsListenerState(pListener);
	Stats.Handshakes = xrtAtomic64Load(
		&pListener->Handshakes,
		XMEMORY_ACQUIRE
	);
	Stats.Accepted = xrtAtomic64Load(
		&pListener->Accepted,
		XMEMORY_ACQUIRE
	);
	Stats.Rejected = xrtAtomic64Load(
		&pListener->Rejected,
		XMEMORY_ACQUIRE
	);
	Stats.HandshakeErrors = xrtAtomic64Load(
		&pListener->HandshakeErrors,
		XMEMORY_ACQUIRE
	);
	Stats.ActiveHandshakes = xrtAtomic32Load(
		&pListener->ActiveHandshakes,
		XMEMORY_ACQUIRE
	);
	Stats.PeakHandshakes = xrtAtomic32Load(
		&pListener->PeakHandshakes,
		XMEMORY_ACQUIRE
	);
	Stats.QueuedAccepts = xrtAtomic32Load(
		&pListener->QueuedAccepts,
		XMEMORY_ACQUIRE
	);
	Stats.PeakQueuedAccepts = xrtAtomic32Load(
		&pListener->PeakQueuedAccepts,
		XMEMORY_ACQUIRE
	);
	Stats.AcceptWaiters = xrtAtomic32Load(
		&pListener->AcceptWaiters,
		XMEMORY_ACQUIRE
	);
	*pStats = Stats;
	return true;
}

#endif
