#include "../internal/xrt_tls_stream.h"



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)

/* 每个等待节点持有 Listener 和 Promise，直到唯一终态完成。 */
struct __xrt_tls_listener_wait {
	volatile int32 References;
	volatile int32 Active;
	const void* Claim;
	__xrt_tls_listener_wait* Next;
	xtlslistener* Listener;
	xpromise* Promise;
	xcancelwatch* Watch;
	xatomic32 Done;
	bool Linked;
	bool Publishing;
	bool Cleared;
};



/* Future 结果析构时释放完成握手的 TLS Stream 引用。 */
static void __xrtTlsListenerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtTlsStreamDestroy((xtlsstream*)pValue);
}

static void __xrtTlsListenerWaitCancel(ptr);
static void __xrtTlsListenerWaitEnter(__xrt_tls_listener_wait* w)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	int32 active = __xrtAtomicRefLoad(&w->Active);
	for (;;) {
		if (active < 0 || active == INT32_MAX || w->Cleared) abort();
		int32 previous = __xrtAtomicRefCompareExchange(&w->Active, active + 1, active);
		if (previous == active) break;
		active = previous;
	}
	if (!xrtOwnershipScopeEnd(&m)) abort();
}
static void __xrtTlsListenerWaitLeave(__xrt_tls_listener_wait* w)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (xrtRefRelease(&w->Active) < 0) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
}
static bool __xrtTlsListenerWaitHold(const void* data)
{
	__xrt_tls_listener_wait* w = (__xrt_tls_listener_wait*)data; xrtownershipscope m = {0};
	if (!xrtOwnershipMutationBegin(&m)) return false;
	bool held = !w->Cleared && xrtRefRetain(&w->References) >= 0;
	if (!xrtOwnershipScopeEnd(&m)) abort();
	return held;
}
static void __xrtTlsListenerWaitDrop(const void* data)
{
	__xrt_tls_listener_wait* w = (__xrt_tls_listener_wait*)data; xrtownershipscope m = {0};
	if (!xrtOwnershipMutationBegin(&m)) abort();
	int32 left = xrtRefRelease(&w->References);
	if (left < 0 || (!left && (!xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE) || w->Publishing || w->Linked ||
		w->Promise || w->Watch || w->Listener || __xrtAtomicRefLoad(&w->Active)))) abort();
	if (!xrtOwnershipScopeEnd(&m)) abort();
	if (!left) xrtFree(w);
}
static bool __xrtTlsListenerWaitBusy(const void* data)
{
	const __xrt_tls_listener_wait* w = data;
	return w && !w->Cleared && (w->Publishing || __xrtAtomicRefLoad(&w->Active) ||
		(!xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE) &&
		 xrtTlsListenerOwnershipQuiescenceV1Get()->Busy(w->Listener)));
}
static bool __xrtTlsListenerWaitCount(const void* data, size_t* count)
{
	const __xrt_tls_listener_wait* w = data;
	if (!w || !count || w->Cleared || __xrtTlsListenerWaitBusy(w)) return false;
	bool done = xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE) != 0;
	if (done && (w->Listener || w->Promise || w->Watch || w->Linked)) return false;
	if (!done && (!w->Listener || !w->Promise || !w->Watch || !w->Linked)) return false;
	int32 refs = __xrtAtomicRefLoad(&w->References); if (refs <= 0) return false;
	*count = (size_t)refs; return true;
}
static bool __xrtTlsListenerWaitTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const __xrt_tls_listener_wait* w = data; size_t count;
	return visit && __xrtTlsListenerWaitCount(w, &count) &&
		(!w->Listener || visit(xrtTlsListenerOwnership(w->Listener), context)) &&
		(!w->Promise || visit(xrtPromiseOwnership(w->Promise), context)) &&
		(!w->Watch || visit(xrtCancelWatchOwnership(w->Watch), context));
}
static const xrtownershipops __xrtTlsListenerWaitOps = {__xrtTlsListenerWaitCount, __xrtTlsListenerWaitTrace};
static xrtownershipref __xrtTlsListenerWaitOwnership(const __xrt_tls_listener_wait* w)
{ return (xrtownershipref){w, w ? &__xrtTlsListenerWaitOps : NULL}; }
static void __xrtTlsListenerWaitOwnedCancel(ptr data)
{
	__xrt_tls_listener_wait* w = data; __xrtTlsListenerWaitEnter(w);
	__xrtTlsListenerWaitCancel(w); __xrtTlsListenerWaitLeave(w);
}
static const xfutureproducerownershipv1 __xrtTlsListenerWaitProducer = {
	sizeof(xfutureproducerownershipv1), __xrtTlsListenerWaitDrop};
