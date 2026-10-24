#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)

#define XRT_TLS_LISTENER_QUEUE_DEFAULT 1024u
#define XRT_TLS_LISTENER_HANDSHAKE_DEFAULT 128u
#define XRT_TLS_LISTENER_OWN_READY 1u
#define XRT_TLS_LISTENER_OWN_CLEARED 2u

static bool __xrtTlsListenerOwnershipCount(const void*, size_t*);
static bool __xrtTlsListenerOwnershipTrace(const void*, xrtownershipvisitor, ptr);
static bool __xrtTlsListenerOwnershipBusy(const void*);
static const xrtownershipops __xrtTlsListenerOwnershipOps = {
	__xrtTlsListenerOwnershipCount, __xrtTlsListenerOwnershipTrace};
static void __xrtTlsListenerRuntimeRelease(xtlslistener*);
static void __xrtTlsListenerStopStream(xtlsstream*);

bool __xrtTlsListenerOwnershipEnter(xtlslistener* l, bool submit)
{
	if (!l) { __xrtErrorSetInvalidArgument(); return false; }
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) return false;
	uint32 active = xrtAtomic32Load(&l->OwnershipActive, XMEMORY_ACQUIRE); bool entered = false;
	if (!(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED) &&
		(!submit || !xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE))) {
		while (active < UINT32_MAX) {
			uint32 expected = active;
			if (xrtAtomic32CompareExchange(&l->OwnershipActive, &expected, active + 1, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
				entered = true; break;
			}
			active = expected;
		}
	}
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!entered) __xrtErrorSetInvalidState();
	return entered;
}
void __xrtTlsListenerOwnershipLeave(xtlslistener* l)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (!xrtAtomic32FetchSub(&l->OwnershipActive, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
}
static bool __xrtTlsListenerOwnershipBusy(const void* data)
{
	const xtlslistener* l = data;
	return l && (xrtAtomic32Load(&l->OwnershipActive, XMEMORY_ACQUIRE) ||
		(l->OwnershipStorageHeld && l->Engine && xrtNetEngineOwnershipQuiescenceV1Get()->Busy(l->Engine)));
}

/* A child context owns one Listener credit, and only owns the Stream creator
 * credit until handoff. Each intrusive list separately owns one record credit. */
bool __xrtTlsListenerStreamNodeHold(const void* data)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data; xrtownershipscope m = {0};
	if (!xrtOwnershipMutationBegin(&m)) return false;
	bool held = n->Owned && !n->Cleared && xrtRefRetain(&n->References) >= 0;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	return held;
}
void __xrtTlsListenerStreamNodeDrop(const void* data)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data; xrtownershipscope m = {0};
	if (!xrtOwnershipMutationBegin(&m)) abort();
	int32 left = xrtRefRelease(&n->References);
	if (left < 0) abort();
	xtlslistener* listener = NULL;
	if (!left) {
		if (!n->Owned || n->CallerHeld || n->Queued || n->HandshakePending || n->HandshakeLinked ||
			xrtAtomic32Load(&n->Active, XMEMORY_ACQUIRE)) abort();
		listener = n->Listener; n->Listener = NULL;
	}
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!left) { xrtTlsListenerDestroy(listener); xrtFree(n); }
}
static bool __xrtTlsListenerNodeBusy(const void* data)
{
	const __xrt_tls_listener_stream* n = data;
	return n && !n->Cleared && (xrtAtomic32Load(&n->Active, XMEMORY_ACQUIRE) ||
		(n->Listener && __xrtTlsListenerOwnershipBusy(n->Listener)));
}
static bool __xrtTlsListenerNodeCount(const void* data, size_t* count)
{
	const __xrt_tls_listener_stream* n = data;
	if (!n || !count || !n->Owned || n->Cleared || !n->Listener || !n->Listener->OwnershipPolicy ||
		__xrtTlsListenerNodeBusy(n) || (n->CallerHeld && !n->Stream) ||
		(n->Closed && (n->Stream || n->CallerHeld || n->HandshakePending || n->Queued || n->HandshakeLinked))) return false;
	int32 refs = __xrtAtomicRefLoad(&n->References);
	if (refs <= 0) return false;
	*count = (size_t)refs; return true;
}
static bool __xrtTlsListenerNodeTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const __xrt_tls_listener_stream* n = data; size_t count;
	return visit && __xrtTlsListenerNodeCount(n, &count) &&
		visit(xrtTlsListenerOwnership(n->Listener), context) &&
		(!n->CallerHeld || visit(xrtTlsStreamOwnership(n->Stream), context));
}
static const xrtownershipops __xrtTlsListenerNodeOps = {__xrtTlsListenerNodeCount, __xrtTlsListenerNodeTrace};
static xrtownershipref __xrtTlsListenerNodeOwnership(const __xrt_tls_listener_stream* n)
{ return (xrtownershipref){n, n ? &__xrtTlsListenerNodeOps : NULL}; }
static bool __xrtTlsListenerNodeReady(const void* data)
{ const __xrt_tls_listener_stream* n = data; size_t count; return __xrtTlsListenerNodeCount(n, &count) && n->Closed; }
static bool __xrtTlsListenerNodeClaim(const void* data, const void* token)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data; size_t count;
	if (!token || (n->Claim && n->Claim != token) || !__xrtTlsListenerNodeCount(n, &count)) return false;
	n->Claim = token; return true;
}
static void __xrtTlsListenerNodeRestore(const void* data, const void* token)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data;
	if (!token || n->Claim != token || n->Cleared) abort();
	n->Claim = NULL;
}
static xrtownershipprepareresult __xrtTlsListenerNodePrepare(const void* data, const void* token)
{
	const __xrt_tls_listener_stream* n = data; xrtownershipscope f = {0};
	if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || n->Claim != token || n->Cleared) abort();
	bool ready = __xrtTlsListenerNodeReady(n);
	if (!xrtOwnershipScopeEnd(&f)) abort();
	return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTlsListenerNodeClear(const void* data, const void* token)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data;
	if (!token || n->Claim != token || n->Cleared || !__xrtTlsListenerNodeReady(n)) abort();
	n->Cleared = true;
}
static bool __xrtTlsListenerNodeFinish(const void* data, const void* token)
{
	__xrt_tls_listener_stream* n = (__xrt_tls_listener_stream*)data; xrtownershipscope m = {0};
	if (!xrtOwnershipMutationBegin(&m)) return false;
	if (!token || n->Claim != token || !n->Cleared) abort();
	xtlslistener* listener = n->Listener; n->Listener = NULL;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	xrtTlsListenerDestroy(listener); return true;
}
XRT_API const xrtownershipquiescencev1* xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtTlsListenerNodeOps, __xrtTlsListenerNodeBusy}; return &p;
}
XRT_API const xrtownershipadapterv1* xrtTlsListenerStreamRecordOwnershipAdapterV1(xrtownershipref ref,
	const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 a = {sizeof(a), __xrtTlsListenerStreamNodeHold, __xrtTlsListenerStreamNodeDrop,
		__xrtTlsListenerNodeClaim, __xrtTlsListenerNodeRestore, NULL, __xrtTlsListenerNodeClear, __xrtTlsListenerNodeFinish};
	static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtTlsListenerNodeReady, __xrtTlsListenerNodePrepare};
	size_t count;
	if (!preparation || ref.Ops != &__xrtTlsListenerNodeOps || !__xrtTlsListenerNodeCount(ref.Data, &count)) return NULL;
	*preparation = &p; return &a;
}



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
	if (!pListener) { __xrtErrorSetInvalidArgument(); return NULL; }
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) return NULL;
	bool held = !(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED) &&
		xrtRefRetain(&pListener->References) >= 0;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!held) {
		__xrtTlsListenerSetError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"retain-tls-listener",
			"TLS listener is retired or its reference count is exhausted",
			NULL
		);
		return NULL;
	}
	return pListener;
}



