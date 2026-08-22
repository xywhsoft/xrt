#include "../internal/xrt_internal.h"
#include "../internal/xrt_runtime_object.h"
#include <xrt/runtime_object_graph.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_OBJECT_GRAPH)

typedef struct xrtobjectgraphnode {
	xrtobject* Object;
	size_t StrongCount;
	size_t IncomingCount;
	bool Reachable;
	bool Claimed;
} xrtobjectgraphnode;



typedef struct xrtobjectgraphtrace {
	xrtobjectgraphnode* Nodes;
	size_t NodeCount;
	size_t* Slots;
	size_t SlotCount;
	size_t* Work;
	size_t WorkCount;
	size_t EdgeCount;
	bool CountEdges;
} xrtobjectgraphtrace;



struct xrtobjectgraph {
	xrt_spinlock Lock;
	xrtobject* Head;
	xrtobject* Tail;
	size_t Count;
};



/* 设置对象图模块结构化错误。 */
static void __xrtObjectGraphError(
	xerrkind Kind,
	xobjectgrapherror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.object-graph";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 为类型追踪或根枚举失败补充对象图上下文。 */
static void __xrtObjectGraphWrap(
	xerrkind DefaultKind,
	xobjectgrapherror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = "xrt.object-graph";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 在已经持有图锁时摘除对象，不改变对象强引用。 */
static void __xrtObjectGraphRemoveLocked(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( pObject->GraphPrevious != NULL ) {
		pObject->GraphPrevious->GraphNext = pObject->GraphNext;
	} else {
		pGraph->Head = pObject->GraphNext;
	}
	if ( pObject->GraphNext != NULL ) {
		pObject->GraphNext->GraphPrevious = pObject->GraphPrevious;
	} else {
		pGraph->Tail = pObject->GraphPrevious;
	}
	pObject->Graph = NULL;
	pObject->GraphPrevious = NULL;
	pObject->GraphNext = NULL;
	if ( pGraph->Count != 0u ) {
		pGraph->Count--;
	}
}



/* 普通最后引用释放通过对象记录的所属图自动摘除对象。 */
void __xrtObjectGraphDetach(xrtobject* pObject)
{
	xrtobjectgraph* pGraph = pObject->Graph;

	if ( pGraph == NULL ) {
		return;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph == pGraph ) {
		__xrtObjectGraphRemoveLocked(pGraph, pObject);
	}
	__xrtSpinUnlock(&pGraph->Lock);
}



/* 混合对象地址的有效位，供开放寻址哈希表使用。 */
static size_t __xrtObjectGraphHash(const xrtobject* pObject)
{
	uintptr_t iValue = (uintptr_t)pObject;

	iValue >>= 3u;
	iValue ^= iValue >> 17u;
	iValue *= (uintptr_t)UINT32_C(0xed5ad4bb);
	iValue ^= iValue >> 11u;
#if UINTPTR_MAX > UINT32_MAX
	iValue *= (uintptr_t)UINT64_C(0x9e3779b97f4a7c15);
	iValue ^= iValue >> 29u;
#endif
	return (size_t)iValue;
}



/* 计算至少容纳两倍节点数量的二次幂哈希容量。 */
static bool __xrtObjectGraphSlotCount(size_t iCount, size_t* pSlotCount)
{
	size_t iRequired;
	size_t iSlots = 8u;

	if ( iCount > (SIZE_MAX / 2u) ) {
		__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
			"collect", "the object graph is too large to index");
		return false;
	}
	iRequired = iCount * 2u;
	while ( iSlots < iRequired ) {
		if ( iSlots > (SIZE_MAX / 2u) ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph hash capacity overflows");
			return false;
		}
		iSlots *= 2u;
	}
	*pSlotCount = iSlots;
	return true;
}



/* 在对象地址哈希表中查找节点，不存在时返回 SIZE_MAX。 */
static size_t __xrtObjectGraphFind(
	const xrtobjectgraphtrace* pTrace,
	const xrtobject* pObject
)
{
	size_t iMask = pTrace->SlotCount - 1u;
	size_t iSlot = __xrtObjectGraphHash(pObject) & iMask;

	for ( size_t i = 0; i < pTrace->SlotCount; i++ ) {
		size_t iNode = pTrace->Slots[iSlot];

		if ( iNode == SIZE_MAX ) {
			return SIZE_MAX;
		}
		if ( pTrace->Nodes[iNode].Object == pObject ) {
			return iNode;
		}
		iSlot = (iSlot + 1u) & iMask;
	}
	return SIZE_MAX;
}



/* 把全部快照节点插入无重复地址的哈希索引。 */
static void __xrtObjectGraphIndex(xrtobjectgraphtrace* pTrace)
{
	size_t iMask = pTrace->SlotCount - 1u;

	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		size_t iSlot = __xrtObjectGraphHash(pTrace->Nodes[i].Object) & iMask;

		while ( pTrace->Slots[iSlot] != SIZE_MAX ) {
			iSlot = (iSlot + 1u) & iMask;
		}
		pTrace->Slots[iSlot] = i;
	}
}