static const xcancelwatchownershipv1 __xrtTlsListenerWaitCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtTlsListenerWaitOwnedCancel, __xrtTlsListenerWaitDrop, &__xrtTlsListenerWaitOps};
static bool __xrtTlsListenerResultTrace(const void* value, const void* data, xrtownershipvisitor visit, ptr context)
{ return value && !data && visit && visit(xrtTlsStreamOwnership(value), context); }
static const xfuturepayloadownershipv1 __xrtTlsListenerStreamPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtTlsListenerStreamFree, __xrtTlsListenerResultTrace};
XRT_API const xfutureproducerownershipv1* xrtTlsListenerFutureProducerPolicyV1Get(void) { return &__xrtTlsListenerWaitProducer; }
XRT_API const xcancelwatchownershipv1* xrtTlsListenerFutureCancelPolicyV1Get(void) { return &__xrtTlsListenerWaitCancelPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtTlsListenerFutureStreamPayloadPolicyV1Get(void) { return &__xrtTlsListenerStreamPayload; }
XRT_API const xrtownershipquiescencev1* xrtTlsListenerFutureOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 p = {sizeof(p), &__xrtTlsListenerWaitOps, __xrtTlsListenerWaitBusy}; return &p;
}
static bool __xrtTlsListenerWaitClaim(const void* data, const void* token)
{
	__xrt_tls_listener_wait* w = (__xrt_tls_listener_wait*)data; size_t count;
	if (!token || (w->Claim && w->Claim != token) || !__xrtTlsListenerWaitCount(w, &count)) return false;
	w->Claim = token; return true;
}
static void __xrtTlsListenerWaitRestore(const void* data, const void* token)
{
	__xrt_tls_listener_wait* w = (__xrt_tls_listener_wait*)data;
	if (!token || w->Claim != token || w->Cleared) abort();
	w->Claim = NULL;
}
static bool __xrtTlsListenerWaitReady(const void* data)
{ const __xrt_tls_listener_wait* w = data; size_t count; return __xrtTlsListenerWaitCount(w, &count) && xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE); }
static xrtownershipprepareresult __xrtTlsListenerWaitPrepare(const void* data, const void* token)
{
	const __xrt_tls_listener_wait* w = data; xrtownershipscope f = {0};
	if (!xrtOwnershipFreezeTryBegin(&f)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!token || w->Claim != token || w->Cleared) abort();
	bool ready = __xrtTlsListenerWaitReady(w);
	if (!xrtOwnershipScopeEnd(&f)) abort();
	return ready ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtTlsListenerWaitClear(const void* data, const void* token)
{
	__xrt_tls_listener_wait* w = (__xrt_tls_listener_wait*)data;
	if (!token || w->Claim != token || w->Cleared || !__xrtTlsListenerWaitReady(w)) abort();
	w->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtTlsListenerFutureOwnershipAdapterV1(xrtownershipref ref,
	const xrtownershippreparationv1** preparation)
{
	static const xrtownershipadapterv1 a = {sizeof(a), __xrtTlsListenerWaitHold, __xrtTlsListenerWaitDrop,
		__xrtTlsListenerWaitClaim, __xrtTlsListenerWaitRestore, NULL, __xrtTlsListenerWaitClear, NULL};
	static const xrtownershippreparationv1 p = {sizeof(p), &a, __xrtTlsListenerWaitReady, __xrtTlsListenerWaitPrepare};
	size_t count;
	if (!preparation || ref.Ops != &__xrtTlsListenerWaitOps || !__xrtTlsListenerWaitCount(ref.Data, &count)) return NULL;
	*preparation = &p; return &a;
}
static void __xrtTlsListenerWaitRetire(__xrt_tls_listener_wait* w)
{
	xrtownershipscope m = {0}; if (!xrtOwnershipMutationBegin(&m)) abort();
	if (w->Promise || w->Watch || w->Linked || xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE)) abort();
	xtlslistener* listener = w->Listener; w->Listener = NULL; w->Publishing = false;
	xrtAtomic32Store(&w->Done, 1, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&m)) abort();
	xrtTlsListenerDestroy(listener); __xrtTlsListenerWaitLeave(w); __xrtTlsListenerWaitDrop(w);
}
static void __xrtTlsListenerWaitDiscard(__xrt_tls_listener_wait* w, xfuture* future)
{
	xerror* error = xrtTakeError();
	xcancelwatch* watch = w->Watch; w->Watch = NULL;
	xpromise* promise = w->Promise; w->Promise = NULL;
	xrtCancelUnwatch(watch); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
	__xrtTlsListenerWaitRetire(w); xrtClearError(); xrtSetErrorTake(error);
}
static bool __xrtTlsListenerWaitSetup(__xrt_tls_listener_wait* w, xfuture** future)
{
	w->Promise = xrtPromiseCreate(future, NULL);
	if (!w->Promise || !__xrtTlsListenerWaitHold(w)) return false;
	if (!xrtPromiseProducerBindTakeV1(w->Promise, __xrtTlsListenerWaitOwnership(w), &__xrtTlsListenerWaitProducer)) {
		__xrtTlsListenerWaitDrop(w); return false;
	}
	xcancel* cancel = xrtPromiseCancelToken(w->Promise); if (!cancel) return false;
	if (!__xrtTlsListenerWaitHold(w)) { xrtCancelDestroy(cancel); return false; }
	w->Watch = xrtCancelWatchOwnedV1(cancel, w, &__xrtTlsListenerWaitCancelPolicy);
	xrtCancelDestroy(cancel);
	if (!w->Watch) { __xrtTlsListenerWaitDrop(w); return false; }
	return true;
}



