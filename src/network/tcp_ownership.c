#include "../internal/xrt_tcp.h"

#if defined(XRT_FEATURE_NET_TCP)

/* Native calls retain their ordinary caller/waiter reference. Active records
 * the borrow, not a fabricated strong reference, and excludes snapshots for
 * the complete metadata transition without holding a lock across callbacks. */
static bool __xrtNetTcpEnter(xatomic32* pActive, xatomic32* pState,
	xatomicptr* pClaim, bool bSubmit)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	uint32 iActive = xrtAtomic32Load(pActive, XMEMORY_ACQUIRE);
	bool bEntered = false;
	if (!(xrtAtomic32Load(pState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED) &&
		(!bSubmit || !xrtAtomicPtrLoad(pClaim, XMEMORY_ACQUIRE))) {
		while (iActive < UINT32_MAX) {
			uint32 iExpected = iActive;
			if (xrtAtomic32CompareExchange(pActive, &iExpected, iActive + 1,
				XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) { bEntered = true; break; }
			iActive = iExpected;
		}
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bEntered) __xrtErrorSetInvalidState();
	return bEntered;
}
static void __xrtNetTcpLeave(xatomic32* pActive)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!xrtAtomic32FetchSub(pActive, 1, XMEMORY_ACQ_REL)) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
bool __xrtNetStreamOwnershipEnter(xnetstream* pStream, bool bSubmit)
{
	if (!pStream) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtNetTcpEnter(&pStream->OwnershipActive, &pStream->OwnershipState,
		&pStream->OwnershipClaim, bSubmit);
}
void __xrtNetStreamOwnershipLeave(xnetstream* pStream)
{ __xrtNetTcpLeave(&pStream->OwnershipActive); }
bool __xrtNetListenerOwnershipEnter(xnetlistener* pListener, bool bSubmit)
{
	if (!pListener) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtNetTcpEnter(&pListener->OwnershipActive, &pListener->OwnershipState,
		&pListener->OwnershipClaim, bSubmit);
}
void __xrtNetListenerOwnershipLeave(xnetlistener* pListener)
{ __xrtNetTcpLeave(&pListener->OwnershipActive); }

