#include "../internal/xrt_tcp_server.h"



#if defined(XRT_FEATURE_NET_TCP_SERVER_FUTURE)

/* 一个等待节点持有 Server 和 Promise，直到唯一终态完成。 */
struct __xrt_net_server_wait {
	volatile int32 References;
	volatile int32 Active;
	const void* Claim;
	xatomic32 Done;
	bool Publishing;
	bool Cleared;
	__xrt_net_server_wait* Next;
	xnetserver* Server;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};

static void __xrtNetServerWaitCancel(ptr pData);
static void __xrtNetServerWaitEnter(__xrt_net_server_wait* pWait)
{
	xrtownershipscope Mutation = {0};if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	int32 iActive = __xrtAtomicRefLoad(&pWait->Active);
	for (;;) {
		if (iActive < 0 || iActive == INT32_MAX || pWait->Cleared) abort();
		int32 iOld = __xrtAtomicRefCompareExchange(&pWait->Active, iActive + 1, iActive);
		if (iOld == iActive) break;
		iActive = iOld;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
static void __xrtNetServerWaitLeave(__xrt_net_server_wait* pWait)
{if (xrtOwnershipRefRelease(&pWait->Active) < 0) abort();}
static bool __xrtNetServerWaitHold(const void* pData)
{
	__xrt_net_server_wait* pWait = (__xrt_net_server_wait*)pData;xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bHeld = !pWait->Cleared && xrtRefRetain(&pWait->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static void __xrtNetServerWaitDrop(const void* pData)
{
	__xrt_net_server_wait* pWait = (__xrt_net_server_wait*)pData;xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	int32 iLeft = xrtRefRelease(&pWait->References);if (iLeft < 0) abort();xnetserver* pServer = NULL;
	if (!iLeft) {
		if (!xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&pWait->Active) ||
			pWait->Publishing || pWait->Linked || pWait->Promise || pWait->Watch) abort();
		pServer = pWait->Server;pWait->Server = NULL;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!iLeft) {xrtNetServerDestroy(pServer);xrtFree(pWait);}
}
static bool __xrtNetServerWaitBusy(const void* pData)
{
	const __xrt_net_server_wait* pWait = pData;if (!pWait || pWait->Cleared) return false;
	if (pWait->Publishing || __xrtAtomicRefLoad(&pWait->Active)) return true;
	return !xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) &&
		xrtNetServerOwnershipQuiescenceV1Get()->Busy(pWait->Server);
}
static bool __xrtNetServerWaitCount(const void* pData, size_t* pCount)
{
	const __xrt_net_server_wait* pWait = pData;
	if (!pWait || !pCount || pWait->Cleared || __xrtNetServerWaitBusy(pData) || !pWait->Server) return false;
	if (xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE)) {
		if (pWait->Promise || pWait->Watch || pWait->Linked) return false;
	} else if (!pWait->Promise || !pWait->Watch || !pWait->Linked) return false;
	int32 iRefs = __xrtAtomicRefLoad(&pWait->References);if (iRefs <= 0) return false;
	*pCount = (size_t)iRefs;return true;
}
static bool __xrtNetServerWaitTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const __xrt_net_server_wait* pWait = pData;size_t iCount;
	if (!pVisit || !__xrtNetServerWaitCount(pData, &iCount) || !pVisit(xrtNetServerOwnership(pWait->Server), pContext)) return false;
	if (pWait->Promise && !pVisit(xrtPromiseOwnership(pWait->Promise), pContext)) return false;
	return !pWait->Watch || pVisit(xrtCancelWatchOwnership(pWait->Watch), pContext);
}
static const xrtownershipops __xrtNetServerWaitOps = {__xrtNetServerWaitCount, __xrtNetServerWaitTrace};
static xrtownershipref __xrtNetServerWaitOwnership(const __xrt_net_server_wait* pWait)
{return (xrtownershipref){pWait, pWait ? &__xrtNetServerWaitOps : NULL};}
static void __xrtNetServerWaitCancelOwned(ptr pData)
{
	__xrt_net_server_wait* pWait = pData;__xrtNetServerWaitEnter(pWait);
	__xrtNetServerWaitCancel(pData);__xrtNetServerWaitLeave(pWait);
}
static const xfutureproducerownershipv1 __xrtNetServerWaitProducer = {sizeof(xfutureproducerownershipv1), __xrtNetServerWaitDrop};
static const xcancelwatchownershipv1 __xrtNetServerWaitCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtNetServerWaitCancelOwned, __xrtNetServerWaitDrop, &__xrtNetServerWaitOps};



