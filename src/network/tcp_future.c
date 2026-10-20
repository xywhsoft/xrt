#include "../internal/xrt_tcp.h"



#if defined(XRT_FEATURE_NET_TCP_FUTURE)

/* 内部等待结果只在线性化取走等待节点后使用。 */
typedef enum __xrt_net_stream_wait_result {
	__XRT_NET_STREAM_WAIT_PENDING = 0,
	__XRT_NET_STREAM_WAIT_READY,
	__XRT_NET_STREAM_WAIT_RECEIVE,
	__XRT_NET_STREAM_WAIT_FAILED,
	__XRT_NET_STREAM_WAIT_CANCELLED,
	__XRT_NET_STREAM_WAIT_CLOSED
} __xrt_net_stream_wait_result;



/* One physical activation: the source list/constructor, Future producer and
 * cancellation Watch each own a real reference. Source/cache/Promise/Watch
 * are actual contained slots, not per-reference child estimates. */
typedef struct __xrt_net_tcp_wait {
	volatile int32 References, Active;
	const void* Claim;
	xpromise* Promise;
	xcancelwatch* Watch;
	xatomicptr CacheWorker;
	union { xnetstream* Stream; xnetlistener* Listener; } Owner;
	xatomic32 Done;
	bool Listener, Publishing, Cleared, Linked;
} __xrt_net_tcp_wait;

/* 一个等待节点持有 Stream 和 Promise，直到唯一终态完成。 */
struct __xrt_net_stream_wait {
	__xrt_net_tcp_wait Base;
	__xrt_net_stream_wait* Next;
	xnetstreamwait Wait;
	bool Receive;
	size_t MaxBytes;
	size_t MinimumBytes;
};



/* Listener 等待节点持有 Listener 和 Promise，直到唯一终态完成。 */
struct __xrt_net_listener_wait {
	__xrt_net_tcp_wait Base;
	__xrt_net_listener_wait* Next;
};

_Static_assert(
	sizeof(__xrt_net_stream_wait) <= 128u,
	"TCP Stream Future waiter left the small-node classes"
);

_Static_assert(
	sizeof(__xrt_net_listener_wait) <= 64u,
	"TCP Listener Future waiter left the 64-byte node class"
);



/* 从 Listener 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_listener_wait* __xrtNetListenerWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_listener_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_listener_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_listener_wait)
		) : (__xrt_net_listener_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_listener_wait)
		);

	if ( pWaiter != NULL ) {
		xrtAtomicPtrInit(&pWaiter->Base.CacheWorker, pWorker);
		xrtAtomic32Init(&pWaiter->Base.Done, 0);
		pWaiter->Base.References = 1; pWaiter->Base.Active = 1;
		pWaiter->Base.Publishing = true; pWaiter->Base.Listener = true;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 从 Stream 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_stream_wait* __xrtNetStreamWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_stream_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_stream_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_stream_wait)
		) : (__xrt_net_stream_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_stream_wait)
		);

	if ( pWaiter != NULL ) {
		xrtAtomicPtrInit(&pWaiter->Base.CacheWorker, pWorker);
		xrtAtomic32Init(&pWaiter->Base.Done, 0);
		pWaiter->Base.References = 1; pWaiter->Base.Active = 1;
		pWaiter->Base.Publishing = true;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 释放 Future 持有的接收结果。 */
static void __xrtNetStreamBytesFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetBytesDestroy((xnetbytes*)pValue);
}



/* 释放 Future 持有的已接受 Stream 引用。 */
static void __xrtNetListenerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetStreamDestroy((xnetstream*)pValue);
}

