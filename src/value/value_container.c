#include "../internal/xrt_value.h"
#include "../internal/xrt_map.h"



#if defined(XRT_FEATURE_VALUE_CONTAINER)

#define XRT_VALUE_BACKING_FINALIZING 0x0001u
#define XRT_VALUE_BACKING_OWNERSHIP_CLEARED 0x0002u

/* 所有容器 backing 共用原子引用头。 */
struct xvaluebacking {
	volatile int32 RefCount;
	uint16 Type;
	uint16 Flags;
	const void* OwnershipClaim;
	/* Each count denotes an ACTUAL adapter Hold, not an estimated root. COW
	 * ignores these observational holds, never real shells or native cursors. */
	size_t OwnershipHolds;
};



/* 数组 backing 复用已经压实的连续指针数组。 */
typedef struct xvaluearraybacking {
	xvaluebacking Base;
	xptrarray Items;
} xvaluearraybacking;



/* 稀疏整数映射 backing 复用拥有式 AVL IntMap。 */
typedef struct xvalueintmapbacking {
	xvaluebacking Base;
	xintmap Items;
} xvalueintmapbacking;



/* 集合 backing 复用稳定地址、稳定顺序哈希 Set。 */
typedef struct xvaluesetbacking {
	xvaluebacking Base;
	xset Items;
} xvaluesetbacking;



/* 对象 backing 直接复用保持插入顺序的二进制键 Map。 */
typedef struct xvalueobjectlifetime {
	volatile int32 RefCount;
	ptr UserData;
	xrtownershiptrace Trace;
	xvalueobjectfinalizerrelease Release;
	const xvalueobjectownershipv1* OwnershipPolicy;
	const void* OwnershipClaim;
	bool OwnershipCleared;
} xvalueobjectlifetime;

typedef struct xvalueobjectbacking {
	xvaluebacking Base;
	xmap Items;
	xvalueobjectfinalizer Finalizer;
	ptr FinalizerUserData;
	xrtownershiptrace FinalizerTrace;
	xvalueobjectfinalizerrelease FinalizerRelease;
	bool FinalizerBound;
	bool FinalizerPrepared;
	xvalueobjectlifetime* Lifetime;
	const xvalueobjectownershipv1* OwnershipPolicy;
	xvalue* OwnershipReceiver; /* Borrowed only from a real plan-held shell. */
	const void* ReceiverClaim;
	xvalueidentityhash OwnedIdentityHash;
	xvalueidentityequal OwnedIdentityEqual;
} xvalueobjectbacking;

static bool __xrtValueObjectPolicyValid(const xvalueobjectownershipv1*);
static bool __xrtValueObjectPolicyCallbacks(const xvalueobjectbacking*, const xvalueobjectownershipv1*);