/* 释放 Future 持有的已接受 Stream 引用。 */
static void __xrtNetServerStreamFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetStreamDestroy((xnetstream*)pValue);
}

static bool __xrtNetServerStreamTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{return pValue && !pData && pVisit && pVisit(xrtNetStreamOwnership(pValue), pContext);}
static const xfuturepayloadownershipv1 __xrtNetServerStreamPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetServerStreamFree, __xrtNetServerStreamTrace};
XRT_API const xfutureproducerownershipv1* xrtNetServerFutureProducerPolicyV1Get(void) {return &__xrtNetServerWaitProducer;}
XRT_API const xcancelwatchownershipv1* xrtNetServerFutureCancelPolicyV1Get(void) {return &__xrtNetServerWaitCancelPolicy;}
XRT_API const xfuturepayloadownershipv1* xrtNetServerFutureStreamPayloadPolicyV1Get(void) {return &__xrtNetServerStreamPayload;}
XRT_API const xrtownershipquiescencev1* xrtNetServerFutureOwnershipQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetServerWaitOps, __xrtNetServerWaitBusy};return &Policy;}
static bool __xrtNetServerWaitClaim(const void* pData, const void* pToken)
{
	__xrt_net_server_wait* pWait = (__xrt_net_server_wait*)pData;size_t iCount;
	if (!pToken || (pWait->Claim && pWait->Claim != pToken) || !__xrtNetServerWaitCount(pData, &iCount)) return false;
	pWait->Claim = pToken;return true;
}
static void __xrtNetServerWaitRestore(const void* pData, const void* pToken)
{
	__xrt_net_server_wait* pWait = (__xrt_net_server_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	pWait->Claim = NULL;
}
static bool __xrtNetServerWaitReady(const void* pData)
{const __xrt_net_server_wait* pWait = pData;size_t iCount;return __xrtNetServerWaitCount(pData, &iCount) && xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE);}
static xrtownershipprepareresult __xrtNetServerWaitPrepare(const void* pData, const void* pToken)
{
	const __xrt_net_server_wait* pWait = pData;xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	bool bReady = __xrtNetServerWaitReady(pData);if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	/* Source closes and delivers its own terminal state; no early Promise close. */
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetServerWaitClear(const void* pData, const void* pToken)
{
	__xrt_net_server_wait* pWait = (__xrt_net_server_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared || !__xrtNetServerWaitReady(pData)) abort();
	xnetserver* pServer = pWait->Server;pWait->Server = NULL;pWait->Cleared = true;xrtNetServerDestroy(pServer);
}
XRT_API const xrtownershipadapterv1* xrtNetServerFutureOwnershipAdapterV1(xrtownershipref Reference,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetServerWaitHold, __xrtNetServerWaitDrop,
		__xrtNetServerWaitClaim, __xrtNetServerWaitRestore, NULL, __xrtNetServerWaitClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetServerWaitReady, __xrtNetServerWaitPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetServerWaitOps || !__xrtNetServerWaitCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation;return &Adapter;
}



/* 从 Server 等待链中移除指定节点；调用方持有 AcceptLock。 */
static bool __xrtNetServerWaitRemove(
	xnetserver* pServer,
	__xrt_net_server_wait* pWaiter
)
{
	__xrt_net_server_wait** ppCurrent = &pServer->WaitHead;
	__xrt_net_server_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pServer->WaitTail == pWaiter ) {
		pServer->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	(void)xrtAtomic32FetchSub(
		&pServer->AcceptWaiters,
		1,
		XMEMORY_ACQ_REL
	);
	return true;
}



/* 完成一个 Server 等待节点并释放全部持有资源。 */
static void __xrtNetServerWaitFinish(
	__xrt_net_server_wait* pWaiter,
	xnetstream* pStream,
	bool bCancelled
)
{
	xnetserver* pServer = pWaiter->Server;
	xpromise* pPromise = pWaiter->Promise;
	__xrtNetServerWaitEnter(pWaiter);
	if (!__xrtNetServerEnter(pServer, false)) abort();

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	__xrtNetServerLeave(pServer);
	if ( pStream != NULL ) {
		if ( !xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pStream,
			&__xrtNetServerStreamPayload
		) ) {
			(void)xrtNetStreamAbort(pStream);
			xrtNetStreamDestroy(pStream);
		}
	} else if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	xrtAtomic32Store(&pWaiter->Done, 1, XMEMORY_RELEASE);
	__xrtNetServerWaitLeave(pWaiter);__xrtNetServerWaitDrop(pWaiter);
}