static void __xrtNetStreamWaitCancel(ptr pData);
static void __xrtNetListenerWaitCancel(ptr pData);
static void __xrtNetTcpWaitEnter(__xrt_net_tcp_wait* pWait)
{
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	int32 iActive = __xrtAtomicRefLoad(&pWait->Active);
	for (;;) {
		if (iActive < 0 || iActive == INT32_MAX || pWait->Cleared) abort();
		int32 iPrevious = __xrtAtomicRefCompareExchange(&pWait->Active, iActive + 1, iActive);
		if (iPrevious == iActive) break;
		iActive = iPrevious;
	}
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
}
static void __xrtNetTcpWaitLeave(__xrt_net_tcp_wait* pWait)
{
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	if (xrtRefRelease(&pWait->Active) < 0) abort();
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
}
static bool __xrtNetTcpWaitHold(const void* pData)
{
	__xrt_net_tcp_wait* pWait = (__xrt_net_tcp_wait*)pData; xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) return false;
	bool bHeld = !pWait->Cleared && xrtRefRetain(&pWait->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	return bHeld;
}
static void __xrtNetTcpWaitDrop(const void* pData)
{
	__xrt_net_tcp_wait* pWait = (__xrt_net_tcp_wait*)pData; xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	xnetworker* pWorker = NULL;
	size_t iSize = pWait->Listener ? sizeof(__xrt_net_listener_wait) : sizeof(__xrt_net_stream_wait);
	if (xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) && !__xrtAtomicRefLoad(&pWait->Active)) {
		/* Ordinary completion returns the same small allocation to its cache.
		 * If real pins survive, detach the allocator lease NOW: native Engine
		 * retirement must not wait for a collector pin on an inert shell. */
		if (__xrtAtomicRefCompareExchange(&pWait->References, 0, 1) == 1) {
			pWorker = xrtAtomicPtrExchange(&pWait->CacheWorker, NULL, XMEMORY_ACQ_REL);
			if (!xrtOwnershipScopeEnd(&Scope)) abort();
			if (pWorker) __xrtNetWorkerNodeRecycleHeld(pWorker, pWait, iSize);
			else xrtFree(pWait);
			return;
		}
		pWorker = xrtAtomicPtrExchange(&pWait->CacheWorker, NULL, XMEMORY_ACQ_REL);
	}
	int32 iLeft = xrtRefRelease(&pWait->References);
	if (iLeft < 0) abort();
	/* After a non-last release another thread may already free the shell.
	 * Only the final owner may inspect its fields below. */
	if (!iLeft) {
		bool bOwner = pWait->Listener ? pWait->Owner.Listener != NULL : pWait->Owner.Stream != NULL;
		if (!xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) || pWait->Publishing || pWait->Linked ||
			pWait->Promise || pWait->Watch || bOwner || __xrtAtomicRefLoad(&pWait->Active) ||
			xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE)) abort();
	}
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	if (!iLeft) xrtFree(pWait);
	if (pWorker) __xrtNetEngineObjectRelease(pWorker->Engine); /* last access to Worker */
}
static bool __xrtNetTcpWaitBusy(const void* pData)
{
	const __xrt_net_tcp_wait* pWait = pData;
	if (!pWait || pWait->Cleared) return false;
	if (pWait->Publishing || __xrtAtomicRefLoad(&pWait->Active)) return true;
	if (xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE)) return false;
	return pWait->Listener ? xrtNetListenerOwnershipQuiescenceV1Get()->Busy(pWait->Owner.Listener) :
		xrtNetStreamOwnershipQuiescenceV1Get()->Busy(pWait->Owner.Stream);
}
static bool __xrtNetTcpWaitCount(const void* pData, size_t* pCount)
{
	const __xrt_net_tcp_wait* pWait = pData;
	if (!pWait || !pCount || pWait->Cleared || __xrtNetTcpWaitBusy(pWait)) return false;
	bool bOwner = pWait->Listener ? pWait->Owner.Listener != NULL : pWait->Owner.Stream != NULL;
	bool bDone = xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) != 0;
	if (bDone && (pWait->Promise || pWait->Watch || bOwner || pWait->Linked ||
		xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE))) return false;
	if (!bDone && (!pWait->Linked || !pWait->Promise || !pWait->Watch || !bOwner)) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pWait->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetTcpWaitTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const __xrt_net_tcp_wait* pWait = pData; size_t iCount;
	if (!pVisit || !__xrtNetTcpWaitCount(pData, &iCount)) return false;
	if (!xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) && !pVisit(pWait->Listener ? xrtNetListenerOwnership(pWait->Owner.Listener) :
		xrtNetStreamOwnership(pWait->Owner.Stream), pContext)) return false;
	xnetworker* pWorker = xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE);
	if (pWorker && !pVisit(xrtNetEngineOwnership(pWorker->Engine), pContext)) return false;
	if (pWait->Promise && !pVisit(xrtPromiseOwnership(pWait->Promise), pContext)) return false;
	return !pWait->Watch || pVisit(xrtCancelWatchOwnership(pWait->Watch), pContext);
}
static const xrtownershipops __xrtNetTcpWaitOps = {__xrtNetTcpWaitCount, __xrtNetTcpWaitTrace};
static xrtownershipref __xrtNetTcpWaitOwnership(const __xrt_net_tcp_wait* pWait)
{ return (xrtownershipref){pWait, pWait ? &__xrtNetTcpWaitOps : NULL}; }
static void __xrtNetTcpWaitCancel(ptr pData)
{
	__xrt_net_tcp_wait* pWait = pData; __xrtNetTcpWaitEnter(pWait);
	if (pWait->Listener) __xrtNetListenerWaitCancel(pData); else __xrtNetStreamWaitCancel(pData);
	__xrtNetTcpWaitLeave(pWait);
}
static const xfutureproducerownershipv1 __xrtNetTcpWaitProducer = {sizeof(xfutureproducerownershipv1), __xrtNetTcpWaitDrop};
static const xcancelwatchownershipv1 __xrtNetTcpWaitCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtNetTcpWaitCancel, __xrtNetTcpWaitDrop, &__xrtNetTcpWaitOps};
static bool __xrtNetTcpBytesTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ return pValue && !pData && pVisit && pVisit(xrtNetBytesOwnership(pValue), pContext); }
static bool __xrtNetTcpStreamTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ return pValue && !pData && pVisit && pVisit(xrtNetStreamOwnership(pValue), pContext); }
static const xfuturepayloadownershipv1 __xrtNetTcpBytesPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetStreamBytesFree, __xrtNetTcpBytesTrace};
static const xfuturepayloadownershipv1 __xrtNetTcpStreamPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetListenerStreamFree, __xrtNetTcpStreamTrace};
XRT_API const xfutureproducerownershipv1* xrtNetTcpFutureProducerPolicyV1Get(void) { return &__xrtNetTcpWaitProducer; }
XRT_API const xcancelwatchownershipv1* xrtNetTcpFutureCancelPolicyV1Get(void) { return &__xrtNetTcpWaitCancelPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtNetTcpFutureBytesPayloadPolicyV1Get(void) { return &__xrtNetTcpBytesPayload; }
XRT_API const xfuturepayloadownershipv1* xrtNetTcpFutureStreamPayloadPolicyV1Get(void) { return &__xrtNetTcpStreamPayload; }
XRT_API const xrtownershipquiescencev1* xrtNetTcpFutureOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetTcpWaitOps, __xrtNetTcpWaitBusy};
	return &Policy;
}
static bool __xrtNetTcpWaitClaim(const void* pData, const void* pToken)
{
	__xrt_net_tcp_wait* pWait = (__xrt_net_tcp_wait*)pData; size_t iCount;
	if (!pToken || (pWait->Claim && pWait->Claim != pToken) || !__xrtNetTcpWaitCount(pData, &iCount)) return false;
	pWait->Claim = pToken; return true;
}
static void __xrtNetTcpWaitRestore(const void* pData, const void* pToken)
{
	__xrt_net_tcp_wait* pWait = (__xrt_net_tcp_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	pWait->Claim = NULL;
}
static bool __xrtNetTcpWaitReady(const void* pData)
{ const __xrt_net_tcp_wait* pWait = pData; size_t iCount;
	return __xrtNetTcpWaitCount(pData, &iCount) && xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE); }
