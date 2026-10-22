#include "../internal/xrt_udp.h"

#if defined(XRT_FEATURE_NET_UDP)

/* This short metadata gate orders call borrows against final runtime/Engine
 * detachment. It never spans user callbacks, I/O, allocation or a native wait.
 * Mutation is entered BEFORE the gate, including nested collector cursors. */
bool __xrtNetUdpOwnershipEnter(xnetudp* pUdp, bool bSubmit)
{
	if (!pUdp) { __xrtErrorSetInvalidArgument(); return false; }
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	__xrtSpinLock(&pUdp->OwnershipGate);
	uint32 iActive = xrtAtomic32Load(&pUdp->OwnershipActive, XMEMORY_RELAXED);
	bool bAllowed = !(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_CLEARED) &&
		(!bSubmit || !xrtAtomicPtrLoad(&pUdp->OwnershipClaim, XMEMORY_ACQUIRE)) && iActive < UINT32_MAX;
	if (bAllowed) xrtAtomic32Store(&pUdp->OwnershipActive, iActive + 1, XMEMORY_RELEASE);
	__xrtSpinUnlock(&pUdp->OwnershipGate);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bAllowed) __xrtErrorSetInvalidState();
	return bAllowed;
}

static bool __xrtNetUdpPrepared(const void* pData)
{
	const xnetudp* pUdp = pData;
	return pUdp && !xrtAtomic32Load(&pUdp->OwnershipActive, XMEMORY_ACQUIRE) &&
		(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_READY);
}
static bool __xrtNetUdpBusy(const void* pData)
{
	const xnetudp* pUdp = pData;
	if (!pUdp) return false;
	if (xrtAtomic32Load(&pUdp->OwnershipActive, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pUdp->OwnershipStop, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_READY) return false;
	return xrtNetEngineOwnershipQuiescenceV1Get()->Busy(pUdp->Engine) ||
		xrtAtomic32Load(&pUdp->SendSubmitters, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pUdp->SendCommands, XMEMORY_ACQUIRE);
}
static bool __xrtNetUdpEventsEmpty(const xnetudp* pUdp)
{
	const __xrt_net_udp_events* e = &pUdp->Events;
	return !e->Open && !e->Receive && !e->Error && !e->HighWater && !e->LowWater && !e->Drain && !e->Close &&
		!xrtAtomicPtrLoad(&pUdp->Data, XMEMORY_ACQUIRE) && (!pUdp->Errors || !pUdp->Errors->Callback);
}
static bool __xrtNetUdpQueues(const xnetudp* pUdp, xrtownershipvisitor pVisit, ptr pContext)
{
	size_t iCount = 0, iBytes = 0; const xnetudppacket* pTail = NULL;
	uint64 iExpected = xrtAtomic64Load(&pUdp->ReceiveQueued, XMEMORY_ACQUIRE);
	const xnetudppacket *pSlow = pUdp->ReceiveHead, *pFast = pSlow;
	while (pFast && pFast->Next) {
		pSlow = pSlow->Next; pFast = pFast->Next->Next; if (pSlow == pFast) return false;
	}
	for (const xnetudppacket* p = pUdp->ReceiveHead; p; p = p->Next) {
		if (++iCount > iExpected || p->Size > SIZE_MAX - iBytes) return false;
		iBytes += p->Size; pTail = p;
		if (pVisit && !pVisit(xrtNetUdpPacketOwnership(p), pContext)) return false;
	}
	if (iCount != iExpected || iBytes != xrtAtomic64Load(&pUdp->ReceiveQueuedBytes, XMEMORY_ACQUIRE) ||
		pTail != pUdp->ReceiveTail) return false;
	if (pUdp->Errors) {
		const __xrt_net_udp_error_state* e = pUdp->Errors;
		const xnetudperrorpacket* pLast = NULL; iCount = iBytes = 0;
		iExpected = xrtAtomic64Load(&e->Queued, XMEMORY_ACQUIRE);
		const xnetudperrorpacket *pErrorSlow = e->Head, *pErrorFast = pErrorSlow;
		while (pErrorFast && pErrorFast->Next) {
			pErrorSlow = pErrorSlow->Next; pErrorFast = pErrorFast->Next->Next; if (pErrorSlow == pErrorFast) return false;
		}
		for (const xnetudperrorpacket* p = e->Head; p; p = p->Next) {
			if (++iCount > iExpected || p->Size > SIZE_MAX - iBytes) return false;
			iBytes += p->Size; pLast = p;
			if (pVisit && !pVisit(xrtNetUdpErrorPacketOwnership(p), pContext)) return false;
		}
		if (iCount != iExpected || iBytes != xrtAtomic64Load(&e->QueuedBytes, XMEMORY_ACQUIRE) || pLast != e->Tail) return false;
	}
	return true;
}
static bool __xrtNetUdpContents(const xnetudp* pUdp, xrtownershipvisitor pVisit, ptr pContext)
{
	/* Pending UDP Future nodes still need their own real RC/Watch protocol.
 * Reject them explicitly; never certify opaque callback memory as a leaf. */
	#if defined(XRT_FEATURE_NET_UDP_FUTURE)
		if (pUdp->WaitHead || pUdp->WaitTail || pUdp->ReceiveFutureHead || pUdp->ReceiveFutureTail || pUdp->WaitPosted ||
			pUdp->ReceiveReadyWaiters || xrtAtomic64Load(&pUdp->ReceiveWaiters, XMEMORY_ACQUIRE) ||
			(pUdp->Errors && (pUdp->Errors->WaitHead || pUdp->Errors->WaitTail || pUdp->Errors->ReadyWaiters ||
				xrtAtomic64Load(&pUdp->Errors->ErrorWaiters, XMEMORY_ACQUIRE)))) return false;
	#endif
	return __xrtNetUdpQueues(pUdp, pVisit, pContext) && __xrtNetUdpReceivesOwnership(pUdp, pVisit, pContext) &&
		__xrtNetUdpSendsOwnership(pUdp, pVisit, pContext);
}
static bool __xrtNetUdpCount(const void* pData, size_t* pCount)
{
	const xnetudp* pUdp = pData;
	if (!pUdp || !pCount || __xrtNetUdpBusy(pData) || pUdp->ActiveDepth || pUdp->SocketExposed ||
		xrtAtomic32Load(&pUdp->OwnershipStop, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pUdp->ControlRequests, XMEMORY_ACQUIRE) ||
		(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_CLEARED) ||
		!__xrtNetUdpEventsEmpty(pUdp) || !__xrtNetUdpContents(pUdp, NULL, NULL)) return false;
	if (__xrtNetUdpPrepared(pData)) {
		if (pUdp->RuntimeHeld || pUdp->EngineHeld || pUdp->EngineStorageHeld || pUdp->RuntimeOwner.Data || pUdp->RuntimeOwner.Previous ||
			pUdp->Socket || pUdp->WatchPending || pUdp->SendHead || xrtNetUdpState(pUdp) != XNET_UDP_CLOSED) return false;
	} else if (!pUdp->RuntimeHeld || (pUdp->EngineHeld == pUdp->EngineStorageHeld) || pUdp->RuntimeOwner.Data != pUdp ||
		pUdp->RuntimeOwner.Engine != pUdp->Engine || !pUdp->RuntimeOwner.Previous ||
		pUdp->RuntimeOwner.Policy != xrtNetUdpRuntimeOwnershipV1Get()) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pUdp->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetUdpTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetudp* pUdp = pData; size_t iCount;
	if (!pVisit || !__xrtNetUdpCount(pData, &iCount) ||
		((pUdp->EngineHeld || pUdp->EngineStorageHeld) && !pVisit(xrtNetEngineOwnership(pUdp->Engine), pContext)) ||
		!__xrtNetUdpContents(pUdp, pVisit, pContext)) return false;
	xrtownershipref Error = xrtErrorOwnership(pUdp->Error);
	return !Error.Data || pVisit(Error, pContext);
}
const xrtownershipops __xrtNetUdpOwnershipOps = {__xrtNetUdpCount, __xrtNetUdpTrace};
XRT_API xrtownershipref xrtNetUdpOwnership(const xnetudp* pUdp)
{ return (xrtownershipref){pUdp, pUdp ? &__xrtNetUdpOwnershipOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtNetUdpOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetUdpOwnershipOps, __xrtNetUdpBusy};
	return &Policy;
}
static bool __xrtNetUdpHold(const void* pData) { return xrtNetUdpRef((xnetudp*)pData) != NULL; }
static void __xrtNetUdpDrop(const void* pData) { xrtNetUdpDestroy((xnetudp*)pData); }
static bool __xrtNetUdpClaim(const void* pData, const void* pToken)
{
	xnetudp* pUdp = (xnetudp*)pData; size_t iCount;
	ptr pOld = xrtAtomicPtrLoad(&pUdp->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!pToken || (pOld && pOld != pToken) || !__xrtNetUdpCount(pData, &iCount)) return false;
	xrtAtomicPtrStore(&pUdp->OwnershipClaim, (ptr)pToken, XMEMORY_RELEASE); return true;
}
static void __xrtNetUdpRestore(const void* pData, const void* pToken)
{
	xnetudp* pUdp = (xnetudp*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pUdp->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_CLEARED)) abort();
	xrtAtomicPtrStore(&pUdp->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static xrtownershipprepareresult __xrtNetUdpPrepare(const void* pData, const void* pToken)
{
	xnetudp* pUdp = (xnetudp*)pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || xrtAtomicPtrLoad(&pUdp->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_CLEARED)) abort();
	xrtownershipprepareresult Result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtNetUdpPrepared(pData)) Result = XRT_OWNERSHIP_PREPARE_READY;
	else if (!__xrtNetUdpBusy(pData) && !__xrtNetUdpOwnershipStop(pUdp)) Result = XRT_OWNERSHIP_PREPARE_FAILED;
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return Result;
}
static void __xrtNetUdpClear(const void* pData, const void* pToken)
{
	xnetudp* pUdp = (xnetudp*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pUdp->OwnershipClaim, XMEMORY_ACQUIRE) != pToken || !__xrtNetUdpPrepared(pData) ||
		(xrtAtomic32Load(&pUdp->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_UDP_OWNERSHIP_CLEARED)) abort();
	xrtAtomic32Store(&pUdp->OwnershipState, XRT_NET_UDP_OWNERSHIP_READY | XRT_NET_UDP_OWNERSHIP_CLEARED, XMEMORY_RELEASE);
}
XRT_API const xrtownershipadapterv1* xrtNetUdpOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetUdpHold, __xrtNetUdpDrop,
		__xrtNetUdpClaim, __xrtNetUdpRestore, NULL, __xrtNetUdpClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetUdpPrepared, __xrtNetUdpPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetUdpOwnershipOps || !__xrtNetUdpCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