/* 释放快照为每一个节点临时持有的强引用。 */
static void __xrtObjectGraphReleaseSnapshot(
	xrtobjectgraphnode* pNodes,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtObjectUnref(pNodes[i].Object);
	}
}



/* 在安全点建立稳定对象快照并为每个对象持有一个临时强引用。 */
static bool __xrtObjectGraphSnapshot(
	xrtobjectgraph* pGraph,
	xrtobjectgraphnode* pNodes,
	size_t iCount
)
{
	xrtobject* pObject;
	size_t i = 0u;

	__xrtSpinLock(&pGraph->Lock);
	if ( pGraph->Count != iCount ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
			"collect", "the object graph changed while collection began");
		return false;
	}
	for ( pObject = pGraph->Head;
		  (pObject != NULL) && (i < iCount);
		  pObject = pObject->GraphNext ) {
		int32 iReferences;

		if ( (__xrtAtomicRefLoad(&pObject->State) !=
			 XRT_OBJECT_STATE_ACTIVE) ||
			 (xrtObjectRef(pObject) == NULL) ) {
			break;
		}
		iReferences = __xrtAtomicRefLoad(&pObject->StrongCount);
		pNodes[i].Object = pObject;
		pNodes[i].StrongCount = iReferences > 0 ?
			(size_t)(iReferences - 1) : 0u;
		i++;
	}
	__xrtSpinUnlock(&pGraph->Lock);
	if ( (i != iCount) || (pObject != NULL) ) {
		__xrtObjectGraphReleaseSnapshot(pNodes, i);
		if ( xrtGetError() != NULL ) {
			__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph snapshot could not retain an object");
		} else {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph list is inconsistent");
		}
		return false;
	}
	return true;
}



/* 第一遍统计图内入边，第二遍把新发现的可达对象压入工作栈。 */
static bool __xrtObjectGraphVisit(xrtobject* pObject, ptr pContext)
{
	xrtobjectgraphtrace* pTrace = (xrtobjectgraphtrace*)pContext;
	size_t iNode;

	if ( pObject == NULL ) {
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
			"trace", "a type trace visited a null strong reference");
		return false;
	}
	iNode = __xrtObjectGraphFind(pTrace, pObject);
	if ( iNode == SIZE_MAX ) {
		return true;
	}
	if ( pTrace->CountEdges ) {
		if ( (pTrace->Nodes[iNode].IncomingCount == SIZE_MAX) ||
			 (pTrace->EdgeCount == SIZE_MAX) ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_TRACE,
				"trace", "the object graph edge count overflows");
			return false;
		}
		pTrace->Nodes[iNode].IncomingCount++;
		pTrace->EdgeCount++;
		return true;
	}
	if ( !pTrace->Nodes[iNode].Reachable ) {
		pTrace->Nodes[iNode].Reachable = true;
		pTrace->Work[pTrace->WorkCount++] = iNode;
	}
	return true;
}



/* 调用类型描述追踪一个快照节点，并把失败包装到对象图域。 */
static bool __xrtObjectGraphTraceNode(
	xrtobjectgraphtrace* pTrace,
	size_t iNode
)
{
	xrtobject* pObject = pTrace->Nodes[iNode].Object;
	const void* pPayload = ((const uint8*)pObject) + pObject->PayloadOffset;

	if ( !xrtTypeTraceInstance(
			pObject->Type, pPayload, __xrtObjectGraphVisit, pTrace
		) ) {
		__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
			"collect", "a runtime object reference trace failed");
		return false;
	}
	return true;
}



