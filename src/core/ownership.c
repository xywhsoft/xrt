#include "../internal/xrt_internal.h"

typedef struct xrt_ownership_node {
	xrtownershipref Reference;
	size_t Incoming;
	size_t StrongCount;
	size_t FirstEdge;
	bool Anchor;
	bool Reachable;
} xrt_ownership_node;
typedef struct xrt_ownership_edge {
	size_t Target;
	size_t Next;
} xrt_ownership_edge;
typedef struct xrt_ownership_scan {
	xrt_ownership_node* Nodes;
	xrt_ownership_edge* Edges;
	size_t* Slots;
	size_t* Work;
	size_t NodeCount, NodeCapacity, EdgeCount, EdgeCapacity, SlotCount;
	size_t Source;
	bool Failed;
	xrtownershipadmitproc Admit;
	ptr AdmitContext;
} xrt_ownership_scan;

struct xrtownershipsnapshot {
	xrt_ownership_node* Nodes;
	size_t NodeCount;
};

static size_t __xrtOwnershipHash(const void* pData)
{
	uintptr_t iValue = (uintptr_t)pData;
	iValue ^= iValue >> 9u;
	iValue *= (uintptr_t)0x9e3779b1u;
	iValue ^= iValue >> 13u;
	return (size_t)iValue;
}

static bool __xrtOwnershipFail(xrt_ownership_scan* pScan)
{
	pScan->Failed = true;
	__xrtErrorSetInvalidState();
	return false;
}

static ptr __xrtOwnershipGrow(ptr pOldItems, size_t* pCapacity, size_t iSize)
{
	size_t iCapacity = *pCapacity == 0 ? 16u : *pCapacity * 2u;
	ptr pItems;
	if (iCapacity < *pCapacity || iCapacity > SIZE_MAX / iSize) {
		__xrtErrorSetSizeOverflow(); return NULL;
	}
	pItems = xrtRealloc(pOldItems, iCapacity * iSize);
	if (pItems == NULL) return NULL;
	*pCapacity = iCapacity;
	return pItems;
}

static bool __xrtOwnershipRehash(xrt_ownership_scan* pScan)
{
	size_t iCount = pScan->SlotCount == 0 ? 32u : pScan->SlotCount * 2u;
	size_t* pSlots;
	if (iCount < pScan->SlotCount || iCount > SIZE_MAX / sizeof(size_t)) {
		__xrtErrorSetSizeOverflow(); return false;
	}
	pSlots = (size_t*)xrtCalloc(iCount, sizeof(size_t));
	if (pSlots == NULL) return false;
	for (size_t i = 0; i < pScan->NodeCount; ++i) {
		size_t iSlot = __xrtOwnershipHash(pScan->Nodes[i].Reference.Data) & (iCount - 1u);
		while (pSlots[iSlot] != 0) iSlot = (iSlot + 1u) & (iCount - 1u);
		pSlots[iSlot] = i + 1u;
	}
	xrtFree(pScan->Slots); pScan->Slots = pSlots; pScan->SlotCount = iCount;
	return true;
}

static bool __xrtOwnershipNode(xrt_ownership_scan* pScan,
	xrtownershipref Reference, size_t* pIndex)
{
	size_t iSlot;
	if (Reference.Data == NULL || Reference.Ops == NULL ||
		Reference.Ops->Count == NULL || Reference.Ops->Trace == NULL)
		return __xrtOwnershipFail(pScan);
	if (pScan->SlotCount == 0 || pScan->NodeCount >= pScan->SlotCount / 2u)
		if (!__xrtOwnershipRehash(pScan)) return false;
	iSlot = __xrtOwnershipHash(Reference.Data) & (pScan->SlotCount - 1u);
	while (pScan->Slots[iSlot] != 0) {
		size_t i = pScan->Slots[iSlot] - 1u;
		if (pScan->Nodes[i].Reference.Data == Reference.Data) {
			if (pScan->Nodes[i].Reference.Ops != Reference.Ops)
				return __xrtOwnershipFail(pScan);
			*pIndex = i; return true;
		}
		iSlot = (iSlot + 1u) & (pScan->SlotCount - 1u);
	}
	/* Never invoke an unadmitted adapter, including an otherwise valid root.
	 * Duplicate identities reuse the same decision and physical descriptor. */
	if (pScan->Admit != NULL && !pScan->Admit(Reference, pScan->AdmitContext))
		return __xrtOwnershipFail(pScan);
	if (pScan->NodeCount == pScan->NodeCapacity) {
		xrt_ownership_node* pNodes = (xrt_ownership_node*)__xrtOwnershipGrow(
			pScan->Nodes, &pScan->NodeCapacity, sizeof(*pScan->Nodes));
		if (pNodes == NULL) return false;
		pScan->Nodes = pNodes;
	}
	*pIndex = pScan->NodeCount++;
	pScan->Nodes[*pIndex] = (xrt_ownership_node){Reference, 0, 0, SIZE_MAX, false, false};
	pScan->Slots[iSlot] = *pIndex + 1u;
	return true;
}