static bool __xrtValueLifetimeOwnershipCount(const void* pData, size_t* pCount)
{
	int32 iCount = __xrtAtomicRefLoad(&((const xvalueobjectlifetime*)pData)->RefCount);
	if (iCount <= 0) return false;
	*pCount = (size_t)iCount; return true;
}
static bool __xrtValueLifetimeOwnershipTrace(const void* pData,
	xrtownershipvisitor pVisit, ptr pContext)
{
	const xvalueobjectlifetime* pLifetime = (const xvalueobjectlifetime*)pData;
	return pLifetime->UserData == NULL || pLifetime->Trace(pLifetime->UserData, pVisit, pContext);
}
static const xrtownershipops __xrtValueLifetimeOwnershipOps = {
	__xrtValueLifetimeOwnershipCount, __xrtValueLifetimeOwnershipTrace
};
static void __xrtValueLifetimeRelease(xvalueobjectlifetime* pLifetime)
{
	xrtownershipscope Mutation = {0};
	if (pLifetime == NULL) return;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (xrtRefRelease(&pLifetime->RefCount) == 0) {
		ptr pContext = pLifetime->UserData;
		xvalueobjectfinalizerrelease pRelease = pLifetime->Release;
		const xvalueobjectownershipv1* pPolicy = pLifetime->OwnershipPolicy;
		bool bPhased = __xrtValueObjectPolicyValid(pPolicy) &&
			pLifetime->Trace == pPolicy->LifetimeTrace && pRelease == pPolicy->LifetimeRelease;
		xrtFree(pLifetime);
		if (bPhased && !xrtOwnershipScopeEnd(&Mutation)) abort();
		pRelease(pContext);
		if (bPhased) return;
	}
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
static bool __xrtValueObjectBackingLifetimeCopy(xvalueobjectbacking* pTarget,
	const xvalueobjectbacking* pSource)
{
	if (pTarget->Lifetime != NULL) { __xrtErrorSetInvalidState(); return false; }
	if (pSource->Lifetime != NULL && xrtRefRetain(&pSource->Lifetime->RefCount) < 0) return false;
	pTarget->Lifetime = pSource->Lifetime;
	pTarget->OwnershipPolicy = pSource->OwnershipPolicy;
	pTarget->OwnedIdentityHash = pSource->OwnedIdentityHash;
	pTarget->OwnedIdentityEqual = pSource->OwnedIdentityEqual;
	/* Only ordinary construction state is copyable. A finalization duty is
	 * reference identity, deliberately never cloned into a new backing. */
	pTarget->FinalizerPrepared = !pSource->FinalizerBound && pSource->FinalizerPrepared;
	return true;
}

static bool __xrtValueIterStartInternal(const xvalue*, xvalueiter*, int, bool);

static bool __xrtValueBackingOwnershipCount(const void* pData, size_t* pCount)
{
	const xvaluebacking* pBacking = (const xvaluebacking*)pData;
	int32 iCount = __xrtAtomicRefLoad(&pBacking->RefCount);
	if (iCount <= 0 || (pBacking->Flags & (XRT_VALUE_BACKING_FINALIZING | XRT_VALUE_BACKING_OWNERSHIP_CLEARED)) != 0) return false;
	*pCount = (size_t)iCount;
	return true;
}

static bool __xrtValueBackingOwnershipTrace(const void* pData,
	xrtownershipvisitor pVisit, ptr pContext)
{
	const xvaluebacking* pBacking = (const xvaluebacking*)pData;
	xvalue View = {0};
	xvalueiter Iterator;
	xvalue* pItem;
	bool bOk = true;
	xvalueiterresult Step;
	size_t iReferences;
	if (!__xrtValueBackingOwnershipCount(pData, &iReferences)) return false;
	/* A custom finalizer's non-NULL context is an opaque ownership boundary.
	 * Do not silently treat an unmodelled native payload as having no edges. */
	if (pBacking->Type == XVALUE_OBJECT) {
		const xvalueobjectbacking* pObject = (const xvalueobjectbacking*)pBacking;
		if (pObject->Lifetime != NULL && !pVisit((xrtownershipref){
			pObject->Lifetime, &__xrtValueLifetimeOwnershipOps}, pContext)) return false;
		if (pObject->FinalizerUserData != NULL) {
			if (pObject->FinalizerTrace == NULL) { __xrtErrorSetUnsupported(); return false; }
			if (!pObject->FinalizerTrace(pObject->FinalizerUserData, pVisit, pContext)) return false;
		}
	}
	/* The cursor borrows a synthetic shell and temporarily retains backing.
	 * OwnershipInspect samples counts only after ALL cursors have ended. */
	View.RefCount = 1; View.WeakCount = 1;
	View.Type = pBacking->Type; View.Data.Backing = (xvaluebacking*)pBacking;
	/* This stack view is NOT a real owning shell. The enclosing inspection
	 * already borrows stable backing storage; never retain a synthetic shell. */
	if (!__xrtValueIterStartInternal(&View, &Iterator, 1, false)) return false;
	while ((Step = xrtValueIterAdvance(&Iterator, NULL, &pItem)) == XVALUE_ITER_ITEM) {
		if (!pVisit(xrtValueOwnership(pItem), pContext)) { bOk = false; break; }
	}
	if (Step == XVALUE_ITER_ERROR) bOk = false;
	xrtValueIterEnd(&Iterator);
	return bOk;
}

static const xrtownershipops __xrtValueBackingOwnershipOps = {
	__xrtValueBackingOwnershipCount, __xrtValueBackingOwnershipTrace
};

xrtownershipref __xrtValueBackingOwnership(const xvalue* pValue)
{
	return (xrtownershipref){pValue->Data.Backing, &__xrtValueBackingOwnershipOps};
}



/* 递归可达性检查的三态结果。 */
typedef enum xvaluecontainsresult {
	XVALUE_CONTAINS_ERROR = -1,
	XVALUE_CONTAINS_NO = 0,
	XVALUE_CONTAINS_YES = 1
} xvaluecontainsresult;



#define XRT_VALUE_VISITED_INLINE 32u



/* 一次值图可达性检查共用目标、活动路径和去重集合。 */
typedef struct xvaluecontainscontext {
	const xvaluebacking* TargetBacking;
	const xvalue* TargetValue;
	const xvaluebacking* Active[XRT_VALUE_DEPTH_MAX];
	const xvaluebacking* Inline[XRT_VALUE_VISITED_INLINE];
	size_t InlineCount;
	xset Overflow;
	bool OverflowReady;
} xvaluecontainscontext;



/* Map 删除值槽时释放其持有的动态值引用。 */
static void __xrtValueObjectDrop(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xvalue** pItem = (xvalue**)pValue;

	(void)Key;
	(void)pUserData;
	xrtValueRelease(*pItem);
	*pItem = NULL;
}



/* IntMap 删除值槽时释放其持有的动态值引用。 */
static void __xrtValueIntMapDrop(
	int64 iKey,
	ptr pValue,
	ptr pUserData
)
{
	xvalue** pItem = (xvalue**)pValue;

	(void)iKey;
	(void)pUserData;
	xrtValueRelease(*pItem);
	*pItem = NULL;
}



/* Set 复制元素时增加动态值引用。 */
static bool __xrtValueSetCopy(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
)
{
	xvalue* pItem = *(xvalue* const*)pSource;
	xvalue* pRetained;

	(void)pUserData;
	pRetained = xrtValueRetain(pItem);
	if ( pRetained == NULL ) {
		return false;
	}
	*(xvalue**)pTarget = pRetained;
	return true;
}



/* Set 删除元素时释放动态值引用。 */
static void __xrtValueSetDrop(ptr pItem, ptr pUserData)
{
	xvalue** pValue = (xvalue**)pItem;

	(void)pUserData;
	xrtValueRelease(*pValue);
	*pValue = NULL;
}



/* Set 使用动态值的规范标量哈希。 */
static uint64 __xrtValueSetHash(const void* pItem, ptr pUserData)
{
	const xvalue* pValue = *(xvalue* const*)pItem;
	const xvalue* tValues[1];
	uint64 iHash;

	(void)pUserData;
	if ( pValue->Type != XVALUE_HANDLE ) {
		return __xrtValueHashKnown(pValue);
	}
	tValues[0] = pValue;
	if ( !__xrtValueCallbackProtect(tValues, 1) ) {
		return 0;
	}
	iHash = __xrtValueHashKnown(pValue);
	__xrtValueCallbackUnprotect(tValues, 1);
	return iHash;
}



/* Set 使用与哈希一致的动态值相等规则。 */
static bool __xrtValueSetEqual(
	const void* pLeft,
	const void* pRight,
	ptr pUserData
)
{
	const xvalue* pLeftValue = *(xvalue* const*)pLeft;
	const xvalue* pRightValue = *(xvalue* const*)pRight;
	const xvalue* tValues[2];
	bool bEqual;

	(void)pUserData;
	if ( (pLeftValue == pRightValue) ||
		 (pLeftValue->Type != XVALUE_HANDLE) ||
		 (pRightValue->Type != XVALUE_HANDLE) ||
		 (pLeftValue->Data.Handle.Ops != pRightValue->Data.Handle.Ops) ||
		 (pLeftValue->Data.Handle.UserData !=
		  pRightValue->Data.Handle.UserData) ) {
		return __xrtValueEqualKnown(pLeftValue, pRightValue);
	}
	tValues[0] = pLeftValue;
	tValues[1] = pRightValue;
	if ( !__xrtValueCallbackProtect(tValues, 2) ) {
		return false;
	}
	bEqual = __xrtValueEqualKnown(pLeftValue, pRightValue);
	__xrtValueCallbackUnprotect(tValues, 2);
	return bEqual;
}



/* 返回类型对应的具体 backing 大小。 */
static size_t __xrtValueBackingSize(xvaluetype Type)
{
	switch ( Type ) {
		case XVALUE_ARRAY:
			return sizeof(xvaluearraybacking);
		case XVALUE_INT_MAP:
			return sizeof(xvalueintmapbacking);
		case XVALUE_SET:
			return sizeof(xvaluesetbacking);
		case XVALUE_OBJECT:
			return sizeof(xvalueobjectbacking);
		default:
			return 0;
	}
}



/* 初始化一个空容器 backing 及其拥有型值策略。 */
static xvaluebacking* __xrtValueBackingCreate(xvaluetype Type)
{
	size_t iSize = __xrtValueBackingSize(Type);
	xvaluebacking* pBacking;
	bool bReady = false;

	if ( iSize == 0 ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pBacking = (xvaluebacking*)xrtCalloc(1, iSize);
	if ( pBacking == NULL ) {
		return NULL;
	}
	pBacking->RefCount = 1;
	pBacking->Type = (uint16)Type;
	if ( Type == XVALUE_ARRAY ) {
		bReady = xrtPtrArrayInit(&((xvaluearraybacking*)pBacking)->Items);
	} else if ( Type == XVALUE_INT_MAP ) {
		xvalueintmapbacking* pMap = (xvalueintmapbacking*)pBacking;

		bReady = xrtIntMapInit(&pMap->Items, sizeof(xvalue*)) &&
			xrtIntMapSetDrop(&pMap->Items, __xrtValueIntMapDrop, NULL);
	} else if ( Type == XVALUE_SET ) {
		xvaluesetbacking* pSet = (xvaluesetbacking*)pBacking;

		bReady = xrtSetInit(&pSet->Items, sizeof(xvalue*)) &&
			xrtSetSetKeyPolicy(
				&pSet->Items,
				__xrtValueSetHash,
				__xrtValueSetEqual,
				NULL
			) &&
			xrtSetSetLifecycle(
				&pSet->Items,
				__xrtValueSetCopy,
				__xrtValueSetDrop,
				NULL
			);
	} else {
		xvalueobjectbacking* pObject = (xvalueobjectbacking*)pBacking;

		bReady = xrtMapInit(&pObject->Items, sizeof(xvalue*)) &&
			xrtMapSetDrop(&pObject->Items, __xrtValueObjectDrop, NULL);
	}
	if ( !bReady ) {
		if ( Type == XVALUE_ARRAY ) {
			xrtPtrArrayUnit(&((xvaluearraybacking*)pBacking)->Items);
		} else if ( Type == XVALUE_INT_MAP ) {
			xrtIntMapUnit(&((xvalueintmapbacking*)pBacking)->Items);
		} else if ( Type == XVALUE_SET ) {
			xrtSetUnit(&((xvaluesetbacking*)pBacking)->Items);
		} else {
			xrtMapUnit(&((xvalueobjectbacking*)pBacking)->Items);
		}
		xrtFree(pBacking);
		return NULL;
	}
	return pBacking;
}



/* 增加 backing 引用并验证引用边界。 */
static bool __xrtValueBackingRetain(xvaluebacking* pBacking)
{
	if ( (pBacking == NULL) || (xrtRefRetain(&pBacking->RefCount) < 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 销毁最后一个 backing 及其持有的全部值。 */
static void __xrtValueBackingReleaseView(xvaluebacking* pBacking, xvalue* pView,
	xrtownershipscope* pMutation)
{
	int32 iReferences;
	bool bPhased = true;

	if ( pBacking == NULL ) {
		return;
	}
	iReferences = xrtRefRelease(&pBacking->RefCount);
	if ( iReferences < 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iReferences != 0 ) {
		return;
	}
	/* Clear under exclusive freeze always has a real backing Hold. Only an
	 * ordinary terminal release may reach zero and phase its OWN scope. */
	if (pMutation == NULL) abort();
	if ( pBacking->Type == XVALUE_OBJECT ) {
		xvalueobjectbacking* pObject = (xvalueobjectbacking*)pBacking;
		const xvalueobjectownershipv1* pPolicy = pObject->OwnershipPolicy;
		bPhased = (pObject->Lifetime == NULL && pObject->Finalizer == NULL &&
			pObject->FinalizerRelease == NULL && pObject->FinalizerTrace == NULL &&
			pObject->FinalizerUserData == NULL) ||
			(pObject->Lifetime != NULL && __xrtValueObjectPolicyValid(pPolicy) &&
			 pObject->Lifetime->OwnershipPolicy == pPolicy && __xrtValueObjectPolicyCallbacks(pObject, pPolicy));
		/* Failed/abandoned construction never acquires a user finalization
		 * duty. Its context and field owners still end in the normal tail. */
		if (pObject->FinalizerPrepared) {
			pObject->Finalizer = NULL;
			pObject->FinalizerPrepared = false;
		}
		if ( pObject->Finalizer != NULL ) {
			xvalueobjectfinalizer pFinalizer = pObject->Finalizer;
			/* Only the atomic last release claims finalization. Public cursors
			 * on such backings keep a real shell until their own End, so this
			 * path never fabricates receiver identity or reads a dead shell. */
			if ( pView == NULL ) { __xrtErrorSetInvalidState(); return; }
			pObject->Finalizer = NULL;
			/* A private release guard permits the documented field mutations.
			 * The shell itself cannot be retained/cloned/released in Finalize.
			 * A cursor made inside Finalize may keep the finalized backing;
			 * its final End then releases fields and the owned context. */
			pBacking->RefCount = 1;
			pBacking->Flags |= XRT_VALUE_BACKING_FINALIZING;
			pView->Flags &= (uint16)~XRT_VALUE_FLAG_BUSY;
			pView->Flags |= XRT_VALUE_FLAG_FINALIZING;
			if (bPhased && !xrtOwnershipScopeEnd(pMutation)) abort();
			pFinalizer(pView, pObject->FinalizerUserData);
			if (bPhased && !xrtOwnershipMutationBegin(pMutation)) abort();
			pView->Flags &= (uint16)~XRT_VALUE_FLAG_FINALIZING;
			pView->Flags |= XRT_VALUE_FLAG_BUSY;
			if ( pObject->FinalizerRelease == NULL ) {
				/* Legacy finalizers dispose their own borrowed context. */
				pObject->FinalizerUserData = NULL;
				pObject->FinalizerTrace = NULL;
			}
			pBacking->Flags &= (uint16)~XRT_VALUE_BACKING_FINALIZING;
			if ( xrtRefRelease(&pBacking->RefCount) != 0 ) return;

		}
	}
	/* Zero-count private backings cannot be admitted by Count/Trace. Their
	 * still-owned child references are real external roots while recursive
	 * releases run, even if a child finalizer inspects/collects another graph.
	 * Opaque legacy finalizers/lifetimes keep their conservative boundary. */
	if (bPhased && !xrtOwnershipScopeEnd(pMutation)) abort();
	if ( pBacking->Type == XVALUE_ARRAY ) {
		xvaluearraybacking* pArray = (xvaluearraybacking*)pBacking;

		for ( size_t i = 0; i < pArray->Items.Count; i++ ) {
			xrtValueRelease((xvalue*)xrtPtrArrayGet(&pArray->Items, i));
		}
		xrtPtrArrayUnit(&pArray->Items);
	} else if ( pBacking->Type == XVALUE_INT_MAP ) {
		xrtIntMapUnit(&((xvalueintmapbacking*)pBacking)->Items);
	} else if ( pBacking->Type == XVALUE_SET ) {
		xrtSetUnit(&((xvaluesetbacking*)pBacking)->Items);
	} else if ( pBacking->Type == XVALUE_OBJECT ) {
		xvalueobjectbacking* pObject = (xvalueobjectbacking*)pBacking;
		xvalueobjectfinalizerrelease pRelease = pObject->FinalizerRelease;
		ptr pContext = pObject->FinalizerUserData;
		xrtMapUnit(&pObject->Items);
		pObject->FinalizerRelease = NULL;
		pObject->FinalizerUserData = NULL;
		pObject->FinalizerTrace = NULL;
		if ( pRelease != NULL ) pRelease(pContext);
		/* Code/type capabilities outlive every field and finalizer-context
		 * callback, including the tails of callbacks that release children. */
		__xrtValueLifetimeRelease(pObject->Lifetime);
	}
	xrtFree(pBacking);
	if (bPhased && !xrtOwnershipMutationBegin(pMutation)) abort();
}

static void __xrtValueBackingRelease(xvaluebacking* pBacking)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	__xrtValueBackingReleaseView(pBacking, NULL, &Mutation);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}

static bool __xrtValueBackingAdapterHold(const void* pData)
{
	xvaluebacking* pBacking = (xvaluebacking*)pData;
	if (pBacking->OwnershipHolds == SIZE_MAX || !__xrtValueBackingRetain(pBacking)) return false;
	++pBacking->OwnershipHolds; return true;
}
static void __xrtValueBackingAdapterDrop(const void* pData)
{
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (((xvaluebacking*)pData)->OwnershipHolds == 0) abort();
	--((xvaluebacking*)pData)->OwnershipHolds;
	__xrtValueBackingReleaseView((xvaluebacking*)pData, NULL, &Mutation);
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
}
static bool __xrtValueBackingAdapterClaim(const void* pData, const void* pToken)
{
	xvaluebacking* pBacking = (xvaluebacking*)pData;
	if (pToken == NULL || (pBacking->Flags & XRT_VALUE_BACKING_OWNERSHIP_CLEARED) != 0 ||
		(pBacking->OwnershipClaim != NULL && pBacking->OwnershipClaim != pToken)) return false;
	pBacking->OwnershipClaim = pToken; return true;
}
static void __xrtValueBackingAdapterRestore(const void* pData, const void* pToken)
{
	xvaluebacking* pBacking = (xvaluebacking*)pData;
	if (pToken == NULL || pBacking->OwnershipClaim != pToken || (pBacking->Flags & XRT_VALUE_BACKING_OWNERSHIP_CLEARED) != 0) abort();
	pBacking->OwnershipClaim = NULL;
}
static void __xrtValueBackingAdapterClear(const void* pData, const void* pToken)
{
	xvaluebacking* pBacking = (xvaluebacking*)pData;
	if (pToken == NULL || pBacking->OwnershipClaim != pToken || (pBacking->Flags & XRT_VALUE_BACKING_OWNERSHIP_CLEARED) != 0) abort();
	/* No EnsureUnique: the transaction reasons about this physical backing,
	 * not a COW shell. Every element has an independent real plan hold. */
	if (pBacking->Type == XVALUE_ARRAY) {
		xvaluearraybacking* pArray = (xvaluearraybacking*)pBacking;
		for (size_t i = 0; i < pArray->Items.Count; ++i)
			xrtValueRelease((xvalue*)xrtPtrArrayGet(&pArray->Items, i));
		xrtPtrArrayClear(&pArray->Items);
	} else if (pBacking->Type == XVALUE_INT_MAP) {
		xrtIntMapClear(&((xvalueintmapbacking*)pBacking)->Items);
	} else if (pBacking->Type == XVALUE_SET) {
		xrtSetClear(&((xvaluesetbacking*)pBacking)->Items);
	} else if (pBacking->Type == XVALUE_OBJECT) {
		xrtMapClear(&((xvalueobjectbacking*)pBacking)->Items);
	} else abort();
	pBacking->Flags |= XRT_VALUE_BACKING_OWNERSHIP_CLEARED;
}
static const xrtownershipadapterv1 __xrtValueBackingAdapterV1 = {
	sizeof(xrtownershipadapterv1), __xrtValueBackingAdapterHold, __xrtValueBackingAdapterDrop,
	__xrtValueBackingAdapterClaim, __xrtValueBackingAdapterRestore, NULL, __xrtValueBackingAdapterClear, NULL
};
const xrtownershipadapterv1* __xrtValueBackingOwnershipAdapterV1(xrtownershipref Reference)
{
	const xvaluebacking* pBacking;
	if (Reference.Ops != &__xrtValueBackingOwnershipOps) return NULL;
	pBacking = (const xvaluebacking*)Reference.Data;
	if (__xrtAtomicRefLoad(&pBacking->RefCount) <= 0 ||
		(pBacking->Flags & (XRT_VALUE_BACKING_FINALIZING | XRT_VALUE_BACKING_OWNERSHIP_CLEARED)) != 0) return NULL;
	if (pBacking->Type == XVALUE_OBJECT) {
		const xvalueobjectbacking* pObject = (const xvalueobjectbacking*)pBacking;
		if (pObject->Lifetime != NULL || pObject->Finalizer != NULL || pObject->FinalizerUserData != NULL ||
			pObject->FinalizerTrace != NULL || pObject->FinalizerRelease != NULL || pObject->FinalizerBound ||
			pObject->FinalizerPrepared) return NULL;
	}
	return &__xrtValueBackingAdapterV1;
}

bool __xrtValueObjectClaimReceiver(xvalue* pValue, const void* pToken)
{
	xvalueobjectbacking* pObject;
	if (pValue->Type != XVALUE_OBJECT || pValue->Data.Backing == NULL) return true;
	pObject = (xvalueobjectbacking*)pValue->Data.Backing;
	if (pObject->OwnershipPolicy == NULL) return true;
	if (pObject->ReceiverClaim != NULL && pObject->ReceiverClaim != pToken) return false;
	if (pObject->OwnershipReceiver == NULL) {
		pObject->OwnershipReceiver = pValue; pObject->ReceiverClaim = pToken;
	}
	return true;
}
void __xrtValueObjectRestoreReceiver(xvalue* pValue, const void* pToken)
{
	xvalueobjectbacking* pObject;
	if (pValue->Type != XVALUE_OBJECT || pValue->Data.Backing == NULL) return;
	pObject = (xvalueobjectbacking*)pValue->Data.Backing;
	if (pObject->OwnershipReceiver == pValue && pObject->ReceiverClaim == pToken) {
		pObject->OwnershipReceiver = NULL; pObject->ReceiverClaim = NULL;
	}
}
static bool __xrtValueObjectAdapterFinalize(const void* pData, const void* pToken)
{
	xvalueobjectbacking* pObject = (xvalueobjectbacking*)pData;
	xvalue* pReceiver; xvalueobjectfinalizer pFinalize; ptr pContext;
	xerror* pPrior; xerror* pFailure; bool bOk;
	xrtownershipscope Mutation = {0};
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (pObject->Base.OwnershipClaim != pToken || pObject->FinalizerPrepared ||
		(pObject->Base.Flags & (XRT_VALUE_BACKING_FINALIZING | XRT_VALUE_BACKING_OWNERSHIP_CLEARED)) != 0) abort();
	pFinalize = pObject->Finalizer;
	if (pFinalize == NULL) { if (!xrtOwnershipScopeEnd(&Mutation)) abort(); return true; }
	pReceiver = pObject->OwnershipReceiver;
	/* Never fabricate a stack receiver or resurrect a zero-count shell. If an
	 * earlier refresh restored our borrowed receiver, abort and build afresh. */
	if (pReceiver == NULL || pObject->ReceiverClaim != pToken ||
		pReceiver->OwnershipClaim != pToken || pReceiver->Data.Backing != &pObject->Base ||
		__xrtAtomicRefLoad(&pReceiver->RefCount) <= 0 ||
		(pReceiver->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING | XRT_VALUE_FLAG_OWNERSHIP_CLEARED)) != 0) {
		if (!xrtOwnershipScopeEnd(&Mutation)) abort();
		__xrtErrorSetInvalidState(); return false;
	}
	pContext = pObject->FinalizerUserData;
	pObject->Finalizer = NULL; /* The physical duty survives abort as disarmed. */
	pObject->Base.Flags |= XRT_VALUE_BACKING_FINALIZING;
	pReceiver->Flags |= XRT_VALUE_FLAG_FINALIZING;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	pPrior = xrtTakeError();
	bOk = pObject->OwnershipPolicy->FinalizeChecked(pReceiver, pContext);
	pFailure = xrtTakeError();
	bOk = bOk && pFailure == NULL;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	pReceiver->Flags &= (uint16)~XRT_VALUE_FLAG_FINALIZING;
	pObject->Base.Flags &= (uint16)~XRT_VALUE_BACKING_FINALIZING;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (pPrior != NULL) { xrtSetErrorTake(pPrior); xrtErrorFree(pFailure); }
	else xrtSetErrorTake(pFailure);
	return bOk;
}
static bool __xrtValueObjectAdapterFinish(const void* pData, const void* pToken)
{
	xvalueobjectbacking* pObject = (xvalueobjectbacking*)pData;
	xrtownershipscope Mutation = {0};
	xvalueobjectlifetime* pLifetime; xvalueobjectfinalizerrelease pRelease; ptr pContext;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if (pObject->Base.OwnershipClaim != pToken || (pObject->Base.Flags & XRT_VALUE_BACKING_OWNERSHIP_CLEARED) == 0 || pObject->Finalizer != NULL) abort();
	pRelease = pObject->FinalizerRelease; pContext = pObject->FinalizerUserData;
	pLifetime = pObject->Lifetime;
	pObject->FinalizerRelease = NULL; pObject->FinalizerUserData = NULL; pObject->FinalizerTrace = NULL;
	pObject->OwnershipReceiver = NULL; pObject->ReceiverClaim = NULL; pObject->Lifetime = NULL;
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	/* The lifetime keeps code until the context's complete return tail. */
	if (pRelease != NULL) pRelease(pContext);
	__xrtValueLifetimeRelease(pLifetime);
	return true;
}
static bool __xrtValueLifetimeAdapterHold(const void* pData)
{ return xrtRefRetain(&((xvalueobjectlifetime*)pData)->RefCount) > 0; }
static void __xrtValueLifetimeAdapterDrop(const void* pData)
{
	xvalueobjectlifetime* pLifetime = (xvalueobjectlifetime*)pData;
	xrtownershipscope Mutation = {0}; int32 iReferences;
	ptr pContext = NULL; xvalueobjectfinalizerrelease pRelease = NULL;
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	iReferences = xrtRefRelease(&pLifetime->RefCount); if (iReferences < 0) abort();
	if (iReferences == 0) { pContext = pLifetime->UserData; pRelease = pLifetime->Release; }
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	if (iReferences == 0) { xrtFree(pLifetime); pRelease(pContext); }
}
static bool __xrtValueLifetimeAdapterClaim(const void* pData, const void* pToken)
{
	xvalueobjectlifetime* pLifetime = (xvalueobjectlifetime*)pData;
	if (pToken == NULL || pLifetime->OwnershipCleared ||
		(pLifetime->OwnershipClaim != NULL && pLifetime->OwnershipClaim != pToken)) return false;
	pLifetime->OwnershipClaim = pToken; return true;
}
static void __xrtValueLifetimeAdapterRestore(const void* pData, const void* pToken)
{
	xvalueobjectlifetime* pLifetime = (xvalueobjectlifetime*)pData;
	if (pLifetime->OwnershipClaim != pToken || pLifetime->OwnershipCleared) abort();
	pLifetime->OwnershipClaim = NULL;
}
static void __xrtValueLifetimeAdapterClear(const void* pData, const void* pToken)
{
	xvalueobjectlifetime* pLifetime = (xvalueobjectlifetime*)pData;
	if (pLifetime->OwnershipClaim != pToken || pLifetime->OwnershipCleared) abort();
	/* Keep the actual code edge until every backing/context tail has returned. */
	pLifetime->OwnershipCleared = true;
}
static bool __xrtValueObjectPolicyValid(const xvalueobjectownershipv1* pPolicy)
{
	return pPolicy != NULL && pPolicy->size == sizeof(*pPolicy) && pPolicy->Finalize != NULL &&
		pPolicy->FinalizeChecked != NULL &&
		pPolicy->FinalizerTrace != NULL && pPolicy->FinalizerRelease != NULL &&
		pPolicy->LifetimeTrace != NULL && pPolicy->LifetimeRelease != NULL;
}
static bool __xrtValueObjectPolicyCallbacks(const xvalueobjectbacking* pObject, const xvalueobjectownershipv1* pPolicy)
{
	return pObject->Lifetime != NULL && pObject->Lifetime->Trace == pPolicy->LifetimeTrace &&
		pObject->Lifetime->Release == pPolicy->LifetimeRelease &&
		(pObject->FinalizerBound
		 ? (pObject->Finalizer == NULL || pObject->Finalizer == pPolicy->Finalize) &&
		   pObject->FinalizerTrace == pPolicy->FinalizerTrace && pObject->FinalizerRelease == pPolicy->FinalizerRelease
		 : pObject->Finalizer == NULL && pObject->FinalizerUserData == NULL &&
		   pObject->FinalizerTrace == NULL && pObject->FinalizerRelease == NULL);
}
const xrtownershipadapterv1* __xrtValueObjectOwnershipAdapterV1(xrtownershipref Reference, const xvalueobjectownershipv1* pPolicy)
{
	static const xrtownershipadapterv1 Object = {sizeof(Object),
		__xrtValueBackingAdapterHold, __xrtValueBackingAdapterDrop, __xrtValueBackingAdapterClaim,
		__xrtValueBackingAdapterRestore, __xrtValueObjectAdapterFinalize, __xrtValueBackingAdapterClear, __xrtValueObjectAdapterFinish};
	static const xrtownershipadapterv1 Lifetime = {sizeof(Lifetime),
		__xrtValueLifetimeAdapterHold, __xrtValueLifetimeAdapterDrop, __xrtValueLifetimeAdapterClaim,
		__xrtValueLifetimeAdapterRestore, NULL, __xrtValueLifetimeAdapterClear, NULL};
	if (Reference.Data == NULL || !__xrtValueObjectPolicyValid(pPolicy)) return NULL;
	if (Reference.Ops == &__xrtValueLifetimeOwnershipOps) {
		const xvalueobjectlifetime* pLifetime = (const xvalueobjectlifetime*)Reference.Data;
		return pLifetime->OwnershipPolicy == pPolicy && !pLifetime->OwnershipCleared &&
			pLifetime->Trace == pPolicy->LifetimeTrace && pLifetime->Release == pPolicy->LifetimeRelease &&
			__xrtAtomicRefLoad(&pLifetime->RefCount) > 0 ? &Lifetime : NULL;
	}
	if (Reference.Ops == &__xrtValueBackingOwnershipOps) {
		const xvalueobjectbacking* pObject = (const xvalueobjectbacking*)Reference.Data;
		if (pObject->Base.Type != XVALUE_OBJECT || __xrtAtomicRefLoad(&pObject->Base.RefCount) <= 0 ||
			(pObject->Base.Flags & (XRT_VALUE_BACKING_FINALIZING | XRT_VALUE_BACKING_OWNERSHIP_CLEARED)) != 0 ||
			pObject->OwnershipPolicy != pPolicy || pObject->FinalizerPrepared ||
			!__xrtValueObjectPolicyCallbacks(pObject, pPolicy) || pObject->Lifetime->OwnershipPolicy != pPolicy) return NULL;
		return &Object;
	}
	return NULL;
}
bool __xrtValueObjectOwnershipPolicyMatches(const xvalue* pValue, const xvalueobjectownershipv1* pPolicy)
{
	const xvalueobjectbacking* pObject;
	if (pValue->Type != XVALUE_OBJECT ||
		__xrtValueObjectOwnershipAdapterV1(__xrtValueBackingOwnership(pValue), pPolicy) == NULL) return false;
	pObject = (const xvalueobjectbacking*)pValue->Data.Backing;
	return pValue->IdentityUserData == NULL && pValue->IdentityHash == pObject->OwnedIdentityHash &&
		pValue->IdentityEqual == pObject->OwnedIdentityEqual;
}
static bool __xrtOwnershipBody_ValueObjectOwnershipBindV1(xvalue* pValue, const xvalueobjectownershipv1* pPolicy)
{
	xvalueobjectbacking* pObject;
	if (pValue == NULL || !__xrtValueObjectPolicyValid(pPolicy) || pValue->Type != XVALUE_OBJECT ||
		__xrtAtomicRefLoad(&pValue->RefCount) != 1 || pValue->OwnershipClaim != NULL ||
		(pValue->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING | XRT_VALUE_FLAG_OWNERSHIP_CLEARED)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pObject = (xvalueobjectbacking*)pValue->Data.Backing;
	if (pObject == NULL || __xrtAtomicRefLoad(&pObject->Base.RefCount) != 1 ||
		pObject->Base.OwnershipClaim != NULL || pObject->OwnershipPolicy != NULL ||
		!pObject->FinalizerPrepared || !__xrtValueObjectPolicyCallbacks(pObject, pPolicy) ||
		__xrtAtomicRefLoad(&pObject->Lifetime->RefCount) != 1 || pObject->Lifetime->OwnershipPolicy != NULL) {
		__xrtErrorSetInvalidState(); return false;
	}
	pObject->OwnershipPolicy = pPolicy; pObject->Lifetime->OwnershipPolicy = pPolicy;
	return true;
}
XRT_API bool xrtValueObjectOwnershipBindV1(xvalue* pObject, const xvalueobjectownershipv1* pPolicy)
{ XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectOwnershipBindV1(pObject, pPolicy)); }

static bool __xrtOwnershipBody_ValueObjectIdentityBindV1(xvalue* pValue,
	xvalueidentityhash pHash, xvalueidentityequal pEqual, const xvalueobjectownershipv1* pPolicy)
{
	xvalueobjectbacking* pObject;
	if (pValue == NULL || pValue->Type != XVALUE_OBJECT || !__xrtValueObjectPolicyValid(pPolicy) ||
		pHash == NULL || pEqual == NULL ||
		(pValue->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_OWNERSHIP_CLEARED)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pObject = (xvalueobjectbacking*)pValue->Data.Backing;
	if (pObject == NULL || pObject->OwnershipPolicy != pPolicy ||
		!__xrtValueObjectPolicyCallbacks(pObject, pPolicy) ||
		(pObject->OwnedIdentityHash != NULL && (pObject->OwnedIdentityHash != pHash || pObject->OwnedIdentityEqual != pEqual))) {
		__xrtErrorSetInvalidState(); return false;
	}
	if (pObject->OwnedIdentityHash == pHash && pObject->OwnedIdentityEqual == pEqual &&
		pValue->IdentityHash == pHash && pValue->IdentityEqual == pEqual && pValue->IdentityUserData == NULL) return true;
	if (pValue->OwnershipClaim != NULL || pObject->Base.OwnershipClaim != NULL ||
		(pValue->Flags & XRT_VALUE_FLAG_FINALIZING) != 0) { __xrtErrorSetInvalidState(); return false; }
	if (!xrtValueIdentityBind(pValue, pHash, pEqual, NULL)) return false;
	pObject->OwnedIdentityHash = pHash; pObject->OwnedIdentityEqual = pEqual;
	return true;
}
XRT_API bool xrtValueObjectIdentityBindV1(xvalue* pValue, xvalueidentityhash pHash,
	xvalueidentityequal pEqual, const xvalueobjectownershipv1* pPolicy)
{ XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectIdentityBindV1(pValue, pHash, pEqual, pPolicy)); }



/* 验证动态值确实持有指定类型的有效 backing。 */
static xvaluebacking* __xrtValueBacking(
	const xvalue* pValue,
	xvaluetype Type
)
{
	xvaluebacking* pBacking;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( pValue->Type != (uint16)Type ) {
		__xrtErrorSetType();
		return NULL;
	}
	pBacking = pValue->Data.Backing;
	if ( (pBacking == NULL) || (pBacking->Type != (uint16)Type) ||
		 (__xrtAtomicRefLoad(&pBacking->RefCount) <= 0) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pBacking;
}



/* 创建持有一个全新空 backing 的容器值。 */
static xvalue* __xrtValueContainerCreate(xvaluetype Type)
{
	xvaluebacking* pBacking = __xrtValueBackingCreate(Type);
	xvalue* pValue;

	if ( pBacking == NULL ) {
		return NULL;
	}
	pValue = __xrtValueCreate(Type);
	if ( pValue == NULL ) {
		__xrtValueBackingRelease(pBacking);
		return NULL;
	}
	pValue->Data.Backing = pBacking;
	return pValue;
}



/* 从旧 backing 浅拷贝数组并增加全部子值引用。 */
static bool __xrtValueArrayBackingCopy(
	xvaluearraybacking* pTarget,
	const xvaluearraybacking* pSource
)
{
	if ( !xrtPtrArrayReserve(&pTarget->Items, pSource->Items.Count) ) {
		return false;
	}
	for ( size_t i = 0; i < pSource->Items.Count; i++ ) {
		xvalue* pItem = xrtValueRetain(
			(const xvalue*)xrtPtrArrayGet(&pSource->Items, i)
		);

		if ( pItem == NULL ) {
			return false;
		}
		if ( !xrtPtrArrayPush(&pTarget->Items, pItem) ) {
			xrtValueRelease(pItem);
			return false;
		}
	}
	return true;
}



/* 从旧 backing 浅拷贝 IntMap 并保持整数键顺序。 */
static bool __xrtValueIntMapBackingCopy(
	xvalueintmapbacking* pTarget,
	xvalueintmapbacking* pSource
)
{
	xintmapiter tIterator;
	ptr pSlot;
	int64 iKey;

	if ( !xrtIntMapIterBegin(&pSource->Items, &tIterator) ) {
		return false;
	}
	while ( (pSlot = xrtIntMapIterNext(&tIterator, &iKey)) != NULL ) {
		xvalue* pItem = xrtValueRetain(*(xvalue**)pSlot);

		if ( pItem == NULL ) {
			xrtIntMapIterEnd(&tIterator);
			return false;
		}
		if ( !xrtIntMapSetPtr(&pTarget->Items, iKey, pItem) ) {
			xrtValueRelease(pItem);
			xrtIntMapIterEnd(&tIterator);
			return false;
		}
	}
	xrtIntMapIterEnd(&tIterator);
	return true;
}



/* 从旧 backing 浅拷贝 Set 并保持规范值与插入顺序。 */
static bool __xrtValueSetBackingCopy(
	xvaluesetbacking* pTarget,
	xvaluesetbacking* pSource
)
{
	xsetiter tIterator;
	const void* pItem;

	if ( !xrtSetReserve(&pTarget->Items, xrtSetCount(&pSource->Items)) ||
		 !xrtSetIterBegin(&pSource->Items, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtSetIterNext(&tIterator)) != NULL ) {
		if ( !xrtSetAdd(&pTarget->Items, pItem) ) {
			xrtSetIterEnd(&tIterator);
			return false;
		}
	}
	xrtSetIterEnd(&tIterator);
	return true;
}



/* 从旧 backing 浅拷贝 Object 并保持首次插入顺序。 */
static bool __xrtValueObjectBackingCopy(
	xvalueobjectbacking* pTarget,
	xvalueobjectbacking* pSource
)
{
	xmapiter tIterator;
	xbytesview Key = {0};
	ptr pSlot;

	if ( !__xrtValueObjectBackingLifetimeCopy(pTarget, pSource) ||
		 !__xrtMapSetDropReverse(
			&pTarget->Items,
			__xrtMapDropsReverse(&pSource->Items)
		 ) ||
		 !xrtMapReserve(&pTarget->Items, xrtMapCount(&pSource->Items)) ||
		 !xrtMapIterBegin(&pSource->Items, &tIterator) ) {
		return false;
	}
	while ( (pSlot = xrtMapIterNext(&tIterator, &Key)) != NULL ) {
		xvalue* pItem = xrtValueRetain(*(xvalue**)pSlot);

		if ( pItem == NULL ) {
			xrtMapIterEnd(&tIterator);
			return false;
		}
		if ( !xrtMapSetPtr(&pTarget->Items, Key, pItem) ) {
			xrtValueRelease(pItem);
			xrtMapIterEnd(&tIterator);
			return false;
		}
	}
	xrtMapIterEnd(&tIterator);
	return true;
}



/* 创建内容相同但引用独立的浅拷贝 backing。 */
static xvaluebacking* __xrtValueBackingCopy(xvaluebacking* pSource)
{
	xvaluetype Type = (xvaluetype)pSource->Type;
	xvaluebacking* pTarget = __xrtValueBackingCreate(Type);
	bool bCopied;

	if ( pTarget == NULL ) {
		return NULL;
	}
	if ( Type == XVALUE_ARRAY ) {
		bCopied = __xrtValueArrayBackingCopy(
			(xvaluearraybacking*)pTarget,
			(const xvaluearraybacking*)pSource
		);
	} else if ( Type == XVALUE_INT_MAP ) {
		bCopied = __xrtValueIntMapBackingCopy(
			(xvalueintmapbacking*)pTarget,
			(xvalueintmapbacking*)pSource
		);
	} else if ( Type == XVALUE_SET ) {
		bCopied = __xrtValueSetBackingCopy(
			(xvaluesetbacking*)pTarget,
			(xvaluesetbacking*)pSource
		);
	} else {
		bCopied = __xrtValueObjectBackingCopy(
			(xvalueobjectbacking*)pTarget,
			(xvalueobjectbacking*)pSource
		);
	}
	if ( !bCopied ) {
		__xrtValueBackingRelease(pTarget);
		return NULL;
	}
	return pTarget;
}



/* 容器写入前按需分离共享 backing。 */
static bool __xrtValueEnsureUnique(xvalue* pValue)
{
	xvaluebacking* pOld;
	xvaluebacking* pCopy;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtErrorSetType();
		return false;
	}
	pOld = __xrtValueBacking(pValue, (xvaluetype)pValue->Type);
	if ( pOld == NULL ) {
		return false;
	}
	if ( (size_t)__xrtAtomicRefLoad(&pOld->RefCount) == pOld->OwnershipHolds + 1u ) {
		return true;
	}
	/* A finalizer denotes one reference-identity object.  Sharing its backing is
	 * allowed, but splitting it would duplicate a single destruction duty. */
	if ( pOld->Type == XVALUE_OBJECT &&
		 ((xvalueobjectbacking*)pOld)->FinalizerBound ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pValue->Flags |= XRT_VALUE_FLAG_BUSY;
	pCopy = __xrtValueBackingCopy(pOld);
	pValue->Flags &= ~XRT_VALUE_FLAG_BUSY;
	if ( pCopy == NULL ) {
		return false;
	}
	pValue->Data.Backing = pCopy;
	__xrtValueBackingRelease(pOld);
	return true;
}



/* 记录已经遍历的 backing，小图完全使用栈内地址。 */
static bool __xrtValueContainsVisit(
	xvaluecontainscontext* pContext,
	const xvaluebacking* pBacking,
	bool* pSeen
)
{
	const xvaluebacking* pKey = pBacking;

	*pSeen = false;
	for ( size_t i = 0; i < pContext->InlineCount; i++ ) {
		if ( pContext->Inline[i] == pBacking ) {
			*pSeen = true;
			return true;
		}
	}
	if ( pContext->InlineCount < XRT_VALUE_VISITED_INLINE ) {
		pContext->Inline[pContext->InlineCount++] = pBacking;
		return true;
	}
	if ( !pContext->OverflowReady ) {
		if ( !xrtSetInit(&pContext->Overflow, sizeof(pBacking)) ) {
			return false;
		}
		pContext->OverflowReady = true;
	}
	if ( xrtSetHas(&pContext->Overflow, &pKey) ) {
		*pSeen = true;
		return true;
	}
	return xrtSetAdd(&pContext->Overflow, &pKey);
}



/* 递归判断一个值图是否可达指定 backing 或值外壳。 */
static xvaluecontainsresult __xrtValueContains(
	const xvalue* pValue,
	xvaluecontainscontext* pContext,
	uint32 iDepth
)
{
	xvaluebacking* pBacking;
	bool bSeen;

	if ( (pContext->TargetValue != NULL) &&
		 (pValue == pContext->TargetValue) ) {
		return XVALUE_CONTAINS_YES;
	}
	if ( pValue == NULL ) {
		return XVALUE_CONTAINS_NO;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return XVALUE_CONTAINS_ERROR;
	}
	if ( !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		return XVALUE_CONTAINS_NO;
	}
	if ( iDepth >= XRT_VALUE_DEPTH_MAX ) {
		__xrtErrorSetValue();
		return XVALUE_CONTAINS_ERROR;
	}
	pBacking = pValue->Data.Backing;
	if ( (pBacking == NULL) || (pBacking->Type != pValue->Type) ||
		 (__xrtAtomicRefLoad(&pBacking->RefCount) <= 0) ) {
		__xrtErrorSetInvalidState();
		return XVALUE_CONTAINS_ERROR;
	}
	if ( (pContext->TargetBacking != NULL) &&
		 (pBacking == pContext->TargetBacking) ) {
		return XVALUE_CONTAINS_YES;
	}
	for ( uint32 i = 0; i < iDepth; i++ ) {
		if ( pContext->Active[i] == pBacking ) {
			__xrtErrorSetValue();
			return XVALUE_CONTAINS_ERROR;
		}
	}
	if ( !__xrtValueContainsVisit(pContext, pBacking, &bSeen) ) {
		return XVALUE_CONTAINS_ERROR;
	}
	if ( bSeen ) {
		return XVALUE_CONTAINS_NO;
	}
	pContext->Active[iDepth] = pBacking;
	if ( pBacking->Type == XVALUE_ARRAY ) {
		xvaluearraybacking* pArray = (xvaluearraybacking*)pBacking;

		for ( size_t i = 0; i < pArray->Items.Count; i++ ) {
			xvaluecontainsresult Result = __xrtValueContains(
				(const xvalue*)xrtPtrArrayGet(&pArray->Items, i),
				pContext,
				iDepth + 1u
			);

			if ( Result != XVALUE_CONTAINS_NO ) {
				return Result;
			}
		}
	} else if ( pBacking->Type == XVALUE_INT_MAP ) {
		xvalueintmapbacking* pMap = (xvalueintmapbacking*)pBacking;
		xintmapiter tIterator;
		ptr pSlot;

		if ( !xrtIntMapIterBegin(&pMap->Items, &tIterator) ) {
			return XVALUE_CONTAINS_ERROR;
		}
		while ( (pSlot = xrtIntMapIterNext(&tIterator, NULL)) != NULL ) {
			xvaluecontainsresult Result = __xrtValueContains(
				*(xvalue**)pSlot,
				pContext,
				iDepth + 1u
			);

			if ( Result != XVALUE_CONTAINS_NO ) {
				xrtIntMapIterEnd(&tIterator);
				return Result;
			}
		}
		xrtIntMapIterEnd(&tIterator);
	} else if ( pBacking->Type == XVALUE_SET ) {
		xvaluesetbacking* pSet = (xvaluesetbacking*)pBacking;
		xsetiter tIterator;
		const void* pItem;

		if ( !xrtSetIterBegin(&pSet->Items, &tIterator) ) {
			return XVALUE_CONTAINS_ERROR;
		}
		while ( (pItem = xrtSetIterNext(&tIterator)) != NULL ) {
			xvaluecontainsresult Result = __xrtValueContains(
				*(xvalue* const*)pItem,
				pContext,
				iDepth + 1u
			);

			if ( Result != XVALUE_CONTAINS_NO ) {
				xrtSetIterEnd(&tIterator);
				return Result;
			}
		}
		xrtSetIterEnd(&tIterator);
	} else if ( pBacking->Type == XVALUE_OBJECT ) {
		xvalueobjectbacking* pObject = (xvalueobjectbacking*)pBacking;
		xmapiter tIterator;
		ptr pSlot;

		if ( !xrtMapIterBegin(&pObject->Items, &tIterator) ) {
			return XVALUE_CONTAINS_ERROR;
		}
		while ( (pSlot = xrtMapIterNext(&tIterator, NULL)) != NULL ) {
			xvaluecontainsresult Result = __xrtValueContains(
				*(xvalue**)pSlot,
				pContext,
				iDepth + 1u
			);

			if ( Result != XVALUE_CONTAINS_NO ) {
				xrtMapIterEnd(&tIterator);
				return Result;
			}
		}
		xrtMapIterEnd(&tIterator);
	}
	return XVALUE_CONTAINS_NO;
}



/* 完成一次去重可达性检查并释放大型图的临时集合。 */
static xvaluecontainsresult __xrtValueContainsGraph(
	const xvalue* pValue,
	const xvaluebacking* pTargetBacking,
	const xvalue* pTargetValue
)
{
	xvaluecontainscontext tContext;
	xvaluecontainsresult Result;

	memset(&tContext, 0, sizeof(tContext));
	tContext.TargetBacking = pTargetBacking;
	tContext.TargetValue = pTargetValue;
	Result = __xrtValueContains(pValue, &tContext, 0);
	if ( tContext.OverflowReady ) {
		xrtSetUnit(&tContext.Overflow);
	}
	return Result;
}



/* 验证准备保存的值仍持有一个可移交或可增加的有效引用。 */
static bool __xrtValueStoreItemValid(const xvalue* pItem)
{
	if ( pItem == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pItem->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( ((pItem->Flags & XRT_VALUE_FLAG_STATIC) == 0) &&
		 (__xrtAtomicRefLoad(&pItem->RefCount) <= 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 写入前分离目标并拒绝形成引用计数环的值图。 */
static bool __xrtValuePrepareStore(xvalue* pTarget, const xvalue* pItem)
{
	xvaluecontainsresult Result;

	if ( !__xrtValueStoreItemValid(pItem) ) {
		return false;
	}
	if ( !__xrtValueEnsureUnique(pTarget) ) {
		return false;
	}
	if ( !__xrtValueContainerType((xvaluetype)pItem->Type) ) {
		return true;
	}
	Result = __xrtValueContainsGraph(
		pItem,
		pTarget->Data.Backing,
		NULL
	);
	if ( Result == XVALUE_CONTAINS_YES ) {
		__xrtErrorSetValue();
	}
	return Result == XVALUE_CONTAINS_NO;
}



/* 检查集合元素是否具有完整哈希与相等契约。 */
static bool __xrtValueSetItemValid(const xvalue* pItem)
{
	if ( !__xrtValueStoreItemValid(pItem) ) {
		return false;
	}
	if ( __xrtValueContainerType((xvaluetype)pItem->Type) ) {
		if ( (pItem->TypeId != 0) &&
			 (pItem->IdentityHash != NULL) &&
			 (pItem->IdentityEqual != NULL) ) {
			return true;
		}
		__xrtErrorSetType();
		return false;
	}
	if ( pItem->Type > XVALUE_HANDLE ) {
		__xrtErrorSetType();
		return false;
	}
	if ( (pItem->Type == XVALUE_HANDLE) &&
		 ((pItem->Data.Handle.Ops == NULL) ||
		  (pItem->Data.Handle.Ops->Hash == NULL) ||
		  (pItem->Data.Handle.Ops->Equal == NULL)) ) {
		__xrtErrorSetType();
		return false;
	}
	return true;
}



/* 把字符串键转换为 Map 使用的无所有权字节视图。 */
static xbytesview __xrtValueObjectKey(xstrview Key)
{
	xbytesview Result;

	Result.Data = (cbytes)Key.Data;
	Result.Size = Key.Size;
	return Result;
}



/* 验证对象键视图，空键合法，非空键必须具有数据地址。 */
static bool __xrtValueObjectKeyValid(xstrview Key)
{
	if ( (Key.Data == NULL) && (Key.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 将借用值升级为目标容器持有的一个引用。 */
static xvalue* __xrtValueStoreRetain(const xvalue* pItem)
{
	if ( !__xrtValueStoreItemValid(pItem) ) {
		return NULL;
	}
	return xrtValueRetain(pItem);
}



/* 验证 Take 来源槽独立于目标外壳和准备移交的值外壳。 */
static bool __xrtValueContainerTakeSlotValid(
	const xvalue* pTarget,
	xvalue* const* pItem
)
{
	xvalue* pSource;

	if ( (pTarget == NULL) || (pItem == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pTarget,
		sizeof(xvalue),
		pItem,
		sizeof(*pItem)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pSource = *pItem;
	if ( (pSource == NULL) || __xrtRangesOverlap(
		pSource,
		sizeof(xvalue),
		pItem,
		sizeof(*pItem)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtValueStoreItemValid(pSource);
}



/* 验证 Edit 目标确实是仍可读取的子容器。 */
static bool __xrtValueEditItemValid(const xvalue* pItem)
{
	if ( pItem == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pItem->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !__xrtValueContainerType((xvaluetype)pItem->Type) ) {
		__xrtErrorSetType();
		return false;
	}
	return true;
}



/* 把容器槽中的子容器替换为独立 COW 外壳。 */
static xvalue* __xrtValueEditSlot(xvalue** pSlot)
{
	xvalue* pOld;
	xvalue* pCopy;
	int32 iReferences;

	if ( (pSlot == NULL) || !__xrtValueEditItemValid(*pSlot) ) {
		return NULL;
	}
	pOld = *pSlot;
	iReferences = __xrtAtomicRefLoad(&pOld->RefCount);
	if ( iReferences <= 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( iReferences == 1 ) {
		return pOld;
	}
	pCopy = xrtValueClone(pOld);
	if ( pCopy == NULL ) {
		return NULL;
	}
	*pSlot = pCopy;
	xrtValueRelease(pOld);
	return pCopy;
}



/* 释放一个值外壳持有的容器 backing。 */
void __xrtValueContainerRelease(xvalue* pValue, xrtownershipscope* pMutation)
{
	__xrtValueBackingReleaseView(pValue->Data.Backing, pValue, pMutation);
	pValue->Data.Backing = NULL;
}



/* 为容器创建共享 backing 的独立外壳。 */
xvalue* __xrtValueContainerClone(const xvalue* pValue)
{
	xvaluebacking* pBacking;
	xvalue* pCopy;

	if ( (pValue == NULL) ||
		 !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtErrorSetType();
		return NULL;
	}
	pBacking = __xrtValueBacking(pValue, (xvaluetype)pValue->Type);
	if ( pBacking == NULL ) {
		return NULL;
	}
	pCopy = __xrtValueCreate((xvaluetype)pValue->Type);
	if ( pCopy == NULL ) {
		return NULL;
	}
	if ( !__xrtValueBackingRetain(pBacking) ) {
		xrtFree(pCopy);
		return NULL;
	}
	pCopy->Data.Backing = pBacking;
	pCopy->TypeId = pValue->TypeId;
	pCopy->IdentityHash = pValue->IdentityHash;
	pCopy->IdentityEqual = pValue->IdentityEqual;
	pCopy->IdentityUserData = pValue->IdentityUserData;
	return pCopy;
}



/* 返回容器真值使用的元素数量。 */
size_t __xrtValueContainerCount(const xvalue* pValue)
{
	xvaluebacking* pBacking = pValue != NULL ? pValue->Data.Backing : NULL;

	if ( pBacking == NULL ) {
		return 0;
	}
	switch ( (xvaluetype)pBacking->Type ) {
		case XVALUE_ARRAY:
			return ((xvaluearraybacking*)pBacking)->Items.Count;
		case XVALUE_INT_MAP:
			return xrtIntMapCount(&((xvalueintmapbacking*)pBacking)->Items);
		case XVALUE_SET:
			return xrtSetCount(&((xvaluesetbacking*)pBacking)->Items);
		case XVALUE_OBJECT:
			return xrtMapCount(&((xvalueobjectbacking*)pBacking)->Items);
		default:
			return 0;
	}
}



/* 借用 Value Set 的底层集合，供集合关系和图层复用通用 Set 实现。 */
const xset* __xrtValueSetItems(const xvalue* pValue)
{
	xvaluesetbacking* pBacking = (xvaluesetbacking*)__xrtValueBacking(
		pValue,
		XVALUE_SET
	);

	return pBacking != NULL ? &pBacking->Items : NULL;
}



/* 查询 Object backing 的拥有值释放顺序。 */
bool __xrtValueObjectDropsReverse(const xvalue* pValue)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pValue,
		XVALUE_OBJECT
	);

	return (pBacking != NULL) && __xrtMapDropsReverse(&pBacking->Items);
}



#if defined(XRT_FEATURE_VALUE_COLLECTION)

/* 把准备容器的完整 backing 原子提交给同类型目标。 */
bool __xrtValueContainerCommit(xvalue* pTarget, xvalue* pPrepared)
{
	xvaluebacking* pTargetBacking;
	xvaluebacking* pPreparedBacking;
	xvaluecontainsresult Result;

	if ( (pTarget == NULL) || (pPrepared == NULL) || (pTarget == pPrepared) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pTarget->Type != pPrepared->Type) ||
		 !__xrtValueContainerType((xvaluetype)pTarget->Type) ) {
		__xrtErrorSetType();
		return false;
	}
	pTargetBacking = __xrtValueBacking(
		pTarget,
		(xvaluetype)pTarget->Type
	);
	pPreparedBacking = __xrtValueBacking(
		pPrepared,
		(xvaluetype)pPrepared->Type
	);
	if ( (pTargetBacking == NULL) || (pPreparedBacking == NULL) ) {
		return false;
	}

	/* 提交会改变目标外壳指向，必须额外拒绝准备图对该外壳的引用。 */
	Result = __xrtValueContainsGraph(pPrepared, NULL, pTarget);
	if ( Result == XVALUE_CONTAINS_YES ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( Result == XVALUE_CONTAINS_ERROR ) {
		return false;
	}
	/* Field mutation is allowed during finalization, replacing the actual
	 * borrowed receiver's entire backing is not. */
	if (((pTarget->Flags | pPrepared->Flags) & XRT_VALUE_FLAG_FINALIZING) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	if ( pTarget->Type == XVALUE_OBJECT ) {
		bool bTargetReverse = __xrtMapDropsReverse(
			&((xvalueobjectbacking*)pTargetBacking)->Items
		);
		bool bPreparedReverse = __xrtMapDropsReverse(
			&((xvalueobjectbacking*)pPreparedBacking)->Items
		);

		if ( bTargetReverse != bPreparedReverse ) {
			if ( !__xrtValueEnsureUnique(pPrepared) ) {
				return false;
			}
			pPreparedBacking = pPrepared->Data.Backing;
			if ( !__xrtMapSetDropReverse(
					&((xvalueobjectbacking*)pPreparedBacking)->Items,
					bTargetReverse
			) ) {
				return false;
			}
		}
	}
	pTarget->Data.Backing = pPreparedBacking;
	pPrepared->Data.Backing = pTargetBacking;
	return true;
}



/* 消费通用 Set 运算结果并包装成 Value Set。 */
xvalue* __xrtValueSetAdopt(xset* pItems)
{
	xvalue* pValue;
	xvaluesetbacking* pBacking;

	if ( pItems == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pValue = xrtValueSet();
	if ( pValue == NULL ) {
		xrtSetDestroy(pItems);
		return NULL;
	}
	pBacking = (xvaluesetbacking*)pValue->Data.Backing;
	xrtSetUnit(&pBacking->Items);
	pBacking->Items = *pItems;
	xrtFree(pItems);
	return pValue;
}

#endif



/* 创建空的稠密动态值数组。 */
static xvalue* __xrtOwnershipBody_ValueArray(void)
{
	return __xrtValueContainerCreate(XVALUE_ARRAY);
}

XRT_API xvalue* xrtValueArray(void)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueArray());
}



/* 创建空的 int64 键稀疏映射。 */
static xvalue* __xrtOwnershipBody_ValueIntMap(void)
{
	return __xrtValueContainerCreate(XVALUE_INT_MAP);
}

XRT_API xvalue* xrtValueIntMap(void)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueIntMap());
}



/* 创建空的可哈希动态值集合。 */
static xvalue* __xrtOwnershipBody_ValueSet(void)
{
	return __xrtValueContainerCreate(XVALUE_SET);
}

XRT_API xvalue* xrtValueSet(void)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueSet());
}



/* 创建保持首次插入顺序的字符串键对象。 */
static xvalue* __xrtOwnershipBody_ValueObject(void)
{
	return __xrtValueContainerCreate(XVALUE_OBJECT);
}

XRT_API xvalue* xrtValueObject(void)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueObject());
}



/* 创建遍历顺序稳定、拥有值按栈顺序析构的对象。 */
static xvalue* __xrtOwnershipBody_ValueObjectLifo(void)
{
	xvalue* pValue = __xrtValueContainerCreate(XVALUE_OBJECT);
	xvaluebacking* pBacking;

	if ( pValue == NULL ) {
		return NULL;
	}
	pBacking = __xrtValueBacking(pValue, XVALUE_OBJECT);
	if ( (pBacking == NULL) ||
		 !__xrtMapSetDropReverse(
			&((xvalueobjectbacking*)pBacking)->Items,
			true
		 ) ) {
		xrtValueRelease(pValue);
		return NULL;
	}
	return pValue;
}

XRT_API xvalue* xrtValueObjectLifo(void)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueObjectLifo());
}



/* Bind one finalization duty to a unique Object backing. */
static bool __xrtOwnershipBody_ValueObjectFinalizerBind(
	xvalue* pObject,
	xvalueobjectfinalizer pFinalizer,
	ptr pUserData
)
{
	xvalueobjectbacking* pBacking;

	if ( (pObject == NULL) || (pFinalizer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pObject->Flags &
		  (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	if ( pBacking == NULL ) {
		return false;
	}
	if ( (__xrtAtomicRefLoad(&pBacking->Base.RefCount) != 1) ||
		 (pBacking->FinalizerBound || pBacking->FinalizerPrepared) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pBacking->Finalizer = pFinalizer;
	pBacking->FinalizerUserData = pUserData;
	pBacking->FinalizerBound = true;
	return true;
}

XRT_API bool xrtValueObjectFinalizerBind(
	xvalue* pObject,
	xvalueobjectfinalizer pFinalizer,
	ptr pUserData
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectFinalizerBind(pObject, pFinalizer, pUserData));
}



static xvalue* __xrtOwnershipBody_ValueObjectFinalizerBindTake(
	xvalue* pObject, xvalueobjectfinalizer pFinalizer, ptr pUserData)
{
	xerror* pPrior = xrtTakeError();
	xerror* pFailure;
	if (xrtValueObjectFinalizerBind(pObject, pFinalizer, pUserData)) {
		if (pPrior != NULL) { xrtClearError(); xrtSetErrorTake(pPrior); }
		return pObject;
	}
	pFailure = xrtTakeError();
	xrtValueRelease(pObject);
	xrtClearError();
	if (pPrior != NULL) { xrtErrorFree(pFailure); pFailure = pPrior; }
	xrtSetErrorTake(pFailure);
	return NULL;
}

XRT_API xvalue* xrtValueObjectFinalizerBindTake(
	xvalue* pObject, xvalueobjectfinalizer pFinalizer, ptr pUserData)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueObjectFinalizerBindTake(pObject, pFinalizer, pUserData));
}

static bool __xrtOwnershipBody_ValueObjectFinalizerOwnershipBind(xvalue* pObject, xrtownershiptrace pTrace)
{
	xvalueobjectbacking* pBacking;
	if (pObject == NULL || pTrace == NULL ||
		__xrtAtomicRefLoad(&pObject->RefCount) != 1 ||
		(pObject->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(pObject, XVALUE_OBJECT);
	if (pBacking == NULL) return false;
	if (__xrtAtomicRefLoad(&pBacking->Base.RefCount) != 1 ||
		pBacking->Finalizer == NULL || pBacking->FinalizerTrace != NULL) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking->FinalizerTrace = pTrace;
	return true;
}

XRT_API bool xrtValueObjectFinalizerOwnershipBind(xvalue* pObject, xrtownershiptrace pTrace)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectFinalizerOwnershipBind(pObject, pTrace));
}

static bool __xrtValueObjectFinalizerBindOwned(xvalue* pObject,
	xvalueobjectfinalizer pFinalizer, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease, bool bPrepared)
{
	xvalueobjectbacking* pBacking;
	if (pObject == NULL || pFinalizer == NULL || pRelease == NULL ||
		(pUserData != NULL && pTrace == NULL)) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (__xrtAtomicRefLoad(&pObject->RefCount) != 1 ||
		(pObject->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(pObject, XVALUE_OBJECT);
	if (pBacking == NULL) return false;
	if (__xrtAtomicRefLoad(&pBacking->Base.RefCount) != 1 || pBacking->FinalizerBound || pBacking->FinalizerPrepared) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking->Finalizer = pFinalizer;
	pBacking->FinalizerUserData = pUserData;
	pBacking->FinalizerTrace = pTrace;
	pBacking->FinalizerRelease = pRelease;
	pBacking->FinalizerBound = true;
	pBacking->FinalizerPrepared = bPrepared;
	return true;
}

static bool __xrtOwnershipBody_ValueObjectFinalizerBindOwned(xvalue* pObject,
	xvalueobjectfinalizer pFinalizer, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	return __xrtValueObjectFinalizerBindOwned(pObject, pFinalizer, pUserData, pTrace, pRelease, false);
}

XRT_API bool xrtValueObjectFinalizerBindOwned(xvalue* pObject,
	xvalueobjectfinalizer pFinalizer, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectFinalizerBindOwned(pObject, pFinalizer, pUserData, pTrace, pRelease));
}

static bool __xrtOwnershipBody_ValueObjectFinalizerPrepareOwned(xvalue* pObject,
	xvalueobjectfinalizer pFinalizer, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	return __xrtValueObjectFinalizerBindOwned(pObject, pFinalizer, pUserData, pTrace, pRelease, true);
}

XRT_API bool xrtValueObjectFinalizerPrepareOwned(xvalue* pObject,
	xvalueobjectfinalizer pFinalizer, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectFinalizerPrepareOwned(pObject, pFinalizer, pUserData, pTrace, pRelease));
}

static bool __xrtValueObjectConstructionCommit(xvalue* pObject, bool bRequireFinalizer)
{
	xvalueobjectbacking* pBacking;
	if (pObject == NULL) { __xrtErrorSetInvalidArgument(); return false; }
	if ((pObject->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(pObject, XVALUE_OBJECT);
	if (pBacking == NULL) return false;
	if (!pBacking->FinalizerPrepared || (bRequireFinalizer && !pBacking->FinalizerBound) ||
		(pBacking->FinalizerBound && (pBacking->Finalizer == NULL || pBacking->FinalizerRelease == NULL))) {
		__xrtErrorSetInvalidState(); return false;
	}
	/* Caller retains a live shell throughout this serialized transition, so
	 * concurrent releases of other owners cannot reach backing finalization. */
	pBacking->FinalizerPrepared = false;
	return true;
}

static bool __xrtOwnershipBody_ValueObjectConstructionPrepare(xvalue* pObject)
{
	xvalueobjectbacking* pBacking;
	if (pObject == NULL) { __xrtErrorSetInvalidArgument(); return false; }
	if (__xrtAtomicRefLoad(&pObject->RefCount) != 1 ||
		(pObject->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(pObject, XVALUE_OBJECT);
	if (pBacking == NULL) return false;
	if (__xrtAtomicRefLoad(&pBacking->Base.RefCount) != 1 || pBacking->FinalizerBound || pBacking->FinalizerPrepared) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking->FinalizerPrepared = true; return true;
}

XRT_API bool xrtValueObjectFinalizerCommit(xvalue* pObject)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtValueObjectConstructionCommit(pObject, true));
}

XRT_API bool xrtValueObjectConstructionCommit(xvalue* pObject)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtValueObjectConstructionCommit(pObject, false));
}

XRT_API bool xrtValueObjectConstructionPrepare(xvalue* pObject)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectConstructionPrepare(pObject));
}

static bool __xrtOwnershipBody_ValueObjectLifetimeBindOwned(xvalue* pObject, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	xvalueobjectbacking* pBacking;
	xvalueobjectlifetime* pLifetime;
	if (pObject == NULL || pRelease == NULL || (pUserData != NULL && pTrace == NULL)) {
		__xrtErrorSetInvalidArgument(); return false;
	}
	if (__xrtAtomicRefLoad(&pObject->RefCount) != 1 ||
		(pObject->Flags & (XRT_VALUE_FLAG_BUSY | XRT_VALUE_FLAG_FINALIZING)) != 0) {
		__xrtErrorSetInvalidState(); return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(pObject, XVALUE_OBJECT);
	if (pBacking == NULL) return false;
	if (__xrtAtomicRefLoad(&pBacking->Base.RefCount) != 1 || pBacking->Lifetime != NULL) {
		__xrtErrorSetInvalidState(); return false;
	}
	pLifetime = (xvalueobjectlifetime*)xrtCalloc(1, sizeof(*pLifetime));
	if (pLifetime == NULL) return false;
	pLifetime->RefCount = 1; pLifetime->UserData = pUserData;
	pLifetime->Trace = pTrace; pLifetime->Release = pRelease;
	pBacking->Lifetime = pLifetime;
	return true;
}

XRT_API bool xrtValueObjectLifetimeBindOwned(xvalue* pObject, ptr pUserData,
	xrtownershiptrace pTrace, xvalueobjectfinalizerrelease pRelease)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectLifetimeBindOwned(pObject, pUserData, pTrace, pRelease));
}

bool __xrtValueObjectLifetimeCopy(xvalue* pTarget, const xvalue* pSource)
{
	xvalueobjectbacking* pTargetBacking;
	const xvalueobjectbacking* pSourceBacking;
	if (pSource->Type != XVALUE_OBJECT) return true;
	pTargetBacking = (xvalueobjectbacking*)__xrtValueBacking(pTarget, XVALUE_OBJECT);
	pSourceBacking = (const xvalueobjectbacking*)__xrtValueBacking(pSource, XVALUE_OBJECT);
	return pTargetBacking != NULL && pSourceBacking != NULL &&
		__xrtValueObjectBackingLifetimeCopy(pTargetBacking, pSourceBacking);
}

/* 返回任一基础容器的元素数。 */
XRT_API size_t xrtValueCount(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtErrorSetType();
		return 0;
	}
	if ( __xrtValueBacking(pValue, (xvaluetype)pValue->Type) == NULL ) {
		return 0;
	}
	return __xrtValueContainerCount(pValue);
}



/* 返回可预留基础容器的当前容量。 */
XRT_API size_t xrtValueCapacity(const xvalue* pValue)
{
	xvaluetype Type;
	xvaluebacking* pBacking;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	Type = (xvaluetype)pValue->Type;
	if ( !__xrtValueContainerType(Type) ) {
		__xrtErrorSetType();
		return 0;
	}
	pBacking = __xrtValueBacking(pValue, Type);
	if ( pBacking == NULL ) {
		return 0;
	}
	if ( Type == XVALUE_ARRAY ) {
		return ((xvaluearraybacking*)pBacking)->Items.Capacity;
	}
	if ( Type == XVALUE_SET ) {
		return xrtSetCapacity(&((xvaluesetbacking*)pBacking)->Items);
	}
	if ( Type == XVALUE_OBJECT ) {
		return xrtMapCapacity(&((xvalueobjectbacking*)pBacking)->Items);
	}
	__xrtErrorSetUnsupported();
	return 0;
}



/* 保证容器至少可容纳指定数量的元素。 */
static bool __xrtOwnershipBody_ValueReserve(xvalue* pValue, size_t iCapacity)
{
	xvaluetype Type;
	xvaluebacking* pBacking;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	Type = (xvaluetype)pValue->Type;
	if ( !__xrtValueContainerType(Type) ) {
		__xrtErrorSetType();
		return false;
	}
	pBacking = __xrtValueBacking(pValue, Type);
	if ( pBacking == NULL ) {
		return false;
	}
	if ( Type == XVALUE_INT_MAP ) {
		__xrtErrorSetUnsupported();
		return false;
	}
	if ( (Type == XVALUE_ARRAY) &&
		 (iCapacity <= ((xvaluearraybacking*)pBacking)->Items.Capacity) ) {
		return true;
	}
	if ( (Type == XVALUE_SET) &&
		 (iCapacity <= xrtSetCapacity(&((xvaluesetbacking*)pBacking)->Items)) ) {
		return true;
	}
	if ( (Type == XVALUE_OBJECT) &&
		 (iCapacity <= xrtMapCapacity(&((xvalueobjectbacking*)pBacking)->Items)) ) {
		return true;
	}
	if ( !__xrtValueEnsureUnique(pValue) ) {
		return false;
	}
	pBacking = pValue->Data.Backing;
	if ( pBacking->Type == XVALUE_ARRAY ) {
		return xrtPtrArrayReserve(
			&((xvaluearraybacking*)pBacking)->Items,
			iCapacity
		);
	}
	if ( pBacking->Type == XVALUE_SET ) {
		return xrtSetReserve(
			&((xvaluesetbacking*)pBacking)->Items,
			iCapacity
		);
	}
	return xrtMapReserve(
		&((xvalueobjectbacking*)pBacking)->Items,
		iCapacity
	);
}

XRT_API bool xrtValueReserve(xvalue* pValue, size_t iCapacity)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueReserve(pValue, iCapacity));
}



/* 释放容器多余容量，保留现有元素。 */
static bool __xrtOwnershipBody_ValueTrim(xvalue* pValue)
{
	xvaluebacking* pBacking;

	if ( !__xrtValueEnsureUnique(pValue) ) {
		return false;
	}
	pBacking = pValue->Data.Backing;
	if ( pBacking->Type == XVALUE_ARRAY ) {
		return xrtPtrArrayTrim(&((xvaluearraybacking*)pBacking)->Items);
	}
	if ( pBacking->Type == XVALUE_INT_MAP ) {
		(void)xrtIntMapTrim(&((xvalueintmapbacking*)pBacking)->Items, 0);
		return true;
	}
	if ( pBacking->Type == XVALUE_SET ) {
		return xrtSetTrim(&((xvaluesetbacking*)pBacking)->Items);
	}
	return xrtMapTrim(&((xvalueobjectbacking*)pBacking)->Items);
}

XRT_API bool xrtValueTrim(xvalue* pValue)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueTrim(pValue));
}