/* 释放最后一个 Listener 引用以及不可变 TLS 配置快照。 */
XRT_API void xrtTlsListenerDestroy(xtlslistener* pListener)
{
	if (!pListener) return;
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	int32 left = xrtRefRelease(&pListener->References);
	if (left < 0 || (!left && (pListener->RuntimeOwner.Data || pListener->RuntimeOwner.Previous ||
		xrtAtomic32Load(&pListener->OwnershipActive, XMEMORY_ACQUIRE)))) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (left) return;
	__xrtTlsListenerTlsUnit(pListener);
	xrtErrorFree(pListener->StartupError);
	xrtErrorFree(pListener->OwnershipFinishError);
	if (pListener->OwnershipDataHeld) pListener->OwnershipPolicy->Drop(pListener->Data);
	if (pListener->OwnershipEngineHeld) __xrtNetEngineObjectRelease(pListener->Engine);
	if (pListener->OwnershipStorageHeld) __xrtNetEngineStorageRelease(pListener->Engine);
	__xrtSpinUnit(&pListener->Lock);
	xrtFree(pListener);
}

static void __xrtTlsListenerRuntimeRelease(xtlslistener* l)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (!l->RuntimeHeld) { if (!xrtOwnershipScopeEnd(&m)) abort(); return; }
	if (l->RuntimeOwner.Data) {
		const xnetruntimeownershipv1* policy = NULL;
		if (__xrtNetEngineRuntimeTake(l->Engine, &l->RuntimeOwner, &policy) != l ||
			policy != xrtTlsListenerRuntimeOwnershipV1Get()) abort();
	}
	l->RuntimeHeld = false;
	bool held = l->OwnershipEngineHeld; l->OwnershipEngineHeld = false;
	if (l->OwnershipPolicy) xrtAtomic32Store(&l->OwnershipState, XRT_TLS_LISTENER_OWN_READY, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (held) __xrtNetEngineObjectRelease(l->Engine);
	xrtTlsListenerDestroy(l);
}
static void __xrtTlsListenerDiscard(xtlslistener* l)
{
	xerror* error = xrtTakeError();
	l->OwnershipDataHeld = false;
	__xrtTlsListenerRuntimeRelease(l);
	if (l->OwnershipPolicy) __xrtTlsListenerOwnershipLeave(l);
	xrtTlsListenerDestroy(l);
	xrtClearError(); xrtSetErrorTake(error);
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
	bool bDropRecord = false;

	__xrtSpinLock(&pListener->Lock);
	if ( pNode->HandshakePending ) {
		bDropRecord = pNode->Owned && pNode->HandshakeLinked;
		pNode->HandshakePending = false;
		__xrtTlsListenerHandshakeUnlink(pListener, pNode);
		pNode->HandshakeLinked = false;
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
	if (bDropRecord) __xrtTlsListenerStreamNodeDrop(pNode);
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
		(iQueued < pListener->AcceptQueueLimit) &&
		(!pNode->Owned || __xrtTlsListenerStreamNodeHold(pNode)) ) {
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
		__xrtTlsListenerStopStream(pStream);
		return false;
	}
	__xrtTlsListenerHandshakeRelease(pNode);
	if ( xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN &&
		!xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE) ) {
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
					__xrtTlsListenerStopStream(pStream);
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
		__xrtTlsListenerStopStream(pStream);
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
	bool bDropQueued = false;

	(void)Result;
	bHandshake = __xrtTlsListenerHandshakeRelease(pNode);
	__xrtSpinLock(&pListener->Lock);
	if ( pNode->Queued ) {
		bDropQueued = __xrtTlsListenerQueueRemove(pListener, pNode) && pNode->Owned;
	}
	bCallerHeld = pNode->CallerHeld;
	pNode->CallerHeld = false;
	if (pNode->Owned) { pNode->Stream = NULL; pNode->Closed = true; }
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
	if (pNode->Owned) {
		if (bDropQueued) __xrtTlsListenerStreamNodeDrop(pNode);
	} else {
		pNode->Stream = NULL;
		xrtTlsListenerDestroy(pListener);
		xrtFree(pNode);
	}
}