static bool __xrtOwnershipVisit(xrtownershipref Reference, ptr pContext)
{
	xrt_ownership_scan* pScan = (xrt_ownership_scan*)pContext;
	size_t iTarget, iEdge;
	if (pScan->Failed) return false;
	if (Reference.Data == NULL) return true;
	if (!__xrtOwnershipNode(pScan, Reference, &iTarget)) {
		pScan->Failed = true; return false;
	}
	if (pScan->Nodes[iTarget].Incoming == SIZE_MAX) return __xrtOwnershipFail(pScan);
	if (pScan->EdgeCount == pScan->EdgeCapacity) {
		xrt_ownership_edge* pEdges = (xrt_ownership_edge*)__xrtOwnershipGrow(
			pScan->Edges, &pScan->EdgeCapacity, sizeof(*pScan->Edges));
		if (pEdges == NULL) { pScan->Failed = true; return false; }
		pScan->Edges = pEdges;
	}
	iEdge = pScan->EdgeCount++;
	pScan->Edges[iEdge] = (xrt_ownership_edge){iTarget, pScan->Nodes[pScan->Source].FirstEdge};
	pScan->Nodes[pScan->Source].FirstEdge = iEdge;
	++pScan->Nodes[iTarget].Incoming;
	return true;
}

static bool __xrtOwnershipInspect(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	bool* pReachable, xrtownershipresult* pResult,
	xrtownershiprootproc pRootPolicy, ptr pRootContext,
	xrtownershipadmitproc pAdmit, ptr pAdmitContext,
	xrtownershipsnapshot** ppSnapshot)
{
	xrt_ownership_scan Scan = {0};
	xrtownershipresult Result = {0};
	size_t iWork = 0, iNext = 0;
	bool bOk = false;
	Scan.Admit = pAdmit; Scan.AdmitContext = pAdmitContext;
	if (pResult == NULL || (iAnchorCount != 0 && pAnchors == NULL) ||
		(iInternalSlotCount != 0 && pInternalSlots == NULL)) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	for (size_t i = 0; i < iAnchorCount; ++i) {
		size_t iIndex;
		if (pAnchors[i].Data == NULL) continue;
		if (!__xrtOwnershipNode(&Scan, pAnchors[i], &iIndex)) goto cleanup;
		Scan.Nodes[iIndex].Anchor = true;
	}
	for (size_t i = 0; i < iInternalSlotCount; ++i) {
		size_t iIndex;
		if (pInternalSlots[i].Data == NULL) continue;
		if (!__xrtOwnershipNode(&Scan, pInternalSlots[i], &iIndex)) goto cleanup;
		if (Scan.Nodes[iIndex].Incoming == SIZE_MAX || Result.EdgeCount == SIZE_MAX) {
			__xrtOwnershipFail(&Scan); goto cleanup;
		}
		++Scan.Nodes[iIndex].Incoming; ++Result.EdgeCount;
	}
	/* Iterative traversal: one Trace per physical node, with no depth cutoff. */
	for (Scan.Source = 0; Scan.Source < Scan.NodeCount; ++Scan.Source) {
		xrtownershipref Reference = Scan.Nodes[Scan.Source].Reference;
		if (!Reference.Ops->Trace(Reference.Data, __xrtOwnershipVisit, &Scan) || Scan.Failed) {
			if (!Scan.Failed) __xrtOwnershipFail(&Scan);
			goto cleanup;
		}
	}
	if (Scan.EdgeCount > SIZE_MAX - Result.EdgeCount) {
		__xrtOwnershipFail(&Scan); goto cleanup;
	}
	Result.NodeCount = Scan.NodeCount; Result.EdgeCount += Scan.EdgeCount;
	if (Scan.NodeCount != 0) {
		Scan.Work = (size_t*)xrtMalloc(Scan.NodeCount * sizeof(size_t));
		if (Scan.Work == NULL) goto cleanup;
	}
	/* Count after all traces return: adapters may use balanced read cursors. */
	for (size_t i = 0; i < Scan.NodeCount; ++i) {
		size_t iCount;
		xrt_ownership_node* pNode = &Scan.Nodes[i];
		if (!pNode->Reference.Ops->Count(pNode->Reference.Data, &iCount) ||
			iCount < pNode->Incoming || iCount == 0) {
			__xrtOwnershipFail(&Scan); goto cleanup;
		}
		pNode->StrongCount = iCount;
		if (iCount > pNode->Incoming ||
			(pRootPolicy != NULL && pRootPolicy(pNode->Reference, pRootContext))) {
			++Result.ExternalRootCount; pNode->Reachable = true;
			Scan.Work[iWork++] = i;
		}
	}
	while (iNext < iWork) {
		xrt_ownership_node* pNode = &Scan.Nodes[Scan.Work[iNext++]];
		for (size_t iEdge = pNode->FirstEdge; iEdge != SIZE_MAX; iEdge = Scan.Edges[iEdge].Next) {
			size_t iTarget = Scan.Edges[iEdge].Target;
			if (!Scan.Nodes[iTarget].Reachable) {
				Scan.Nodes[iTarget].Reachable = true; Scan.Work[iWork++] = iTarget;
			}
		}
	}
	for (size_t i = 0; i < Scan.NodeCount; ++i)
		if (Scan.Nodes[i].Anchor && Scan.Nodes[i].Reachable) ++Result.ReachableAnchorCount;
	/* Transfer the discovered records only after every fallible trace/count and
	 * reachability computation succeeds. Inspect retains its original allocation
	 * sequence; only the new snapshot path allocates this owning bookkeeping. */
	if (ppSnapshot != NULL) {
		xrtownershipsnapshot* pSnapshot = (xrtownershipsnapshot*)xrtMalloc(sizeof(*pSnapshot));
		if (pSnapshot == NULL) goto cleanup;
		*pSnapshot = (xrtownershipsnapshot){Scan.Nodes, Scan.NodeCount};
		*ppSnapshot = pSnapshot;
		Scan.Nodes = NULL;
		*pResult = Result; bOk = true; goto cleanup;
	}
	/* All fallible work has finished. Resolve existing identities without
	 * calling adapters again or growing the index during output commit. */
	for (size_t i = 0; pReachable != NULL && i < iAnchorCount; ++i) {
		if (pAnchors[i].Data == NULL) pReachable[i] = false;
		else {
			size_t iSlot = __xrtOwnershipHash(pAnchors[i].Data) & (Scan.SlotCount - 1u);
			while (Scan.Nodes[Scan.Slots[iSlot] - 1u].Reference.Data != pAnchors[i].Data)
				iSlot = (iSlot + 1u) & (Scan.SlotCount - 1u);
			pReachable[i] = Scan.Nodes[Scan.Slots[iSlot] - 1u].Reachable;
		}
	}
	*pResult = Result; bOk = true;
cleanup:
	xrtFree(Scan.Work); xrtFree(Scan.Slots); xrtFree(Scan.Edges); xrtFree(Scan.Nodes);
	return bOk;
}