/* 在 AcceptLock 内把连接线性化交给最早的异步消费者。 */
bool __xrtNetServerFutureAccept(
	xnetserver* pServer,
	xnetstream* pStream
)
{
	__xrt_net_server_wait* pWaiter = NULL;

	__xrtSpinLock(&pServer->AcceptLock);
	if ( (xrtNetServerState(pServer) == XNET_SERVER_OPEN) &&
		 (pServer->WaitHead != NULL) ) {
		pWaiter = pServer->WaitHead;
		(void)__xrtNetServerWaitRemove(pServer, pWaiter);
	}
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( pWaiter == NULL ) {
		return false;
	}
	__xrtNetServerWaitFinish(pWaiter, pStream, false);
	return true;
}



/* 取消只移除当前聚合 Accept 等待，不停止 Server。 */
static void __xrtNetServerWaitCancel(ptr pData)
{
	__xrt_net_server_wait* pWaiter =
		(__xrt_net_server_wait*)pData;
	xnetserver* pServer = pWaiter->Server;
	bool bRemoved;
	if (!__xrtNetServerEnter(pServer, false)) abort();

	__xrtSpinLock(&pServer->AcceptLock);
	bRemoved = pWaiter->Linked &&
		__xrtNetServerWaitRemove(pServer, pWaiter);
	__xrtSpinUnlock(&pServer->AcceptLock);
	__xrtNetServerLeave(pServer);
	if ( bRemoved ) {
		__xrtNetServerWaitFinish(pWaiter, NULL, true);
	}
}



/* 按 FIFO 配对排队 Stream 与 Future，并关闭终止后的等待。 */
void __xrtNetServerFutureNotify(xnetserver* pServer)
{
	for ( ;; ) {
		__xrt_net_server_wait* pWaiter = NULL;
		xnetstream* pStream = NULL;

		__xrtSpinLock(&pServer->AcceptLock);
		if ( pServer->WaitHead != NULL ) {
			if ( xrtNetServerState(pServer) != XNET_SERVER_OPEN ) {
				pWaiter = pServer->WaitHead;
				(void)__xrtNetServerWaitRemove(pServer, pWaiter);
			} else if ( pServer->AcceptHead != NULL ) {
				pWaiter = pServer->WaitHead;
				(void)__xrtNetServerWaitRemove(pServer, pWaiter);
				pStream = __xrtNetServerTakeQueued(pServer);
			}
		}
		__xrtSpinUnlock(&pServer->AcceptLock);
		if ( pWaiter == NULL ) {
			break;
		}
		__xrtNetServerWaitFinish(pWaiter, pStream, false);
	}
}