/* 释放动态 IntMap 的空闲节点池页。 */
static size_t __xrtOwnershipBody_ValueIntMapTrim(xvalue* pMap, size_t iRetainEmpty)
{
	xvalueintmapbacking* pBacking;

	if ( pMap == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( pMap->Type != XVALUE_INT_MAP ) {
		__xrtErrorSetType();
		return 0;
	}
	if ( !__xrtValueEnsureUnique(pMap) ) {
		return 0;
	}
	pBacking = (xvalueintmapbacking*)pMap->Data.Backing;
	return xrtIntMapTrim(&pBacking->Items, iRetainEmpty);
}

XRT_API size_t xrtValueIntMapTrim(xvalue* pMap, size_t iRetainEmpty)
{
	XRT_VALUE_MUTATION_RETURN(size_t, __xrtOwnershipBody_ValueIntMapTrim(pMap, iRetainEmpty));
}



/* 清空容器并释放其中持有的全部值引用。 */
static bool __xrtOwnershipBody_ValueClear(xvalue* pValue)
{
	xvaluebacking* pBacking;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtErrorSetType();
		return false;
	}
	pBacking = __xrtValueBacking(pValue, (xvaluetype)pValue->Type);
	if ( pBacking == NULL ) {
		return false;
	}
	if ( __xrtValueContainerCount(pValue) == 0 ) {
		return true;
	}
	if ( !__xrtValueEnsureUnique(pValue) ) {
		return false;
	}
	pBacking = pValue->Data.Backing;
	pValue->Flags |= XRT_VALUE_FLAG_BUSY;
	if ( pBacking->Type == XVALUE_ARRAY ) {
		xvaluearraybacking* pArray = (xvaluearraybacking*)pBacking;

		for ( size_t i = 0; i < pArray->Items.Count; i++ ) {
			xrtValueRelease((xvalue*)xrtPtrArrayGet(&pArray->Items, i));
		}
		xrtPtrArrayClear(&pArray->Items);
	} else if ( pBacking->Type == XVALUE_INT_MAP ) {
		xrtIntMapClear(&((xvalueintmapbacking*)pBacking)->Items);
	} else if ( pBacking->Type == XVALUE_SET ) {
		xrtSetClear(&((xvaluesetbacking*)pBacking)->Items);
	} else {
		xrtMapClear(&((xvalueobjectbacking*)pBacking)->Items);
	}
	pValue->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return true;
}