XRT_API bool xrtOwnershipInspect(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	xrtownershipresult* pResult)
{
	return __xrtOwnershipInspect(pAnchors, iAnchorCount, pInternalSlots,
		iInternalSlotCount, NULL, pResult, NULL, NULL, NULL, NULL, NULL);
}

XRT_API bool xrtOwnershipInspectReachable(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	bool* pReachable, xrtownershipresult* pResult,
	xrtownershiprootproc pRootPolicy, ptr pRootContext)
{
	if (iAnchorCount != 0 && pReachable == NULL) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtOwnershipInspect(pAnchors, iAnchorCount, pInternalSlots,
		iInternalSlotCount, pReachable, pResult, pRootPolicy, pRootContext, NULL, NULL, NULL);
}

XRT_API bool xrtOwnershipSnapshotCreate(
	const xrtownershipref* pAnchors, size_t iAnchorCount,
	const xrtownershipref* pInternalSlots, size_t iInternalSlotCount,
	xrtownershipadmitproc pAdmit, ptr pAdmitContext,
	xrtownershipsnapshot** ppSnapshot)
{
	xrtownershipresult Result;
	if (ppSnapshot == NULL) { __xrtErrorSetInvalidArgument(); return false; }
	return __xrtOwnershipInspect(pAnchors, iAnchorCount, pInternalSlots,
		iInternalSlotCount, NULL, &Result, NULL, NULL, pAdmit, pAdmitContext, ppSnapshot);
}

XRT_API size_t xrtOwnershipSnapshotNodeCount(const xrtownershipsnapshot* pSnapshot)
{
	return pSnapshot != NULL ? pSnapshot->NodeCount : 0;
}

XRT_API bool xrtOwnershipSnapshotNode(const xrtownershipsnapshot* pSnapshot,
	size_t iIndex, xrtownershipnode* pNode)
{
	const xrt_ownership_node* pSource;
	if (pSnapshot == NULL || pNode == NULL || iIndex >= pSnapshot->NodeCount) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	pSource = &pSnapshot->Nodes[iIndex];
	*pNode = (xrtownershipnode){pSource->Reference, pSource->StrongCount,
		pSource->Incoming, pSource->Reachable};
	return true;
}

XRT_API void xrtOwnershipSnapshotDestroy(xrtownershipsnapshot* pSnapshot)
{
	if (pSnapshot == NULL) return;
	xrtFree(pSnapshot->Nodes); xrtFree(pSnapshot);
}
