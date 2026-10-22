#include "../internal/xrt_udp.h"



#if defined(XRT_FEATURE_NET_UDP_FUTURE)

#define XRT_NET_UDP_FUTURE_BATCH_MAX 256u



/* 内部等待结果只在线性化取走等待节点后使用。 */
typedef enum __xrt_net_udp_wait_result {
	__XRT_NET_UDP_WAIT_PENDING = 0,
	__XRT_NET_UDP_WAIT_READY,
	__XRT_NET_UDP_WAIT_RECEIVE,
	__XRT_NET_UDP_WAIT_ERROR,
	__XRT_NET_UDP_WAIT_FAILED,
	__XRT_NET_UDP_WAIT_CANCELLED,
	__XRT_NET_UDP_WAIT_CLOSED
} __xrt_net_udp_wait_result;



/* 批量结果拥有每个尚未转移的数据包。 */
struct xnetudpbatch {
	volatile int32 References;
	size_t Count;
	size_t Capacity;
	xatomicptr Packets[1];
};



/* 一个等待节点持有 UDP 和 Promise，直到唯一终态完成。 */
struct __xrt_net_udp_wait {
	volatile int32 References, Active;
	const void* Claim;
	__xrt_net_udp_wait* Next;
	__xrt_net_udp_wait* FinishNext;
	xnetudp* Udp;
	xatomicptr CacheWorker;
	xpromise* Promise;
	xcancelwatch* Watch;
	xnetudpbatch* Batch;
	xnetudppacket* Packet;
	xnetudperrorpacket* ErrorPacket;
	xnetudpwait Wait;
	__xrt_net_udp_wait_result Result;
	size_t RequiredBytes;
	xatomic32 Done;
	bool Receive;
	bool Writable;
	bool Linked;
	bool Publishing, Cleared;
};

_Static_assert(
	sizeof(__xrt_net_udp_wait) <= 128u,
	"UDP Future waiter left the 128-byte node class"
);



/* 从 UDP 所属 Worker 的统一缓存取得一个等待节点。 */
static __xrt_net_udp_wait* __xrtNetUdpWaitAlloc(
	xnetworker* pWorker
)
{
	__xrt_net_udp_wait* pWaiter = pWorker != NULL ?
		(__xrt_net_udp_wait*)__xrtNetWorkerNodeAlloc(
			pWorker,
			sizeof(__xrt_net_udp_wait)
		) : (__xrt_net_udp_wait*)xrtCalloc(
			1,
			sizeof(__xrt_net_udp_wait)
		);

	if ( pWaiter != NULL ) {
		xrtAtomicPtrInit(&pWaiter->CacheWorker, pWorker);
		xrtAtomic32Init(&pWaiter->Done, 0);
		pWaiter->References = 1;
		pWaiter->Active = 1;
		pWaiter->Publishing = true;
	} else if ( pWorker != NULL ) {
		__xrtNetEngineObjectRelease(pWorker->Engine);
	}
	return pWaiter;
}



/* 增加批量结果引用。 */
XRT_API xnetudpbatch* xrtNetUdpBatchRef(xnetudpbatch* pBatch)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;
	bool bHeld = pBatch && xrtRefRetain(&pBatch->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bHeld) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pBatch;
}



/* 销毁批量结果及其中仍由结果拥有的数据包。 */
XRT_API void xrtNetUdpBatchDestroy(xnetudpbatch* pBatch)
{
	if (!pBatch) return;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	int32 iReferences = xrtRefRelease(&pBatch->References);
	if (iReferences < 0) abort();
	if (!iReferences) {
		for (size_t i = 0; i < pBatch->Count; i++)
			xrtNetUdpPacketDestroy(xrtAtomicPtrLoad(&pBatch->Packets[i], XMEMORY_RELAXED));
		xrtFree(pBatch);
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}



/* 返回批量结果中的数据包数量。 */
XRT_API size_t xrtNetUdpBatchCount(const xnetudpbatch* pBatch)
{
	return pBatch != NULL ? pBatch->Count : 0;
}



/* 返回批量结果中一个借用的数据包。 */
XRT_API xnetudppacket* xrtNetUdpBatchPacket(
	const xnetudpbatch* pBatch,
	size_t iIndex
)
{
	if ( (pBatch == NULL) || (iIndex >= pBatch->Count) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return xrtAtomicPtrLoad(&pBatch->Packets[iIndex], XMEMORY_ACQUIRE);
}



/* 从批量结果转移一个数据包所有权。 */
XRT_API xnetudppacket* xrtNetUdpBatchTake(
	xnetudpbatch* pBatch,
	size_t iIndex
)
{
	xnetudppacket* pPacket;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return NULL;

	if ( (pBatch == NULL) || (iIndex >= pBatch->Count) ) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pPacket = xrtAtomicPtrExchange(&pBatch->Packets[iIndex], NULL, XMEMORY_ACQ_REL);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return pPacket;
}

static bool __xrtNetUdpBatchCount(const void* pData, size_t* pCount)
{
	const xnetudpbatch* pBatch = pData;
	if (!pBatch || !pCount || pBatch->Count > pBatch->Capacity) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pBatch->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetUdpBatchTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetudpbatch* pBatch = pData; size_t iCount;
	if (!pVisit || !__xrtNetUdpBatchCount(pData, &iCount)) return false;
	for (size_t i = 0; i < pBatch->Count; ++i) {
		xnetudppacket* pPacket = xrtAtomicPtrLoad(&pBatch->Packets[i], XMEMORY_ACQUIRE);
		if (pPacket && !pVisit(xrtNetUdpPacketOwnership(pPacket), pContext)) return false;
	}
	return true;
}
static const xrtownershipops __xrtNetUdpBatchOps = {__xrtNetUdpBatchCount, __xrtNetUdpBatchTrace};
XRT_API xrtownershipref xrtNetUdpBatchOwnership(const xnetudpbatch* pBatch)
{ return (xrtownershipref){pBatch, pBatch ? &__xrtNetUdpBatchOps : NULL}; }
static bool __xrtNetUdpBatchHold(const void* pData)
{ return xrtNetUdpBatchRef((xnetudpbatch*)pData) != NULL; }
static void __xrtNetUdpBatchDrop(const void* pData)
{ xrtNetUdpBatchDestroy((xnetudpbatch*)pData); }
static bool __xrtNetUdpBatchClaim(const void* pData, const void* pToken)
{ return pData && pToken; }
static void __xrtNetUdpBatchKeep(const void* pData, const void* pToken)
{ (void)pData; (void)pToken; }
XRT_API const xrtownershipadapterv1* xrtNetUdpBatchOwnershipAdapterV1(xrtownershipref Reference)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetUdpBatchHold, __xrtNetUdpBatchDrop,
		__xrtNetUdpBatchClaim, __xrtNetUdpBatchKeep, NULL, __xrtNetUdpBatchKeep, NULL};
	size_t iCount;
	return Reference.Ops == &__xrtNetUdpBatchOps && __xrtNetUdpBatchCount(Reference.Data, &iCount) ? &Adapter : NULL;
}