XRT_API bool xrtValueClear(xvalue* pValue)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueClear(pValue));
}



/* 把正负数组索引解析为现有元素的 0 基位置。 */
static bool __xrtValueArrayResolve(
	const xvaluearraybacking* pBacking,
	int64 iIndex,
	size_t* pResolved
)
{
	size_t iResolved;

	if ( iIndex >= 0 ) {
		if ( (uint64)iIndex > (uint64)SIZE_MAX ) {
			__xrtErrorSetRange();
			return false;
		}
		iResolved = (size_t)iIndex;
	} else {
		uint64 iOffset = (uint64)(-(iIndex + 1)) + 1u;

		if ( iOffset > pBacking->Items.Count ) {
			__xrtErrorSetRange();
			return false;
		}
		iResolved = pBacking->Items.Count - (size_t)iOffset;
	}
	if ( iResolved >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	*pResolved = iResolved;
	return true;
}



/* 公开解析正负数组索引，失败时保持输出不变。 */
XRT_API bool xrtValueArrayResolve(
	const xvalue* pArray,
	int64 iIndex,
	size_t* pResolved
)
{
	xvaluearraybacking* pBacking;
	size_t iResolved;

	if ( pResolved == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pArray != NULL) && __xrtRangesOverlap(
		pArray,
		sizeof(xvalue),
		pResolved,
		sizeof(*pResolved)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	if ( (pBacking == NULL) ||
		 !__xrtValueArrayResolve(pBacking, iIndex, &iResolved) ) {
		return false;
	}
	*pResolved = iResolved;
	return true;
}



/* 返回数组指定 0 基索引处借用的值。 */
XRT_API xvalue* xrtValueArrayGet(const xvalue* pArray, size_t iIndex)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);

	if ( pBacking == NULL ) {
		return NULL;
	}
	return (xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex);
}