static bool __xrtTlsListenerNodeFrameEnter(__xrt_tls_listener_stream* n)
{
	if (!__xrtTlsListenerStreamNodeHold(n)) return false;
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (xrtAtomic32FetchAdd(&n->Active, 1, XMEMORY_ACQ_REL) == UINT32_MAX) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!__xrtTlsListenerOwnershipEnter(n->Listener, false)) abort();
	return true;
}
static void __xrtTlsListenerNodeFrameLeave(__xrt_tls_listener_stream* n)
{
	__xrtTlsListenerOwnershipLeave(n->Listener);
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (!xrtAtomic32FetchSub(&n->Active, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
	__xrtTlsListenerStreamNodeDrop(n);
}
static void __xrtTlsListenerOwnedStreamOpen(xtlsstream* s, ptr data)
{
	__xrt_tls_listener_stream* n = data; if (!__xrtTlsListenerNodeFrameEnter(n)) return;
	if (__xrtTlsListenerStreamOpen(s, n) && n->Listener->StreamEvents.Open)
		n->Listener->StreamEvents.Open(s, n->Listener->Data);
	__xrtTlsListenerNodeFrameLeave(n);
}
static void __xrtTlsListenerOwnedStreamRead(xtlsstream* s, const xnetbuf* buffer, ptr data)
{
	__xrt_tls_listener_stream* n = data; if (!__xrtTlsListenerNodeFrameEnter(n)) return;
	if (n->Listener->StreamEvents.Read) n->Listener->StreamEvents.Read(s, buffer, n->Listener->Data);
	__xrtTlsListenerNodeFrameLeave(n);
}
#define XRT_TLS_LISTENER_FORWARD_EVENT(Name) \
	static void __xrtTlsListenerOwnedStream##Name(xtlsstream* s, ptr data) { \
		__xrt_tls_listener_stream* n = data; if (!__xrtTlsListenerNodeFrameEnter(n)) return; \
		if (n->Listener->StreamEvents.Name) n->Listener->StreamEvents.Name(s, n->Listener->Data); \
		__xrtTlsListenerNodeFrameLeave(n); }
XRT_TLS_LISTENER_FORWARD_EVENT(End)
XRT_TLS_LISTENER_FORWARD_EVENT(Writable)
XRT_TLS_LISTENER_FORWARD_EVENT(Drain)
XRT_TLS_LISTENER_FORWARD_EVENT(Ticket)
#undef XRT_TLS_LISTENER_FORWARD_EVENT
static void __xrtTlsListenerOwnedStreamClose(xtlsstream* s, xnetresult result, const xerror* error, ptr data)
{
	__xrt_tls_listener_stream* n = data; if (!__xrtTlsListenerNodeFrameEnter(n)) return;
	if (n->Listener->StreamEvents.Close) n->Listener->StreamEvents.Close(s, result, error, n->Listener->Data);
	__xrtTlsListenerStreamClose(s, result, error, n);
	__xrtTlsListenerNodeFrameLeave(n);
}
XRT_API const xtlsstreamownershipv1* xrtTlsListenerStreamOwnershipV1Get(void)
{
	static const xtlsstreamownershipv1 p = {sizeof(p), {
		__xrtTlsListenerOwnedStreamOpen, NULL, __xrtTlsListenerOwnedStreamEnd,
		__xrtTlsListenerOwnedStreamWritable, __xrtTlsListenerOwnedStreamDrain, __xrtTlsListenerOwnedStreamClose,
		__xrtTlsListenerOwnedStreamTicket}, __xrtTlsListenerStreamNodeDrop, &__xrtTlsListenerNodeOps};
	return &p;
}
XRT_API const xtlsstreamownershipv1* xrtTlsListenerStreamPushOwnershipV1Get(void)
{
	static const xtlsstreamownershipv1 p = {sizeof(p), {
		__xrtTlsListenerOwnedStreamOpen, __xrtTlsListenerOwnedStreamRead, __xrtTlsListenerOwnedStreamEnd,
		__xrtTlsListenerOwnedStreamWritable, __xrtTlsListenerOwnedStreamDrain, __xrtTlsListenerOwnedStreamClose,
		__xrtTlsListenerOwnedStreamTicket}, __xrtTlsListenerStreamNodeDrop, &__xrtTlsListenerNodeOps};
	return &p;
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
		(pListener->OwnershipPolicy && (!pListener->Published ||
		 xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE))) ||
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
	pNode->Owned = pListener->OwnershipPolicy != NULL;
	if (pNode->Owned) { pNode->References = 1; xrtAtomic32Init(&pNode->Active, 1); }
	bool accepted = pNode->Owned ? xrtTlsStreamAcceptOwnedV1(pTransport, &pListener->Tls, &pListener->Stream,
		pNode, pListener->StreamEvents.Read ? xrtTlsListenerStreamPushOwnershipV1Get() : xrtTlsListenerStreamOwnershipV1Get(), &pStream) :
		xrtTlsStreamAccept(pTransport, &pListener->Tls, &pListener->Stream, &pListener->StreamEvents, pListener->Data, &pStream);
	if (!accepted) {
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
		if (pNode->Owned) {
			pNode->CallerHeld = false; pNode->Closed = true;
			xrtAtomic32Store(&pNode->Active, 0, XMEMORY_RELEASE);
			__xrtTlsListenerStreamNodeDrop(pNode);
		} else { xrtTlsListenerDestroy(pNode->Listener); xrtFree(pNode); }
		return false;
	}
	pNode->Stream = pStream;
	if (!pNode->Owned) __xrtTlsStreamManage(
		pStream,
		__xrtTlsListenerStreamOpen,
		__xrtTlsListenerStreamClose,
		pNode
	);
	__xrtSpinLock(&pListener->Lock);
	if ( xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN &&
		!xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE) ) {
		if (pNode->Owned && !__xrtTlsListenerStreamNodeHold(pNode)) abort();
		pNode->HandshakeLinked = true;
		pNode->HandshakeNext = pListener->HandshakeHead;
		if ( pListener->HandshakeHead != NULL ) {
			pListener->HandshakeHead->HandshakePrevious = pNode;
		}
		pListener->HandshakeHead = pNode;
	} else {
		pNode->CloseMarked = true;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if (pNode->Owned) xrtAtomic32Store(&pNode->Active, 0, XMEMORY_RELEASE);
	if ( pNode->CloseMarked ) {
		__xrtTlsListenerStopStream(pStream);
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
	for (;;) {
		__xrtSpinLock(&pListener->Lock);
		__xrt_tls_listener_stream* node = __xrtTlsListenerTakeQueued(pListener);
		bool owned = node && node->Owned;
		xtlsstream* stream = node && node->Stream ? xrtTlsStreamRef(node->Stream) : NULL;
		__xrtSpinUnlock(&pListener->Lock);
		if (!node) break;
		if (stream) { __xrtTlsListenerStopStream(stream); xrtTlsStreamDestroy(stream); }
		if (owned) __xrtTlsListenerStreamNodeDrop(node); /* transferred list credit */
	}
}

static void __xrtTlsListenerStopStream(xtlsstream* stream)
{ (void)__xrtTlsStreamAbortAccepted(stream); }



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
			__xrtTlsListenerStopStream(Streams[i]);
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
		__xrtTlsListenerRuntimeRelease(pListener);
	}
}