/* 标记一个显式根以及从它传播到的所有图内对象。 */
static bool __xrtObjectGraphVisitRoot(xrtobject* pObject, ptr pContext)
{
	xrtobjectgraphtrace* pTrace = (xrtobjectgraphtrace*)pContext;
	size_t iNode;

	if ( pObject == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ROOTS,
			"roots", "the root enumerator visited a null object");
		return false;
	}
	iNode = __xrtObjectGraphFind(pTrace, pObject);
	if ( (iNode != SIZE_MAX) && !pTrace->Nodes[iNode].Reachable ) {
		pTrace->Nodes[iNode].Reachable = true;
		pTrace->Work[pTrace->WorkCount++] = iNode;
	}
	return true;
}



/* 收集前确认快照引用计数和对象状态没有在安全点内变化。 */
static bool __xrtObjectGraphValidateCandidates(
	const xrtobjectgraph* pGraph,
	const xrtobjectgraphtrace* pTrace
)
{
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		const xrtobjectgraphnode* pNode = &pTrace->Nodes[i];
		int32 iReferences;

		iReferences = __xrtAtomicRefLoad(&pNode->Object->StrongCount);
		if ( (pNode->StrongCount == SIZE_MAX) ||
			 (iReferences <= 0) ||
			 ((size_t)iReferences != (pNode->StrongCount + 1u)) ||
			 (__xrtAtomicRefLoad(&pNode->Object->State) !=
				XRT_OBJECT_STATE_ACTIVE) ||
			 (pNode->Object->Graph != pGraph) ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph changed outside its collection safe point");
			return false;
		}
	}
	return true;
}



/* 在任何负载销毁前原子取得全部候选对象的终结权。 */
static bool __xrtObjectGraphClaimCandidates(xrtobjectgraphtrace* pTrace)
{
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		xrtobjectgraphnode* pNode = &pTrace->Nodes[i];

		if ( pNode->Reachable ) {
			continue;
		}
		if ( !__xrtObjectBeginFinalize(pNode->Object) ) {
			for ( size_t j = 0; j < i; j++ ) {
				if ( pTrace->Nodes[j].Claimed ) {
					__xrtObjectCancelFinalize(pTrace->Nodes[j].Object);
					pTrace->Nodes[j].Claimed = false;
				}
			}
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "an object could not enter graph finalization");
			return false;
		}
		pNode->Claimed = true;
	}
	return true;
}



/* 一次摘除全部候选对象，随后统一销毁负载并发布终结状态。 */
static size_t __xrtObjectGraphFinalizeCandidates(
	xrtobjectgraph* pGraph,
	xrtobjectgraphtrace* pTrace
)
{
	size_t iCollected = 0u;

	__xrtSpinLock(&pGraph->Lock);
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		xrtobjectgraphnode* pNode = &pTrace->Nodes[i];

		if ( pNode->Claimed && (pNode->Object->Graph == pGraph) ) {
			__xrtObjectGraphRemoveLocked(pGraph, pNode->Object);
		}
	}
	__xrtSpinUnlock(&pGraph->Lock);

	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		if ( pTrace->Nodes[i].Claimed ) {
			__xrtObjectDropPayload(pTrace->Nodes[i].Object);
			iCollected++;
		}
	}
	for ( size_t i = 0; i < pTrace->NodeCount; i++ ) {
		if ( pTrace->Nodes[i].Claimed ) {
			__xrtObjectEndFinalize(pTrace->Nodes[i].Object);
		}
	}
	return iCollected;
}



/* 创建一个空对象图。 */
XRT_API xrtobjectgraph* xrtObjectGraphCreate(void)
{
	xrtobjectgraph* pGraph = (xrtobjectgraph*)xrtCalloc(1u, sizeof(*pGraph));

	if ( pGraph != NULL ) {
		__xrtSpinInit(&pGraph->Lock);
	}
	return pGraph;
}