/* 支持负数倒序索引。 */
XRT_API xvalue* xrtValueArrayAt(const xvalue* pArray, int64 iIndex)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	size_t iResolved;

	if ( (pBacking == NULL) ||
		 !__xrtValueArrayResolve(pBacking, iIndex, &iResolved) ) {
		return NULL;
	}
	return (xvalue*)xrtPtrArrayGet(&pBacking->Items, iResolved);
}



/* 返回已经沿 COW 路径分离的可变数组子容器。 */
static xvalue* __xrtOwnershipBody_ValueArrayEdit(xvalue* pArray, size_t iIndex)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue** pItems;

	if ( pBacking == NULL ) {
		return NULL;
	}
	if ( iIndex >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return NULL;
	}
	if ( !__xrtValueEditItemValid(
		(const xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex)
	) ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pArray) ) {
		return NULL;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	pItems = (xvalue**)xrtPtrArrayData(&pBacking->Items);
	return __xrtValueEditSlot(&pItems[iIndex]);
}

XRT_API xvalue* xrtValueArrayEdit(xvalue* pArray, size_t iIndex)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueArrayEdit(pArray, iIndex));
}



/* 增加引用后向数组末尾加入值。 */
static bool __xrtOwnershipBody_ValueArrayAppend(xvalue* pArray, const xvalue* pItem)
{
	xvaluearraybacking* pBacking;
	xvalue* pStored;

	if ( !__xrtValuePrepareStore(pArray, pItem) ) {
		return false;
	}
	pStored = __xrtValueStoreRetain(pItem);
	if ( pStored == NULL ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	if ( !xrtPtrArrayPush(&pBacking->Items, pStored) ) {
		xrtValueRelease(pStored);
		return false;
	}
	return true;
}

XRT_API bool xrtValueArrayAppend(xvalue* pArray, const xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayAppend(pArray, pItem));
}