static bool __xrtTlsListenerNativeFrameEnter(xtlslistener* l)
{
	if (!xrtTlsListenerRef(l)) return false;
	if (__xrtTlsListenerOwnershipEnter(l, false)) return true;
	xrtTlsListenerDestroy(l); return false;
}
static void __xrtTlsListenerNativeFrameLeave(xtlslistener* l)
{ __xrtTlsListenerOwnershipLeave(l); xrtTlsListenerDestroy(l); }
static bool __xrtTlsListenerOwnedAcceptTransport(xnetlistener* transport, xnetstream* stream, ptr data)
{
	xtlslistener* l = data; if (!__xrtTlsListenerNativeFrameEnter(l)) return false;
	bool result = __xrtTlsListenerAcceptTransport(transport, stream, l);
	__xrtTlsListenerNativeFrameLeave(l); return result;
}
static void __xrtTlsListenerOwnedNativeError(xnetlistener* transport, const xerror* error, ptr data)
{
	xtlslistener* l = data; if (!__xrtTlsListenerNativeFrameEnter(l)) return;
	__xrtTlsListenerNativeError(transport, error, l); __xrtTlsListenerNativeFrameLeave(l);
}
static void __xrtTlsListenerOwnedNativeClose(xnetlistener* transport, ptr data)
{
	xtlslistener* l = data; if (!__xrtTlsListenerNativeFrameEnter(l)) return;
	__xrtTlsListenerNativeClose(transport, l); __xrtTlsListenerNativeFrameLeave(l);
}
static void __xrtTlsListenerNativeDataDrop(const void* data)
{ xrtTlsListenerDestroy((xtlslistener*)data); }
XRT_API const xnetlistenereventsownershipv1* xrtTlsListenerTransportOwnershipV1Get(void)
{
	static const xnetlistenereventsownershipv1 p = {sizeof(p), {
		__xrtTlsListenerOwnedAcceptTransport, __xrtTlsListenerOwnedNativeError, __xrtTlsListenerOwnedNativeClose},
		__xrtTlsListenerNativeDataDrop, &__xrtTlsListenerOwnershipOps}; return &p;
}