static xrtownershipprepareresult __xrtNetTcpWaitPrepare(const void* pData, const void* pToken)
{
	const __xrt_net_tcp_wait* pWait = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	bool bReady = __xrtNetTcpWaitReady(pData);
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	/* The actual source performs normal close/notification. Never close a
	 * produced Future early or cancel accepted work through an adapter Drop. */
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetTcpWaitClear(const void* pData, const void* pToken)
{
	__xrt_net_tcp_wait* pWait = (__xrt_net_tcp_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared || !__xrtNetTcpWaitReady(pData)) abort();
	pWait->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetTcpFutureOwnershipAdapterV1(xrtownershipref Reference,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetTcpWaitHold, __xrtNetTcpWaitDrop,
		__xrtNetTcpWaitClaim, __xrtNetTcpWaitRestore, NULL, __xrtNetTcpWaitClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetTcpWaitReady, __xrtNetTcpWaitPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetTcpWaitOps || !__xrtNetTcpWaitCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}
static bool __xrtNetTcpWaitSetup(__xrt_net_tcp_wait* pWait, xfuture** ppFuture)
{
	pWait->Promise = xrtPromiseCreate(ppFuture, NULL);
	if (!pWait->Promise) return false;
	if (!__xrtNetTcpWaitHold(pWait)) return false;
	if (!xrtPromiseProducerBindTakeV1(pWait->Promise, __xrtNetTcpWaitOwnership(pWait), &__xrtNetTcpWaitProducer)) {
		__xrtNetTcpWaitDrop(pWait); return false;
	}
	xcancel* pCancel = xrtPromiseCancelToken(pWait->Promise);
	if (!pCancel) return false;
	if (!__xrtNetTcpWaitHold(pWait)) { xrtCancelDestroy(pCancel); return false; }
	pWait->Watch = xrtCancelWatchOwnedV1(pCancel, pWait, &__xrtNetTcpWaitCancelPolicy);
	xrtCancelDestroy(pCancel);
	if (!pWait->Watch) { __xrtNetTcpWaitDrop(pWait); return false; }
	return true;
}
/* Exactly one unlink winner owns the original constructor/list reference.
 * Watch retirement precedes removing the source slot: a concurrent callback
 * still borrows that source until Unwatch has synchronized its return. */
static void __xrtNetTcpWaitRetire(__xrt_net_tcp_wait* pWait)
{
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	if (pWait->Promise || pWait->Watch || pWait->Linked || xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE)) abort();
	bool bListener = pWait->Listener;
	xnetlistener* pListener = bListener ? pWait->Owner.Listener : NULL;
	xnetstream* pStream = bListener ? NULL : pWait->Owner.Stream;
	if (bListener) pWait->Owner.Listener = NULL; else pWait->Owner.Stream = NULL;
	pWait->Publishing = false;
	/* A late cancellation dispatch can drop its real Watch.Data reference
	 * concurrently. Publish terminal slots before that owner's acquire load. */
	xrtAtomic32Store(&pWait->Done, 1, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	if (bListener) xrtNetListenerDestroy(pListener); else xrtNetStreamDestroy(pStream);
	__xrtNetTcpWaitLeave(pWait); __xrtNetTcpWaitDrop(pWait);
}
static void __xrtNetTcpWaitDiscard(__xrt_net_tcp_wait* pWait, xfuture* pFuture)
{
	xerror* pError = xrtTakeError();
	xcancelwatch* pWatch = pWait->Watch; pWait->Watch = NULL;
	xpromise* pPromise = pWait->Promise; pWait->Promise = NULL;
	xrtCancelUnwatch(pWatch); xrtPromiseDestroy(pPromise); xrtFutureDestroy(pFuture);
	__xrtNetTcpWaitRetire(pWait);
	xrtClearError(); xrtSetErrorTake(pError);
}



/* 从 Listener 等待链表中移除指定节点；调用方持有 AcceptLock。 */
static bool __xrtNetListenerWaitRemove(
	xnetlistener* pListener,
	__xrt_net_listener_wait* pWaiter
)
{
	__xrt_net_listener_wait** ppCurrent = &pListener->WaitHead;
	__xrt_net_listener_wait* pPrevious = NULL;

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
	pWaiter->Base.Linked = false;
	(void)xrtAtomic32FetchSub(
		&pListener->AcceptWaiters,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 完成一个 Listener 等待节点并释放全部持有资源。 */
static void __xrtNetListenerWaitFinish(
	__xrt_net_listener_wait* pWaiter,
	xnetstream* pStream,
	bool bCancelled
)
{
	xnetlistener* pListener = pWaiter->Base.Owner.Listener;
	xpromise* pPromise = pWaiter->Base.Promise;
	__xrtNetTcpWaitEnter(&pWaiter->Base);
	if (!__xrtNetListenerOwnershipEnter(pListener, false)) abort();

	xrtCancelUnwatch(pWaiter->Base.Watch);
	pWaiter->Base.Watch = NULL;
	pWaiter->Base.Promise = NULL;
	__xrtNetListenerOwnershipLeave(pListener);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pStream,
			&__xrtNetTcpStreamPayload
		) ) {
			xrtNetStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	__xrtNetTcpWaitRetire(&pWaiter->Base);
}



/* 在 AcceptLock 内把连接线性化交给最早的异步消费者。 */
bool __xrtNetListenerFutureAccept(
	xnetlistener* pListener,
	xnetstream* pStream
)
{
	__xrt_net_listener_wait* pWaiter = NULL;

	__xrtSpinLock(&pListener->AcceptLock);
	if ( (xrtNetListenerState(pListener) == XNET_LISTENER_OPEN) &&
		 (pListener->WaitHead != NULL) ) {
		pWaiter = pListener->WaitHead;
		(void)__xrtNetListenerWaitRemove(pListener, pWaiter);
	}
	__xrtSpinUnlock(&pListener->AcceptLock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtNetListenerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* 取消只移除当前 Accept 等待，不停止 Listener。 */
static void __xrtNetListenerWaitCancel(ptr pData)
{
	__xrt_net_listener_wait* pWaiter =
		(__xrt_net_listener_wait*)pData;
	xnetlistener* pListener = pWaiter->Base.Owner.Listener;
	bool bRemoved;

	if (!__xrtNetListenerOwnershipEnter(pListener, false)) abort();
	__xrtSpinLock(&pListener->AcceptLock);
	bRemoved = pWaiter->Base.Linked &&
		__xrtNetListenerWaitRemove(pListener, pWaiter);
	__xrtSpinUnlock(&pListener->AcceptLock);
	__xrtNetListenerOwnershipLeave(pListener);
	if ( bRemoved ) {
		__xrtNetListenerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对排队 Stream 与 Accept Future，并关闭终止后的等待。 */
void __xrtNetListenerFutureNotify(xnetlistener* pListener)
{
	for ( ;; ) {
		__xrt_net_listener_wait* pWaiter = NULL;
		xnetstream* pStream = NULL;

		__xrtSpinLock(&pListener->AcceptLock);
		if ( pListener->WaitHead != NULL ) {
			if ( xrtNetListenerState(pListener) !=
				 XNET_LISTENER_OPEN ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtNetListenerWaitRemove(
					pListener,
					pWaiter
				);
			} else if ( pListener->AcceptHead != NULL ) {
				pWaiter = pListener->WaitHead;
				(void)__xrtNetListenerWaitRemove(
					pListener,
					pWaiter
				);
				pStream = __xrtNetListenerTakeQueued(pListener);
			}
		}
		__xrtSpinUnlock(&pListener->AcceptLock);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtNetListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
}



/* 建立一个 Listener 拉取 Accept Future。 */
static xfuture* __xrtNetListenerAcceptAsync(xnetlistener* pListener)
{
	__xrt_net_listener_wait* pWaiter;
	xnetlistener* pOwned;
	xfuture* pFuture = NULL;
	xnetstream* pStream = NULL;
	xnetworker* pCacheWorker = NULL;
	bool bClosed = false;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_LISTENER_ACCEPT,
			"accept-listener",
			"TCP accept Future requires a listener without an Accept callback",
			0
		);
		return NULL;
	}
	pOwned = xrtNetListenerRef(pListener);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->AcceptLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->AcceptLock);
			xrtNetListenerDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->AcceptLock);
	pWaiter = __xrtNetListenerWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetListenerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Base.Owner.Listener = pOwned;
	if (!__xrtNetTcpWaitSetup(&pWaiter->Base, &pFuture)) {
		__xrtNetTcpWaitDiscard(&pWaiter->Base, pFuture); return NULL;
	}
	xrtownershipscope Publish = {0}; if (!xrtOwnershipMutationBegin(&Publish)) abort();
	__xrtSpinLock(&pListener->AcceptLock);
	if ( xrtNetListenerState(pListener) != XNET_LISTENER_OPEN ) {
		bClosed = true;
	} else {
		if ( pListener->WaitHead == NULL ) {
			pStream = __xrtNetListenerTakeQueued(pListener);
		}
		if ( pStream == NULL ) {
			pWaiter->Base.Linked = true;
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
	pWaiter->Base.Publishing = false;
	if (xrtRefRelease(&pWaiter->Base.Active) != 0) abort();
	__xrtSpinUnlock(&pListener->AcceptLock);
	if (!xrtOwnershipScopeEnd(&Publish)) abort();
	if ( (pStream != NULL) || bClosed ) {
		__xrtNetListenerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}

XRT_API xfuture* xrtNetListenerAcceptAsync(xnetlistener* pListener)
{
	if (!__xrtNetListenerOwnershipEnter(pListener, true)) return NULL;
	xfuture* pFuture = __xrtNetListenerAcceptAsync(pListener);
	__xrtNetListenerOwnershipLeave(pListener);
	return pFuture;
}


/* 返回 Stream 终止原因对应的统一 Future 结果。 */
static __xrt_net_stream_wait_result __xrtNetStreamWaitTerminal(
	const xnetstream* pStream
)
{
	if ( pStream->CloseResult == XNET_RESULT_CANCELLED ) {
		return __XRT_NET_STREAM_WAIT_CANCELLED;
	}
	if ( (pStream->Error != NULL) ||
		 (pStream->CloseResult == XNET_RESULT_ERROR) ||
		 (pStream->CloseResult == XNET_RESULT_TIMEOUT) ) {
		return __XRT_NET_STREAM_WAIT_FAILED;
	}
	return __XRT_NET_STREAM_WAIT_CLOSED;
}



/* 在 Stream Worker 上判断一个等待条件是否已经进入终态。 */
static __xrt_net_stream_wait_result __xrtNetStreamWaitResult(
	const __xrt_net_stream_wait* pWaiter
)
{
	const xnetstream* pStream = pWaiter->Base.Owner.Stream;
	xnetstreamstate State = xrtNetStreamState(pStream);
	size_t iQueued = xrtNetStreamPending(pStream);

	if ( pWaiter->Wait == XNET_STREAM_WAIT_OPEN ) {
		if ( State == XNET_STREAM_OPEN ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( State >= XNET_STREAM_CLOSING ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		if ( xrtNetBufSize(&pStream->ReadBuffer) >=
			 pWaiter->MinimumBytes ) {
			return pWaiter->Receive ?
				__XRT_NET_STREAM_WAIT_RECEIVE :
				__XRT_NET_STREAM_WAIT_READY;
		}
		if ( xrtAtomic32Load(
			&pStream->ReadEnded,
			XMEMORY_ACQUIRE
		) || (State >= XNET_STREAM_CLOSING) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_WRITE ) {
		if ( (State == XNET_STREAM_OPEN) &&
			 !xrtAtomic32Load(
				&pStream->WriteGate,
				XMEMORY_ACQUIRE
			 ) && !xrtAtomic32Load(
				&pStream->WriteEnded,
				XMEMORY_ACQUIRE
			 ) && ((iQueued <= pStream->Config.WriteLowWater) ||
			 (!xrtAtomic32Load(
				&pStream->WriteBackpressured,
				XMEMORY_ACQUIRE
			 ) && (iQueued < pStream->Config.WriteHighWater))) ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( (State >= XNET_STREAM_CLOSING) ||
			 xrtAtomic32Load(
				&pStream->WriteGate,
				XMEMORY_ACQUIRE
			 ) || xrtAtomic32Load(
				&pStream->WriteEnded,
				XMEMORY_ACQUIRE
			 ) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_DRAIN ) {
		if ( ((State >= XNET_STREAM_CLOSING) ||
			  pStream->AbortRequested) &&
			 (pStream->CloseResult != XNET_RESULT_OK) ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		if ( iQueued == 0 ) {
			return __XRT_NET_STREAM_WAIT_READY;
		}
		if ( State == XNET_STREAM_CLOSED ) {
			return __xrtNetStreamWaitTerminal(pStream);
		}
		return __XRT_NET_STREAM_WAIT_PENDING;
	}
	if ( State == XNET_STREAM_CLOSED ) {
		__xrt_net_stream_wait_result Result =
			__xrtNetStreamWaitTerminal(pStream);

		return Result == __XRT_NET_STREAM_WAIT_CLOSED ?
			__XRT_NET_STREAM_WAIT_READY : Result;
	}
	return __XRT_NET_STREAM_WAIT_PENDING;
}



/* 为没有结构化原因的异常终止补充一个与等待操作匹配的稳定错误。 */
static xerror* __xrtNetStreamWaitError(
	const __xrt_net_stream_wait* pWaiter
)
{
	const xnetstream* pStream = pWaiter->Base.Owner.Stream;
	int32 iCode;
	cstr sMessage;

	if ( pStream->Error != NULL ) {
		return xrtErrorRef(pStream->Error);
	}
	if ( pWaiter->Wait == XNET_STREAM_WAIT_OPEN ) {
		iCode = XNET_ERROR_STREAM_CONNECT;
		sMessage = "TCP stream open wait failed";
	} else if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		iCode = XNET_ERROR_STREAM_READ;
		sMessage = "TCP stream read wait failed";
	} else if ( (pWaiter->Wait == XNET_STREAM_WAIT_WRITE) ||
		 (pWaiter->Wait == XNET_STREAM_WAIT_DRAIN) ) {
		iCode = XNET_ERROR_STREAM_WRITE;
		sMessage = "TCP stream write wait failed";
	} else {
		iCode = XNET_ERROR_STREAM_CLOSE;
		sMessage = "TCP stream close wait failed";
	}
	return xrtErrorCreate(
		pStream->CloseResult == XNET_RESULT_TIMEOUT ?
			XERR_TIMEOUT : XERR_IO,
		"xrt.net",
		iCode,
		sMessage
	);
}



/* 从等待链表中移除指定节点；调用方持有 WaitLock。 */
static bool __xrtNetStreamWaitRemove(
	xnetstream* pStream,
	__xrt_net_stream_wait* pWaiter
)
{
	__xrt_net_stream_wait** ppCurrent = &pStream->WaitHead;
	__xrt_net_stream_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pStream->WaitTail == pWaiter ) {
		pStream->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Base.Linked = false;
	if ( pWaiter->Wait == XNET_STREAM_WAIT_READ ) {
		pStream->ReadWaiters--;
	}
	return true;
}



static void __xrtNetStreamWaitTask(xnetworker* pWorker, ptr pData);



/* 用 Stream 内嵌命令无分配地唤醒所属 Worker；调用方持有 WaitLock。 */
static void __xrtNetStreamWaitSchedule(xnetstream* pStream)
{
	if ( pStream->WaitClosed || pStream->WaitPosted ||
		 (pStream->WaitHead == NULL) ) {
		return;
	}
	pStream->WaitPosted = true;
	(void)xrtNetStreamRef(pStream);
	if ( !__xrtNetEnginePostInternal(
		pStream->Worker,
		&pStream->WaitCommand,
		__xrtNetStreamWaitTask,
		pStream
	) ) {
		pStream->WaitPosted = false;
		xrtNetStreamDestroy(pStream);
	}
}



/* 取走链表中第一个已经满足或终止的等待节点。 */
static __xrt_net_stream_wait* __xrtNetStreamWaitTake(
	xnetstream* pStream,
	__xrt_net_stream_wait_result* pResult
)
{
	__xrt_net_stream_wait* pWaiter;

	__xrtSpinLock(&pStream->WaitLock);
	for ( pWaiter = pStream->WaitHead;
		pWaiter != NULL; pWaiter = pWaiter->Next ) {
		*pResult = __xrtNetStreamWaitResult(pWaiter);
		if ( *pResult != __XRT_NET_STREAM_WAIT_PENDING ) {
			(void)__xrtNetStreamWaitRemove(pStream, pWaiter);
			break;
		}
	}
	__xrtSpinUnlock(&pStream->WaitLock);
	return pWaiter;
}



/* 准备一份当前可用字节，在发布 Future 前完成底层读取。 */
static xnetbytes* __xrtNetStreamWaitReceive(
	__xrt_net_stream_wait* pWaiter,
	bool* pConsumed,
	xerror** ppError
)
{
	xnetstream* pStream = pWaiter->Base.Owner.Stream;
	size_t iSize = xrtNetBufSize(&pStream->ReadBuffer);
	xnetbytes* pBytes;
	xnetwspan Span;

	*pConsumed = false;
	*ppError = NULL;

	if ( (pWaiter->MaxBytes != 0) && (iSize > pWaiter->MaxBytes) ) {
		iSize = pWaiter->MaxBytes;
	}
	pBytes = __xrtNetBytesAlloc(iSize, &Span);
	if ( pBytes == NULL ) {
		*ppError = xrtTakeError();
		return NULL;
	}
	if ( xrtNetBufRead(
		&pStream->ReadBuffer,
		Span.Data,
		iSize
	) != iSize ) {
		xrtNetBytesDestroy(pBytes);
		__xrtNetSetError(
			XERR_INTERNAL,
			XNET_ERROR_STREAM_READ,
			"receive-stream",
			"TCP receive buffer changed outside its worker",
			0
		);
		*ppError = xrtTakeError();
		return NULL;
	}
	*pConsumed = true;
	__xrtNetStreamReadRefresh(pStream, false);
	return pBytes;
}



/* 完成一个已从 Stream 取走的等待节点并释放全部持有资源。 */
static bool __xrtNetStreamWaitFinish(
	__xrt_net_stream_wait* pWaiter,
	__xrt_net_stream_wait_result Result
)
{
	xnetstream* pStream = pWaiter->Base.Owner.Stream;
	xpromise* pPromise = pWaiter->Base.Promise;
	xnetbytes* pBytes = NULL;
	xerror* pError = NULL;
	bool bConsumed = false;

	__xrtNetTcpWaitEnter(&pWaiter->Base);
	if (!__xrtNetStreamOwnershipEnter(pStream, false)) abort();
	if ( Result == __XRT_NET_STREAM_WAIT_RECEIVE ) {
		pBytes = __xrtNetStreamWaitReceive(
			pWaiter,
			&bConsumed,
			&pError
		);
	} else if ( Result == __XRT_NET_STREAM_WAIT_FAILED ) {
		pError = __xrtNetStreamWaitError(pWaiter);
	}
	xrtCancelUnwatch(pWaiter->Base.Watch);
	pWaiter->Base.Watch = NULL;
	pWaiter->Base.Promise = NULL;
	__xrtNetStreamOwnershipLeave(pStream);
	if ( Result == __XRT_NET_STREAM_WAIT_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_NET_STREAM_WAIT_RECEIVE ) {
		if ( pBytes != NULL ) {
			if ( xrtPromiseResolveOwnedPolicyV1(
				pPromise,
				pBytes,
				&__xrtNetTcpBytesPayload
			) ) {
				pBytes = NULL;
			}
		} else if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_STREAM_WAIT_FAILED ) {
		if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_STREAM_WAIT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtNetBytesDestroy(pBytes);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
	__xrtNetTcpWaitRetire(&pWaiter->Base);
	return bConsumed;
}



/* 取消只移除当前等待，不关闭或中断底层 Stream。 */
static void __xrtNetStreamWaitCancel(ptr pData)
{
	__xrt_net_stream_wait* pWaiter =
		(__xrt_net_stream_wait*)pData;
	xnetstream* pStream = pWaiter->Base.Owner.Stream;
	bool bRemoved;

	if (!__xrtNetStreamOwnershipEnter(pStream, false)) abort();
	__xrtSpinLock(&pStream->WaitLock);
	bRemoved = pWaiter->Base.Linked &&
		__xrtNetStreamWaitRemove(pStream, pWaiter);
	__xrtSpinUnlock(&pStream->WaitLock);
	__xrtNetStreamOwnershipLeave(pStream);
	if ( bRemoved ) {
		(void)__xrtNetStreamWaitFinish(
			pWaiter,
			__XRT_NET_STREAM_WAIT_CANCELLED
		);
	}
}



/* 在所属 Worker 上迭代完成全部当前可推进的等待。 */
void __xrtNetStreamFutureNotify(xnetstream* pStream, bool bDriveRead)
{
	bool bConsumed = false;

	if ( !xrtNetWorkerIsCurrent(pStream->Worker) ) {
		__xrtSpinLock(&pStream->WaitLock);
		__xrtNetStreamWaitSchedule(pStream);
		__xrtSpinUnlock(&pStream->WaitLock);
		return;
	}
	for ( ;; ) {
		__xrt_net_stream_wait_result Result;
		__xrt_net_stream_wait* pWaiter =
			__xrtNetStreamWaitTake(pStream, &Result);

		if ( pWaiter == NULL ) {
			break;
		}
		bConsumed = __xrtNetStreamWaitFinish(pWaiter, Result) ||
			bConsumed;
	}
	if ( bConsumed && bDriveRead ) {
		__xrtNetStreamReadRefresh(pStream, true);
	}
}



/* 内嵌命令进入 Worker 后允许下一次登记再次投递。 */
static void __xrtNetStreamWaitTask(xnetworker* pWorker, ptr pData)
{
	xnetstream* pStream = (xnetstream*)pData;

	(void)pWorker;
	__xrtSpinLock(&pStream->WaitLock);
	pStream->WaitPosted = false;
	__xrtSpinUnlock(&pStream->WaitLock);
	__xrtNetStreamFutureNotify(pStream, true);
	xrtNetStreamDestroy(pStream);
}



/* 建立 Promise、取消监听和 Stream 等待节点。 */
static xfuture* __xrtNetStreamWaitCreateImpl(
	xnetstream* pStream,
	xnetstreamwait Wait,
	bool bReceive,
	size_t iMaxBytes,
	size_t iMinimumBytes
)
{
	__xrt_net_stream_wait* pWaiter;
	xnetstream* pOwned;
	xfuture* pFuture = NULL;
	xnetworker* pCacheWorker = NULL;
	__xrt_net_stream_wait_result Immediate =
		__XRT_NET_STREAM_WAIT_PENDING;

	if ( (pStream == NULL) || (Wait < XNET_STREAM_WAIT_OPEN) ||
		 (Wait > XNET_STREAM_WAIT_CLOSE) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Wait == XNET_STREAM_WAIT_READ) &&
		 (iMinimumBytes > pStream->Config.ReadLimit) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_STREAM_READ,
			"wait-stream-available",
			"minimum readable bytes exceed the TCP receive limit",
			0
		);
		return NULL;
	}
	pOwned = xrtNetStreamRef(pStream);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->WaitLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->WaitLock);
			xrtNetStreamDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->WaitLock);
	pWaiter = __xrtNetStreamWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetStreamDestroy(pOwned);
		return NULL;
	}
	pWaiter->Base.Owner.Stream = pOwned;
	pWaiter->Wait = Wait;
	pWaiter->Receive = bReceive;
	pWaiter->MaxBytes = iMaxBytes;
	pWaiter->MinimumBytes = iMinimumBytes;
	if (!__xrtNetTcpWaitSetup(&pWaiter->Base, &pFuture)) {
		__xrtNetTcpWaitDiscard(&pWaiter->Base, pFuture); return NULL;
	}
	xrtownershipscope Publish = {0}; if (!xrtOwnershipMutationBegin(&Publish)) abort();
	__xrtSpinLock(&pStream->WaitLock);
	if ( (Wait == XNET_STREAM_WAIT_READ) && pStream->ReadPush ) {
		__xrtSpinUnlock(&pStream->WaitLock);
		if (!xrtOwnershipScopeEnd(&Publish)) abort();
		__xrtNetTcpWaitDiscard(&pWaiter->Base, pFuture);
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_READ,
			"wait-stream",
			"read waits require a stream without a Read callback",
			0
		);
		return NULL;
	}
	pWaiter->Base.Linked = true;
	if ( Wait == XNET_STREAM_WAIT_READ ) {
		pStream->ReadWaiters++;
	}
	if ( pStream->WaitTail != NULL ) {
		pStream->WaitTail->Next = pWaiter;
	} else {
		pStream->WaitHead = pWaiter;
	}
	pStream->WaitTail = pWaiter;
	if ( pStream->WaitClosed ) {
		(void)__xrtNetStreamWaitRemove(pStream, pWaiter);
		Immediate = __xrtNetStreamWaitTerminal(pStream);
		if ( (Wait == XNET_STREAM_WAIT_CLOSE) &&
			 (Immediate == __XRT_NET_STREAM_WAIT_CLOSED) ) {
			Immediate = __XRT_NET_STREAM_WAIT_READY;
		} else if ( (Wait == XNET_STREAM_WAIT_DRAIN) &&
			 (Immediate == __XRT_NET_STREAM_WAIT_CLOSED) &&
			 (xrtNetStreamPending(pStream) == 0) ) {
			Immediate = __XRT_NET_STREAM_WAIT_READY;
		}
	} else {
		__xrtNetStreamWaitSchedule(pStream);
	}
	pWaiter->Base.Publishing = false;
	if (xrtRefRelease(&pWaiter->Base.Active) != 0) abort();
	__xrtSpinUnlock(&pStream->WaitLock);
	if (!xrtOwnershipScopeEnd(&Publish)) abort();
	if ( Immediate != __XRT_NET_STREAM_WAIT_PENDING ) {
		(void)__xrtNetStreamWaitFinish(pWaiter, Immediate);
	}
	return pFuture;
}

static xfuture* __xrtNetStreamWaitCreate(xnetstream* pStream,
	xnetstreamwait Wait, bool bReceive, size_t iMaxBytes, size_t iMinimumBytes)
{
	if (!__xrtNetStreamOwnershipEnter(pStream, true)) return NULL;
	xfuture* pFuture = __xrtNetStreamWaitCreateImpl(pStream, Wait, bReceive, iMaxBytes, iMinimumBytes);
	__xrtNetStreamOwnershipLeave(pStream);
	return pFuture;
}


/* 建立一个统一 Stream 条件 Future。 */
XRT_API xfuture* xrtNetStreamWaitAsync(
	xnetstream* pStream,
	xnetstreamwait Wait
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		Wait,
		false,
		0,
		Wait == XNET_STREAM_WAIT_READ ? 1u : 0u
	);
}



/* 异步等待拉取缓冲增长到指定字节数，不消费当前前缀。 */
XRT_API xfuture* xrtNetStreamWaitAvailableAsync(
	xnetstream* pStream,
	size_t iMinimum
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		XNET_STREAM_WAIT_READ,
		false,
		0,
		iMinimum
	);
}



/* 建立一个拉取模式字节接收 Future。 */
XRT_API xfuture* xrtNetStreamRecvAsync(
	xnetstream* pStream,
	size_t iMaxBytes
)
{
	return __xrtNetStreamWaitCreate(
		pStream,
		XNET_STREAM_WAIT_READ,
		true,
		iMaxBytes,
		1u
	);
}

/* These chains own the original activation reference; Next is structural.
 * Read only after the source quiescence gate, with independent child admission. */
bool __xrtNetStreamWaitOwnershipWalk(const xnetstream* pStream, xrtownershipvisitor pVisit, ptr pContext)
{
	if (pStream->WaitPosted) return false;
	const __xrt_net_stream_wait *pSlow = pStream->WaitHead, *pFast = pSlow, *pTail = NULL;
	while (pFast && pFast->Next) {
		pSlow = pSlow->Next; pFast = pFast->Next->Next; if (pSlow == pFast) return false;
	}
	size_t iReads = 0;
	for (const __xrt_net_stream_wait* pWait = pStream->WaitHead; pWait; pWait = pWait->Next) {
		if (pWait->Base.Listener || !pWait->Base.Linked || xrtAtomic32Load(&pWait->Base.Done, XMEMORY_ACQUIRE) || pWait->Base.Publishing ||
			pWait->Base.Owner.Stream != pStream || !pWait->Base.Promise || !pWait->Base.Watch ||
			__xrtAtomicRefLoad(&pWait->Base.References) <= 0 || pWait->Wait < XNET_STREAM_WAIT_OPEN ||
			pWait->Wait > XNET_STREAM_WAIT_CLOSE) return false;
		if (pWait->Wait == XNET_STREAM_WAIT_READ) ++iReads;
		if (pVisit && !pVisit(__xrtNetTcpWaitOwnership(&pWait->Base), pContext)) return false;
		pTail = pWait;
	}
	return pTail == pStream->WaitTail && iReads == pStream->ReadWaiters;
}
bool __xrtNetListenerWaitOwnershipWalk(const xnetlistener* pListener, xrtownershipvisitor pVisit, ptr pContext)
{
	const __xrt_net_listener_wait *pSlow = pListener->WaitHead, *pFast = pSlow, *pTail = NULL;
	while (pFast && pFast->Next) {
		pSlow = pSlow->Next; pFast = pFast->Next->Next; if (pSlow == pFast) return false;
	}
	size_t iCount = 0;
	for (const __xrt_net_listener_wait* pWait = pListener->WaitHead; pWait; pWait = pWait->Next) {
		if (!pWait->Base.Listener || !pWait->Base.Linked || xrtAtomic32Load(&pWait->Base.Done, XMEMORY_ACQUIRE) || pWait->Base.Publishing ||
			pWait->Base.Owner.Listener != pListener || !pWait->Base.Promise || !pWait->Base.Watch ||
			__xrtAtomicRefLoad(&pWait->Base.References) <= 0) return false;
		++iCount;
		if (pVisit && !pVisit(__xrtNetTcpWaitOwnership(&pWait->Base), pContext)) return false;
		pTail = pWait;
	}
	return pTail == pListener->WaitTail && iCount == xrtAtomic32Load(&pListener->AcceptWaiters, XMEMORY_ACQUIRE);
}

#endif
