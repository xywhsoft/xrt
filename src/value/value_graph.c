#include "../internal/xrt_value.h"



#if defined(XRT_FEATURE_VALUE_GRAPH)

#define XRT_VALUE_GRAPH_INLINE 32u
#define XRT_VALUE_GRAPH_GUARD_MAX ((XRT_VALUE_DEPTH_MAX * 2u) + 2u)



/* 已复制表记录目标值和递归构造状态。 */
typedef struct xvalueclonestate {
	xvalue* Target;
	bool Active;
} xvalueclonestate;



/* 小图直接在栈内记录源值与复制状态。 */
typedef struct xvaluecloneentry {
	const xvalue* Source;
	xvalueclonestate State;
} xvaluecloneentry;



/* 一次深拷贝共用栈内身份表、按需溢出表和活动源路径。 */
typedef struct xvalueclonecontext {
	xvaluecloneentry Inline[XRT_VALUE_GRAPH_INLINE];
	size_t InlineCount;
	xmap Overflow;
	bool OverflowReady;
	const xvalue* Active[XRT_VALUE_DEPTH_MAX + 1u];
	size_t ActiveCount;
} xvalueclonecontext;



/* 结构相等的映射键由一对有方向的值身份组成。 */
typedef struct xvalueequalkey {
	const xvalue* Left;
	const xvalue* Right;
} xvalueequalkey;



/* 已比较值对只需记录当前仍在递归还是已经相等。 */
typedef struct xvalueequalstate {
	bool Active;
} xvalueequalstate;



/* 小图直接在栈内记录比较值对。 */
typedef struct xvalueequalentry {
	xvalueequalkey Key;
	xvalueequalstate State;
} xvalueequalentry;



/* 一次结构比较共用值对记忆表和回调活动祖先。 */
typedef struct xvalueequalcontext {
	xvalueequalentry Inline[XRT_VALUE_GRAPH_INLINE];
	size_t InlineCount;
	xmap Overflow;
	bool OverflowReady;
	const xvalue* Guards[XRT_VALUE_GRAPH_GUARD_MAX];
	size_t GuardCount;
} xvalueequalcontext;



/* 把调用者持有的值地址变量转换为 Map 键。 */
static xbytesview __xrtValueGraphPointerKey(
	const xvalue* const* pValue
)
{
	xbytesview Key;

	Key.Data = (cbytes)pValue;
	Key.Size = sizeof(*pValue);
	return Key;
}



/* 把结构相等值对转换为 Map 键。 */
static xbytesview __xrtValueGraphEqualKey(
	const xvalueequalkey* pKey
)
{
	xbytesview Key;

	Key.Data = (cbytes)pKey;
	Key.Size = sizeof(*pKey);
	return Key;
}



/* 查找源值已经登记的复制状态。 */
static xvalueclonestate* __xrtValueCloneState(
	xvalueclonecontext* pContext,
	const xvalue* pSource
)
{
	for ( size_t i = 0; i < pContext->InlineCount; i++ ) {
		if ( pContext->Inline[i].Source == pSource ) {
			return &pContext->Inline[i].State;
		}
	}
	if ( !pContext->OverflowReady ) {
		return NULL;
	}
	return (xvalueclonestate*)xrtMapGet(
		&pContext->Overflow,
		__xrtValueGraphPointerKey(&pSource)
	);
}



/* 查找已完成的目标值，返回负数表示源图含环。 */
static int __xrtValueCloneFind(
	xvalueclonecontext* pContext,
	const xvalue* pSource,
	xvalue** pTarget
)
{
	xvalueclonestate* pState = __xrtValueCloneState(
		pContext,
		pSource
	);

	*pTarget = NULL;
	if ( pState == NULL ) {
		return 0;
	}
	if ( pState->Active ) {
		__xrtErrorSetValue();
		return -1;
	}
	*pTarget = xrtValueRetain(pState->Target);
	return *pTarget != NULL ? 1 : -1;
}



/* 登记一个正在构造的目标值，小图不分配身份表。 */
static bool __xrtValueCloneStart(
	xvalueclonecontext* pContext,
	const xvalue* pSource,
	xvalue* pTarget
)
{
	xvalueclonestate State;

	State.Target = pTarget;
	State.Active = true;
	if ( pContext->InlineCount < XRT_VALUE_GRAPH_INLINE ) {
		xvaluecloneentry* pEntry =
			&pContext->Inline[pContext->InlineCount++];

		pEntry->Source = pSource;
		pEntry->State = State;
		return true;
	}
	if ( !pContext->OverflowReady ) {
		if ( !xrtMapInit(
			&pContext->Overflow,
			sizeof(xvalueclonestate)
		) ) {
			return false;
		}
		pContext->OverflowReady = true;
	}
	return xrtMapSet(
		&pContext->Overflow,
		__xrtValueGraphPointerKey(&pSource),
		&State
	);
}