static bool __xrtNetStreamPrepared(const void* pData)
{
	const xnetstream* pStream = pData;
	return (xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_READY) &&
		!xrtAtomic32Load(&pStream->OwnershipActive, XMEMORY_ACQUIRE);
}
static bool __xrtNetListenerPrepared(const void* pData)
{
	const xnetlistener* pListener = pData;
	return (xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_READY) &&
		!xrtAtomic32Load(&pListener->OwnershipActive, XMEMORY_ACQUIRE);
}
static bool __xrtNetStreamOwnershipBusy(const void* pData)
{
	const xnetstream* pStream = pData;
	if (!pStream) return false;
	if (xrtAtomic32Load(&pStream->OwnershipActive, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_READY) return false;
	return xrtNetEngineOwnershipQuiescenceV1Get()->Busy(pStream->Engine) ||
		xrtAtomic32Load(&pStream->SendSubmitters, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pStream->SendCommands, XMEMORY_ACQUIRE);
}
static bool __xrtNetListenerOwnershipBusy(const void* pData)
{
	const xnetlistener* pListener = pData;
	if (!pListener) return false;
	if (xrtAtomic32Load(&pListener->OwnershipActive, XMEMORY_ACQUIRE)) return true;
	if (xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_READY) return false;
	return xrtNetEngineOwnershipQuiescenceV1Get()->Busy(pListener->Engine);
}
static bool __xrtNetTcpEventsEmpty(const xnetstreamevents* pEvents)
{
	return !pEvents->Open && !pEvents->Read && !pEvents->End && !pEvents->HighWater &&
		!pEvents->LowWater && !pEvents->Drain && !pEvents->Close;
}
static bool __xrtNetStreamCount(const void* pData, size_t* pCount)
{
	const xnetstream* pStream = pData;
	if (!pStream || !pCount || __xrtNetStreamOwnershipBusy(pData) ||
		(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED) ||
		!__xrtNetTcpEventsEmpty(&pStream->Events) || xrtAtomicPtrLoad(&pStream->Data, XMEMORY_ACQUIRE) ||
		pStream->ReadBuffer.Head || pStream->WriteBuffer.Head || pStream->ActiveDepth || pStream->StartPending ||
		xrtAtomic32Load(&pStream->ControlRequests, XMEMORY_ACQUIRE)) return false;
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		if (pStream->Control.Open || pStream->Control.Published || pStream->Control.Close ||
			pStream->Control.Release || pStream->Control.Data) return false;
	#endif
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		if (pStream->WaitHead || pStream->WaitTail || pStream->WaitPosted || pStream->ReadWaiters) return false;
	#endif
	if (__xrtNetStreamPrepared(pData)) {
		if (pStream->RuntimeHeld || pStream->EngineHeld || pStream->RuntimeOwner.Data || pStream->RuntimeOwner.Previous ||
			pStream->Socket || pStream->BuffersReady || xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) return false;
	} else if (!pStream->RuntimeHeld || !pStream->EngineHeld || pStream->RuntimeOwner.Data != pStream ||
		pStream->RuntimeOwner.Engine != pStream->Engine || !pStream->RuntimeOwner.Previous ||
		pStream->RuntimeOwner.Policy != xrtNetStreamRuntimeOwnershipV1Get()) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pStream->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetStreamTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetstream* pStream = pData; size_t iCount;
	if (!pVisit || !__xrtNetStreamCount(pData, &iCount) ||
		(pStream->EngineHeld && !pVisit(xrtNetEngineOwnership(pStream->Engine), pContext))) return false;
	/* Diagnostics are real immutable Error/Cause owners too. Static errors
	 * have an empty view; ordinary errors must not become unreported roots.
	 * This DAG cannot point back to a stream and remains owned until last Drop,
	 * just like Error's own immutable Cause tail after Clear. */
	xrtownershipref Error = xrtErrorOwnership(pStream->Error);
	return !Error.Data || pVisit(Error, pContext);
}
const xrtownershipops __xrtNetStreamOwnershipOps = {__xrtNetStreamCount, __xrtNetStreamTrace};
static bool __xrtNetListenerCount(const void* pData, size_t* pCount)
{
	const xnetlistener* pListener = pData;
	if (!pListener || !pCount || __xrtNetListenerOwnershipBusy(pData) ||
		(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED) ||
		pListener->Data || pListener->Events.Accept || pListener->Events.Error || pListener->Events.Close ||
		!__xrtNetTcpEventsEmpty(&pListener->StreamEvents) || pListener->StartPending ||
		xrtAtomic32Load(&pListener->ActiveDispatches, XMEMORY_ACQUIRE)) return false;
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		if (pListener->WaitHead || pListener->WaitTail || xrtAtomic32Load(&pListener->AcceptWaiters, XMEMORY_ACQUIRE)) return false;
	#endif
	size_t iQueued = xrtAtomic32Load(&pListener->QueuedAccepts, XMEMORY_ACQUIRE), iSeen = 0;
	const xnetstream* pTail = NULL;
	for (const xnetstream* pStream = pListener->AcceptHead; pStream; pStream = pStream->AcceptNext) {
		if (++iSeen > iQueued || __xrtAtomicRefLoad(&pStream->References) <= 0) return false;
		pTail = pStream;
	}
	if (iSeen != iQueued || pTail != pListener->AcceptTail) return false;
	if (__xrtNetListenerPrepared(pData)) {
		if (pListener->RuntimeHeld || pListener->EngineHeld || pListener->RuntimeOwner.Data || pListener->RuntimeOwner.Previous ||
			pListener->Socket || iQueued || xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED) return false;
	} else if (!pListener->RuntimeHeld || !pListener->EngineHeld || pListener->RuntimeOwner.Data != pListener ||
		pListener->RuntimeOwner.Engine != pListener->Engine || !pListener->RuntimeOwner.Previous ||
		pListener->RuntimeOwner.Policy != xrtNetListenerRuntimeOwnershipV1Get()) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pListener->References);
	if (iReferences <= 0) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetListenerTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetlistener* pListener = pData; size_t iCount;
	if (!pVisit || !__xrtNetListenerCount(pData, &iCount) ||
		(pListener->EngineHeld && !pVisit(xrtNetEngineOwnership(pListener->Engine), pContext))) return false;
	/* Each queue slot owns ONE caller reference. AcceptNext is a structural
	 * link between those slots, never a second strong edge between streams. */
	for (const xnetstream* pStream = pListener->AcceptHead; pStream; pStream = pStream->AcceptNext)
		if (!pVisit(xrtNetStreamOwnership(pStream), pContext)) return false;
	return true;
}
const xrtownershipops __xrtNetListenerOwnershipOps = {__xrtNetListenerCount, __xrtNetListenerTrace};
XRT_API xrtownershipref xrtNetStreamOwnership(const xnetstream* pStream)
{ return (xrtownershipref){pStream, pStream ? &__xrtNetStreamOwnershipOps : NULL}; }
XRT_API xrtownershipref xrtNetListenerOwnership(const xnetlistener* pListener)
{ return (xrtownershipref){pListener, pListener ? &__xrtNetListenerOwnershipOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtNetStreamOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetStreamOwnershipOps, __xrtNetStreamOwnershipBusy};
	return &Policy;
}
XRT_API const xrtownershipquiescencev1* xrtNetListenerOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetListenerOwnershipOps, __xrtNetListenerOwnershipBusy};
	return &Policy;
}
static bool __xrtNetStreamHold(const void* pData)
{
	xnetstream* pStream = (xnetstream*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bHeld = !(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED) &&
		xrtRefRetain(&pStream->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static void __xrtNetStreamDrop(const void* pData)
{ xrtNetStreamDestroy((xnetstream*)pData); }
static bool __xrtNetListenerHold(const void* pData)
{
	xnetlistener* pListener = (xnetlistener*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bHeld = !(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED) &&
		xrtRefRetain(&pListener->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static void __xrtNetListenerDrop(const void* pData)
{ xrtNetListenerDestroy((xnetlistener*)pData); }
static bool __xrtNetStreamClaim(const void* pData, const void* pToken)
{
	xnetstream* pStream = (xnetstream*)pData; size_t iCount;
	ptr pOld = xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!pToken || (pOld && pOld != pToken) || !__xrtNetStreamCount(pData, &iCount)) return false;
	xrtAtomicPtrStore(&pStream->OwnershipClaim, (ptr)pToken, XMEMORY_RELEASE); return true;
}
static bool __xrtNetListenerClaim(const void* pData, const void* pToken)
{
	xnetlistener* pListener = (xnetlistener*)pData; size_t iCount;
	ptr pOld = xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE);
	if (!pToken || (pOld && pOld != pToken) || !__xrtNetListenerCount(pData, &iCount)) return false;
	xrtAtomicPtrStore(&pListener->OwnershipClaim, (ptr)pToken, XMEMORY_RELEASE); return true;
}
static void __xrtNetStreamRestore(const void* pData, const void* pToken)
{
	xnetstream* pStream = (xnetstream*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtAtomicPtrStore(&pStream->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static void __xrtNetListenerRestore(const void* pData, const void* pToken)
{
	xnetlistener* pListener = (xnetlistener*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtAtomicPtrStore(&pListener->OwnershipClaim, NULL, XMEMORY_RELEASE);
}
static xrtownershipprepareresult __xrtNetStreamPrepare(const void* pData, const void* pToken)
{
	xnetstream* pStream = (xnetstream*)pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtownershipprepareresult Result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtNetStreamPrepared(pData)) Result = XRT_OWNERSHIP_PREPARE_READY;
	else if (!__xrtNetStreamOwnershipBusy(pData) && !__xrtNetStreamOwnershipStop(pStream)) Result = XRT_OWNERSHIP_PREPARE_FAILED;
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return Result;
}
static xrtownershipprepareresult __xrtNetListenerPrepare(const void* pData, const void* pToken)
{
	xnetlistener* pListener = (xnetlistener*)pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtownershipprepareresult Result = XRT_OWNERSHIP_PREPARE_BUSY;
	if (__xrtNetListenerPrepared(pData)) Result = XRT_OWNERSHIP_PREPARE_READY;
	else if (!__xrtNetListenerOwnershipBusy(pData) && !__xrtNetListenerOwnershipStop(pListener)) Result = XRT_OWNERSHIP_PREPARE_FAILED;
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return Result;
}
static void __xrtNetStreamClear(const void* pData, const void* pToken)
{
	xnetstream* pStream = (xnetstream*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pStream->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		!__xrtNetStreamPrepared(pData) ||
		(xrtAtomic32Load(&pStream->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtAtomic32Store(&pStream->OwnershipState, XRT_NET_TCP_OWNERSHIP_READY | XRT_NET_TCP_OWNERSHIP_CLEARED, XMEMORY_RELEASE);
}
static void __xrtNetListenerClear(const void* pData, const void* pToken)
{
	xnetlistener* pListener = (xnetlistener*)pData;
	if (!pToken || xrtAtomicPtrLoad(&pListener->OwnershipClaim, XMEMORY_ACQUIRE) != pToken ||
		!__xrtNetListenerPrepared(pData) ||
		(xrtAtomic32Load(&pListener->OwnershipState, XMEMORY_ACQUIRE) & XRT_NET_TCP_OWNERSHIP_CLEARED)) abort();
	xrtAtomic32Store(&pListener->OwnershipState, XRT_NET_TCP_OWNERSHIP_READY | XRT_NET_TCP_OWNERSHIP_CLEARED, XMEMORY_RELEASE);
}
XRT_API const xrtownershipadapterv1* xrtNetStreamOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetStreamHold, __xrtNetStreamDrop,
		__xrtNetStreamClaim, __xrtNetStreamRestore, NULL, __xrtNetStreamClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtNetStreamPrepared, __xrtNetStreamPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetStreamOwnershipOps || !__xrtNetStreamCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}
XRT_API const xrtownershipadapterv1* xrtNetListenerOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetListenerHold, __xrtNetListenerDrop,
		__xrtNetListenerClaim, __xrtNetListenerRestore, NULL, __xrtNetListenerClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtNetListenerPrepared, __xrtNetListenerPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetListenerOwnershipOps || !__xrtNetListenerCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
