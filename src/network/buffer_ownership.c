#include "../internal/xrt_net_buffer.h"

#if defined(XRT_FEATURE_NET_BUFFER)

/* These counters are independent: References is physical creator/lease/pin
 * ownership, Leases is outstanding block release work, Active is only a call
 * borrow. Pool payload operations remain thread-affine as before. */
static bool __xrtNetPoolActiveAdd(xnetbufpool* pPool)
{
	int32 iActive = __xrtAtomicRefLoad(&pPool->OwnershipActive);
	while (iActive < INT32_MAX) {
		int32 iPrevious = __xrtAtomicRefCompareExchange(&pPool->OwnershipActive, iActive + 1, iActive);
		if (iPrevious == iActive) return true;
		iActive = iPrevious;
	}
	return false;
}
bool __xrtNetBufPoolEnter(xnetbufpool* pPool, bool bTerminal)
{
	if (!pPool) return true;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bEntered = pPool->CreatorOwned && !pPool->Retired && !pPool->OwnershipCleared &&
		(bTerminal || !pPool->OwnershipClaim) && __xrtNetPoolActiveAdd(pPool);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bEntered) __xrtErrorSetInvalidState();
	return bEntered;
}
void __xrtNetBufPoolLeave(xnetbufpool* pPool)
{
	if (!pPool) return;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (xrtRefRelease(&pPool->OwnershipActive) < 0) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
bool __xrtNetBufPoolAcquire(xnetbufpool* pPool)
{
	if (!pPool) return true;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bEntered = pPool->CreatorOwned && !pPool->Retired && !pPool->OwnershipCleared &&
		!pPool->OwnershipClaim && __xrtNetPoolActiveAdd(pPool);
	bool bHeld = bEntered && pPool->Leases < SIZE_MAX && xrtRefRetain(&pPool->References) >= 0;
	#if defined(XRT_FEATURE_NET_ENGINE)
		if (bHeld && !pPool->Leases && pPool->ParentEngine) {
			if (!__xrtNetEnginePoolRetain(pPool->ParentEngine)) {
				if (xrtRefRelease(&pPool->References) <= 0) abort();
				bHeld = false;
			} else pPool->ParentHeld = true;
		}
	#endif
	if (bHeld) ++pPool->Leases;
	else if (bEntered && xrtRefRelease(&pPool->OwnershipActive) < 0) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (!bHeld) { if (bEntered) __xrtErrorSetSizeOverflow(); else __xrtErrorSetInvalidState(); }
	return bHeld;
}
void __xrtNetBufPoolDrop(const void* pData)
{
	xnetbufpool* pPool = (xnetbufpool*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	int32 iReferences = xrtRefRelease(&pPool->References);
	if (iReferences < 0) abort();
	if (!iReferences) {
		if (pPool->CreatorOwned || !pPool->Retired || pPool->Leases || pPool->Info.LiveBlocks ||
			pPool->Info.CachedBlocks || __xrtAtomicRefLoad(&pPool->OwnershipActive)) abort();
		#if defined(XRT_FEATURE_NET_ENGINE)
			if (pPool->ParentHeld || pPool->ParentEngine) abort();
		#endif
		xrtFree(pPool);
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
void __xrtNetBufPoolRelease(xnetbufpool* pPool)
{
	if (!pPool) return;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (!pPool->Leases || !pPool->CreatorOwned) abort();
	--pPool->Leases;
	#if defined(XRT_FEATURE_NET_ENGINE)
		xnetengine* pParent = NULL;
		if (!pPool->Leases && pPool->ParentHeld) {
			pParent = pPool->ParentEngine; pPool->ParentHeld = false;
		}
	#endif
	if (xrtRefRelease(&pPool->OwnershipActive) < 0 || xrtRefRelease(&pPool->References) <= 0) abort();
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* Last pool access precedes the parent release: that release can allow a
	 * concurrent Engine retirement to consume the creator and free this pool. */
	#if defined(XRT_FEATURE_NET_ENGINE)
		if (pParent) __xrtNetEnginePoolRelease(pParent);
	#endif
}

static bool __xrtNetPoolBusy(const void* pData)
{
	const xnetbufpool* pPool = pData;
	if (!pPool || pPool->Retired) return false;
	if (__xrtAtomicRefLoad(&pPool->OwnershipActive)) return true;
	#if defined(XRT_FEATURE_NET_ENGINE)
		if (pPool->ParentEngine) return xrtNetEngineOwnershipQuiescenceV1Get()->Busy(pPool->ParentEngine);
	#endif
	return false;
}
static bool __xrtNetPoolCount(const void* pData, size_t* pCount)
{
	const xnetbufpool* pPool = pData;
	if (!pPool || !pCount || __xrtNetPoolBusy(pData) || pPool->OwnershipCleared ||
		pPool->Leases != pPool->Info.LiveBlocks || pPool->Leases > (size_t)INT32_MAX ||
		(!pPool->CreatorOwned && !pPool->Retired)) return false;
	#if defined(XRT_FEATURE_NET_ENGINE)
		if ((pPool->ParentHeld && (!pPool->ParentEngine || !pPool->Leases)) ||
			(pPool->ParentEngine && pPool->Leases && !pPool->ParentHeld)) return false;
	#endif
	int32 iReferences = __xrtAtomicRefLoad(&pPool->References);
	if (iReferences <= 0 || (size_t)iReferences < pPool->Leases + (pPool->CreatorOwned ? 1u : 0u)) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetPoolTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xnetbufpool* pPool = pData; size_t iCount;
	if (!pVisit || !__xrtNetPoolCount(pData, &iCount)) return false;
	#if defined(XRT_FEATURE_NET_ENGINE)
		if (pPool->ParentHeld) return pVisit(xrtNetEngineOwnership(pPool->ParentEngine), pContext);
	#else
		(void)pPool; (void)pContext;
	#endif
	return true;
}
static const xrtownershipops __xrtNetPoolOps = {__xrtNetPoolCount, __xrtNetPoolTrace};
XRT_API xrtownershipref xrtNetBufPoolOwnership(const xnetbufpool* pPool)
{ return (xrtownershipref){pPool, pPool ? &__xrtNetPoolOps : NULL}; }
XRT_API const xrtownershipquiescencev1* xrtNetBufPoolOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtNetPoolOps, __xrtNetPoolBusy};
	return &Policy;
}
static bool __xrtNetPoolHold(const void* pData)
{
	xnetbufpool* pPool = (xnetbufpool*)pData; xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bHeld = !pPool->OwnershipCleared && xrtRefRetain(&pPool->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static bool __xrtNetPoolClaim(const void* pData, const void* pToken)
{
	xnetbufpool* pPool = (xnetbufpool*)pData; size_t iCount;
	if (!pToken || (pPool->OwnershipClaim && pPool->OwnershipClaim != pToken) || !__xrtNetPoolCount(pData, &iCount)) return false;
	pPool->OwnershipClaim = pToken; return true;
}
static void __xrtNetPoolRestore(const void* pData, const void* pToken)
{
	xnetbufpool* pPool = (xnetbufpool*)pData;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared) abort();
	pPool->OwnershipClaim = NULL;
}
static bool __xrtNetPoolReady(const void* pData)
{
	const xnetbufpool* pPool = pData;
	return !__xrtNetPoolBusy(pData) && !pPool->Leases && !pPool->Info.LiveBlocks;
}
static xrtownershipprepareresult __xrtNetPoolPrepare(const void* pData, const void* pToken)
{
	const xnetbufpool* pPool = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared) abort();
	bool bReady = __xrtNetPoolReady(pData);
	if (!xrtOwnershipScopeEnd(&Freeze)) abort();
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtNetPoolClear(const void* pData, const void* pToken)
{
	xnetbufpool* pPool = (xnetbufpool*)pData;
	if (!pToken || pPool->OwnershipClaim != pToken || pPool->OwnershipCleared || !__xrtNetPoolReady(pData)) abort();
	pPool->OwnershipCleared = true;
}
XRT_API const xrtownershipadapterv1* xrtNetBufPoolOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetPoolHold, __xrtNetBufPoolDrop,
		__xrtNetPoolClaim, __xrtNetPoolRestore, NULL, __xrtNetPoolClear, NULL};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter, __xrtNetPoolReady, __xrtNetPoolPrepare};
	size_t iCount;
	if (!ppPreparation || Reference.Ops != &__xrtNetPoolOps || !__xrtNetPoolCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

static bool __xrtNetBytesCount(const void* pData, size_t* pCount)
{
	const xnetbytes* pBytes = pData;
	if (!pBytes || !pCount) return false;
	int32 iReferences = __xrtAtomicRefLoad(&pBytes->References);
	if (iReferences <= 0 || pBytes->View.Data != pBytes->Data) return false;
	*pCount = (size_t)iReferences; return true;
}
static bool __xrtNetBytesTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{ size_t iCount; (void)pContext; return pVisit && __xrtNetBytesCount(pData, &iCount); }
static const xrtownershipops __xrtNetBytesOps = {__xrtNetBytesCount, __xrtNetBytesTrace};
XRT_API xrtownershipref xrtNetBytesOwnership(const xnetbytes* pBytes)
{ return (xrtownershipref){pBytes, pBytes ? &__xrtNetBytesOps : NULL}; }
static bool __xrtNetBytesHold(const void* pData)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) return false;
	bool bHeld = xrtRefRetain(&((xnetbytes*)pData)->References) >= 0;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	return bHeld;
}
static void __xrtNetBytesDrop(const void* pData) { xrtNetBytesDestroy((xnetbytes*)pData); }
static bool __xrtNetBytesClaim(const void* pData, const void* pToken) { return pData && pToken; }
static void __xrtNetBytesKeep(const void* pData, const void* pToken) { (void)pData; (void)pToken; }
XRT_API const xrtownershipadapterv1* xrtNetBytesOwnershipAdapterV1(xrtownershipref Reference)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtNetBytesHold, __xrtNetBytesDrop,
		__xrtNetBytesClaim, __xrtNetBytesKeep, NULL, __xrtNetBytesKeep, NULL};
	size_t iCount;
	return Reference.Ops == &__xrtNetBytesOps && __xrtNetBytesCount(Reference.Data, &iCount) ? &Adapter : NULL;
}

#endif