/* 将已登记目标标记为完整可复用。 */
static bool __xrtValueCloneFinish(
	xvalueclonecontext* pContext,
	const xvalue* pSource
)
{
	xvalueclonestate* pState = __xrtValueCloneState(
		pContext,
		pSource
	);

	if ( pState == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pState->Active = false;
	return true;
}



/* 释放深拷贝按需创建的溢出身份表。 */
static void __xrtValueCloneUnit(xvalueclonecontext* pContext)
{
	if ( pContext->OverflowReady ) {
		xrtMapUnit(&pContext->Overflow);
		pContext->OverflowReady = false;
	}
}



/* 把当前源值压入活动路径。 */
static bool __xrtValueClonePush(
	xvalueclonecontext* pContext,
	const xvalue* pSource
)
{
	if ( pContext->ActiveCount >= (XRT_VALUE_DEPTH_MAX + 1u) ) {
		__xrtErrorSetValue();
		return false;
	}
	pContext->Active[pContext->ActiveCount++] = pSource;
	return true;
}



/* 弹出当前源值。 */
static void __xrtValueClonePop(xvalueclonecontext* pContext)
{
	if ( pContext->ActiveCount != 0 ) {
		pContext->ActiveCount--;
		pContext->Active[pContext->ActiveCount] = NULL;
	}
}



/* 保护完整活动源路径并执行一次句柄释放。 */
static void __xrtValueCloneDropHandle(
	xvalueclonecontext* pContext,
	const xvaluehandleops* pOps,
	ptr pHandle,
	ptr pUserData
)
{
	bool bProtected = __xrtValueCallbackProtect(
		pContext->Active,
		pContext->ActiveCount
	);

	pOps->Drop(pHandle, pUserData);
	if ( bProtected ) {
		__xrtValueCallbackUnprotect(
			pContext->Active,
			pContext->ActiveCount
		);
	}
}



/* 保护完整活动源路径并释放部分构造的目标图。 */
static void __xrtValueCloneRelease(
	xvalueclonecontext* pContext,
	xvalue* pTarget
)
{
	bool bProtected = __xrtValueCallbackProtect(
		pContext->Active,
		pContext->ActiveCount
	);

	xrtValueRelease(pTarget);
	if ( bProtected ) {
		__xrtValueCallbackUnprotect(
			pContext->Active,
			pContext->ActiveCount
		);
	}
}



/* 按源容器类型创建空目标容器。 */
static xvalue* __xrtValueCloneContainer(const xvalue* pSource)
{
	switch ( (xvaluetype)pSource->Type ) {
		case XVALUE_ARRAY:
			return xrtValueArray();
		case XVALUE_INT_MAP:
			return xrtValueIntMap();
		case XVALUE_SET:
			return xrtValueSet();
		case XVALUE_OBJECT:
			return __xrtValueObjectDropsReverse(pSource)
				? xrtValueObjectLifo()
				: xrtValueObject();
		default:
			__xrtErrorSetType();
			return NULL;
	}
}



/* 深度复制句柄，禁止把不可克隆拥有资源伪装成独立副本。 */
static xvalue* __xrtValueCloneHandle(
	xvalueclonecontext* pContext,
	const xvalue* pSource
)
{
	const xvaluehandleops* pOps = pSource->Data.Handle.Ops;
	ptr pUserData = pSource->Data.Handle.UserData;
	ptr pClone = NULL;
	xvalue* pTarget;
	const xerror* pErrorBefore;
	bool bCloned;
	int iFound;

	iFound = __xrtValueCloneFind(pContext, pSource, &pTarget);
	if ( iFound > 0 ) {
		return pTarget;
	}
	if ( iFound < 0 ) {
		return NULL;
	}
	if ( pOps->Clone == NULL ) {
		__xrtErrorSetUnsupported();
		return NULL;
	}
	if ( !__xrtValueClonePush(pContext, pSource) ) {
		return NULL;
	}
	if ( !__xrtValueCallbackProtect(
			pContext->Active,
			pContext->ActiveCount
	) ) {
		__xrtValueClonePop(pContext);
		return NULL;
	}
	pErrorBefore = xrtGetError();
	bCloned = pOps->Clone(
		pSource->Data.Handle.Data,
		&pClone,
		pUserData
	);
	if ( !bCloned && (pClone != NULL) ) {
		pOps->Drop(pClone, pUserData);
		pClone = NULL;
	}
	__xrtValueCallbackUnprotect(
		pContext->Active,
		pContext->ActiveCount
	);
	if ( !bCloned ) {
		__xrtValueClonePop(pContext);
		if ( xrtGetError() == pErrorBefore ) {
			__xrtErrorSetInvalidState();
		}
		return NULL;
	}
	pTarget = xrtValueHandleTake(&pClone, pOps, pUserData);
	if ( pTarget == NULL ) {
		__xrtValueCloneDropHandle(
			pContext,
			pOps,
			pClone,
			pUserData
		);
		__xrtValueClonePop(pContext);
		return NULL;
	}
	pTarget->TypeId = pSource->TypeId;
	pTarget->IdentityHash = pSource->IdentityHash;
	pTarget->IdentityEqual = pSource->IdentityEqual;
	pTarget->IdentityUserData = pSource->IdentityUserData;
	if ( !__xrtValueCloneStart(pContext, pSource, pTarget) ||
		 !__xrtValueCloneFinish(pContext, pSource) ) {
		__xrtValueCloneRelease(pContext, pTarget);
		__xrtValueClonePop(pContext);
		return NULL;
	}
	__xrtValueClonePop(pContext);
	return pTarget;
}



/* 前置声明供容器递归复制。 */
static xvalue* __xrtValueDeepClone(
	xvalueclonecontext* pContext,
	const xvalue* pSource,
	uint32 iDepth
);



/* 把一个已复制值按源键类型移交到目标容器。 */
static bool __xrtValueCloneInsert(
	xvalueclonecontext* pContext,
	xvalue* pTarget,
	xvaluekey Key,
	xvalue** pCopy
)
{
	bool bResult;

	if ( Key.Type == XVALUE_KEY_INDEX ) {
		return xrtValueArrayAppendTake(pTarget, pCopy);
	}
	if ( Key.Type == XVALUE_KEY_INT ) {
		return xrtValueIntMapSetTake(
			pTarget,
			Key.Integer,
			pCopy
		);
	}
	if ( Key.Type == XVALUE_KEY_STRING ) {
		return xrtValueObjectSetTake(
			pTarget,
			Key.String,
			pCopy
		);
	}
	if ( !__xrtValueCallbackProtect(
		pContext->Active,
		pContext->ActiveCount
	) ) {
		return false;
	}
	bResult = xrtValueSetAddTake(pTarget, pCopy);
	__xrtValueCallbackUnprotect(
		pContext->Active,
		pContext->ActiveCount
	);
	return bResult;
}



/* 深度复制一个已经登记的容器内容。 */
static bool __xrtValueCloneContainerItems(
	xvalueclonecontext* pContext,
	const xvalue* pSource,
	xvalue* pTarget,
	uint32 iDepth
)
{
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bResult = true;

	if ( (pTarget->Type != XVALUE_INT_MAP) &&
		 !xrtValueReserve(
			pTarget,
			__xrtValueContainerCount(pSource)
		 ) ) {
		return false;
	}
	if ( !xrtValueIterBegin(pSource, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		xvalue* pCopy = __xrtValueDeepClone(
			pContext,
			pItem,
			iDepth + 1u
		);

		if ( pCopy == NULL ) {
			bResult = false;
			break;
		}
		bResult = __xrtValueCloneInsert(
			pContext,
			pTarget,
			Key,
			&pCopy
		);
		if ( !bResult ) {
			xrtValueRelease(pCopy);
			break;
		}
	}
	xrtValueIterEnd(&tIterator);
	return bResult;
}



/* 深度复制标量、句柄或容器。 */
static xvalue* __xrtValueDeepClone(
	xvalueclonecontext* pContext,
	const xvalue* pSource,
	uint32 iDepth
)
{
	xvaluetype Type;
	xvalue* pTarget;
	bool bReady;
	int iFound;

	if ( pSource == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pSource->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	if ( iDepth >= XRT_VALUE_DEPTH_MAX ) {
		__xrtErrorSetValue();
		return NULL;
	}
	Type = (xvaluetype)pSource->Type;
	if ( Type == XVALUE_HANDLE ) {
		return __xrtValueCloneHandle(pContext, pSource);
	}
	if ( !__xrtValueContainerType(Type) ) {
		return xrtValueRetain(pSource);
	}
	iFound = __xrtValueCloneFind(pContext, pSource, &pTarget);
	if ( iFound > 0 ) {
		return pTarget;
	}
	if ( iFound < 0 ) {
		return NULL;
	}
	pTarget = __xrtValueCloneContainer(pSource);
	if ( pTarget == NULL ) {
		return NULL;
	}
	pTarget->TypeId = pSource->TypeId;
	pTarget->IdentityHash = pSource->IdentityHash;
	pTarget->IdentityEqual = pSource->IdentityEqual;
	pTarget->IdentityUserData = pSource->IdentityUserData;
	if ( !__xrtValueCloneStart(pContext, pSource, pTarget) ) {
		xrtValueRelease(pTarget);
		return NULL;
	}
	if ( !__xrtValueClonePush(pContext, pSource) ) {
		xrtValueRelease(pTarget);
		return NULL;
	}
	bReady = __xrtValueCloneContainerItems(
		pContext,
		pSource,
		pTarget,
		iDepth
	) && __xrtValueCloneFinish(pContext, pSource);
	if ( !bReady ) {
		__xrtValueCloneRelease(pContext, pTarget);
		__xrtValueClonePop(pContext);
		return NULL;
	}
	__xrtValueClonePop(pContext);
	return pTarget;
}



/* 深度复制完整无环值图，并保留重复子值的共享身份。 */
XRT_API xvalue* xrtValueDeepClone(const xvalue* pValue)
{
	xvalueclonecontext Context;
	xvalue* pResult;

	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	memset(&Context, 0, sizeof(Context));
	pResult = __xrtValueDeepClone(&Context, pValue, 0);
	__xrtValueCloneUnit(&Context);
	return pResult;
}



/* 查找已经登记的结构比较状态。 */
static xvalueequalstate* __xrtValueEqualState(
	xvalueequalcontext* pContext,
	xvalueequalkey Key
)
{
	for ( size_t i = 0; i < pContext->InlineCount; i++ ) {
		xvalueequalentry* pEntry = &pContext->Inline[i];

		if ( (pEntry->Key.Left == Key.Left) &&
			 (pEntry->Key.Right == Key.Right) ) {
			return &pEntry->State;
		}
	}
	if ( !pContext->OverflowReady ) {
		return NULL;
	}
	return (xvalueequalstate*)xrtMapGet(
		&pContext->Overflow,
		__xrtValueGraphEqualKey(&Key)
	);
}



/* 查找已完成值对，返回负数表示两侧活动路径形成递归环。 */
static int __xrtValueEqualFind(
	xvalueequalcontext* pContext,
	xvalueequalkey Key
)
{
	xvalueequalstate* pState = __xrtValueEqualState(pContext, Key);

	if ( pState == NULL ) {
		return 0;
	}
	if ( pState->Active ) {
		__xrtErrorSetValue();
		return -1;
	}
	return 1;
}



/* 登记一个正在递归比较的值对，小图不分配记忆表。 */
static bool __xrtValueEqualStart(
	xvalueequalcontext* pContext,
	xvalueequalkey Key
)
{
	xvalueequalstate State;

	State.Active = true;
	if ( pContext->InlineCount < XRT_VALUE_GRAPH_INLINE ) {
		xvalueequalentry* pEntry =
			&pContext->Inline[pContext->InlineCount++];

		pEntry->Key = Key;
		pEntry->State = State;
		return true;
	}
	if ( !pContext->OverflowReady ) {
		if ( !xrtMapInit(
			&pContext->Overflow,
			sizeof(xvalueequalstate)
		) ) {
			return false;
		}
		pContext->OverflowReady = true;
	}
	return xrtMapSet(
		&pContext->Overflow,
		__xrtValueGraphEqualKey(&Key),
		&State
	);
}



/* 将已经登记的值对标记为结构相等。 */
static bool __xrtValueEqualFinish(
	xvalueequalcontext* pContext,
	xvalueequalkey Key
)
{
	xvalueequalstate* pState = __xrtValueEqualState(pContext, Key);

	if ( pState == NULL ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pState->Active = false;
	return true;
}



/* 释放结构相等按需创建的溢出记忆表。 */
static void __xrtValueEqualUnit(xvalueequalcontext* pContext)
{
	if ( pContext->OverflowReady ) {
		xrtMapUnit(&pContext->Overflow);
		pContext->OverflowReady = false;
	}
}



/* 把当前比较值对加入回调活动祖先。 */
static bool __xrtValueEqualPush(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	if ( pContext->GuardCount > (XRT_VALUE_GRAPH_GUARD_MAX - 2u) ) {
		__xrtErrorSetValue();
		return false;
	}
	pContext->Guards[pContext->GuardCount++] = pLeft;
	pContext->Guards[pContext->GuardCount++] = pRight;
	return true;
}



/* 弹出当前比较值对。 */
static void __xrtValueEqualPop(xvalueequalcontext* pContext)
{
	if ( pContext->GuardCount >= 2u ) {
		pContext->GuardCount -= 2u;
		pContext->Guards[pContext->GuardCount] = NULL;
		pContext->Guards[pContext->GuardCount + 1u] = NULL;
	}
}



/* 前置声明供容器结构相等递归。 */
static bool __xrtValueEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight,
	uint32 iDepth
);



/* 比较数组的每个有序元素。 */
static bool __xrtValueArrayEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight,
	uint32 iDepth
)
{
	size_t iCount = __xrtValueContainerCount(pLeft);

	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtValueEqual(
			pContext,
			xrtValueArrayGet(pLeft, i),
			xrtValueArrayGet(pRight, i),
			iDepth + 1u
		) ) {
			return false;
		}
	}
	return true;
}