/* 成功时把来源引用移交给数组。 */
static bool __xrtOwnershipBody_ValueArrayAppendTake(xvalue* pArray, xvalue** pItem)
{
	xvaluearraybacking* pBacking;

	if ( !__xrtValueContainerTakeSlotValid(pArray, pItem) ) {
		return false;
	}
	if ( !__xrtValuePrepareStore(pArray, *pItem) ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	if ( !xrtPtrArrayPush(&pBacking->Items, *pItem) ) {
		return false;
	}
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueArrayAppendTake(xvalue* pArray, xvalue** pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayAppendTake(pArray, pItem));
}



/* 无论成功失败都消费临时值。 */
static bool __xrtOwnershipBody_ValueArrayAppendNew(xvalue* pArray, xvalue* pItem)
{
	bool bResult = (pItem != NULL) && xrtValueArrayAppendTake(pArray, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueArrayAppendNew(xvalue* pArray, xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayAppendNew(pArray, pItem));
}



/* 增加引用后在指定位置插入值。 */
static bool __xrtOwnershipBody_ValueArrayInsert(
	xvalue* pArray,
	size_t iIndex,
	const xvalue* pItem
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue* pStored;

	if ( pBacking == NULL ) {
		return false;
	}
	if ( iIndex > pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtValuePrepareStore(pArray, pItem) ) {
		return false;
	}
	pStored = __xrtValueStoreRetain(pItem);
	if ( pStored == NULL ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	if ( !xrtPtrArrayInsert(&pBacking->Items, iIndex, pStored) ) {
		xrtValueRelease(pStored);
		return false;
	}
	return true;
}

XRT_API bool xrtValueArrayInsert(
	xvalue* pArray,
	size_t iIndex,
	const xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayInsert(pArray, iIndex, pItem));
}



/* 成功时把来源引用移交到指定插入位置。 */
static bool __xrtOwnershipBody_ValueArrayInsertTake(
	xvalue* pArray,
	size_t iIndex,
	xvalue** pItem
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);

	if ( (pBacking == NULL) ||
		 !__xrtValueContainerTakeSlotValid(pArray, pItem) ) {
		return false;
	}
	if ( iIndex > pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtValuePrepareStore(pArray, *pItem) ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	if ( !xrtPtrArrayInsert(&pBacking->Items, iIndex, *pItem) ) {
		return false;
	}
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueArrayInsertTake(
	xvalue* pArray,
	size_t iIndex,
	xvalue** pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayInsertTake(pArray, iIndex, pItem));
}



/* 无论成功失败都消费临时值并在指定位置插入。 */
static bool __xrtOwnershipBody_ValueArrayInsertNew(
	xvalue* pArray,
	size_t iIndex,
	xvalue* pItem
)
{
	bool bResult = (pItem != NULL) &&
		xrtValueArrayInsertTake(pArray, iIndex, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueArrayInsertNew(
	xvalue* pArray,
	size_t iIndex,
	xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayInsertNew(pArray, iIndex, pItem));
}



/* 替换数组值的共同移交路径。 */
static bool __xrtValueArraySetOwned(
	xvalue* pArray,
	size_t iIndex,
	xvalue* pItem
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue** pItems;
	xvalue* pOld;

	if ( (pBacking == NULL) || !__xrtValueStoreItemValid(pItem) ) {
		return false;
	}
	if ( iIndex >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	pOld = (xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex);
	if ( pOld == pItem ) {
		xrtValueRelease(pItem);
		return true;
	}
	if ( !__xrtValuePrepareStore(pArray, pItem) ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	pItems = (xvalue**)xrtPtrArrayData(&pBacking->Items);
	pOld = pItems[iIndex];
	pItems[iIndex] = pItem;
	pArray->Flags |= XRT_VALUE_FLAG_BUSY;
	xrtValueRelease(pOld);
	pArray->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return true;
}



/* 增加引用后替换指定位置的旧值。 */
static bool __xrtOwnershipBody_ValueArraySet(
	xvalue* pArray,
	size_t iIndex,
	const xvalue* pItem
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue* pStored;

	if ( pBacking == NULL ) {
		return false;
	}
	if ( iIndex >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	pStored = __xrtValueStoreRetain(pItem);
	if ( pStored == NULL ) {
		return false;
	}
	if ( !__xrtValueArraySetOwned(pArray, iIndex, pStored) ) {
		xrtValueRelease(pStored);
		return false;
	}
	return true;
}

XRT_API bool xrtValueArraySet(
	xvalue* pArray,
	size_t iIndex,
	const xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArraySet(pArray, iIndex, pItem));
}



/* 成功时把来源引用移交到指定位置。 */
static bool __xrtOwnershipBody_ValueArraySetTake(
	xvalue* pArray,
	size_t iIndex,
	xvalue** pItem
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);

	if ( (pBacking == NULL) ||
		 !__xrtValueContainerTakeSlotValid(pArray, pItem) ) {
		return false;
	}
	if ( iIndex >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( !__xrtValueArraySetOwned(pArray, iIndex, *pItem) ) {
		return false;
	}
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueArraySetTake(
	xvalue* pArray,
	size_t iIndex,
	xvalue** pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArraySetTake(pArray, iIndex, pItem));
}



/* 无论成功失败都消费临时值并替换指定位置。 */
static bool __xrtOwnershipBody_ValueArraySetNew(
	xvalue* pArray,
	size_t iIndex,
	xvalue* pItem
)
{
	bool bResult = (pItem != NULL) &&
		xrtValueArraySetTake(pArray, iIndex, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueArraySetNew(
	xvalue* pArray,
	size_t iIndex,
	xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArraySetNew(pArray, iIndex, pItem));
}



/* 删除数组区间并释放其中的值。 */
static bool __xrtOwnershipBody_ValueArrayRemove(
	xvalue* pArray,
	size_t iIndex,
	size_t iCount
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);

	if ( pBacking == NULL ) {
		return false;
	}
	if ( (iIndex > pBacking->Items.Count) ||
		 (iCount > (pBacking->Items.Count - iIndex)) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iCount == 0 ) {
		return true;
	}
	if ( !__xrtValueEnsureUnique(pArray) ) {
		return false;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	pArray->Flags |= XRT_VALUE_FLAG_BUSY;
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtValueRelease((xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex + i));
	}
	if ( !xrtPtrArrayRemove(&pBacking->Items, iIndex, iCount) ) {
		pArray->Flags &= ~XRT_VALUE_FLAG_BUSY;
		return false;
	}
	pArray->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return true;
}

XRT_API bool xrtValueArrayRemove(
	xvalue* pArray,
	size_t iIndex,
	size_t iCount
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArrayRemove(pArray, iIndex, iCount));
}



/* 从数组移交指定值。 */
static xvalue* __xrtOwnershipBody_ValueArrayTake(xvalue* pArray, size_t iIndex)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue* pItem;

	if ( pBacking == NULL ) {
		return NULL;
	}
	if ( iIndex >= pBacking->Items.Count ) {
		__xrtErrorSetRange();
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pArray) ) {
		return NULL;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	pItem = (xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex);
	if ( !xrtPtrArrayRemove(&pBacking->Items, iIndex, 1) ) {
		return NULL;
	}
	return pItem;
}

XRT_API xvalue* xrtValueArrayTake(xvalue* pArray, size_t iIndex)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueArrayTake(pArray, iIndex));
}



/* 从数组末尾移交一个值。 */
static xvalue* __xrtOwnershipBody_ValueArrayPop(xvalue* pArray)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);
	xvalue* pItem = NULL;

	if ( pBacking == NULL ) {
		return NULL;
	}
	if ( pBacking->Items.Count == 0 ) {
		__xrtErrorSetRange();
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pArray) ) {
		return NULL;
	}
	pBacking = (xvaluearraybacking*)pArray->Data.Backing;
	if ( !xrtPtrArrayPop(&pBacking->Items, (ptr*)&pItem) ) {
		return NULL;
	}
	return pItem;
}

XRT_API xvalue* xrtValueArrayPop(xvalue* pArray)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueArrayPop(pArray));
}



/* 交换两个数组元素。 */
static bool __xrtOwnershipBody_ValueArraySwap(
	xvalue* pArray,
	size_t iLeft,
	size_t iRight
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)__xrtValueBacking(
		pArray,
		XVALUE_ARRAY
	);

	if ( pBacking == NULL ) {
		return false;
	}
	if ( (iLeft >= pBacking->Items.Count) ||
		 (iRight >= pBacking->Items.Count) ) {
		__xrtErrorSetRange();
		return false;
	}
	if ( iLeft == iRight ) {
		return true;
	}
	if ( !__xrtValueEnsureUnique(pArray) ) {
		return false;
	}
	return xrtPtrArraySwap(
		&((xvaluearraybacking*)pArray->Data.Backing)->Items,
		iLeft,
		iRight
	);
}

XRT_API bool xrtValueArraySwap(
	xvalue* pArray,
	size_t iLeft,
	size_t iRight
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueArraySwap(pArray, iLeft, iRight));
}



/* 返回稀疏整数键借用的值。 */
XRT_API xvalue* xrtValueIntMapGet(const xvalue* pMap, int64 iKey)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);
	const xvalue* const* pSlot;

	if ( pBacking == NULL ) {
		return NULL;
	}
	pSlot = (const xvalue* const*)xrtIntMapConstGet(&pBacking->Items, iKey);
	return pSlot != NULL ? (xvalue*)*pSlot : NULL;
}