/* 从等待链移除指定节点；调用方持有 Listener.Lock。 */
static bool __xrtTlsListenerWaitRemove(
	xtlslistener* pListener,
	__xrt_tls_listener_wait* pWaiter
)
{
	__xrt_tls_listener_wait** ppCurrent = &pListener->WaitHead;
	__xrt_tls_listener_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pListener->WaitTail == pWaiter ) {
		pListener->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	(void)xrtAtomic32FetchSub(
		&pListener->AcceptWaiters,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 完成一个等待节点，并转移 Stream 的调用方引用。 */
static void __xrtTlsListenerWaitFinish(
	__xrt_tls_listener_wait* pWaiter,
	xtlsstream* pStream,
	bool bCancelled
)
{
	xtlslistener* pListener = pWaiter->Listener;
	xpromise* pPromise = pWaiter->Promise;

	__xrtTlsListenerWaitEnter(pWaiter);
	if (!__xrtTlsListenerOwnershipEnter(pListener, false)) abort();
	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	__xrtTlsListenerOwnershipLeave(pListener);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pStream,
			&__xrtTlsListenerStreamPayload
		) ) {
			(void)__xrtTlsStreamAbortAccepted(pStream);
			xrtTlsStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	__xrtTlsListenerWaitRetire(pWaiter);
}



/* 把握手完成连接直接交给最早的等待者。 */
bool __xrtTlsListenerFutureAccept(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pNode
)
{
	__xrt_tls_listener_wait* pWaiter = NULL;
	xtlsstream* pStream = NULL;

	__xrtSpinLock(&pListener->Lock);
	if ( (xrtTlsListenerState(pListener) == XTLS_LISTENER_OPEN) &&
		(pListener->WaitHead != NULL) ) {
		pWaiter = pListener->WaitHead;
		(void)__xrtTlsListenerWaitRemove(pListener, pWaiter);
		pNode->CallerHeld = false;
		pStream = pNode->Stream;
	}
	__xrtSpinUnlock(&pListener->Lock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtTlsListenerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* Future 取消只移除当前等待，不影响 Listener 和其他消费者。 */
static void __xrtTlsListenerWaitCancel(ptr pData)
{
	__xrt_tls_listener_wait* pWaiter =
		(__xrt_tls_listener_wait*)pData;
	xtlslistener* pListener = pWaiter->Listener;
	bool bRemoved;

	if (!__xrtTlsListenerOwnershipEnter(pListener, false)) abort();
	__xrtSpinLock(&pListener->Lock);
	bRemoved = pWaiter->Linked &&
		__xrtTlsListenerWaitRemove(pListener, pWaiter);
	__xrtSpinUnlock(&pListener->Lock);
	__xrtTlsListenerOwnershipLeave(pListener);
	if ( bRemoved ) {
		__xrtTlsListenerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对完成队列与等待者，并在关闭后终结全部等待。 */
static void __xrtTlsListenerFutureNotifyImpl(xtlslistener* pListener)
{
	for ( ;; ) {
		__xrt_tls_listener_wait* pWaiter = NULL;
		__xrt_tls_listener_stream* pNode = NULL;
		xtlsstream* pStream = NULL;
		bool dropRecord = false;

		__xrtSpinLock(&pListener->Lock);
		if ( pListener->WaitHead != NULL ) {
			if ( xrtTlsListenerState(pListener) !=
				XTLS_LISTENER_OPEN ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtTlsListenerWaitRemove(
					pListener,
					pWaiter
				);
			} else if ( pListener->AcceptHead != NULL ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtTlsListenerWaitRemove(
					pListener,
					pWaiter
				);
				pNode = __xrtTlsListenerTakeQueued(pListener);
				dropRecord = pNode->Owned;
				pNode->CallerHeld = false;
				pStream = pNode->Stream;
			}
		}
		__xrtSpinUnlock(&pListener->Lock);
		if (dropRecord) __xrtTlsListenerStreamNodeDrop(pNode);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtTlsListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
}

void __xrtTlsListenerFutureNotify(xtlslistener* l)
{
	xtlslistener* held = xrtTlsListenerRef(l); if (!held) return;
	if (__xrtTlsListenerOwnershipEnter(held, false)) {
		__xrtTlsListenerFutureNotifyImpl(held); __xrtTlsListenerOwnershipLeave(held);
	}
	xrtTlsListenerDestroy(held);
}



/* 建立一个可取消的 TLS Listener accept Future。 */
static xfuture* __xrtTlsListenerAcceptAsync(xtlslistener* pListener)
{
	__xrt_tls_listener_wait* pWaiter;
	xtlslistener* pOwned;
	xfuture* pFuture = NULL;
	__xrt_tls_listener_stream* pNode = NULL;
	xtlsstream* pStream = NULL;
	bool bClosed = false;
	bool dropRecord = false;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtTlsError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"accept-tls-listener",
			"TLS listener accept Future requires no Accept callback",
			SIZE_MAX
		);
		return NULL;
	}
	pOwned = xrtTlsListenerRef(pListener);
	if ( pOwned == NULL ) {
		return NULL;
	}
	pWaiter = (__xrt_tls_listener_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		xrtTlsListenerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Listener = pOwned;
	pWaiter->References = 1; pWaiter->Active = 1; pWaiter->Publishing = true;
	xrtAtomic32Init(&pWaiter->Done, 0);
	if (!__xrtTlsListenerWaitSetup(pWaiter, &pFuture)) {
		__xrtTlsListenerWaitDiscard(pWaiter, pFuture); return NULL;
	}
	xrtownershipscope publish = {0}; if (!xrtOwnershipMutationBegin(&publish)) abort();
	__xrtSpinLock(&pListener->Lock);
	if ( xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN ) {
		bClosed = true;
	} else {
		if ( pListener->WaitHead == NULL ) {
			pNode = __xrtTlsListenerTakeQueued(pListener);
			if ( pNode != NULL ) {
				dropRecord = pNode->Owned;
				pNode->CallerHeld = false;
				pStream = pNode->Stream;
			}
		}
		if ( pNode == NULL ) {
			pWaiter->Linked = true;
			if ( pListener->WaitTail != NULL ) {
				pListener->WaitTail->Next = pWaiter;
			} else {
				pListener->WaitHead = pWaiter;
			}
			pListener->WaitTail = pWaiter;
			(void)xrtAtomic32FetchAdd(
				&pListener->AcceptWaiters,
				1,
				XMEMORY_ACQ_REL
			);
		}
	}
	pWaiter->Publishing = false;
	if (xrtRefRelease(&pWaiter->Active) != 0) abort();
	__xrtSpinUnlock(&pListener->Lock);
	if (!xrtOwnershipScopeEnd(&publish)) abort();
	if (dropRecord) __xrtTlsListenerStreamNodeDrop(pNode);
	if ( (pStream != NULL) || bClosed ) {
		__xrtTlsListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}

XRT_API xfuture* xrtTlsListenerAcceptAsync(xtlslistener* l)
{
	if (!__xrtTlsListenerOwnershipEnter(l, true)) return NULL;
	xfuture* result = __xrtTlsListenerAcceptAsync(l); __xrtTlsListenerOwnershipLeave(l); return result;
}

bool __xrtTlsListenerWaitOwnershipWalk(const xtlslistener* l, xrtownershipvisitor visit, ptr context)
{
	const __xrt_tls_listener_wait *slow = l->WaitHead, *fast = slow, *last = NULL;
	while (fast && fast->Next) { slow = slow->Next; fast = fast->Next->Next; if (slow == fast) return false; }
	size_t count = 0;
	for (const __xrt_tls_listener_wait* w = l->WaitHead; w; w = w->Next) {
		if (!w->Linked || w->Publishing || w->Cleared || w->Listener != l || !w->Promise || !w->Watch ||
			xrtAtomic32Load(&w->Done, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&w->Active) ||
			__xrtAtomicRefLoad(&w->References) <= 0 || count == SIZE_MAX) return false;
		if (visit && !visit(__xrtTlsListenerWaitOwnership(w), context)) return false;
		++count; last = w;
	}
	return last == l->WaitTail && count == xrtAtomic32Load(&l->AcceptWaiters, XMEMORY_ACQUIRE);
}

#endif