/* 比较 IntMap 的键集合和对应值。 */
static bool __xrtValueIntMapEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight,
	uint32 iDepth
)
{
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bEqual = true;

	if ( !xrtValueIterBegin(pLeft, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		xvalue* pOther = xrtValueIntMapGet(pRight, Key.Integer);

		if ( (pOther == NULL) ||
			 !__xrtValueEqual(
				pContext,
				pItem,
				pOther,
				iDepth + 1u
			 ) ) {
			bEqual = false;
			break;
		}
	}
	xrtValueIterEnd(&tIterator);
	return bEqual;
}



/* 比较 Object 的键集合和对应值，插入顺序不影响相等性。 */
static bool __xrtValueObjectEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight,
	uint32 iDepth
)
{
	xvalueiter tIterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bEqual = true;

	if ( !xrtValueIterBegin(pLeft, &tIterator) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&tIterator, &Key)) != NULL ) {
		xvalue* pOther = xrtValueObjectGet(pRight, Key.String);

		if ( (pOther == NULL) ||
			 !__xrtValueEqual(
				pContext,
				pItem,
				pOther,
				iDepth + 1u
			 ) ) {
			bEqual = false;
			break;
		}
	}
	xrtValueIterEnd(&tIterator);
	return bEqual;
}



/* 复用通用 Set 关系实现比较等价元素集合。 */
static bool __xrtValueSetStructuralEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	const xset* pLeftItems = __xrtValueSetItems(pLeft);
	const xset* pRightItems = __xrtValueSetItems(pRight);
	bool bEqual;

	if ( (pLeftItems == NULL) || (pRightItems == NULL) ||
		 !__xrtValueCallbackProtect(
			pContext->Guards,
			pContext->GuardCount
		 ) ) {
		return false;
	}
	bEqual = xrtSetEqual(pLeftItems, pRightItems);
	__xrtValueCallbackUnprotect(
		pContext->Guards,
		pContext->GuardCount
	);
	return bEqual;
}