/* 返回已经沿 COW 路径分离的可变 IntMap 子容器。 */
static xvalue* __xrtOwnershipBody_ValueIntMapEdit(xvalue* pMap, int64 iKey)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);
	const xvalue* const* pCurrent;
	xvalue** pSlot;

	if ( pBacking == NULL ) {
		return NULL;
	}
	pCurrent = (const xvalue* const*)xrtIntMapConstGet(
		&pBacking->Items,
		iKey
	);
	if ( pCurrent == NULL ) {
		return NULL;
	}
	if ( !__xrtValueEditItemValid(*pCurrent) ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pMap) ) {
		return NULL;
	}
	pBacking = (xvalueintmapbacking*)pMap->Data.Backing;
	pSlot = (xvalue**)xrtIntMapGet(&pBacking->Items, iKey);
	return __xrtValueEditSlot(pSlot);
}

XRT_API xvalue* xrtValueIntMapEdit(xvalue* pMap, int64 iKey)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueIntMapEdit(pMap, iKey));
}



/* 设置 IntMap 已拥有值的共同路径。 */
static bool __xrtValueIntMapSetOwned(
	xvalue* pMap,
	int64 iKey,
	xvalue* pItem
)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);
	const xvalue* const* pCurrent;
	bool bResult;

	if ( (pBacking == NULL) || !__xrtValueStoreItemValid(pItem) ) {
		return false;
	}
	pCurrent = (const xvalue* const*)xrtIntMapConstGet(
		&pBacking->Items,
		iKey
	);
	if ( (pCurrent != NULL) && (*pCurrent == pItem) ) {
		xrtValueRelease(pItem);
		return true;
	}
	if ( !__xrtValuePrepareStore(pMap, pItem) ) {
		return false;
	}
	pMap->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtIntMapSetPtr(
		&((xvalueintmapbacking*)pMap->Data.Backing)->Items,
		iKey,
		pItem
	);
	pMap->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return bResult;
}



/* 增加引用后设置整数键值。 */
static bool __xrtOwnershipBody_ValueIntMapSet(
	xvalue* pMap,
	int64 iKey,
	const xvalue* pItem
)
{
	xvalue* pStored = __xrtValueStoreRetain(pItem);

	if ( pStored == NULL ) {
		return false;
	}
	if ( !__xrtValueIntMapSetOwned(pMap, iKey, pStored) ) {
		xrtValueRelease(pStored);
		return false;
	}
	return true;
}

XRT_API bool xrtValueIntMapSet(
	xvalue* pMap,
	int64 iKey,
	const xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIntMapSet(pMap, iKey, pItem));
}



/* 成功时把来源引用移交到整数键。 */
static bool __xrtOwnershipBody_ValueIntMapSetTake(
	xvalue* pMap,
	int64 iKey,
	xvalue** pItem
)
{
	if ( !__xrtValueContainerTakeSlotValid(pMap, pItem) ) {
		return false;
	}
	if ( !__xrtValueIntMapSetOwned(pMap, iKey, *pItem) ) {
		return false;
	}
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueIntMapSetTake(
	xvalue* pMap,
	int64 iKey,
	xvalue** pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIntMapSetTake(pMap, iKey, pItem));
}



/* 无论成功失败都消费临时值并设置整数键。 */
static bool __xrtOwnershipBody_ValueIntMapSetNew(
	xvalue* pMap,
	int64 iKey,
	xvalue* pItem
)
{
	bool bResult = (pItem != NULL) &&
		xrtValueIntMapSetTake(pMap, iKey, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueIntMapSetNew(
	xvalue* pMap,
	int64 iKey,
	xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIntMapSetNew(pMap, iKey, pItem));
}



/* 判断整数键是否存在。 */
XRT_API bool xrtValueIntMapHas(const xvalue* pMap, int64 iKey)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);

	return (pBacking != NULL) && xrtIntMapHas(&pBacking->Items, iKey);
}



/* 删除整数键并释放对应值。 */
static bool __xrtOwnershipBody_ValueIntMapRemove(xvalue* pMap, int64 iKey)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);

	if ( pBacking == NULL ) {
		return false;
	}
	if ( !xrtIntMapHas(&pBacking->Items, iKey) ) {
		return false;
	}
	if ( !__xrtValueEnsureUnique(pMap) ) {
		return false;
	}
	pMap->Flags |= XRT_VALUE_FLAG_BUSY;
	if ( !xrtIntMapRemove(
		&((xvalueintmapbacking*)pMap->Data.Backing)->Items,
		iKey
	) ) {
		pMap->Flags &= ~XRT_VALUE_FLAG_BUSY;
		return false;
	}
	pMap->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return true;
}

XRT_API bool xrtValueIntMapRemove(xvalue* pMap, int64 iKey)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIntMapRemove(pMap, iKey));
}



/* 移交整数键对应值。 */
static xvalue* __xrtOwnershipBody_ValueIntMapTake(xvalue* pMap, int64 iKey)
{
	xvalueintmapbacking* pBacking = (xvalueintmapbacking*)__xrtValueBacking(
		pMap,
		XVALUE_INT_MAP
	);
	xvalue* pItem = NULL;

	if ( pBacking == NULL ) {
		return NULL;
	}
	if ( !xrtIntMapHas(&pBacking->Items, iKey) ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pMap) ) {
		return NULL;
	}
	if ( !xrtIntMapTakePtr(
		&((xvalueintmapbacking*)pMap->Data.Backing)->Items,
		iKey,
		(ptr*)&pItem
	) ) {
		return NULL;
	}
	return pItem;
}

XRT_API xvalue* xrtValueIntMapTake(xvalue* pMap, int64 iKey)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueIntMapTake(pMap, iKey));
}



/* 返回对象字符串键借用的值。 */
XRT_API xvalue* xrtValueObjectGet(const xvalue* pObject, xstrview Key)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	const xvalue* const* pSlot;

	if ( (pBacking == NULL) || !__xrtValueObjectKeyValid(Key) ) {
		return NULL;
	}
	pSlot = (const xvalue* const*)xrtMapConstGet(
		&pBacking->Items,
		__xrtValueObjectKey(Key)
	);
	return pSlot != NULL ? (xvalue*)*pSlot : NULL;
}



/* 按首次插入顺序返回对象中借用的键和值。 */
XRT_API xvalue* xrtValueObjectAt(
	const xvalue* pObject,
	size_t iIndex,
	xstrview* pKey
)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	xmapiter Iterator;
	xbytesview Key = {0};
	xvalue* const* pSlot = NULL;

	if ( pBacking == NULL ) {
		return NULL;
	}
	if ( iIndex >= xrtMapCount(&pBacking->Items) ) {
		__xrtErrorSetRange();
		return NULL;
	}
	if ( !xrtMapIterBegin(&pBacking->Items, &Iterator) ) {
		return NULL;
	}
	for ( size_t i = 0; i <= iIndex; i++ ) {
		pSlot = (xvalue* const*)xrtMapIterNext(&Iterator, &Key);
	}
	xrtMapIterEnd(&Iterator);
	if ( pSlot == NULL ) {
		return NULL;
	}
	if ( pKey != NULL ) {
		pKey->Data = (const char*)Key.Data;
		pKey->Size = Key.Size;
	}
	return *pSlot;
}



/* 返回已经沿 COW 路径分离的可变 Object 子容器。 */
static xvalue* __xrtOwnershipBody_ValueObjectEdit(xvalue* pObject, xstrview Key)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	const xvalue* const* pCurrent;
	xvalue** pSlot;

	if ( (pBacking == NULL) || !__xrtValueObjectKeyValid(Key) ) {
		return NULL;
	}
	pCurrent = (const xvalue* const*)xrtMapConstGet(
		&pBacking->Items,
		__xrtValueObjectKey(Key)
	);
	if ( pCurrent == NULL ) {
		return NULL;
	}
	if ( !__xrtValueEditItemValid(*pCurrent) ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pObject) ) {
		return NULL;
	}
	pBacking = (xvalueobjectbacking*)pObject->Data.Backing;
	pSlot = (xvalue**)xrtMapGet(
		&pBacking->Items,
		__xrtValueObjectKey(Key)
	);
	return __xrtValueEditSlot(pSlot);
}

XRT_API xvalue* xrtValueObjectEdit(xvalue* pObject, xstrview Key)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueObjectEdit(pObject, Key));
}



/* 设置 Object 已拥有值的共同路径。 */
static bool __xrtValueObjectSetOwned(
	xvalue* pObject,
	xstrview Key,
	xvalue* pItem
)
{
	xvalueobjectbacking* pBacking;
	const xvalue* const* pCurrent;
	bool bResult;

	if ( !__xrtValueObjectKeyValid(Key) ) {
		return false;
	}
	pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	if ( (pBacking == NULL) || !__xrtValueStoreItemValid(pItem) ) {
		return false;
	}
	pCurrent = (const xvalue* const*)xrtMapConstGet(
		&pBacking->Items,
		__xrtValueObjectKey(Key)
	);
	if ( (pCurrent != NULL) && (*pCurrent == pItem) ) {
		xrtValueRelease(pItem);
		return true;
	}
	if ( !__xrtValuePrepareStore(pObject, pItem) ) {
		return false;
	}
	pObject->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtMapSetPtr(
		&((xvalueobjectbacking*)pObject->Data.Backing)->Items,
		__xrtValueObjectKey(Key),
		pItem
	);
	pObject->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return bResult;
}



/* 增加引用后设置对象键值。 */
static bool __xrtOwnershipBody_ValueObjectSet(
	xvalue* pObject,
	xstrview Key,
	const xvalue* pItem
)
{
	xvalue* pStored = __xrtValueStoreRetain(pItem);

	if ( pStored == NULL ) {
		return false;
	}
	if ( !__xrtValueObjectSetOwned(pObject, Key, pStored) ) {
		xrtValueRelease(pStored);
		return false;
	}
	return true;
}

XRT_API bool xrtValueObjectSet(
	xvalue* pObject,
	xstrview Key,
	const xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectSet(pObject, Key, pItem));
}



/* 成功时把来源引用移交到对象键。 */
static bool __xrtOwnershipBody_ValueObjectSetTake(
	xvalue* pObject,
	xstrview Key,
	xvalue** pItem
)
{
	if ( !__xrtValueContainerTakeSlotValid(pObject, pItem) ) {
		return false;
	}
	if ( !__xrtValueObjectSetOwned(pObject, Key, *pItem) ) {
		return false;
	}
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueObjectSetTake(
	xvalue* pObject,
	xstrview Key,
	xvalue** pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectSetTake(pObject, Key, pItem));
}



/* 无论成功失败都消费临时值并设置对象键。 */
static bool __xrtOwnershipBody_ValueObjectSetNew(
	xvalue* pObject,
	xstrview Key,
	xvalue* pItem
)
{
	bool bResult = (pItem != NULL) &&
		xrtValueObjectSetTake(pObject, Key, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueObjectSetNew(
	xvalue* pObject,
	xstrview Key,
	xvalue* pItem
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectSetNew(pObject, Key, pItem));
}



/* 判断对象键是否存在。 */
XRT_API bool xrtValueObjectHas(const xvalue* pObject, xstrview Key)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);

	return (pBacking != NULL) && __xrtValueObjectKeyValid(Key) &&
		xrtMapHas(&pBacking->Items, __xrtValueObjectKey(Key));
}



/* 删除对象键并释放对应值。 */
static bool __xrtOwnershipBody_ValueObjectRemove(xvalue* pObject, xstrview Key)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);

	if ( (pBacking == NULL) || !__xrtValueObjectKeyValid(Key) ) {
		return false;
	}
	if ( !xrtMapHas(&pBacking->Items, __xrtValueObjectKey(Key)) ) {
		return false;
	}
	if ( !__xrtValueEnsureUnique(pObject) ) {
		return false;
	}
	pObject->Flags |= XRT_VALUE_FLAG_BUSY;
	if ( !xrtMapRemove(
		&((xvalueobjectbacking*)pObject->Data.Backing)->Items,
		__xrtValueObjectKey(Key)
	) ) {
		pObject->Flags &= ~XRT_VALUE_FLAG_BUSY;
		return false;
	}
	pObject->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return true;
}

XRT_API bool xrtValueObjectRemove(xvalue* pObject, xstrview Key)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueObjectRemove(pObject, Key));
}



/* 移交对象键对应值。 */
static xvalue* __xrtOwnershipBody_ValueObjectTake(xvalue* pObject, xstrview Key)
{
	xvalueobjectbacking* pBacking = (xvalueobjectbacking*)__xrtValueBacking(
		pObject,
		XVALUE_OBJECT
	);
	xvalue* pItem = NULL;

	if ( (pBacking == NULL) || !__xrtValueObjectKeyValid(Key) ) {
		return NULL;
	}
	if ( !xrtMapHas(&pBacking->Items, __xrtValueObjectKey(Key)) ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pObject) ) {
		return NULL;
	}
	if ( !xrtMapTakePtr(
		&((xvalueobjectbacking*)pObject->Data.Backing)->Items,
		__xrtValueObjectKey(Key),
		(ptr*)&pItem
	) ) {
		return NULL;
	}
	return pItem;
}

XRT_API xvalue* xrtValueObjectTake(xvalue* pObject, xstrview Key)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueObjectTake(pObject, Key));
}



/* 增加引用后把可哈希标量加入集合。 */
static bool __xrtOwnershipBody_ValueSetAdd(xvalue* pSet, const xvalue* pItem)
{
	xvaluesetbacking* pBacking = (xvaluesetbacking*)__xrtValueBacking(
		pSet,
		XVALUE_SET
	);
	xvalue* pKey = (xvalue*)pItem;
	bool bResult;

	if ( (pBacking == NULL) || !__xrtValueSetItemValid(pItem) ) {
		return false;
	}
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetHas(&pBacking->Items, &pKey);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	if ( bResult ) {
		return true;
	}
	if ( !__xrtValueEnsureUnique(pSet) ) {
		return false;
	}
	pBacking = (xvaluesetbacking*)pSet->Data.Backing;
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetAdd(&pBacking->Items, &pKey);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return bResult;
}

XRT_API bool xrtValueSetAdd(xvalue* pSet, const xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueSetAdd(pSet, pItem));
}



/* 成功时消费来源引用；重复元素同样视为成功。 */
static bool __xrtOwnershipBody_ValueSetAddTake(xvalue* pSet, xvalue** pItem)
{
	if ( !__xrtValueContainerTakeSlotValid(pSet, pItem) ) {
		return false;
	}
	if ( !xrtValueSetAdd(pSet, *pItem) ) {
		return false;
	}
	xrtValueRelease(*pItem);
	*pItem = NULL;
	return true;
}

XRT_API bool xrtValueSetAddTake(xvalue* pSet, xvalue** pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueSetAddTake(pSet, pItem));
}



/* 无论成功失败都消费临时值并尝试加入集合。 */
static bool __xrtOwnershipBody_ValueSetAddNew(xvalue* pSet, xvalue* pItem)
{
	bool bResult = (pItem != NULL) && xrtValueSetAddTake(pSet, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}

XRT_API bool xrtValueSetAddNew(xvalue* pSet, xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueSetAddNew(pSet, pItem));
}



/* 判断等价值是否在集合中。 */
XRT_API bool xrtValueSetHas(const xvalue* pSet, const xvalue* pItem)
{
	xvaluesetbacking* pBacking = (xvaluesetbacking*)__xrtValueBacking(
		pSet,
		XVALUE_SET
	);
	xvalue* pKey = (xvalue*)pItem;
	bool bResult;

	if ( (pBacking == NULL) || !__xrtValueSetItemValid(pItem) ) {
		return false;
	}
	((xvalue*)pSet)->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetHas(&pBacking->Items, &pKey);
	((xvalue*)pSet)->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return bResult;
}