/* 分配一个尚未包含数据包的批量结果。 */
static xnetudpbatch* __xrtNetUdpBatchCreate(size_t iCapacity)
{
	xnetudpbatch* pBatch;
	size_t iBase = offsetof(xnetudpbatch, Packets);
	size_t iAllocation;

	if ( iCapacity > ((SIZE_MAX - iBase) / sizeof(xatomicptr)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocation = iBase + (iCapacity * sizeof(xatomicptr));
	pBatch = (xnetudpbatch*)xrtCalloc(1, iAllocation);
	if ( pBatch != NULL ) {
		pBatch->References = 1;
		pBatch->Capacity = iCapacity;
		for (size_t i = 0; i < iCapacity; ++i) xrtAtomicPtrInit(&pBatch->Packets[i], NULL);
	}
	return pBatch;
}



/* Future 持有的数据包析构过程。 */
static void __xrtNetUdpPacketFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpPacketDestroy((xnetudppacket*)pValue);
}



/* Future 持有的数据报错误包析构过程。 */
static void __xrtNetUdpErrorPacketFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpErrorPacketDestroy((xnetudperrorpacket*)pValue);
}



/* Future 持有的批量结果析构过程。 */
static void __xrtNetUdpBatchFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtNetUdpBatchDestroy((xnetudpbatch*)pValue);
}

/* One allocation, three real owners: source list/constructor, the Future
 * producer slot, and cancellation Watch.Data. Active is only a call borrow.
 * Result slots are enumerated once; Next/FinishNext never invent RC edges. */
static void __xrtNetUdpWaitCancel(ptr pData);
static void __xrtNetUdpWaitEnter(__xrt_net_udp_wait* pWait)
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
static void __xrtNetUdpWaitLeave(__xrt_net_udp_wait* pWait)
{
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	if (xrtRefRelease(&pWait->Active) < 0) abort();
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
}
static bool __xrtNetUdpWaitHold(const void* pData)
{
	__xrt_net_udp_wait* pWait = (__xrt_net_udp_wait*)pData;
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) return false;
	bool bHeld = !pWait->Cleared && xrtRefRetain(&pWait->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	return bHeld;
}
static void __xrtNetUdpWaitDrop(const void* pData)
{
	__xrt_net_udp_wait* pWait = (__xrt_net_udp_wait*)pData;
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	xnetworker* pWorker = NULL;
	if (xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) && !__xrtAtomicRefLoad(&pWait->Active)) {
		/* Keep normal small-node recycling; retained collector pins must not
		 * keep an operational Engine alive after the waiter is inert. */
		if (__xrtAtomicRefCompareExchange(&pWait->References, 0, 1) == 1) {
			pWorker = xrtAtomicPtrExchange(&pWait->CacheWorker, NULL, XMEMORY_ACQ_REL);
			if (!xrtOwnershipScopeEnd(&Scope)) abort();
			if (pWorker) __xrtNetWorkerNodeRecycleHeld(pWorker, pWait, sizeof(*pWait));
			else xrtFree(pWait);
			return;
		}
		pWorker = xrtAtomicPtrExchange(&pWait->CacheWorker, NULL, XMEMORY_ACQ_REL);
	}
	int32 iLeft = xrtRefRelease(&pWait->References);
	if (iLeft < 0) abort();
	/* A non-final owner must not read the allocation after its release. */
	if (!iLeft && (!xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) || pWait->Publishing || pWait->Linked ||
		pWait->Next || pWait->FinishNext || pWait->Promise || pWait->Watch || pWait->Udp ||
		pWait->Packet || pWait->ErrorPacket || pWait->Batch || __xrtAtomicRefLoad(&pWait->Active) ||
		xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE))) abort();
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	if (!iLeft) xrtFree(pWait);
	if (pWorker) __xrtNetEngineObjectRelease(pWorker->Engine); /* final Worker access */
}
static bool __xrtNetUdpWaitBusy(const void* pData)
{
	const __xrt_net_udp_wait* pWait = pData;
	if (!pWait || pWait->Cleared) return false;
	if (pWait->Publishing || __xrtAtomicRefLoad(&pWait->Active)) return true;
	if (xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE)) return false;
	return xrtNetUdpOwnershipQuiescenceV1Get()->Busy(pWait->Udp);
}
static bool __xrtNetUdpWaitCount(const void* pData, size_t* pCount)
{
	const __xrt_net_udp_wait* pWait = pData;
	if (!pWait || !pCount || pWait->Cleared || __xrtNetUdpWaitBusy(pData)) return false;
	bool bDone = xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) != 0;
	if (bDone && (pWait->Promise || pWait->Watch || pWait->Udp || pWait->Linked || pWait->Next || pWait->FinishNext ||
		pWait->Packet || pWait->ErrorPacket || pWait->Batch || xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE))) return false;
	if (!bDone && (!pWait->Linked || !pWait->Promise || !pWait->Watch || !pWait->Udp || pWait->FinishNext ||
		pWait->Packet || pWait->ErrorPacket || pWait->Result != __XRT_NET_UDP_WAIT_PENDING ||
		(pWait->Batch && (!pWait->Receive || pWait->Wait != XNET_UDP_WAIT_RECEIVE || pWait->Batch->Count ||
			!pWait->Batch->Capacity || pWait->Batch->Capacity > XRT_NET_UDP_FUTURE_BATCH_MAX)))) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pWait->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetUdpWaitTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const __xrt_net_udp_wait* pWait = pData; size_t iCount;
	if (!pVisit || !__xrtNetUdpWaitCount(pData, &iCount)) return false;
	if (pWait->Udp && !pVisit(xrtNetUdpOwnership(pWait->Udp), pContext)) return false;
	xnetworker* pWorker = xrtAtomicPtrLoad(&pWait->CacheWorker, XMEMORY_ACQUIRE);
	if (pWorker && !pVisit(xrtNetEngineOwnership(pWorker->Engine), pContext)) return false;
	if (pWait->Promise && !pVisit(xrtPromiseOwnership(pWait->Promise), pContext)) return false;
	if (pWait->Watch && !pVisit(xrtCancelWatchOwnership(pWait->Watch), pContext)) return false;
	if (pWait->Packet && !pVisit(xrtNetUdpPacketOwnership(pWait->Packet), pContext)) return false;
	if (pWait->ErrorPacket && !pVisit(xrtNetUdpErrorPacketOwnership(pWait->ErrorPacket), pContext)) return false;
	return !pWait->Batch || pVisit(xrtNetUdpBatchOwnership(pWait->Batch), pContext);
}
static const xrtownershipops __xrtNetUdpWaitOps = {__xrtNetUdpWaitCount, __xrtNetUdpWaitTrace};
static xrtownershipref __xrtNetUdpWaitOwnership(const __xrt_net_udp_wait* pWait)
{ return (xrtownershipref){pWait, pWait ? &__xrtNetUdpWaitOps : NULL}; }
static void __xrtNetUdpWaitCancelOwned(ptr pData)
{
	__xrt_net_udp_wait* pWait = pData;
	__xrtNetUdpWaitEnter(pWait); __xrtNetUdpWaitCancel(pWait); __xrtNetUdpWaitLeave(pWait);
}
static const xfutureproducerownershipv1 __xrtNetUdpWaitProducer = {sizeof(xfutureproducerownershipv1), __xrtNetUdpWaitDrop};
static const xcancelwatchownershipv1 __xrtNetUdpWaitCancelPolicy = {
	sizeof(xcancelwatchownershipv1), __xrtNetUdpWaitCancelOwned, __xrtNetUdpWaitDrop, &__xrtNetUdpWaitOps};
static bool __xrtNetUdpPacketPayloadTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ return pValue && !pData && pVisit && pVisit(xrtNetUdpPacketOwnership(pValue), pContext); }
static bool __xrtNetUdpErrorPayloadTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ return pValue && !pData && pVisit && pVisit(xrtNetUdpErrorPacketOwnership(pValue), pContext); }
static bool __xrtNetUdpBatchPayloadTrace(const void* pValue, const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ return pValue && !pData && pVisit && pVisit(xrtNetUdpBatchOwnership(pValue), pContext); }
static const xfuturepayloadownershipv1 __xrtNetUdpPacketPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetUdpPacketFree, __xrtNetUdpPacketPayloadTrace};
static const xfuturepayloadownershipv1 __xrtNetUdpErrorPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetUdpErrorPacketFree, __xrtNetUdpErrorPayloadTrace};
static const xfuturepayloadownershipv1 __xrtNetUdpBatchPayload = {
	sizeof(xfuturepayloadownershipv1), __xrtNetUdpBatchFree, __xrtNetUdpBatchPayloadTrace};
XRT_API const xfutureproducerownershipv1* xrtNetUdpFutureProducerPolicyV1Get(void) { return &__xrtNetUdpWaitProducer; }
XRT_API const xcancelwatchownershipv1* xrtNetUdpFutureCancelPolicyV1Get(void) { return &__xrtNetUdpWaitCancelPolicy; }
XRT_API const xfuturepayloadownershipv1* xrtNetUdpFuturePacketPayloadPolicyV1Get(void) { return &__xrtNetUdpPacketPayload; }
XRT_API const xfuturepayloadownershipv1* xrtNetUdpFutureErrorPayloadPolicyV1Get(void) { return &__xrtNetUdpErrorPayload; }
XRT_API const xfuturepayloadownershipv1* xrtNetUdpFutureBatchPayloadPolicyV1Get(void) { return &__xrtNetUdpBatchPayload; }
XRT_API const xrtownershipquiescencev1* xrtNetUdpFutureOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetUdpWaitOps, __xrtNetUdpWaitBusy};
	return &Policy;
}
static bool __xrtNetUdpWaitClaim(const void* pData, const void* pToken)
{
	__xrt_net_udp_wait* pWait = (__xrt_net_udp_wait*)pData; size_t iCount;
	if (!pToken || (pWait->Claim && pWait->Claim != pToken) || !__xrtNetUdpWaitCount(pData, &iCount)) return false;
	pWait->Claim = pToken; return true;
}
static void __xrtNetUdpWaitRestore(const void* pData, const void* pToken)
{
	__xrt_net_udp_wait* pWait = (__xrt_net_udp_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	pWait->Claim = NULL;
}
static bool __xrtNetUdpWaitReady(const void* pData)
{
	const __xrt_net_udp_wait* pWait = pData; size_t iCount;
	return __xrtNetUdpWaitCount(pData, &iCount) && xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE);
}
static xrtownershipprepareresult __xrtNetUdpWaitPrepare(const void* pData, const void* pToken)
{
	const __xrt_net_udp_wait* pWait = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared) abort();
	bool bReady = __xrtNetUdpWaitReady(pData);
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	/* The actual UDP source closes and publishes terminal results. A producer
	 * adapter never fabricates completion or cancels accepted work on Drop. */
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetUdpWaitClear(const void* pData, const void* pToken)
{
	__xrt_net_udp_wait* pWait = (__xrt_net_udp_wait*)pData;
	if (!pToken || pWait->Claim != pToken || pWait->Cleared || !__xrtNetUdpWaitReady(pData)) abort();
	pWait->Cleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetUdpFutureOwnershipAdapterV1(xrtownershipref Reference,
	const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetUdpWaitHold, __xrtNetUdpWaitDrop,
		__xrtNetUdpWaitClaim, __xrtNetUdpWaitRestore, NULL, __xrtNetUdpWaitClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetUdpWaitReady, __xrtNetUdpWaitPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetUdpWaitOps || !__xrtNetUdpWaitCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}
static bool __xrtNetUdpWaitSetup(__xrt_net_udp_wait* pWait, xfuture** ppFuture)
{
	pWait->Promise = xrtPromiseCreate(ppFuture, NULL);
	if (!pWait->Promise || !__xrtNetUdpWaitHold(pWait)) return false;
	if (!xrtPromiseProducerBindTakeV1(pWait->Promise, __xrtNetUdpWaitOwnership(pWait), &__xrtNetUdpWaitProducer)) {
		__xrtNetUdpWaitDrop(pWait); return false;
	}
	xcancel* pCancel = xrtPromiseCancelToken(pWait->Promise);
	if (!pCancel) return false;
	if (!__xrtNetUdpWaitHold(pWait)) { xrtCancelDestroy(pCancel); return false; }
	pWait->Watch = xrtCancelWatchOwnedV1(pCancel, pWait, &__xrtNetUdpWaitCancelPolicy);
	xrtCancelDestroy(pCancel);
	if (!pWait->Watch) { __xrtNetUdpWaitDrop(pWait); return false; }
	return true;
}
static void __xrtNetUdpWaitRetire(__xrt_net_udp_wait* pWait)
{
	xrtownershipscope Scope = {0};
	if (!xrtOwnershipMutationBegin(&Scope)) abort();
	if (pWait->Promise || pWait->Watch || pWait->Linked || pWait->Next || pWait->FinishNext ||
		pWait->Packet || pWait->ErrorPacket || pWait->Batch || xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE)) abort();
	xnetudp* pUdp = pWait->Udp; pWait->Udp = NULL;
	pWait->Publishing = false;
	xrtAtomic32Store(&pWait->Done, 1, XMEMORY_RELEASE);
	if (!xrtOwnershipScopeEnd(&Scope)) abort();
	xrtNetUdpDestroy(pUdp); __xrtNetUdpWaitLeave(pWait); __xrtNetUdpWaitDrop(pWait);
}