/* 摘除全部借用对象并销毁空图，不影响对象强生命周期。 */
XRT_API void xrtObjectGraphDestroy(xrtobjectgraph* pGraph)
{
	if ( pGraph == NULL ) {
		return;
	}
	__xrtSpinLock(&pGraph->Lock);
	while ( pGraph->Head != NULL ) {
		__xrtObjectGraphRemoveLocked(pGraph, pGraph->Head);
	}
	__xrtSpinUnlock(&pGraph->Lock);
	__xrtSpinUnit(&pGraph->Lock);
	xrtFree(pGraph);
}



/* 把活动对象幂等加入图尾。 */
XRT_API bool xrtObjectGraphTrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"track", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph == pGraph ) {
		__xrtSpinUnlock(&pGraph->Lock);
		return true;
	}
	if ( pObject->Graph != NULL ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_EXISTS, XOBJECT_GRAPH_ERROR_TRACK,
			"track", "the runtime object already belongs to another graph");
		return false;
	}
	if ( (__xrtAtomicRefLoad(&pObject->State) !=
		 XRT_OBJECT_STATE_ACTIVE) ||
		 (__xrtAtomicRefLoad(&pObject->StrongCount) <= 0) ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACK,
			"track", "only a live runtime object can be tracked");
		return false;
	}
	if ( pGraph->Count == SIZE_MAX ) {
		__xrtSpinUnlock(&pGraph->Lock);
		__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
			"track", "the object graph member count overflows");
		return false;
	}
	pObject->Graph = pGraph;
	pObject->GraphPrevious = pGraph->Tail;
	pObject->GraphNext = NULL;
	if ( pGraph->Tail != NULL ) {
		pGraph->Tail->GraphNext = pObject;
	} else {
		pGraph->Head = pObject;
	}
	pGraph->Tail = pObject;
	pGraph->Count++;
	__xrtSpinUnlock(&pGraph->Lock);
	return true;
}



/* 从指定对象图摘除对象。 */
XRT_API bool xrtObjectGraphUntrack(
	xrtobjectgraph* pGraph,
	xrtobject* pObject
)
{
	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"untrack", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pGraph->Lock);
	if ( pObject->Graph != pGraph ) {
		__xrtSpinUnlock(&pGraph->Lock);
		return false;
	}
	__xrtObjectGraphRemoveLocked(pGraph, pObject);
	__xrtSpinUnlock(&pGraph->Lock);
	return true;
}



/* 查询对象当前是否由指定对象图跟踪。 */
XRT_API bool xrtObjectGraphContains(
	const xrtobjectgraph* pGraph,
	const xrtobject* pObject
)
{
	xrtobjectgraph* pMutable = (xrtobjectgraph*)pGraph;
	bool bContains;

	if ( (pGraph == NULL) || (pObject == NULL) ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"contains", "the object graph or runtime object is null");
		return false;
	}
	__xrtSpinLock(&pMutable->Lock);
	bContains = pObject->Graph == pGraph;
	__xrtSpinUnlock(&pMutable->Lock);
	return bContains;
}



/* 返回对象图当前跟踪的借用对象数量。 */
XRT_API size_t xrtObjectGraphCount(const xrtobjectgraph* pGraph)
{
	xrtobjectgraph* pMutable = (xrtobjectgraph*)pGraph;
	size_t iCount;

	if ( pGraph == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"count", "the object graph is null");
		return 0u;
	}
	__xrtSpinLock(&pMutable->Lock);
	iCount = pGraph->Count;
	__xrtSpinUnlock(&pMutable->Lock);
	return iCount;
}