/* 返回底层 Listener 使用的静态事件表。 */
static const xnetlistenerevents* __xrtTlsListenerNativeEvents(void)
{
	return &xrtTlsListenerTransportOwnershipV1Get()->Events;
}



/* 创建不可变 TLS 配置快照并同步绑定底层 TCP Listener。 */
static xtlslistener* __xrtTlsListenerStart(
	xnetengine* pEngine,
	const xtlslistenerconfig* pConfig,
	const xtlslistenerevents* pEvents,
	const xtlsstreamevents* pStreamEvents,
	ptr pData,
	const xtlslistenerownershipv1* pPolicy
)
{
	xtlslistener* pListener;
	xnetlistener* pNative;
	xerror* pError = NULL;
	size_t iViewsOffset;
	size_t iDataOffset;
	size_t iTotal;
	bool bClosed;

	if (!pEngine) { __xrtErrorSetInvalidArgument(); return NULL; }
	if ( !__xrtTlsListenerConfigValid(pConfig) ||
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
	xrtAtomicPtrInit(&pListener->OwnershipClaim, NULL);
	xrtAtomic32Init(&pListener->OwnershipState, 0);
	xrtAtomic32Init(&pListener->OwnershipActive, pPolicy ? 1u : 0u);
	pListener->OwnershipPolicy = pPolicy;
	pListener->OwnershipDataHeld = pPolicy != NULL;
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
	if (pPolicy) {
		if (!__xrtNetEngineObjectHold(pEngine)) { __xrtTlsListenerDiscard(pListener); return NULL; }
		pListener->OwnershipEngineHeld = true;
		if (!__xrtNetEngineStorageRetain(pEngine)) { __xrtTlsListenerDiscard(pListener); return NULL; }
		pListener->OwnershipStorageHeld = true;
	}
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
		__xrtTlsListenerDiscard(pListener);
		return NULL;
	}
	if (pPolicy && !__xrtNetEngineRuntimeAdopt(pEngine, &pListener->RuntimeOwner, pListener,
		xrtTlsListenerRuntimeOwnershipV1Get())) { __xrtTlsListenerDiscard(pListener); return NULL; }
	if (pPolicy) {
		if (!xrtTlsListenerRef(pListener)) { __xrtTlsListenerDiscard(pListener); return NULL; }
		pNative = xrtNetListenOwnedV1(pEngine, &pConfig->Listen, pListener, xrtTlsListenerTransportOwnershipV1Get());
		if (!pNative) xrtTlsListenerDestroy(pListener); /* unconsumed native Data credit */
	} else pNative = xrtNetListen(pEngine, &pConfig->Listen, __xrtTlsListenerNativeEvents(), NULL, pListener);
	if ( pNative == NULL ) {
		__xrtTlsListenerDiscard(pListener);
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
		__xrtTlsListenerDiscard(pListener);
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
	if (pPolicy) __xrtTlsListenerOwnershipLeave(pListener);
	return pListener;
}

XRT_API xtlslistener* xrtTlsListenerStart(xnetengine* engine, const xtlslistenerconfig* config,
	const xtlslistenerevents* events, const xtlsstreamevents* streamEvents, ptr data)
{ return __xrtTlsListenerStart(engine, config, events, streamEvents, data, NULL); }
XRT_API xtlslistener* xrtTlsListenerStartOwnedV1(xnetengine* engine, const xtlslistenerconfig* config,
	ptr data, const xtlslistenerownershipv1* policy)
{
	if (!data || !policy || policy->size != sizeof(*policy) || !policy->Drop || !policy->Ops ||
		!policy->Ops->Count || !policy->Ops->Trace) { __xrtErrorSetInvalidArgument(); return NULL; }
	return __xrtTlsListenerStart(engine, config, &policy->Events, &policy->StreamEvents, data, policy);
}



/* pull 模式下非阻塞转移一个完成握手的 Stream 引用。 */
static xtlsstream* __xrtTlsListenerAccept(xtlslistener* pListener)
{
	__xrt_tls_listener_stream* pNode = NULL;
	xtlsstream* pStream = NULL;
	bool bWaiting = false;
	bool bDropRecord = false;

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
			bDropRecord = pNode->Owned;
			pNode->CallerHeld = false;
			pStream = pNode->Stream;
		}
	}
	__xrtSpinUnlock(&pListener->Lock);
	if (bDropRecord) __xrtTlsListenerStreamNodeDrop(pNode);
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