/* 比较两个具有同一策略域的拥有句柄。 */
static bool __xrtValueHandleEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	xvalueequalkey Key;
	bool bEqual;
	int iFound;

	if ( (pLeft->Data.Handle.Ops != pRight->Data.Handle.Ops) ||
		 (pLeft->Data.Handle.UserData !=
		  pRight->Data.Handle.UserData) ) {
		return false;
	}
	if ( pLeft->Data.Handle.Ops->Equal == NULL ) {
		__xrtErrorSetType();
		return false;
	}
	Key.Left = pLeft;
	Key.Right = pRight;
	iFound = __xrtValueEqualFind(pContext, Key);
	if ( iFound != 0 ) {
		return iFound > 0;
	}
	if ( !__xrtValueEqualStart(pContext, Key) ) {
		return false;
	}
	if ( !__xrtValueEqualPush(pContext, pLeft, pRight) ) {
		return false;
	}
	if ( !__xrtValueCallbackProtect(
			pContext->Guards,
			pContext->GuardCount
	) ) {
		__xrtValueEqualPop(pContext);
		return false;
	}
	bEqual = pLeft->Data.Handle.Ops->Equal(
		pLeft->Data.Handle.Data,
		pRight->Data.Handle.Data,
		pLeft->Data.Handle.UserData
	);
	__xrtValueCallbackUnprotect(
		pContext->Guards,
		pContext->GuardCount
	);
	__xrtValueEqualPop(pContext);
	return bEqual ? __xrtValueEqualFinish(pContext, Key) : false;
}