/* 删除等价值并释放集合持有的引用。 */
static bool __xrtOwnershipBody_ValueSetRemove(xvalue* pSet, const xvalue* pItem)
{
	xvaluesetbacking* pBacking = (xvaluesetbacking*)__xrtValueBacking(
		pSet,
		XVALUE_SET
	);
	xvalue* pKey = (xvalue*)pItem;
	bool bResult;

	if ( (pBacking == NULL) || !__xrtValueSetItemValid(pItem) ) {
		return false;
	}
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetHas(&pBacking->Items, &pKey);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	if ( !bResult ) {
		return false;
	}
	if ( !__xrtValueEnsureUnique(pSet) ) {
		return false;
	}
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetRemove(
		&((xvaluesetbacking*)pSet->Data.Backing)->Items,
		&pKey
	);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	return bResult;
}

XRT_API bool xrtValueSetRemove(xvalue* pSet, const xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueSetRemove(pSet, pItem));
}



/* 移交集合中的规范值。 */
static xvalue* __xrtOwnershipBody_ValueSetTake(xvalue* pSet, const xvalue* pItem)
{
	xvaluesetbacking* pBacking = (xvaluesetbacking*)__xrtValueBacking(
		pSet,
		XVALUE_SET
	);
	xvalue* pKey = (xvalue*)pItem;
	xvalue* pStored = NULL;
	bool bResult;

	if ( (pBacking == NULL) || !__xrtValueSetItemValid(pItem) ) {
		return NULL;
	}
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetHas(&pBacking->Items, &pKey);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	if ( !bResult ) {
		return NULL;
	}
	if ( !__xrtValueEnsureUnique(pSet) ) {
		return NULL;
	}
	pSet->Flags |= XRT_VALUE_FLAG_BUSY;
	bResult = xrtSetTake(
		&((xvaluesetbacking*)pSet->Data.Backing)->Items,
		&pKey,
		&pStored
	);
	pSet->Flags &= ~XRT_VALUE_FLAG_BUSY;
	if ( !bResult ) {
		return NULL;
	}
	return pStored;
}

XRT_API xvalue* xrtValueSetTake(xvalue* pSet, const xvalue* pItem)
{
	XRT_VALUE_MUTATION_RETURN(xvalue*, __xrtOwnershipBody_ValueSetTake(pSet, pItem));
}



/* 启动一个方向明确的 backing 快照迭代器。 */
static bool __xrtValueIterStartInternal(
	const xvalue* pValue,
	xvalueiter* pIterator,
	int iDirection,
	bool bKeepFinalizerShell
)
{
	xvaluebacking* pBacking;
	bool bReady = true;

	if ( (pValue == NULL) || (pIterator == NULL) ) {
		if ( pIterator != NULL ) {
			memset(pIterator, 0, sizeof(xvalueiter));
		}
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pValue,
		sizeof(xvalue),
		pIterator,
		sizeof(xvalueiter)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pIterator, 0, sizeof(xvalueiter));
	if ( !__xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtErrorSetType();
		return false;
	}
	pBacking = __xrtValueBacking(pValue, (xvaluetype)pValue->Type);
	if ( (pBacking == NULL) || !__xrtValueBackingRetain(pBacking) ) {
		return false;
	}
	pIterator->Backing = pBacking;
	pIterator->Type = (xvaluetype)pBacking->Type;
	pIterator->Direction = iDirection;
	if (bKeepFinalizerShell && pBacking->Type == XVALUE_OBJECT &&
		((xvalueobjectbacking*)pBacking)->Finalizer != NULL) {
		pIterator->FinalizerOwner = xrtValueRetain(pValue);
		if (pIterator->FinalizerOwner == NULL) {
			__xrtValueBackingRelease(pBacking);
			memset(pIterator, 0, sizeof(*pIterator));
			return false;
		}
	}
	if ( pBacking->Type == XVALUE_ARRAY ) {
		pIterator->Index = iDirection > 0
			? 0
			: ((xvaluearraybacking*)pBacking)->Items.Count;
	} else if ( pBacking->Type == XVALUE_INT_MAP ) {
		bReady = iDirection > 0
			? xrtIntMapIterBegin(
				&((xvalueintmapbacking*)pBacking)->Items,
				&pIterator->State.IntMap
			)
			: xrtIntMapIterRBegin(
				&((xvalueintmapbacking*)pBacking)->Items,
				&pIterator->State.IntMap
			);
	} else if ( pBacking->Type == XVALUE_SET ) {
		bReady = iDirection > 0
			? xrtSetIterBegin(
				&((xvaluesetbacking*)pBacking)->Items,
				&pIterator->State.Set
			)
			: xrtSetIterRBegin(
				&((xvaluesetbacking*)pBacking)->Items,
				&pIterator->State.Set
			);
	} else {
		bReady = iDirection > 0
			? xrtMapIterBegin(
				&((xvalueobjectbacking*)pBacking)->Items,
				&pIterator->State.Map
			)
			: xrtMapIterRBegin(
				&((xvalueobjectbacking*)pBacking)->Items,
				&pIterator->State.Map
			);
	}
	if ( !bReady ) {
		__xrtValueBackingRelease(pBacking);
		xrtValueRelease(pIterator->FinalizerOwner);
		memset(pIterator, 0, sizeof(xvalueiter));
		return false;
	}
	return true;
}



/* 启动按容器稳定顺序的快照迭代。 */
static bool __xrtOwnershipBody_ValueIterBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	return __xrtValueIterStartInternal(pValue, pIterator, 1, true);
}

XRT_API bool xrtValueIterBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIterBegin(pValue, pIterator));
}



/* 启动按容器稳定逆序的快照迭代。 */
static bool __xrtOwnershipBody_ValueIterRBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	return __xrtValueIterStartInternal(pValue, pIterator, -1, true);
}

XRT_API bool xrtValueIterRBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	XRT_VALUE_MUTATION_RETURN(bool, __xrtOwnershipBody_ValueIterRBegin(pValue, pIterator));
}



/* 创建拥有式快照迭代器，并按指定方向启动。 */
static xvalueiter* __xrtValueIterCreate(
	const xvalue* pValue,
	int iDirection
)
{
	xvalueiter* pIterator;
	bool bReady;

	pIterator = (xvalueiter*)xrtMalloc(sizeof(xvalueiter));
	if ( pIterator == NULL ) {
		return NULL;
	}
	bReady = (iDirection > 0)
		? xrtValueIterBegin(pValue, pIterator)
		: xrtValueIterRBegin(pValue, pIterator);
	if ( !bReady ) {
		xrtFree(pIterator);
		return NULL;
	}
	return pIterator;
}



/* 创建按稳定正序推进的拥有式快照迭代器。 */
static xvalueiter* __xrtOwnershipBody_ValueIterCreate(const xvalue* pValue)
{
	return __xrtValueIterCreate(pValue, 1);
}

XRT_API xvalueiter* xrtValueIterCreate(const xvalue* pValue)
{
	XRT_VALUE_MUTATION_RETURN(xvalueiter*, __xrtOwnershipBody_ValueIterCreate(pValue));
}



/* 创建按稳定逆序推进的拥有式快照迭代器。 */
static xvalueiter* __xrtOwnershipBody_ValueIterRCreate(const xvalue* pValue)
{
	return __xrtValueIterCreate(pValue, -1);
}

XRT_API xvalueiter* xrtValueIterRCreate(const xvalue* pValue)
{
	XRT_VALUE_MUTATION_RETURN(xvalueiter*, __xrtOwnershipBody_ValueIterRCreate(pValue));
}



/* 返回数组快照中的下一项。 */
static xvalue* __xrtValueArrayIterNext(
	xvalueiter* pIterator,
	xvaluekey* pKey
)
{
	xvaluearraybacking* pBacking = (xvaluearraybacking*)pIterator->Backing;
	size_t iIndex;

	if ( pIterator->Direction > 0 ) {
		if ( pIterator->Index >= pBacking->Items.Count ) {
			return NULL;
		}
		iIndex = pIterator->Index++;
	} else {
		if ( pIterator->Index == 0 ) {
			return NULL;
		}
		iIndex = --pIterator->Index;
	}
	if ( pKey != NULL ) {
		pKey->Type = XVALUE_KEY_INDEX;
		pKey->Index = iIndex;
	}
	return (xvalue*)xrtPtrArrayGet(&pBacking->Items, iIndex);
}



/* 返回下一借用值及其键。 */
XRT_API xvalue* xrtValueIterNext(
	xvalueiter* pIterator,
	xvaluekey* pKey
)
{
	ptr pSlot;

	if ( (pIterator == NULL) || (pIterator->Backing == NULL) ) {
		return NULL;
	}
	if ( (pKey != NULL) && __xrtRangesOverlap(
		pIterator,
		sizeof(xvalueiter),
		pKey,
		sizeof(xvaluekey)
	) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( ((pIterator->Direction != 1) && (pIterator->Direction != -1)) ||
		 !__xrtValueContainerType(pIterator->Type) ||
		 (((xvaluebacking*)pIterator->Backing)->Type !=
		  (uint16)pIterator->Type) ||
		 (__xrtAtomicRefLoad(
			&((xvaluebacking*)pIterator->Backing)->RefCount
		 ) <= 0) ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( pKey != NULL ) {
		memset(pKey, 0, sizeof(xvaluekey));
	}
	if ( pIterator->Type == XVALUE_ARRAY ) {
		return __xrtValueArrayIterNext(pIterator, pKey);
	}
	if ( pIterator->Type == XVALUE_INT_MAP ) {
		int64 iKey;

		pSlot = xrtIntMapIterNext(&pIterator->State.IntMap, &iKey);
		if ( (pSlot != NULL) && (pKey != NULL) ) {
			pKey->Type = XVALUE_KEY_INT;
			pKey->Integer = iKey;
		}
		return pSlot != NULL ? *(xvalue**)pSlot : NULL;
	}
	if ( pIterator->Type == XVALUE_SET ) {
		const void* pItem = xrtSetIterNext(&pIterator->State.Set);

		return pItem != NULL ? *(xvalue* const*)pItem : NULL;
	}
	if ( pIterator->Type == XVALUE_OBJECT ) {
		xbytesview Key = {0};

		pSlot = xrtMapIterNext(&pIterator->State.Map, &Key);
		if ( (pSlot != NULL) && (pKey != NULL) ) {
			pKey->Type = XVALUE_KEY_STRING;
			pKey->String.Data = (cstr)Key.Data;
			pKey->String.Size = Key.Size;
		}
		return pSlot != NULL ? *(xvalue**)pSlot : NULL;
	}
	__xrtErrorSetInvalidState();
	return NULL;
}



/* 隔离调用前错误并以三态结果推进一个快照元素。 */
XRT_API xvalueiterresult xrtValueIterAdvance(
	xvalueiter* pIterator,
	xvaluekey* pKey,
	xvalue** ppValue
)
{
	xerror* pPrevious;
	xerror* pCurrent;
	xerror* pDiscard;
	xvalue* pValue;

	if ( (pIterator == NULL) || (ppValue == NULL) ||
		((pIterator != NULL) && __xrtRangesOverlap(
			pIterator,
			sizeof(xvalueiter),
			ppValue,
			sizeof(xvalue*)
		)) ||
		((pKey != NULL) && __xrtRangesOverlap(
			pKey,
			sizeof(xvaluekey),
			ppValue,
			sizeof(xvalue*)
		)) ) {
		__xrtErrorSetInvalidArgument();
		return XVALUE_ITER_ERROR;
	}
	if ( pIterator->Backing == NULL ) {
		*ppValue = NULL;
		__xrtErrorSetInvalidState();
		return XVALUE_ITER_ERROR;
	}
	*ppValue = NULL;
	pPrevious = __xrtErrorSwapOwned(NULL);
	pValue = xrtValueIterNext(pIterator, pKey);
	pCurrent = __xrtErrorSwapOwned(pPrevious);
	if ( pCurrent != NULL ) {
		pDiscard = __xrtErrorSwapOwned(pCurrent);
		xrtErrorFree(pDiscard);
		return XVALUE_ITER_ERROR;
	}
	*ppValue = pValue;
	return pValue != NULL ? XVALUE_ITER_ITEM : XVALUE_ITER_END;
}



/* 结束迭代并释放 backing 快照。 */
XRT_API void xrtValueIterEnd(xvalueiter* pIterator)
{
	xvaluebacking* pBacking;
	xvalue* pOwner;
	xrtownershipscope Mutation = {0};
	if ( (pIterator == NULL) || (pIterator->Backing == NULL) ) {
		return;
	}
	if (!xrtOwnershipMutationBegin(&Mutation)) abort();
	if ( pIterator->Type == XVALUE_INT_MAP ) {
		xrtIntMapIterEnd(&pIterator->State.IntMap);
	} else if ( pIterator->Type == XVALUE_SET ) {
		xrtSetIterEnd(&pIterator->State.Set);
	} else if ( pIterator->Type == XVALUE_OBJECT ) {
		xrtMapIterEnd(&pIterator->State.Map);
	}
	pBacking = (xvaluebacking*)pIterator->Backing;
	pOwner = pIterator->FinalizerOwner;
	/* Re-entry observes an ended cursor before either release can call user
	 * code. The backing slot must go first; the shell owns the final duty. */
	memset(pIterator, 0, sizeof(xvalueiter));
	if (!xrtOwnershipScopeEnd(&Mutation)) abort();
	__xrtValueBackingRelease(pBacking);
	xrtValueRelease(pOwner);
}



/* 结束并释放拥有式迭代器。 */
XRT_API void xrtValueIterDestroy(xvalueiter* pIterator)
{
	if ( pIterator == NULL ) {
		return;
	}
	xrtValueIterEnd(pIterator);
	xrtFree(pIterator);
}

static bool __xrtValueIterOwnershipCount(const void* pData, size_t* pCount)
{
	const xvalueiter* pIterator = (const xvalueiter*)pData;
	if (pIterator == NULL || pCount == NULL) return false;
	if (pIterator->Backing == NULL) {
		if (pIterator->Type != 0 || pIterator->Direction != 0 || pIterator->Index != 0 || pIterator->FinalizerOwner != NULL) return false;
	} else if ((pIterator->Direction != 1 && pIterator->Direction != -1) ||
		!__xrtValueContainerType(pIterator->Type) ||
		((const xvaluebacking*)pIterator->Backing)->Type != (uint16)pIterator->Type ||
		__xrtAtomicRefLoad(&((const xvaluebacking*)pIterator->Backing)->RefCount) <= 0) return false;
	if (pIterator->FinalizerOwner != NULL && (pIterator->Type != XVALUE_OBJECT ||
		pIterator->FinalizerOwner->Data.Backing != pIterator->Backing ||
		__xrtAtomicRefLoad(&pIterator->FinalizerOwner->RefCount) <= 0)) return false;
	/* Unique End/Destroy ownership; borrowed cursor aliases acquire no refs. */
	*pCount = 1;
	return true;
}
static bool __xrtValueIterOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xvalueiter* pIterator = (const xvalueiter*)pData;
	size_t iCount;
	if (pVisit == NULL || !__xrtValueIterOwnershipCount(pData, &iCount)) return false;
	if (pIterator->Backing != NULL && !pVisit(
		(xrtownershipref){pIterator->Backing, &__xrtValueBackingOwnershipOps}, pContext)) return false;
	return pIterator->FinalizerOwner == NULL || pVisit(xrtValueOwnership(pIterator->FinalizerOwner), pContext);
}
static const xrtownershipops __xrtValueIterOwnershipOps = {
	__xrtValueIterOwnershipCount, __xrtValueIterOwnershipTrace
};
XRT_API xrtownershipref xrtValueIterOwnership(const xvalueiter* pIterator)
{
	return (xrtownershipref){pIterator, pIterator != NULL ? &__xrtValueIterOwnershipOps : NULL};
}

#endif