XRT_API xtlsstream* xrtTlsListenerAccept(xtlslistener* l)
{
	if (!__xrtTlsListenerOwnershipEnter(l, true)) return NULL;
	xtlsstream* result = __xrtTlsListenerAccept(l); __xrtTlsListenerOwnershipLeave(l); return result;
}



/* 原子关闭底层 Listener，并让完成队列和 Future 观察同一终态。 */
static bool __xrtTlsListenerClose(xtlslistener* pListener)
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

XRT_API bool xrtTlsListenerClose(xtlslistener* l)
{
	if (!__xrtTlsListenerOwnershipEnter(l, true)) return false;
	bool result = __xrtTlsListenerClose(l); __xrtTlsListenerOwnershipLeave(l); return result;
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

/* Each list link is an independently retained record, never a guessed Stream
 * or Listener reference. Handshake and ready queues are disjoint phases. */
static bool __xrtTlsListenerRecordsWalk(const xtlslistener* l, xrtownershipvisitor visit, ptr context)
{
	const __xrt_tls_listener_stream *slow = l->HandshakeHead, *fast = slow, *previous = NULL;
	while (fast && fast->HandshakeNext) {
		slow = slow->HandshakeNext; fast = fast->HandshakeNext->HandshakeNext; if (slow == fast) return false;
	}
	size_t handshakes = 0, queued = 0;
	for (const __xrt_tls_listener_stream* n = l->HandshakeHead; n; n = n->HandshakeNext) {
		if (!n->Owned || n->Closed || n->Cleared || !n->HandshakePending || !n->HandshakeLinked || n->Queued ||
			!n->CallerHeld || !n->Stream || n->Listener != l || n->HandshakePrevious != previous ||
			xrtAtomic32Load(&n->Active, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&n->References) <= 0) return false;
		if (visit && !visit(__xrtTlsListenerNodeOwnership(n), context)) return false;
		previous = n; ++handshakes;
	}
	slow = l->AcceptHead; fast = slow; previous = NULL;
	while (fast && fast->Next) { slow = slow->Next; fast = fast->Next->Next; if (slow == fast) return false; }
	for (const __xrt_tls_listener_stream* n = l->AcceptHead; n; n = n->Next) {
		if (!n->Owned || n->Closed || n->Cleared || n->HandshakePending || n->HandshakeLinked || !n->Queued ||
			!n->CallerHeld || !n->Stream || n->Listener != l || n->HandshakeNext || n->HandshakePrevious ||
			xrtAtomic32Load(&n->Active, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&n->References) <= 0) return false;
		if (visit && !visit(__xrtTlsListenerNodeOwnership(n), context)) return false;
		previous = n; ++queued;
	}
	return previous == l->AcceptTail && handshakes <= l->HandshakeLimit && queued <= l->AcceptQueueLimit &&
		handshakes == xrtAtomic32Load(&l->ActiveHandshakes, XMEMORY_ACQUIRE) &&
		queued == xrtAtomic32Load(&l->QueuedAccepts, XMEMORY_ACQUIRE);
}
static bool __xrtTlsListenerOwnershipCount(const void* data, size_t* count)
{
	const xtlslistener* l = data;
	if (!l || !count || !l->OwnershipPolicy || !l->OwnershipDataHeld || !l->Data || !l->OwnershipStorageHeld ||
		!l->Engine || __xrtTlsListenerOwnershipBusy(l) ||
		(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED)) return false;
	if (l->RuntimeHeld) {
		if (!l->OwnershipEngineHeld || l->RuntimeOwner.Data != l || !l->RuntimeOwner.Previous ||
			l->RuntimeOwner.Engine != l->Engine || l->RuntimeOwner.Policy != xrtTlsListenerRuntimeOwnershipV1Get()) return false;
	} else if (l->OwnershipEngineHeld || l->RuntimeOwner.Data || l->RuntimeOwner.Previous ||
		!(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_READY)) return false;
	if (l->Native && xrtNetListenerState(l->Native) != XNET_LISTENER_CLOSED &&
		(l->Native->EventPolicy != xrtTlsListenerTransportOwnershipV1Get() ||
		 xrtNetListenerData(l->Native) != l)) return false;
	if (!__xrtTlsListenerRecordsWalk(l, NULL, NULL)) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		if (!__xrtTlsListenerWaitOwnershipWalk(l, NULL, NULL)) return false;
	#endif
	int32 refs = __xrtAtomicRefLoad(&l->References);
	if (refs <= 0) return false;
	*count = (size_t)refs; return true;
}
static bool __xrtTlsListenerOwnershipTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const xtlslistener* l = data; size_t count;
	if (!visit || !__xrtTlsListenerOwnershipCount(l, &count)) return false;
	if (!visit(xrtNetEngineOwnership(l->Engine), context) ||
		(l->OwnershipEngineHeld && !visit(xrtNetEngineOwnership(l->Engine), context)) ||
		(l->Native && !visit(xrtNetListenerOwnership(l->Native), context)) ||
		!visit((xrtownershipref){l->Data, l->OwnershipPolicy->Ops}, context) ||
		!__xrtTlsListenerRecordsWalk(l, visit, context)) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		if (!__xrtTlsListenerWaitOwnershipWalk(l, visit, context)) return false;
	#endif
	return !l->StartupError || visit(xrtErrorOwnership(l->StartupError), context);
}
XRT_API xrtownershipref xrtTlsListenerOwnership(const xtlslistener* l)
{ return (xrtownershipref){l, l ? &__xrtTlsListenerOwnershipOps : NULL}; }
static void __xrtTlsListenerRuntimeDrop(ptr data) { xrtTlsListenerDestroy(data); }
XRT_API const xnetruntimeownershipv1* xrtTlsListenerRuntimeOwnershipV1Get(void)
{
	static const xnetruntimeownershipv1 p = {sizeof(p), &__xrtTlsListenerOwnershipOps, __xrtTlsListenerRuntimeDrop, NULL, 0}; return &p;
}
XRT_API const xrtownershipquiescencev1* xrtTlsListenerOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtTlsListenerOwnershipOps, __xrtTlsListenerOwnershipBusy}; return &p;
}
static bool __xrtTlsListenerHold(const void* data) { return xrtTlsListenerRef((xtlslistener*)data) != NULL; }
static void __xrtTlsListenerDrop(const void* data) { xrtTlsListenerDestroy((xtlslistener*)data); }
static bool __xrtTlsListenerClaim(const void* data, const void* token)
{
	xtlslistener* l = (xtlslistener*)data; size_t count; ptr old = xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!token || (old && old != token) || !__xrtTlsListenerOwnershipCount(l, &count)) return false;
	xrtAtomicPtrStore(&l->OwnershipClaim, (ptr)token, XMEMORY_RELEASE); return true;
}
static void __xrtTlsListenerRestore(const void* data, const void* token)
{
	xtlslistener* l = (xtlslistener*)data;
	if (!token || xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED)) abort();
	xrtAtomicPtrStore(&l->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static bool __xrtTlsListenerReady(const void* data)
{
	const xtlslistener* l = data;
	if (!l || __xrtTlsListenerOwnershipBusy(l) || !l->Closed || l->RuntimeHeld || l->OwnershipEngineHeld || l->Native ||
		l->HandshakeHead || l->AcceptHead || l->AcceptTail || xrtAtomic32Load(&l->ActiveHandshakes, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&l->QueuedAccepts, XMEMORY_ACQUIRE)) return false;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		if (l->WaitHead || l->WaitTail || xrtAtomic32Load(&l->AcceptWaiters, XMEMORY_ACQUIRE)) return false;
	#endif
	return true;
}
static xrtownershipprepareresult __xrtTlsListenerPrepare(const void* data, const void* token)
{
	xtlslistener* l = (xtlslistener*)data; xrtownershipscope f = {0};
	if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED)) abort();
	xrtownershipprepareresult result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtTlsListenerReady(l)) result = XRT_OWNERSHIP_PREPARE_READY;
	else if (!__xrtTlsListenerOwnershipBusy(l) && l->Native) {
		xrtAtomic32Store(&l->State, XTLS_LISTENER_CLOSING, XMEMORY_RELEASE);
		if (l->Native->RuntimeHeld && !__xrtNetListenerOwnershipStop(l->Native)) result = XRT_OWNERSHIP_PREPARE_FAILED;
	}
	if (!xrtOwnershipScopeEnd(&f)) abort();
	return result;
}
static void __xrtTlsListenerClear(const void* data, const void* token)
{
	xtlslistener* l = (xtlslistener*)data;
	if (!token || xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE) != token || !__xrtTlsListenerReady(l) ||
		(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED)) abort();
	xrtAtomic32Store(&l->OwnershipState, XRT_TLS_LISTENER_OWN_READY | XRT_TLS_LISTENER_OWN_CLEARED, XMEMORY_RELEASE);
}
static bool __xrtTlsListenerFinish(const void* data, const void* token)
{
	xtlslistener* l = (xtlslistener*)data; xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) return false;
	if (!token || xrtAtomicPtrLoad(&l->OwnershipClaim, XMEMORY_ACQUIRE) != token ||
		!(xrtAtomic32Load(&l->OwnershipState, XMEMORY_ACQUIRE) & XRT_TLS_LISTENER_OWN_CLEARED)) abort();
	if (l->OwnershipFinishing) { if (!xrtOwnershipScopeEnd(&m)) abort(); return false; }
	if (l->OwnershipFinished) {
		xerror* error = l->OwnershipFinishError;
		if (!xrtOwnershipScopeEnd(&m)) abort();
		if (error && !xrtGetError()) xrtSetError(error);
		return error == NULL;
	}
	l->OwnershipFinishing = true;
	ptr context = l->OwnershipDataHeld ? l->Data : NULL; l->Data = NULL; l->OwnershipDataHeld = false;
	xerror* startup = l->StartupError; l->StartupError = NULL;
	xnetengine* engine = l->Engine; bool storage = l->OwnershipStorageHeld;
	l->Engine = NULL; l->OwnershipStorageHeld = false;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	xerror* prior = xrtTakeError();
	__xrtTlsListenerTlsUnit(l); xrtErrorFree(startup);
	if (context) l->OwnershipPolicy->Drop(context);
	if (storage) __xrtNetEngineStorageRelease(engine);
	xerror* error = xrtTakeError();
	if (!xrtOwnershipMutationBegin(&m)) abort();
	l->OwnershipFinishError = error; l->OwnershipFinishing = false; l->OwnershipFinished = true;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (prior) xrtSetErrorTake(prior); else if (error) xrtSetError(error);
	return error == NULL;
}
XRT_API const xrtownershipadapterv1* xrtTlsListenerOwnershipAdapterV1(xrtownershipref ref,
	const xtlslistenerownershipv1* const* policies, size_t policyCount, const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 a = {sizeof(a), __xrtTlsListenerHold, __xrtTlsListenerDrop,
		__xrtTlsListenerClaim, __xrtTlsListenerRestore, NULL, __xrtTlsListenerClear, __xrtTlsListenerFinish};
	static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtTlsListenerReady, __xrtTlsListenerPrepare};
	if (!preparation || ref.Ops != &__xrtTlsListenerOwnershipOps || !ref.Data || (policyCount && !policies)) return NULL;
	const xtlslistener* l = ref.Data; bool known = false;
	for (size_t i = 0; i < policyCount; ++i) if (policies[i] == l->OwnershipPolicy) { known = true; break; }
	if (!known || !l->OwnershipPolicy || l->OwnershipPolicy->size != sizeof(*l->OwnershipPolicy) ||
		!l->OwnershipPolicy->Drop || !l->OwnershipPolicy->Ops || !l->OwnershipPolicy->Ops->Count || !l->OwnershipPolicy->Ops->Trace) return NULL;
	size_t count; if (!__xrtTlsListenerOwnershipCount(l, &count)) return NULL;
	*preparation = &p; return &a;
}

#endif