/* 从等待链表移除指定节点；调用方持有 ReceiveLock。 */
static bool __xrtNetUdpWaitRemove(
	xnetudp* pUdp,
	__xrt_net_udp_wait* pWaiter
)
{
	__xrt_net_udp_wait** ppHead;
	__xrt_net_udp_wait** ppTail;
	__xrt_net_udp_wait** ppCurrent;
	__xrt_net_udp_wait* pPrevious = NULL;

	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		ppHead = &pUdp->Errors->WaitHead;
		ppTail = &pUdp->Errors->WaitTail;
	} else if ( pWaiter->Receive ) {
		ppHead = &pUdp->ReceiveFutureHead;
		ppTail = &pUdp->ReceiveFutureTail;
	} else {
		ppHead = &pUdp->WaitHead;
		ppTail = &pUdp->WaitTail;
	}
	ppCurrent = ppHead;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( *ppTail == pWaiter ) {
		*ppTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		(void)xrtAtomic64FetchSub(
			&pUdp->Errors->ErrorWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( pWaiter->Receive ) {
		(void)xrtAtomic64FetchSub(
			&pUdp->ReceiveWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		pUdp->Errors->ReadyWaiters--;
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		pUdp->ReceiveReadyWaiters--;
	}
	return true;
}



/* 按用途把等待节点挂入条件链或消费式接收链；调用方持有 ReceiveLock。 */
static void __xrtNetUdpWaitAppendLocked(
	xnetudp* pUdp,
	__xrt_net_udp_wait* pWaiter
)
{
	__xrt_net_udp_wait** ppHead;
	__xrt_net_udp_wait** ppTail;

	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		ppHead = &pUdp->Errors->WaitHead;
		ppTail = &pUdp->Errors->WaitTail;
	} else if ( pWaiter->Receive ) {
		ppHead = &pUdp->ReceiveFutureHead;
		ppTail = &pUdp->ReceiveFutureTail;
	} else {
		ppHead = &pUdp->WaitHead;
		ppTail = &pUdp->WaitTail;
	}

	pWaiter->Linked = true;
	if ( *ppTail != NULL ) {
		(*ppTail)->Next = pWaiter;
	} else {
		*ppHead = pWaiter;
	}
	*ppTail = pWaiter;
	if ( pWaiter->Receive && (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		(void)xrtAtomic64FetchAdd(
			&pUdp->Errors->ErrorWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( pWaiter->Receive ) {
		(void)xrtAtomic64FetchAdd(
			&pUdp->ReceiveWaiters,
			1,
			XMEMORY_ACQ_REL
		);
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ) {
		pUdp->Errors->ReadyWaiters++;
	} else if ( !pWaiter->Writable &&
		 (pWaiter->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		pUdp->ReceiveReadyWaiters++;
	}
}



/* 从拉取队列取走一个数据包；调用方持有 ReceiveLock。 */
static xnetudppacket* __xrtNetUdpTakeReceiveLocked(xnetudp* pUdp)
{
	xnetudppacket* pPacket = pUdp->ReceiveHead;

	if ( pPacket == NULL ) {
		return NULL;
	}
	pUdp->ReceiveHead = pPacket->Next;
	if ( pUdp->ReceiveHead == NULL ) {
		pUdp->ReceiveTail = NULL;
	}
	pPacket->Next = NULL;
	(void)xrtAtomic64FetchSub(
		&pUdp->ReceiveQueued,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchSub(
		&pUdp->ReceiveQueuedBytes,
		(uint64)pPacket->Size,
		XMEMORY_ACQ_REL
	);
	return pPacket;
}



/* 把完成节点压入锁外终结链。 */
static void __xrtNetUdpWaitFinishPush(
	__xrt_net_udp_wait** ppHead,
	__xrt_net_udp_wait* pWaiter,
	__xrt_net_udp_wait_result Result
)
{
	pWaiter->Result = Result;
	pWaiter->FinishNext = *ppHead;
	*ppHead = pWaiter;
}



/* 调用方持有 ReceiveLock 时，按 FIFO 为拉取 Future 分配排队数据包。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairReceiveLocked(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;

	if ( xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}

	while ( (pUdp->ReceiveHead != NULL) &&
		 (pUdp->ReceiveFutureHead != NULL) ) {
		__xrt_net_udp_wait* pWaiter = pUdp->ReceiveFutureHead;

		(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
		if ( pWaiter->Batch != NULL ) {
			while ( (pWaiter->Batch->Count < pWaiter->Batch->Capacity) &&
				 (pUdp->ReceiveHead != NULL) ) {
				xrtAtomicPtrStore(&pWaiter->Batch->Packets[pWaiter->Batch->Count++],
					__xrtNetUdpTakeReceiveLocked(pUdp), XMEMORY_RELEASE);
			}
		} else {
			pWaiter->Packet = __xrtNetUdpTakeReceiveLocked(pUdp);
		}
		__xrtNetUdpWaitFinishPush(
			&pFinished,
			pWaiter,
			__XRT_NET_UDP_WAIT_RECEIVE
		);
	}
	return pFinished;
}



/* 调用方持有 ReceiveLock 时，按 FIFO 为拉取 Future 分配排队错误。 */
__xrt_net_udp_wait* __xrtNetUdpFuturePairErrorLocked(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;
	__xrt_net_udp_error_state* pState = pUdp->Errors;

	if ( (pState == NULL) || xrtAtomic32Load(
		&pUdp->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return NULL;
	}
	while ( (pState->Head != NULL) && (pState->WaitHead != NULL) ) {
		__xrt_net_udp_wait* pWaiter = pState->WaitHead;

		(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
		pWaiter->ErrorPacket = __xrtNetUdpTakeErrorLocked(pUdp);
		__xrtNetUdpWaitFinishPush(
			&pFinished,
			pWaiter,
			__XRT_NET_UDP_WAIT_ERROR
		);
	}
	return pFinished;
}



/* 返回 UDP 终止原因对应的统一 Future 结果。 */
static __xrt_net_udp_wait_result __xrtNetUdpWaitTerminal(
	const xnetudp* pUdp
)
{
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pUdp->CloseResult == XNET_RESULT_CANCELLED ) {
		return __XRT_NET_UDP_WAIT_CANCELLED;
	}
	if ( (pUdp->Error != NULL) ||
		 (pUdp->CloseResult == XNET_RESULT_ERROR) ||
		 (pUdp->CloseResult == XNET_RESULT_TIMEOUT) ) {
		return __XRT_NET_UDP_WAIT_FAILED;
	}
	return __XRT_NET_UDP_WAIT_CLOSED;
}



/* 调用方持有 ReceiveLock 时判断一个等待条件。 */
static __xrt_net_udp_wait_result __xrtNetUdpWaitResult(
	const __xrt_net_udp_wait* pWaiter
)
{
	const xnetudp* pUdp = pWaiter->Udp;
	xnetudpstate State = xrtNetUdpState(pUdp);
	uint64 iQueuedBytes = xrtAtomic64Load(
		&pUdp->QueuedBytes,
		XMEMORY_ACQUIRE
	);
	uint64 iQueuedPackets = xrtAtomic64Load(
		&pUdp->QueuedPackets,
		XMEMORY_ACQUIRE
	);
	bool bClosing = (State >= XNET_UDP_CLOSING) ||
		(xrtAtomic32Load(
			&pUdp->CloseGate,
			XMEMORY_ACQUIRE
		) != 0);
	bool bAborting = xrtAtomic32Load(
		&pUdp->AbortGate,
		XMEMORY_ACQUIRE
	) != 0;

	if ( pWaiter->Writable ) {
		if ( (State == XNET_UDP_OPEN) && !bClosing &&
			 (iQueuedPackets < (uint64)pUdp->Config.SendPacketLimit) &&
			 ((uint64)pWaiter->RequiredBytes <=
			  ((uint64)pUdp->Config.SendLimit - iQueuedBytes)) ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State >= XNET_UDP_CLOSING ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_OPEN ) {
		if ( (State == XNET_UDP_OPEN) && !bClosing ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_RECEIVE ) {
		if ( !bClosing && (pUdp->ReceiveHead != NULL) ) {
			return pWaiter->Receive ?
				__XRT_NET_UDP_WAIT_RECEIVE :
				__XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_ERROR ) {
		if ( !bClosing && (pUdp->Errors != NULL) &&
			 (pUdp->Errors->Head != NULL) ) {
			return pWaiter->Receive ?
				__XRT_NET_UDP_WAIT_ERROR :
				__XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_DRAIN ) {
		if ( bAborting ) {
			return State == XNET_UDP_CLOSED ?
				__xrtNetUdpWaitTerminal(pUdp) :
				__XRT_NET_UDP_WAIT_PENDING;
		}
		if ( iQueuedPackets == 0 ) {
			return __XRT_NET_UDP_WAIT_READY;
		}
		if ( State == XNET_UDP_CLOSED ) {
			return __xrtNetUdpWaitTerminal(pUdp);
		}
		return __XRT_NET_UDP_WAIT_PENDING;
	}
	if ( State == XNET_UDP_CLOSED ) {
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitTerminal(pUdp);

		return Result == __XRT_NET_UDP_WAIT_CLOSED ?
			__XRT_NET_UDP_WAIT_READY : Result;
	}
	return __XRT_NET_UDP_WAIT_PENDING;
}



/* 为没有结构化原因的异常终止补充稳定错误。 */
static xerror* __xrtNetUdpWaitError(const __xrt_net_udp_wait* pWaiter)
{
	const xnetudp* pUdp = pWaiter->Udp;
	int32 iCode;
	cstr sMessage;

	if ( pUdp->Error != NULL ) {
		return xrtErrorRef(pUdp->Error);
	}
	if ( pWaiter->Wait == XNET_UDP_WAIT_OPEN ) {
		iCode = XNET_ERROR_UDP_CREATE;
		sMessage = "UDP open wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_RECEIVE ) {
		iCode = XNET_ERROR_UDP_RECEIVE;
		sMessage = "UDP receive wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_ERROR ) {
		iCode = XNET_ERROR_UDP_RECEIVE;
		sMessage = "UDP datagram error wait failed";
	} else if ( pWaiter->Wait == XNET_UDP_WAIT_DRAIN ||
		 pWaiter->Writable ) {
		iCode = XNET_ERROR_UDP_SEND;
		sMessage = "UDP send wait failed";
	} else {
		iCode = XNET_ERROR_UDP_CLOSE;
		sMessage = "UDP close wait failed";
	}
	return xrtErrorCreate(
		pUdp->CloseResult == XNET_RESULT_TIMEOUT ? XERR_TIMEOUT : XERR_IO,
		"xrt.net",
		iCode,
		sMessage
	);
}



/* 终结一个已经从等待链取出的节点并释放全部持有资源。 */
static void __xrtNetUdpWaitFinish(__xrt_net_udp_wait* pWaiter)
{
	xnetudp* pUdp = pWaiter->Udp;
	xpromise* pPromise = pWaiter->Promise;
	xnetudppacket* pPacket = pWaiter->Packet;
	xnetudperrorpacket* pErrorPacket = pWaiter->ErrorPacket;
	xnetudpbatch* pBatch = pWaiter->Batch;
	__xrt_net_udp_wait_result Result = pWaiter->Result;
	xerror* pError = NULL;

	__xrtNetUdpWaitEnter(pWaiter);
	if (!__xrtNetUdpOwnershipEnter(pUdp, false)) abort();
	if ( Result == __XRT_NET_UDP_WAIT_FAILED ) {
		pError = __xrtNetUdpWaitError(pWaiter);
	}
	pWaiter->Packet = NULL;
	pWaiter->ErrorPacket = NULL;
	pWaiter->Batch = NULL;
	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	pWaiter->Promise = NULL;
	/* A terminal result must not leave an operational cache lease behind
	 * after callers are allowed to retire a closed Engine. The hot Worker
	 * receive path can still recycle normally: the same Worker cannot close
	 * its source before this finish returns. Foreign cancellation/completion
	 * and closing sources detach the cache lease BEFORE publishing a result.
	 * Real producer/Watch/list references keep the allocation itself alive. */
	xnetworker* pCacheWorker = xrtAtomicPtrLoad(&pWaiter->CacheWorker, XMEMORY_ACQUIRE);
	if (pCacheWorker && (xrtNetUdpState(pUdp) >= XNET_UDP_CLOSING || !xrtNetWorkerIsCurrent(pCacheWorker))) {
		pCacheWorker = xrtAtomicPtrExchange(&pWaiter->CacheWorker, NULL, XMEMORY_ACQ_REL);
		if (pCacheWorker) __xrtNetEngineObjectRelease(pCacheWorker->Engine);
	}
	__xrtNetUdpOwnershipLeave(pUdp);
	if ( Result == __XRT_NET_UDP_WAIT_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_NET_UDP_WAIT_RECEIVE ) {
		if ( pBatch != NULL ) {
			if ( xrtPromiseResolveOwnedPolicyV1(
				pPromise,
				pBatch,
				&__xrtNetUdpBatchPayload
			) ) {
				pBatch = NULL;
			}
		} else {
			if ( xrtPromiseResolveOwnedPolicyV1(
				pPromise,
				pPacket,
				&__xrtNetUdpPacketPayload
			) ) {
				pPacket = NULL;
			}
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_ERROR ) {
		if ( xrtPromiseResolveOwnedPolicyV1(
			pPromise,
			pErrorPacket,
			&__xrtNetUdpErrorPayload
		) ) {
			pErrorPacket = NULL;
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_FAILED ) {
		if ( pError != NULL ) {
			(void)xrtPromiseReject(pPromise, pError);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_NET_UDP_WAIT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtNetUdpPacketDestroy(pPacket);
	xrtNetUdpErrorPacketDestroy(pErrorPacket);
	xrtNetUdpBatchDestroy(pBatch);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
	__xrtNetUdpWaitRetire(pWaiter);
}



/* 完成一条已经在线性化点取出的 Future 链。 */
void __xrtNetUdpFutureFinishList(__xrt_net_udp_wait* pWaiter)
{
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->FinishNext;

		pWaiter->FinishNext = NULL;
		__xrtNetUdpWaitFinish(pWaiter);
		pWaiter = pNext;
	}
}



static void __xrtNetUdpWaitTask(xnetworker* pWorker, ptr pData);



/* 用 UDP 内嵌命令无分配地唤醒所属 Worker；调用方持有 ReceiveLock。 */
static void __xrtNetUdpWaitScheduleLocked(xnetudp* pUdp)
{
	bool bErrorWait = (pUdp->Errors != NULL) &&
		(pUdp->Errors->WaitHead != NULL);

	if ( pUdp->WaitClosed || pUdp->WaitPosted ||
		 ((pUdp->WaitHead == NULL) &&
		  (pUdp->ReceiveFutureHead == NULL) &&
		  !bErrorWait) ) {
		return;
	}
	pUdp->WaitPosted = true;
	(void)xrtNetUdpRef(pUdp);
	if ( !__xrtNetEnginePostInternal(
		pUdp->Worker,
		&pUdp->WaitCommand,
		__xrtNetUdpWaitTask,
		pUdp
	) ) {
		pUdp->WaitPosted = false;
		xrtNetUdpDestroy(pUdp);
	}
}



/* 推进所有当前可满足或已经终止的 UDP Future。 */
static void __xrtNetUdpFutureNotifyImpl(xnetudp* pUdp)
{
	__xrt_net_udp_wait* pFinished = NULL;
	__xrt_net_udp_wait* pErrorFinished = NULL;
	__xrt_net_udp_wait* pWaiter;

	if ( !pUdp->ReceiveLockReady ) {
		return;
	}
	__xrtSpinLock(&pUdp->ReceiveLock);
	/* A terminal shell can outlive Engine workers. WaitClosed is published
	 * under this lock before CLOSED, so do not dereference that old Worker. */
	if ( !pUdp->WaitClosed && !xrtNetWorkerIsCurrent(pUdp->Worker) ) {
		__xrtNetUdpWaitScheduleLocked(pUdp);
		__xrtSpinUnlock(&pUdp->ReceiveLock);
		return;
	}
	pFinished = __xrtNetUdpFuturePairReceiveLocked(pUdp);
	pErrorFinished = __xrtNetUdpFuturePairErrorLocked(pUdp);
	pWaiter = pUdp->WaitHead;
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->Next;
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitResult(pWaiter);

		if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
			(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
			__xrtNetUdpWaitFinishPush(&pFinished, pWaiter, Result);
		}
		pWaiter = pNext;
	}
	pWaiter = pUdp->ReceiveFutureHead;
	while ( pWaiter != NULL ) {
		__xrt_net_udp_wait* pNext = pWaiter->Next;
		__xrt_net_udp_wait_result Result = __xrtNetUdpWaitResult(pWaiter);

		if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
			(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
			__xrtNetUdpWaitFinishPush(&pFinished, pWaiter, Result);
		}
		pWaiter = pNext;
	}
	if ( pUdp->Errors != NULL ) {
		pWaiter = pUdp->Errors->WaitHead;
		while ( pWaiter != NULL ) {
			__xrt_net_udp_wait* pNext = pWaiter->Next;
			__xrt_net_udp_wait_result Result =
				__xrtNetUdpWaitResult(pWaiter);

			if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
				(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
				__xrtNetUdpWaitFinishPush(
					&pErrorFinished,
					pWaiter,
					Result
				);
			}
			pWaiter = pNext;
		}
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	__xrtNetUdpFutureFinishList(pFinished);
	__xrtNetUdpFutureFinishList(pErrorFinished);
}

void __xrtNetUdpFutureNotify(xnetudp* pUdp)
{
	if (!__xrtNetUdpOwnershipEnter(pUdp, false)) abort();
	__xrtNetUdpFutureNotifyImpl(pUdp);
	__xrtNetUdpOwnershipLeave(pUdp);
}



/* 内嵌命令进入 Worker 后允许下一次条件变化再次投递。 */
static void __xrtNetUdpWaitTask(xnetworker* pWorker, ptr pData)
{
	xnetudp* pUdp = (xnetudp*)pData;

	(void)pWorker;
	__xrtNetUdpActiveEnter(pUdp);
	__xrtSpinLock(&pUdp->ReceiveLock);
	pUdp->WaitPosted = false;
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	__xrtNetUdpFutureNotify(pUdp);
	__xrtNetUdpActiveLeave(pUdp);
	xrtNetUdpDestroy(pUdp);
}



/* 取消只移除当前等待，不关闭 UDP 或丢弃已经排队的数据包。 */
static void __xrtNetUdpWaitCancel(ptr pData)
{
	__xrt_net_udp_wait* pWaiter = (__xrt_net_udp_wait*)pData;
	xnetudp* pUdp = pWaiter->Udp;
	bool bRemoved;

	if (!__xrtNetUdpOwnershipEnter(pUdp, false)) abort();
	__xrtSpinLock(&pUdp->ReceiveLock);
	bRemoved = pWaiter->Linked && __xrtNetUdpWaitRemove(pUdp, pWaiter);
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	if ( bRemoved ) {
		pWaiter->Result = __XRT_NET_UDP_WAIT_CANCELLED;
		__xrtNetUdpFutureNotify(pUdp);
	}
	__xrtNetUdpOwnershipLeave(pUdp);
	if (bRemoved) __xrtNetUdpFutureFinishList(pWaiter);
}



/* 判断拉取消费 Future 和可读条件 Future 是否发生不清晰的混用。 */
static bool __xrtNetUdpWaitModeConflictLocked(
	const xnetudp* pUdp,
	const __xrt_net_udp_wait* pCandidate
)
{
	if ( pCandidate->Receive &&
		 (pCandidate->Wait == XNET_UDP_WAIT_ERROR) ) {
		return pUdp->Errors->ReadyWaiters != 0;
	} else if ( pCandidate->Receive ) {
		return pUdp->ReceiveReadyWaiters != 0;
	} else if ( !pCandidate->Writable &&
		 (pCandidate->Wait == XNET_UDP_WAIT_ERROR) ) {
		return xrtAtomic64Load(
			&pUdp->Errors->ErrorWaiters,
			XMEMORY_RELAXED
		) != 0;
	} else if ( !pCandidate->Writable &&
		 (pCandidate->Wait == XNET_UDP_WAIT_RECEIVE) ) {
		return xrtAtomic64Load(
			&pUdp->ReceiveWaiters,
			XMEMORY_RELAXED
		) != 0;
	}
	return false;
}



/* 清理尚未挂入 UDP 的等待节点，同时保留调用方稍后设置的诊断。 */
static void __xrtNetUdpWaitDiscard(
	__xrt_net_udp_wait* pWaiter,
	xfuture* pFuture
)
{
	xerror* pError = xrtTakeError();
	xcancelwatch* pWatch = pWaiter->Watch; pWaiter->Watch = NULL;
	xpromise* pPromise = pWaiter->Promise; pWaiter->Promise = NULL;
	xnetudpbatch* pBatch = pWaiter->Batch; pWaiter->Batch = NULL;
	xrtCancelUnwatch(pWatch); xrtPromiseDestroy(pPromise); xrtFutureDestroy(pFuture);
	xrtNetUdpBatchDestroy(pBatch); __xrtNetUdpWaitRetire(pWaiter);
	xrtClearError(); xrtSetErrorTake(pError);
}



/* 建立 Promise、取消监听和 UDP 等待节点。 */
static xfuture* __xrtNetUdpWaitCreateImpl(
	xnetudp* pUdp,
	xnetudpwait Wait,
	bool bReceive,
	bool bWritable,
	size_t iValue
)
{
	__xrt_net_udp_wait* pWaiter;
	__xrt_net_udp_wait* pFinished = NULL;
	xnetudp* pOwned;
	xfuture* pFuture = NULL;
	xnetworker* pCacheWorker = NULL;
	bool bConflict;

	if ( (pUdp == NULL) || (Wait < XNET_UDP_WAIT_OPEN) ||
		 (Wait > XNET_UDP_WAIT_CLOSE) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (Wait == XNET_UDP_WAIT_RECEIVE) &&
		 (pUdp->Events.Receive != NULL) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"wait-udp-receive",
			"UDP receive Futures require pull mode without a Receive callback"
		);
		return NULL;
	}
	if ( (Wait == XNET_UDP_WAIT_ERROR) &&
		 ((pUdp->Errors == NULL) ||
		  (pUdp->Errors->Callback != NULL)) ) {
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			"wait-udp-error",
			"UDP error Futures require pulled datagram errors"
		);
		return NULL;
	}
	if ( bWritable && ((iValue > XNET_UDP_PAYLOAD_MAX) ||
		 (iValue > pUdp->Config.SendLimit)) ) {
		__xrtNetUdpSetError(
			XERR_RANGE,
			XNET_ERROR_UDP_SEND,
			"wait-udp-writable",
			"requested UDP datagram can never fit the configured send limit"
		);
		return NULL;
	}
	pOwned = xrtNetUdpRef(pUdp);
	if ( pOwned == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pOwned->ReceiveLock);
	if ( !pOwned->WaitClosed ) {
		pCacheWorker = pOwned->Worker;
		if ( !__xrtNetEngineObjectHold(pOwned->Engine) ) {
			__xrtSpinUnlock(&pOwned->ReceiveLock);
			xrtNetUdpDestroy(pOwned);
			return NULL;
		}
	}
	__xrtSpinUnlock(&pOwned->ReceiveLock);
	pWaiter = __xrtNetUdpWaitAlloc(pCacheWorker);
	if ( pWaiter == NULL ) {
		xrtNetUdpDestroy(pOwned);
		return NULL;
	}
	pWaiter->Udp = pOwned;
	if ( bReceive && (iValue != 0) ) {
		pWaiter->Batch = __xrtNetUdpBatchCreate(iValue);
		if ( pWaiter->Batch == NULL ) {
			__xrtNetUdpWaitDiscard(pWaiter, NULL);
			return NULL;
		}
	}
	pWaiter->Wait = Wait;
	pWaiter->Receive = bReceive;
	pWaiter->Writable = bWritable;
	pWaiter->RequiredBytes = bWritable ? iValue : 0;
	if (!__xrtNetUdpWaitSetup(pWaiter, &pFuture)) {
		__xrtNetUdpWaitDiscard(pWaiter, pFuture); return NULL;
	}
	xrtownershipscope Publish = {0}; if (!xrtOwnershipMutationBegin(&Publish)) abort();
	__xrtSpinLock(&pUdp->ReceiveLock);
	bConflict = __xrtNetUdpWaitModeConflictLocked(pUdp, pWaiter);
	if ( !bConflict ) {
		__xrtNetUdpWaitAppendLocked(pUdp, pWaiter);
		if ( pWaiter->Receive ) {
			pFinished = (pWaiter->Wait == XNET_UDP_WAIT_ERROR) ?
				__xrtNetUdpFuturePairErrorLocked(pUdp) :
				__xrtNetUdpFuturePairReceiveLocked(pUdp);
		}
		if ( pWaiter->Linked ) {
			__xrt_net_udp_wait_result Result =
				__xrtNetUdpWaitResult(pWaiter);

			if ( Result != __XRT_NET_UDP_WAIT_PENDING ) {
				(void)__xrtNetUdpWaitRemove(pUdp, pWaiter);
				__xrtNetUdpWaitFinishPush(
					&pFinished,
					pWaiter,
					Result
				);
			} else {
				__xrtNetUdpWaitScheduleLocked(pUdp);
			}
		}
	}
	if (!bConflict) {
		pWaiter->Publishing = false;
		if (xrtRefRelease(&pWaiter->Active) != 0) abort();
	}
	__xrtSpinUnlock(&pUdp->ReceiveLock);
	if (!xrtOwnershipScopeEnd(&Publish)) abort();
	if ( bConflict ) {
		bool bErrorWait = Wait == XNET_UDP_WAIT_ERROR;

		__xrtNetUdpWaitDiscard(pWaiter, pFuture);
		__xrtNetUdpSetError(
			XERR_STATE,
			XNET_ERROR_UDP_RECEIVE_QUEUE,
			bErrorWait ? "wait-udp-error" : "wait-udp-receive",
			bErrorWait ?
				"readiness and consuming UDP error Futures cannot be mixed" :
				"readiness and consuming UDP receive Futures cannot be mixed"
		);
		return NULL;
	}
	__xrtNetUdpFutureFinishList(pFinished);
	return pFuture;
}



static xfuture* __xrtNetUdpWaitCreate(xnetudp* pUdp, xnetudpwait Wait,
    bool bReceive, bool bWritable, size_t iValue)
{
    if (!__xrtNetUdpOwnershipEnter(pUdp, true)) return NULL;
    xfuture* pFuture = __xrtNetUdpWaitCreateImpl(pUdp, Wait, bReceive, bWritable, iValue);
    __xrtNetUdpOwnershipLeave(pUdp); return pFuture;
}

/* 建立一个统一 UDP 条件 Future。 */
XRT_API xfuture* xrtNetUdpWaitAsync(xnetudp* pUdp, xnetudpwait Wait)
{
	return __xrtNetUdpWaitCreate(pUdp, Wait, false, false, 0);
}



/* 建立一个按数据报大小判断预算的可写 Future。 */
XRT_API xfuture* xrtNetUdpWritableAsync(xnetudp* pUdp, size_t iSize)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_DRAIN,
		false,
		true,
		iSize
	);
}



/* 建立一个拉取模式单包接收 Future。 */
XRT_API xfuture* xrtNetUdpReceiveAsync(xnetudp* pUdp)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_RECEIVE,
		true,
		false,
		0
	);
}



/* 建立一个拉取模式数据报错误 Future。 */
XRT_API xfuture* xrtNetUdpReceiveErrorAsync(xnetudp* pUdp)
{
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_ERROR,
		true,
		false,
		0
	);
}



/* 建立一个拉取模式批量接收 Future。 */
XRT_API xfuture* xrtNetUdpReceiveBatchAsync(
	xnetudp* pUdp,
	size_t iCapacity
)
{
	if ( (iCapacity == 0) ||
		 (iCapacity > XRT_NET_UDP_FUTURE_BATCH_MAX) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtNetUdpWaitCreate(
		pUdp,
		XNET_UDP_WAIT_RECEIVE,
		true,
		false,
		iCapacity
	);
}

/* Frozen source chains own exactly the constructor/list reference of every
 * waiter. Children are admitted independently using the published policies.
 * The three lists are disjoint by mode; all observed counters must agree. */
static bool __xrtNetUdpWaitChain(const xnetudp* pUdp, const __xrt_net_udp_wait* pHead,
	const __xrt_net_udp_wait* pTail, unsigned iChain, size_t* pCount, size_t* pReady,
	xrtownershipvisitor pVisit, ptr pContext)
{
	const __xrt_net_udp_wait *pSlow = pHead, *pFast = pHead, *pLast = NULL;
	while (pFast && pFast->Next) {
		pSlow = pSlow->Next; pFast = pFast->Next->Next; if (pSlow == pFast) return false;
	}
	for (const __xrt_net_udp_wait* pWait = pHead; pWait; pWait = pWait->Next) {
		size_t iReferences;
		if (!__xrtNetUdpWaitCount(pWait, &iReferences) || pWait->Udp != pUdp ||
			xrtAtomic32Load(&pWait->Done, XMEMORY_ACQUIRE) || pWait->Wait < XNET_UDP_WAIT_OPEN ||
			pWait->Wait > XNET_UDP_WAIT_CLOSE ||
			(iChain == 0 ? pWait->Receive : (!pWait->Receive || pWait->Writable ||
				pWait->Wait != (iChain == 1 ? XNET_UDP_WAIT_RECEIVE : XNET_UDP_WAIT_ERROR))) ||
			(pWait->Writable && (pWait->Wait != XNET_UDP_WAIT_DRAIN ||
				pWait->RequiredBytes > XNET_UDP_PAYLOAD_MAX || pWait->RequiredBytes > pUdp->Config.SendLimit))) return false;
		if (++*pCount == 0) return false;
		if (iChain == 0 && !pWait->Writable && pWait->Wait == XNET_UDP_WAIT_RECEIVE) ++pReady[0];
		if (iChain == 0 && !pWait->Writable && pWait->Wait == XNET_UDP_WAIT_ERROR) ++pReady[1];
		if (pVisit && !pVisit(__xrtNetUdpWaitOwnership(pWait), pContext)) return false;
		pLast = pWait;
	}
	return pLast == pTail;
}
bool __xrtNetUdpWaitOwnershipWalk(const xnetudp* pUdp, xrtownershipvisitor pVisit, ptr pContext)
{
	if (pUdp->WaitPosted) return false;
	size_t iConditions = 0, iReceive = 0, iErrors = 0, iReady[2] = {0, 0};
	if (!__xrtNetUdpWaitChain(pUdp, pUdp->WaitHead, pUdp->WaitTail, 0, &iConditions, iReady, pVisit, pContext) ||
		!__xrtNetUdpWaitChain(pUdp, pUdp->ReceiveFutureHead, pUdp->ReceiveFutureTail, 1, &iReceive, iReady, pVisit, pContext) ||
		iReady[0] != pUdp->ReceiveReadyWaiters || iReceive != xrtAtomic64Load(&pUdp->ReceiveWaiters, XMEMORY_ACQUIRE) ||
		(iReceive && iReady[0])) return false;
	if (pUdp->Errors) {
		if (!__xrtNetUdpWaitChain(pUdp, pUdp->Errors->WaitHead, pUdp->Errors->WaitTail, 2, &iErrors, iReady, pVisit, pContext) ||
			iReady[1] != pUdp->Errors->ReadyWaiters || iErrors != xrtAtomic64Load(&pUdp->Errors->ErrorWaiters, XMEMORY_ACQUIRE) ||
			(iErrors && iReady[1])) return false;
	} else if (iReady[1]) return false;
	return true;
}

#endif