/* 递归判断标量或容器结构相等。 */
static bool __xrtValueEqual(
	xvalueequalcontext* pContext,
	const xvalue* pLeft,
	const xvalue* pRight,
	uint32 iDepth
)
{
	xvalueequalkey Key;
	xvaluetype Type;
	bool bEqual;
	int iFound;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( ((pLeft->Flags & XRT_VALUE_FLAG_BUSY) != 0) ||
		 ((pRight->Flags & XRT_VALUE_FLAG_BUSY) != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pLeft == pRight ) {
		return true;
	}
	if ( (pLeft->IdentityEqual != NULL) || (pRight->IdentityEqual != NULL) ) {
		return __xrtValueEqualKnown(pLeft, pRight);
	}
	if ( iDepth >= XRT_VALUE_DEPTH_MAX ) {
		__xrtErrorSetValue();
		return false;
	}
	if ( ((pLeft->Type == XVALUE_INT) || (pLeft->Type == XVALUE_UINT) ||
		  (pLeft->Type == XVALUE_FLOAT)) &&
		 ((pRight->Type == XVALUE_INT) || (pRight->Type == XVALUE_UINT) ||
		  (pRight->Type == XVALUE_FLOAT)) ) {
		return __xrtValueEqualKnown(pLeft, pRight);
	}
	Type = (xvaluetype)pLeft->Type;
	if ( Type != (xvaluetype)pRight->Type ) {
		return false;
	}
	if ( Type == XVALUE_HANDLE ) {
		return __xrtValueHandleEqual(pContext, pLeft, pRight);
	}
	if ( !__xrtValueContainerType(Type) ) {
		return __xrtValueEqualKnown(pLeft, pRight);
	}
	if ( pLeft->Data.Backing == pRight->Data.Backing ) {
		return true;
	}
	if ( __xrtValueContainerCount(pLeft) !=
		 __xrtValueContainerCount(pRight) ) {
		return false;
	}
	Key.Left = pLeft;
	Key.Right = pRight;
	iFound = __xrtValueEqualFind(pContext, Key);
	if ( iFound != 0 ) {
		return iFound > 0;
	}
	if ( !__xrtValueEqualStart(pContext, Key) ||
		 !__xrtValueEqualPush(pContext, pLeft, pRight) ) {
		return false;
	}
	if ( Type == XVALUE_ARRAY ) {
		bEqual = __xrtValueArrayEqual(
			pContext,
			pLeft,
			pRight,
			iDepth
		);
	} else if ( Type == XVALUE_INT_MAP ) {
		bEqual = __xrtValueIntMapEqual(
			pContext,
			pLeft,
			pRight,
			iDepth
		);
	} else if ( Type == XVALUE_SET ) {
		bEqual = __xrtValueSetStructuralEqual(
			pContext,
			pLeft,
			pRight
		);
	} else {
		bEqual = __xrtValueObjectEqual(
			pContext,
			pLeft,
			pRight,
			iDepth
		);
	}
	__xrtValueEqualPop(pContext);
	return bEqual ? __xrtValueEqualFinish(pContext, Key) : false;
}



/* 按数值和容器内容递归判断结构相等。 */
XRT_API bool xrtValueEqual(const xvalue* pLeft, const xvalue* pRight)
{
	xvalueequalcontext Context;
	bool bEqual;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((pLeft->Flags & XRT_VALUE_FLAG_BUSY) != 0) ||
		 ((pRight->Flags & XRT_VALUE_FLAG_BUSY) != 0) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	memset(&Context, 0, sizeof(Context));
	bEqual = __xrtValueEqual(&Context, pLeft, pRight, 0);
	__xrtValueEqualUnit(&Context);
	return bEqual;
}

#endif
