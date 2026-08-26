#include "../internal/xrt_value.h"
#include "../internal/xrt_map.h"



#if defined(XRT_FEATURE_VALUE_CONTAINER)

/* 所有容器 backing 共用原子引用头。 */
struct xvaluebacking {
	volatile int32 RefCount;
	uint16 Type;
	uint16 Flags;
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
typedef struct xvalueobjectbacking {
	xvaluebacking Base;
	xmap Items;
} xvalueobjectbacking;



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
static void __xrtValueBackingRelease(xvaluebacking* pBacking)
{
	int32 iReferences;

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
		xrtMapUnit(&((xvalueobjectbacking*)pBacking)->Items);
	}
	xrtFree(pBacking);
}



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

	if ( !__xrtMapSetDropReverse(
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
	if ( __xrtAtomicRefLoad(&pOld->RefCount) == 1 ) {
		return true;
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
	if ( __xrtValueContainerType((xvaluetype)pItem->Type) ||
		 (pItem->Type > XVALUE_HANDLE) ) {
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
void __xrtValueContainerRelease(xvalue* pValue)
{
	__xrtValueBackingRelease(pValue->Data.Backing);
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
XRT_API xvalue* xrtValueArray(void)
{
	return __xrtValueContainerCreate(XVALUE_ARRAY);
}



/* 创建空的 int64 键稀疏映射。 */
XRT_API xvalue* xrtValueIntMap(void)
{
	return __xrtValueContainerCreate(XVALUE_INT_MAP);
}



/* 创建空的可哈希动态值集合。 */
XRT_API xvalue* xrtValueSet(void)
{
	return __xrtValueContainerCreate(XVALUE_SET);
}



/* 创建保持首次插入顺序的字符串键对象。 */
XRT_API xvalue* xrtValueObject(void)
{
	return __xrtValueContainerCreate(XVALUE_OBJECT);
}



/* 创建遍历顺序稳定、拥有值按栈顺序析构的对象。 */
XRT_API xvalue* xrtValueObjectLifo(void)
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



/* 保证容器至少可容纳指定数量的元素。 */
XRT_API bool xrtValueReserve(xvalue* pValue, size_t iCapacity)
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



/* 释放容器多余容量，保留现有元素。 */
XRT_API bool xrtValueTrim(xvalue* pValue)
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



/* 清空容器并释放其中持有的全部值引用。 */
XRT_API bool xrtValueClear(xvalue* pValue)
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
XRT_API xvalue* xrtValueArrayEdit(xvalue* pArray, size_t iIndex)
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



/* 增加引用后向数组末尾加入值。 */
XRT_API bool xrtValueArrayAppend(xvalue* pArray, const xvalue* pItem)
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



/* 成功时把来源引用移交给数组。 */
XRT_API bool xrtValueArrayAppendTake(xvalue* pArray, xvalue** pItem)
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



/* 无论成功失败都消费临时值。 */
XRT_API bool xrtValueArrayAppendNew(xvalue* pArray, xvalue* pItem)
{
	bool bResult = (pItem != NULL) && xrtValueArrayAppendTake(pArray, &pItem);

	xrtValueRelease(pItem);
	return bResult;
}



/* 增加引用后在指定位置插入值。 */
XRT_API bool xrtValueArrayInsert(
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



/* 成功时把来源引用移交到指定插入位置。 */
XRT_API bool xrtValueArrayInsertTake(
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



/* 无论成功失败都消费临时值并在指定位置插入。 */
XRT_API bool xrtValueArrayInsertNew(
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
XRT_API bool xrtValueArraySet(
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



/* 成功时把来源引用移交到指定位置。 */
XRT_API bool xrtValueArraySetTake(
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



/* 无论成功失败都消费临时值并替换指定位置。 */
XRT_API bool xrtValueArraySetNew(
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



/* 删除数组区间并释放其中的值。 */
XRT_API bool xrtValueArrayRemove(
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



/* 从数组移交指定值。 */
XRT_API xvalue* xrtValueArrayTake(xvalue* pArray, size_t iIndex)
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



/* 从数组末尾移交一个值。 */
XRT_API xvalue* xrtValueArrayPop(xvalue* pArray)
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



/* 交换两个数组元素。 */
XRT_API bool xrtValueArraySwap(
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
XRT_API xvalue* xrtValueIntMapEdit(xvalue* pMap, int64 iKey)
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
XRT_API bool xrtValueIntMapSet(
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



/* 成功时把来源引用移交到整数键。 */
XRT_API bool xrtValueIntMapSetTake(
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



/* 无论成功失败都消费临时值并设置整数键。 */
XRT_API bool xrtValueIntMapSetNew(
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
XRT_API bool xrtValueIntMapRemove(xvalue* pMap, int64 iKey)
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



/* 移交整数键对应值。 */
XRT_API xvalue* xrtValueIntMapTake(xvalue* pMap, int64 iKey)
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
XRT_API xvalue* xrtValueObjectEdit(xvalue* pObject, xstrview Key)
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
XRT_API bool xrtValueObjectSet(
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



/* 成功时把来源引用移交到对象键。 */
XRT_API bool xrtValueObjectSetTake(
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



/* 无论成功失败都消费临时值并设置对象键。 */
XRT_API bool xrtValueObjectSetNew(
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
XRT_API bool xrtValueObjectRemove(xvalue* pObject, xstrview Key)
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



/* 移交对象键对应值。 */
XRT_API xvalue* xrtValueObjectTake(xvalue* pObject, xstrview Key)
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



/* 增加引用后把可哈希标量加入集合。 */
XRT_API bool xrtValueSetAdd(xvalue* pSet, const xvalue* pItem)
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



/* 成功时消费来源引用；重复元素同样视为成功。 */
XRT_API bool xrtValueSetAddTake(xvalue* pSet, xvalue** pItem)
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



/* 无论成功失败都消费临时值并尝试加入集合。 */
XRT_API bool xrtValueSetAddNew(xvalue* pSet, xvalue* pItem)
{
	bool bResult = (pItem != NULL) && xrtValueSetAddTake(pSet, &pItem);

	xrtValueRelease(pItem);
	return bResult;
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
XRT_API bool xrtValueSetRemove(xvalue* pSet, const xvalue* pItem)
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



/* 移交集合中的规范值。 */
XRT_API xvalue* xrtValueSetTake(xvalue* pSet, const xvalue* pItem)
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



/* 启动一个方向明确的 backing 快照迭代器。 */
static bool __xrtValueIterStart(
	const xvalue* pValue,
	xvalueiter* pIterator,
	int iDirection
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
		memset(pIterator, 0, sizeof(xvalueiter));
		return false;
	}
	return true;
}



/* 启动按容器稳定顺序的快照迭代。 */
XRT_API bool xrtValueIterBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	return __xrtValueIterStart(pValue, pIterator, 1);
}



/* 启动按容器稳定逆序的快照迭代。 */
XRT_API bool xrtValueIterRBegin(
	const xvalue* pValue,
	xvalueiter* pIterator
)
{
	return __xrtValueIterStart(pValue, pIterator, -1);
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
XRT_API xvalueiter* xrtValueIterCreate(const xvalue* pValue)
{
	return __xrtValueIterCreate(pValue, 1);
}



/* 创建按稳定逆序推进的拥有式快照迭代器。 */
XRT_API xvalueiter* xrtValueIterRCreate(const xvalue* pValue)
{
	return __xrtValueIterCreate(pValue, -1);
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
	if ( (pIterator == NULL) || (pIterator->Backing == NULL) ) {
		return;
	}
	if ( pIterator->Type == XVALUE_INT_MAP ) {
		xrtIntMapIterEnd(&pIterator->State.IntMap);
	} else if ( pIterator->Type == XVALUE_SET ) {
		xrtSetIterEnd(&pIterator->State.Set);
	} else if ( pIterator->Type == XVALUE_OBJECT ) {
		xrtMapIterEnd(&pIterator->State.Map);
	}
	__xrtValueBackingRelease((xvaluebacking*)pIterator->Backing);
	memset(pIterator, 0, sizeof(xvalueiter));
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

#endif