/* 建立一个 Server 拉取 Accept Future。 */
static xfuture* __xrtNetServerAcceptAsync(xnetserver* pServer)
{
	__xrt_net_server_wait* pWaiter;
	xnetserver* pOwned;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	xnetstream* pStream = NULL;
	bool bClosed = false;

	if ( pServer == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pServer->Events.Accept != NULL ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_SERVER_ACCEPT,
			"accept-server",
			"TCP server accept Future requires no Accept callback",
			0
		);
		return NULL;
	}
	pOwned = xrtNetServerRef(pServer);
	if ( pOwned == NULL ) {
		return NULL;
	}
	pWaiter = (__xrt_net_server_wait*)xrtCalloc(1, sizeof(*pWaiter));
	if ( pWaiter == NULL ) {
		xrtNetServerDestroy(pOwned);
		return NULL;
	}
	pWaiter->Server = pOwned;
	pWaiter->References = 1;pWaiter->Active = 1;pWaiter->Publishing = true;
	xrtAtomic32Init(&pWaiter->Done, 0);
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFree(pWaiter);
		xrtNetServerDestroy(pOwned);
		return NULL;
	}
	bool bBound = __xrtNetServerWaitHold(pWaiter);
	if (bBound && !xrtPromiseProducerBindTakeV1(pWaiter->Promise, __xrtNetServerWaitOwnership(pWaiter), &__xrtNetServerWaitProducer)) {
		__xrtNetServerWaitDrop(pWaiter);bBound = false;
	}
	pCancel = bBound ? xrtPromiseCancelToken(pWaiter->Promise) : NULL;
	if ( pCancel != NULL ) {
		bool bHeld = __xrtNetServerWaitHold(pWaiter);
		pWaiter->Watch = bHeld ? xrtCancelWatchOwnedV1(
			pCancel,
			pWaiter,
			&__xrtNetServerWaitCancelPolicy
		) : NULL;
		if (bHeld && !pWaiter->Watch) __xrtNetServerWaitDrop(pWaiter);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xpromise* pPromise = pWaiter->Promise;pWaiter->Promise = NULL;
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pPromise);pWaiter->Publishing = false;
		xrtAtomic32Store(&pWaiter->Done, 1, XMEMORY_RELEASE);
		__xrtNetServerWaitLeave(pWaiter);__xrtNetServerWaitDrop(pWaiter);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	__xrtSpinLock(&pServer->AcceptLock);
	if ( xrtNetServerState(pServer) != XNET_SERVER_OPEN ) {
		bClosed = true;
	} else {
		if ( pServer->WaitHead == NULL ) {
			pStream = __xrtNetServerTakeQueued(pServer);
		}
		if ( pStream == NULL ) {
			pWaiter->Linked = true;
			if ( pServer->WaitTail != NULL ) {
				pServer->WaitTail->Next = pWaiter;
			} else {
				pServer->WaitHead = pWaiter;
			}
			pServer->WaitTail = pWaiter;
			(void)xrtAtomic32FetchAdd(
				&pServer->AcceptWaiters,
				1,
				XMEMORY_ACQ_REL
			);
		}
	}
	/* Hand off the constructor/list credit BEFORE another source thread can
	 * unlink it; no access to a linked waiter is allowed after unlocking. */
	pWaiter->Publishing = false;__xrtNetServerWaitLeave(pWaiter);
	__xrtSpinUnlock(&pServer->AcceptLock);
	if ( (pStream != NULL) || bClosed ) {
		__xrtNetServerWaitFinish(
			pWaiter,
			pStream,
			false
		);
	}
	return pFuture;
}

XRT_API xfuture* xrtNetServerAcceptAsync(xnetserver* pServer)
{
	if (!__xrtNetServerEnter(pServer, true)) return NULL;
	xfuture* pResult = __xrtNetServerAcceptAsync(pServer);
	__xrtNetServerLeave(pServer);return pResult;
}
bool __xrtNetServerWaitOwnershipWalk(const xnetserver* pServer, xrtownershipvisitor pVisit, ptr pContext)
{
	size_t iSeen = 0;size_t iExpected = xrtAtomic32Load(&pServer->AcceptWaiters, XMEMORY_ACQUIRE);
	const __xrt_net_server_wait* pTail = NULL;
	for (const __xrt_net_server_wait* pWait = pServer->WaitHead; pWait; pWait = pWait->Next) {
		if (++iSeen > iExpected || pWait->Server != pServer || !pWait->Linked || pWait->Publishing || pWait->Cleared ||
			xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) || __xrtAtomicRefLoad(&pWait->References) <= 0) return false;
		if (pVisit && !pVisit(__xrtNetServerWaitOwnership(pWait), pContext)) return false;
		pTail = pWait;
	}
	return iSeen == iExpected && pTail == pServer->WaitTail;
}

#endif