/* 使用可选宿主根执行失败原子的强引用环收集。 */
XRT_API bool xrtObjectGraphCollectRoots(
	xrtobjectgraph* pGraph,
	xrtobjectrootproc pRoots,
	ptr pContext,
	xrtobjectgraphresult* pResult
)
{
	xrtobjectgraphresult Result = { 0u, 0u, 0u, 0u };
	xrtobjectgraphtrace Trace;
	xrtobjectgraphnode* pNodes = NULL;
	size_t* pSlots = NULL;
	size_t* pWork = NULL;
	size_t iSlotCount;
	size_t iCount;
	bool bSnapshot = false;
	bool bSuccess = false;
	xerror* pPrevious;
	xerror* pDiscard;

	memset(&Trace, 0, sizeof(Trace));
	if ( pGraph == NULL ) {
		__xrtObjectGraphError(XERR_ARGUMENT, XOBJECT_GRAPH_ERROR_ARGUMENT,
			"collect", "the object graph is null");
		return false;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	iCount = xrtObjectGraphCount(pGraph);
	Result.TrackedCount = iCount;
	if ( iCount == 0u ) {
		if ( pResult != NULL ) {
			*pResult = Result;
		}
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
		return true;
	}
	if ( (iCount > (SIZE_MAX / sizeof(*pNodes))) ||
		 (iCount > (SIZE_MAX / sizeof(*pWork))) ||
		 !__xrtObjectGraphSlotCount(iCount, &iSlotCount) ||
		 (iSlotCount > (SIZE_MAX / sizeof(*pSlots))) ) {
		if ( xrtGetError() == NULL ) {
			__xrtObjectGraphError(XERR_RANGE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph snapshot size overflows");
		}
		xrtErrorFree(pPrevious);
		return false;
	}
	pNodes = (xrtobjectgraphnode*)xrtCalloc(iCount, sizeof(*pNodes));
	pSlots = (size_t*)xrtMalloc(iSlotCount * sizeof(*pSlots));
	pWork = (size_t*)xrtMalloc(iCount * sizeof(*pWork));
	if ( (pNodes == NULL) || (pSlots == NULL) || (pWork == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iSlotCount; i++ ) {
		pSlots[i] = SIZE_MAX;
	}
	if ( !__xrtObjectGraphSnapshot(pGraph, pNodes, iCount) ) {
		goto cleanup;
	}
	bSnapshot = true;
	Trace.Nodes = pNodes;
	Trace.NodeCount = iCount;
	Trace.Slots = pSlots;
	Trace.SlotCount = iSlotCount;
	Trace.Work = pWork;
	Trace.CountEdges = true;
	__xrtObjectGraphIndex(&Trace);

	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtObjectGraphTraceNode(&Trace, i) ) {
			goto cleanup;
		}
	}
	Result.EdgeCount = Trace.EdgeCount;
	Trace.CountEdges = false;
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pNodes[i].IncomingCount > pNodes[i].StrongCount ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_TRACE,
				"collect", "a type trace reported more strong references than exist");
			goto cleanup;
		}
		if ( pNodes[i].StrongCount > pNodes[i].IncomingCount ) {
			pNodes[i].Reachable = true;
			pWork[Trace.WorkCount++] = i;
			Result.RootCount++;
		}
	}
	if ( (pRoots != NULL) &&
		 !pRoots(__xrtObjectGraphVisitRoot, &Trace, pContext) ) {
		__xrtObjectGraphWrap(XERR_STATE, XOBJECT_GRAPH_ERROR_ROOTS,
			"collect", "the object graph root enumeration failed");
		goto cleanup;
	}
	Result.RootCount = Trace.WorkCount;
	while ( Trace.WorkCount != 0u ) {
		size_t iNode = pWork[--Trace.WorkCount];

		if ( !__xrtObjectGraphTraceNode(&Trace, iNode) ) {
			goto cleanup;
		}
	}
	if ( !__xrtObjectGraphValidateCandidates(pGraph, &Trace) ||
		 !__xrtObjectGraphClaimCandidates(&Trace) ) {
		goto cleanup;
	}
	Result.CollectedCount = __xrtObjectGraphFinalizeCandidates(pGraph, &Trace);
	bSuccess = true;

cleanup:
	if ( bSnapshot ) {
		__xrtObjectGraphReleaseSnapshot(pNodes, iCount);
	}
	xrtFree(pWork);
	xrtFree(pSlots);
	xrtFree(pNodes);
	if ( bSuccess ) {
		if ( pResult != NULL ) {
			*pResult = Result;
		}
		pDiscard = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pDiscard);
	} else {
		xrtErrorFree(pPrevious);
		if ( xrtGetError() == NULL ) {
			__xrtObjectGraphError(XERR_STATE, XOBJECT_GRAPH_ERROR_STATE,
				"collect", "the object graph collection failed");
		}
	}
	return bSuccess;
}



/* 使用引用计数自动根识别执行常规收集。 */
XRT_API bool xrtObjectGraphCollect(
	xrtobjectgraph* pGraph,
	xrtobjectgraphresult* pResult
)
{
	return xrtObjectGraphCollectRoots(pGraph, NULL, NULL, pResult);
}

#endif
