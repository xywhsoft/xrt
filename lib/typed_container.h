

// Typed Container 函数库
// 这一层把 XRT 现有 array/list/dict 基础设施包装为带元素类型的容器。
// 元素复制、释放、默认初始化统一走 type ops，便于 xlang 静态泛型直接落到 XRT。

static uint32 __xrtTypedItemLength(const xrt_type_desc* pType)
{
	if ( pType != NULL && pType->Size > 0 ) {
		return (uint32)pType->Size;
	}
	return (uint32)sizeof(ptr);
}


static void __xrtTypedInitItem(const xrt_type_desc* pType, ptr pItem)
{
	if ( pItem == NULL ) {
		return;
	}
	if ( pType != NULL && pType->Ops != NULL && pType->Ops->init != NULL ) {
		pType->Ops->init(pItem);
	} else {
		memset(pItem, 0, __xrtTypedItemLength(pType));
	}
}


static void __xrtTypedCopyItem(const xrt_type_desc* pType, ptr pDst, const ptr pSrc)
{
	if ( pDst == NULL ) {
		return;
	}
	if ( pSrc == NULL ) {
		__xrtTypedInitItem(pType, pDst);
		return;
	}
	if ( pType != NULL && pType->Ops != NULL && pType->Ops->copy != NULL ) {
		pType->Ops->copy(pDst, pSrc);
	} else {
		memcpy(pDst, pSrc, __xrtTypedItemLength(pType));
	}
}


static void __xrtTypedDropItem(const xrt_type_desc* pType, ptr pItem)
{
	if ( pItem == NULL ) {
		return;
	}
	if ( pType != NULL && pType->Ops != NULL && pType->Ops->drop != NULL ) {
		pType->Ops->drop(pItem);
	}
}


static ptr __xrtTypedUnboxTemp(const xrt_type_desc* pType, xvalue pVal)
{
	ptr pItem;
	uint32 iLen;

	iLen = __xrtTypedItemLength(pType);
	pItem = xrtMalloc(iLen);
	if ( pItem == NULL ) {
		return NULL;
	}
	__xrtTypedInitItem(pType, pItem);
	if ( !xrtTypeUnboxValue(pType, pVal, pItem) ) {
		__xrtTypedDropItem(pType, pItem);
		xrtFree(pItem);
		return NULL;
	}
	return pItem;
}


static void __xrtTypedFreeTemp(const xrt_type_desc* pType, ptr pItem)
{
	if ( pItem != NULL ) {
		__xrtTypedDropItem(pType, pItem);
		xrtFree(pItem);
	}
}


static bool __xrtTypedSameItemType(const xrt_type_desc* pLeft, const xrt_type_desc* pRight)
{
	return xrtTypeSame(pLeft, pRight);
}


static int __xrtTypedCompareItem(const xrt_type_desc* pType, const ptr pLeft, const ptr pRight)
{
	if ( pType != NULL && pType->Ops != NULL && pType->Ops->compare != NULL ) {
		return pType->Ops->compare(pLeft, pRight);
	}
	return memcmp(pLeft, pRight, __xrtTypedItemLength(pType));
}


static xvalue __xrtTypedBoxItem(const xrt_type_desc* pType, const ptr pItem)
{
	xvalue pVal;
	pVal = xrtTypeBoxValue(pType, pItem);
	return pVal != NULL ? pVal : xvoCreateNull();
}


// ------------------------------------ typed array ------------------------------------

XXAPI xtarray xrtTypedArrayCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtarray pArray = (xtarray)xrtMalloc(sizeof(xrt_typed_array_struct));
	if ( pArray == NULL ) {
		return NULL;
	}
	xrtTypedArrayInit(pArray, pItemType, iMode);
	pArray->RefCount = 1;
	pArray->HeapOwned = TRUE;
	return pArray;
}


XXAPI xtarray xrtTypedArrayRetain(xtarray pArray)
{
	if ( pArray != NULL && pArray->HeapOwned && pArray->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pArray->RefCount, 1);
	}
	return pArray;
}


XXAPI void xrtTypedArrayRelease(xtarray pArray)
{
	if ( pArray == NULL || !pArray->HeapOwned || pArray->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pArray->RefCount, -1) == 0 ) {
		xrtTypedArrayUnit(pArray);
		xrtFree(pArray);
	}
}


XXAPI void xrtTypedArrayDestroy(xtarray pArray)
{
	xrtTypedArrayRelease(pArray);
}


XXAPI void xrtTypedArrayInit(xtarray pArray, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pArray == NULL ) {
		return;
	}
	pArray->ItemType = pItemType;
	pArray->ItemLength = __xrtTypedItemLength(pItemType);
	pArray->RefCount = 0;
	pArray->HeapOwned = FALSE;
	xrtArrayInit(&pArray->Array, pArray->ItemLength, iMode);
}


XXAPI void xrtTypedArrayUnit(xtarray pArray)
{
	uint32 i;
	if ( pArray == NULL ) {
		return;
	}
	for ( i = 1; i <= pArray->Array.Count; ++i ) {
		__xrtTypedDropItem(pArray->ItemType, xrtArrayGet_Unsafe(&pArray->Array, i));
	}
	xrtArrayUnit(&pArray->Array);
	pArray->ItemType = NULL;
	pArray->ItemLength = 0;
}


XXAPI ptr xrtTypedArrayAppend(xtarray pArray, const ptr pItem)
{
	uint32 iPos;
	ptr pSlot;
	if ( pArray == NULL ) {
		return NULL;
	}
	iPos = xrtArrayAppend(&pArray->Array, 1);
	if ( iPos == 0 ) {
		return NULL;
	}
	pSlot = xrtArrayGet_Unsafe(&pArray->Array, iPos);
	__xrtTypedCopyItem(pArray->ItemType, pSlot, pItem);
	return pSlot;
}


XXAPI bool xrtTypedArrayAppendValue(xtarray pArray, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pArray == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pArray->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedArrayAppend(pArray, pItem) != NULL;
	__xrtTypedFreeTemp(pArray->ItemType, pItem);
	return bOk;
}


XXAPI ptr xrtTypedArrayInsert(xtarray pArray, uint32 iPos, const ptr pItem)
{
	uint32 iNewPos;
	ptr pSlot;
	if ( pArray == NULL ) {
		return NULL;
	}
	iNewPos = xrtArrayInsert(&pArray->Array, iPos > 0 ? iPos - 1 : 0, 1);
	if ( iNewPos == 0 ) {
		return NULL;
	}
	pSlot = xrtArrayGet_Unsafe(&pArray->Array, iNewPos);
	__xrtTypedCopyItem(pArray->ItemType, pSlot, pItem);
	return pSlot;
}


XXAPI bool xrtTypedArrayInsertValue(xtarray pArray, uint32 iPos, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pArray == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pArray->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedArrayInsert(pArray, iPos, pItem) != NULL;
	__xrtTypedFreeTemp(pArray->ItemType, pItem);
	return bOk;
}


XXAPI bool xrtTypedArraySet(xtarray pArray, uint32 iPos, const ptr pItem)
{
	ptr pSlot;
	if ( pArray == NULL ) {
		return FALSE;
	}
	pSlot = xrtArrayGet(&pArray->Array, iPos);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	__xrtTypedDropItem(pArray->ItemType, pSlot);
	__xrtTypedCopyItem(pArray->ItemType, pSlot, pItem);
	return TRUE;
}


XXAPI bool xrtTypedArraySetValue(xtarray pArray, uint32 iPos, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pArray == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pArray->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedArraySet(pArray, iPos, pItem);
	__xrtTypedFreeTemp(pArray->ItemType, pItem);
	return bOk;
}


XXAPI ptr xrtTypedArrayGet(xtarray pArray, uint32 iPos)
{
	if ( pArray == NULL ) {
		return NULL;
	}
	return xrtArrayGet(&pArray->Array, iPos);
}


XXAPI bool xrtTypedArrayRemove(xtarray pArray, uint32 iPos, uint32 iCount)
{
	uint32 i;
	uint32 iEnd;
	if ( pArray == NULL || iCount == 0 || iPos == 0 || iPos > pArray->Array.Count ) {
		return FALSE;
	}
	iEnd = iPos + iCount;
	if ( iEnd < iPos || iEnd > pArray->Array.Count + 1 ) {
		iEnd = pArray->Array.Count + 1;
	}
	for ( i = iPos; i < iEnd; ++i ) {
		__xrtTypedDropItem(pArray->ItemType, xrtArrayGet_Unsafe(&pArray->Array, i));
	}
	return xrtArrayRemove(&pArray->Array, iPos, iEnd - iPos);
}


XXAPI bool xrtTypedArrayTake(xtarray pArray, uint32 iPos, ptr pOut)
{
	ptr pSlot;
	if ( pArray == NULL || iPos == 0 || iPos > pArray->Array.Count ) {
		return FALSE;
	}
	pSlot = xrtArrayGet_Unsafe(&pArray->Array, iPos);
	if ( pOut != NULL ) {
		__xrtTypedCopyItem(pArray->ItemType, pOut, pSlot);
	}
	return xrtTypedArrayRemove(pArray, iPos, 1);
}


XXAPI xvalue xrtTypedArrayTakeValue(xtarray pArray, uint32 iPos)
{
	ptr pSlot;
	xvalue pRet;
	if ( pArray == NULL || iPos == 0 || iPos > pArray->Array.Count ) {
		return xvoCreateNull();
	}
	pSlot = xrtArrayGet_Unsafe(&pArray->Array, iPos);
	pRet = __xrtTypedBoxItem(pArray->ItemType, pSlot);
	xrtTypedArrayRemove(pArray, iPos, 1);
	return pRet;
}


XXAPI bool xrtTypedArrayPop(xtarray pArray, ptr pOut)
{
	if ( pArray == NULL || pArray->Array.Count == 0 ) {
		return FALSE;
	}
	return xrtTypedArrayTake(pArray, pArray->Array.Count, pOut);
}


XXAPI xvalue xrtTypedArrayPopValue(xtarray pArray)
{
	if ( pArray == NULL || pArray->Array.Count == 0 ) {
		return xvoCreateNull();
	}
	return xrtTypedArrayTakeValue(pArray, pArray->Array.Count);
}


XXAPI bool xrtTypedArrayClear(xtarray pArray)
{
	uint32 i;
	if ( pArray == NULL ) {
		return FALSE;
	}
	for ( i = 1; i <= pArray->Array.Count; ++i ) {
		__xrtTypedDropItem(pArray->ItemType, xrtArrayGet_Unsafe(&pArray->Array, i));
	}
	pArray->Array.Count = 0;
	return TRUE;
}


XXAPI bool xrtTypedArrayReverse(xtarray pArray)
{
	uint32 i;
	uint32 iCount;
	if ( pArray == NULL ) {
		return FALSE;
	}
	iCount = pArray->Array.Count;
	for ( i = 1; i <= iCount / 2; ++i ) {
		if ( !xrtArraySwap(&pArray->Array, i, iCount - i + 1) ) {
			return FALSE;
		}
	}
	return TRUE;
}


XXAPI int64 xrtTypedArrayIndexOf(xtarray pArray, const ptr pItem)
{
	uint32 i;
	if ( pArray == NULL || pItem == NULL ) {
		return -1;
	}
	for ( i = 1; i <= pArray->Array.Count; ++i ) {
		if ( __xrtTypedCompareItem(pArray->ItemType, xrtArrayGet_Unsafe(&pArray->Array, i), pItem) == 0 ) {
			return (int64)(i - 1);
		}
	}
	return -1;
}


XXAPI int64 xrtTypedArrayIndexOfValue(xtarray pArray, xvalue pVal)
{
	ptr pItem;
	int64 iRet;
	if ( pArray == NULL ) {
		return -1;
	}
	pItem = __xrtTypedUnboxTemp(pArray->ItemType, pVal);
	if ( pItem == NULL ) {
		return -1;
	}
	iRet = xrtTypedArrayIndexOf(pArray, pItem);
	__xrtTypedFreeTemp(pArray->ItemType, pItem);
	return iRet;
}


XXAPI bool xrtTypedArrayContains(xtarray pArray, const ptr pItem)
{
	return xrtTypedArrayIndexOf(pArray, pItem) >= 0;
}


XXAPI bool xrtTypedArrayContainsValue(xtarray pArray, xvalue pVal)
{
	return xrtTypedArrayIndexOfValue(pArray, pVal) >= 0;
}


XXAPI bool xrtTypedArrayExtend(xtarray pTarget, xtarray pSource)
{
	uint32 i;
	uint32 iCount;
	xtarray pSnapshot = NULL;
	if ( pTarget == NULL || pSource == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pTarget->ItemType, pSource->ItemType) ) {
		return FALSE;
	}
	if ( pTarget == pSource ) {
		pSnapshot = xrtTypedArrayClone(pSource);
		if ( pSnapshot == NULL ) {
			return FALSE;
		}
		pSource = pSnapshot;
	}
	iCount = pSource->Array.Count;
	for ( i = 1; i <= iCount; ++i ) {
		if ( xrtTypedArrayAppend(pTarget, xrtArrayGet_Unsafe(&pSource->Array, i)) == NULL ) {
			if ( pSnapshot != NULL ) {
				xrtTypedArrayDestroy(pSnapshot);
			}
			return FALSE;
		}
	}
	if ( pSnapshot != NULL ) {
		xrtTypedArrayDestroy(pSnapshot);
	}
	return TRUE;
}


XXAPI xtarray xrtTypedArrayCloneEx(xtarray pArray, uint32 iMode)
{
	xtarray pRet;
	if ( pArray == NULL ) {
		return NULL;
	}
	pRet = xrtTypedArrayCreate(pArray->ItemType, iMode);
	if ( pRet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayExtend(pRet, pArray) ) {
		xrtTypedArrayDestroy(pRet);
		return NULL;
	}
	return pRet;
}


XXAPI xtarray xrtTypedArrayMoveToShared(xtarray pArray)
{
	xtarray pResult;
	if ( pArray == NULL || xrtOwnerGetMode(&pArray->Array.Owner) == XRT_OBJMODE_SHARED ) {
		return pArray;
	}
	pResult = xrtTypedArrayCloneEx(pArray, XRT_OBJMODE_SHARED);
	if ( pResult != NULL ) {
		if ( pArray->HeapOwned ) {
			xrtTypedArrayDestroy(pArray);
		} else {
			xrtTypedArrayUnit(pArray);
		}
	}
	return pResult;
}


XXAPI xtarray xrtTypedArrayClone(xtarray pArray)
{
	return pArray != NULL ?
		xrtTypedArrayCloneEx(pArray, xrtOwnerGetMode(&pArray->Array.Owner)) : NULL;
}


XXAPI xtarray xrtTypedArrayConcat(xtarray pLeft, xtarray pRight)
{
	xtarray pRet;
	if ( pLeft == NULL || pRight == NULL ) {
		return NULL;
	}
	if ( !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return NULL;
	}
	pRet = xrtTypedArrayClone(pLeft);
	if ( pRet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedArrayExtend(pRet, pRight) ) {
		xrtTypedArrayDestroy(pRet);
		return NULL;
	}
	return pRet;
}


XXAPI bool xrtTypedArrayEquals(xtarray pLeft, xtarray pRight)
{
	uint32 i;
	if ( pLeft == NULL || pRight == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	if ( pLeft->Array.Count != pRight->Array.Count ) {
		return FALSE;
	}
	for ( i = 1; i <= pLeft->Array.Count; ++i ) {
		if ( __xrtTypedCompareItem(pLeft->ItemType, xrtArrayGet_Unsafe(&pLeft->Array, i), xrtArrayGet_Unsafe(&pRight->Array, i)) != 0 ) {
			return FALSE;
		}
	}
	return TRUE;
}


XXAPI uint32 xrtTypedArrayCount(xtarray pArray)
{
	return pArray != NULL ? pArray->Array.Count : 0;
}


XXAPI const xrt_type_desc* xrtTypedArrayItemType(xtarray pArray)
{
	return pArray != NULL ? pArray->ItemType : NULL;
}


// ------------------------------------ typed list ------------------------------------

static bool __xrtTypedListDropProc(int64 iKey, ptr pVal, ptr pArg)
{
	xtlist pList = (xtlist)pArg;
	(void)iKey;
	if ( pList != NULL ) {
		__xrtTypedDropItem(pList->ItemType, pVal);
	}
	return FALSE;
}


typedef struct __xrt_typed_list_item_at_arg {
	uint32 Index;
	uint32 Pos;
	int64 Key;
	ptr Item;
} __xrt_typed_list_item_at_arg;


static bool __xrtTypedListItemAtProc(int64 iKey, ptr pVal, ptr pArg)
{
	__xrt_typed_list_item_at_arg* pState = (__xrt_typed_list_item_at_arg*)pArg;
	if ( pState == NULL ) {
		return TRUE;
	}
	if ( pState->Pos == pState->Index ) {
		pState->Key = iKey;
		pState->Item = pVal;
		return TRUE;
	}
	pState->Pos++;
	return FALSE;
}


typedef struct __xrt_typed_list_contains_arg {
	const xrt_type_desc* ItemType;
	ptr Item;
	bool Found;
} __xrt_typed_list_contains_arg;


static bool __xrtTypedListContainsProc(int64 iKey, ptr pVal, ptr pArg)
{
	__xrt_typed_list_contains_arg* pState = (__xrt_typed_list_contains_arg*)pArg;
	(void)iKey;
	if ( pState == NULL || pState->Item == NULL ) {
		return TRUE;
	}
	if ( __xrtTypedCompareItem(pState->ItemType, pVal, pState->Item) == 0 ) {
		pState->Found = TRUE;
		return TRUE;
	}
	return FALSE;
}


typedef struct __xrt_typed_list_merge_arg {
	xtlist Target;
	bool Failed;
} __xrt_typed_list_merge_arg;


static bool __xrtTypedListMergeProc(int64 iKey, ptr pVal, ptr pArg)
{
	__xrt_typed_list_merge_arg* pState = (__xrt_typed_list_merge_arg*)pArg;
	if ( pState == NULL || pState->Target == NULL ) {
		return TRUE;
	}
	if ( xrtTypedListSet(pState->Target, iKey, pVal) == NULL ) {
		pState->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


typedef struct __xrt_typed_list_equals_arg {
	xtlist Right;
	const xrt_type_desc* ItemType;
	bool Equal;
} __xrt_typed_list_equals_arg;


static bool __xrtTypedListEqualsProc(int64 iKey, ptr pVal, ptr pArg)
{
	ptr pRightVal;
	__xrt_typed_list_equals_arg* pState = (__xrt_typed_list_equals_arg*)pArg;
	if ( pState == NULL || pState->Right == NULL ) {
		return TRUE;
	}
	pRightVal = xrtTypedListGet(pState->Right, iKey);
	if ( pRightVal == NULL || __xrtTypedCompareItem(pState->ItemType, pVal, pRightVal) != 0 ) {
		pState->Equal = FALSE;
		return TRUE;
	}
	return FALSE;
}


XXAPI xtlist xrtTypedListCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtlist pList = (xtlist)xrtMalloc(sizeof(xrt_typed_list_struct));
	if ( pList == NULL ) {
		return NULL;
	}
	xrtTypedListInit(pList, pItemType, iMode);
	pList->RefCount = 1;
	pList->HeapOwned = TRUE;
	return pList;
}


XXAPI xtlist xrtTypedListRetain(xtlist pList)
{
	if ( pList != NULL && pList->HeapOwned && pList->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pList->RefCount, 1);
	}
	return pList;
}


XXAPI void xrtTypedListRelease(xtlist pList)
{
	if ( pList == NULL || !pList->HeapOwned || pList->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pList->RefCount, -1) == 0 ) {
		xrtTypedListUnit(pList);
		xrtFree(pList);
	}
}


XXAPI void xrtTypedListDestroy(xtlist pList)
{
	xrtTypedListRelease(pList);
}


XXAPI void xrtTypedListInit(xtlist pList, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pList == NULL ) {
		return;
	}
	pList->ItemType = pItemType;
	pList->ItemLength = __xrtTypedItemLength(pItemType);
	pList->RefCount = 0;
	pList->HeapOwned = FALSE;
	xrtListInit(&pList->List, pList->ItemLength, iMode);
}


XXAPI void xrtTypedListUnit(xtlist pList)
{
	if ( pList == NULL ) {
		return;
	}
	xrtListWalk(&pList->List, __xrtTypedListDropProc, pList);
	xrtListUnit(&pList->List);
	pList->ItemType = NULL;
	pList->ItemLength = 0;
}


XXAPI ptr xrtTypedListSet(xtlist pList, int64 iKey, const ptr pItem)
{
	bool bNew = FALSE;
	ptr pSlot;
	if ( pList == NULL ) {
		return NULL;
	}
	pSlot = xrtListSet(&pList->List, iKey, &bNew);
	if ( pSlot == NULL ) {
		return NULL;
	}
	if ( !bNew ) {
		__xrtTypedDropItem(pList->ItemType, pSlot);
	}
	__xrtTypedCopyItem(pList->ItemType, pSlot, pItem);
	return pSlot;
}


XXAPI bool xrtTypedListSetValue(xtlist pList, int64 iKey, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pList == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pList->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedListSet(pList, iKey, pItem) != NULL;
	__xrtTypedFreeTemp(pList->ItemType, pItem);
	return bOk;
}


XXAPI ptr xrtTypedListGet(xtlist pList, int64 iKey)
{
	if ( pList == NULL ) {
		return NULL;
	}
	return xrtListGet(&pList->List, iKey);
}


XXAPI ptr xrtTypedListItemAt(xtlist pList, uint32 iIndex, int64* pKey)
{
	__xrt_typed_list_item_at_arg state;

	if ( pList == NULL ) {
		return NULL;
	}

	state.Index = iIndex;
	state.Pos = 0;
	state.Key = 0;
	state.Item = NULL;
	xrtListWalk(&pList->List, __xrtTypedListItemAtProc, &state);
	if ( state.Item != NULL && pKey != NULL ) {
		*pKey = state.Key;
	}
	return state.Item;
}


XXAPI bool xrtTypedListRemove(xtlist pList, int64 iKey)
{
	ptr pSlot;
	if ( pList == NULL ) {
		return FALSE;
	}
	pSlot = xrtListGet(&pList->List, iKey);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	__xrtTypedDropItem(pList->ItemType, pSlot);
	return xrtListRemove(&pList->List, iKey);
}


XXAPI bool xrtTypedListTake(xtlist pList, int64 iKey, ptr pOut)
{
	ptr pSlot;
	if ( pList == NULL ) {
		return FALSE;
	}
	pSlot = xrtListGet(&pList->List, iKey);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	if ( pOut != NULL ) {
		__xrtTypedCopyItem(pList->ItemType, pOut, pSlot);
	}
	__xrtTypedDropItem(pList->ItemType, pSlot);
	return xrtListRemove(&pList->List, iKey);
}


XXAPI xvalue xrtTypedListTakeValue(xtlist pList, int64 iKey)
{
	ptr pSlot;
	xvalue pRet;
	if ( pList == NULL ) {
		return xvoCreateNull();
	}
	pSlot = xrtListGet(&pList->List, iKey);
	if ( pSlot == NULL ) {
		return xvoCreateNull();
	}
	pRet = __xrtTypedBoxItem(pList->ItemType, pSlot);
	__xrtTypedDropItem(pList->ItemType, pSlot);
	xrtListRemove(&pList->List, iKey);
	return pRet;
}


XXAPI bool xrtTypedListClear(xtlist pList)
{
	const xrt_type_desc* pType;
	uint32 iLen;
	if ( pList == NULL ) {
		return FALSE;
	}
	pType = pList->ItemType;
	iLen = pList->ItemLength;
	xrtListWalk(&pList->List, __xrtTypedListDropProc, pList);
	xrtListUnit(&pList->List);
	xrtListInit(&pList->List, iLen, XRT_OBJMODE_LOCAL);
	pList->ItemType = pType;
	pList->ItemLength = iLen;
	return TRUE;
}


XXAPI bool xrtTypedListContains(xtlist pList, const ptr pItem)
{
	__xrt_typed_list_contains_arg state;
	if ( pList == NULL || pItem == NULL ) {
		return FALSE;
	}
	state.ItemType = pList->ItemType;
	state.Item = pItem;
	state.Found = FALSE;
	xrtListWalk(&pList->List, __xrtTypedListContainsProc, &state);
	return state.Found;
}


XXAPI bool xrtTypedListContainsValue(xtlist pList, xvalue pVal)
{
	ptr pItem;
	bool bRet;
	if ( pList == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pList->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bRet = xrtTypedListContains(pList, pItem);
	__xrtTypedFreeTemp(pList->ItemType, pItem);
	return bRet;
}


XXAPI bool xrtTypedListMerge(xtlist pTarget, xtlist pSource)
{
	__xrt_typed_list_merge_arg state;
	xtlist pSnapshot = NULL;
	if ( pTarget == NULL || pSource == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pTarget->ItemType, pSource->ItemType) ) {
		return FALSE;
	}
	if ( pTarget == pSource ) {
		pSnapshot = xrtTypedListClone(pSource);
		if ( pSnapshot == NULL ) {
			return FALSE;
		}
		pSource = pSnapshot;
	}
	state.Target = pTarget;
	state.Failed = FALSE;
	xrtListWalk(&pSource->List, __xrtTypedListMergeProc, &state);
	if ( pSnapshot != NULL ) {
		xrtTypedListDestroy(pSnapshot);
	}
	return state.Failed == FALSE;
}


XXAPI xtlist xrtTypedListCloneEx(xtlist pList, uint32 iMode)
{
	xtlist pRet;
	if ( pList == NULL ) {
		return NULL;
	}
	pRet = xrtTypedListCreate(pList->ItemType, iMode);
	if ( pRet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedListMerge(pRet, pList) ) {
		xrtTypedListDestroy(pRet);
		return NULL;
	}
	return pRet;
}


XXAPI xtlist xrtTypedListMoveToShared(xtlist pList)
{
	xtlist pResult;
	if ( pList == NULL || xrtOwnerGetMode(&pList->List.Owner) == XRT_OBJMODE_SHARED ) {
		return pList;
	}
	pResult = xrtTypedListCloneEx(pList, XRT_OBJMODE_SHARED);
	if ( pResult != NULL ) {
		if ( pList->HeapOwned ) {
			xrtTypedListDestroy(pList);
		} else {
			xrtTypedListUnit(pList);
		}
	}
	return pResult;
}


XXAPI xtlist xrtTypedListClone(xtlist pList)
{
	return pList != NULL ?
		xrtTypedListCloneEx(pList, xrtOwnerGetMode(&pList->List.Owner)) : NULL;
}


XXAPI bool xrtTypedListEquals(xtlist pLeft, xtlist pRight)
{
	__xrt_typed_list_equals_arg state;
	if ( pLeft == NULL || pRight == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	if ( xrtListCount(&pLeft->List) != xrtListCount(&pRight->List) ) {
		return FALSE;
	}
	state.Right = pRight;
	state.ItemType = pLeft->ItemType;
	state.Equal = TRUE;
	xrtListWalk(&pLeft->List, __xrtTypedListEqualsProc, &state);
	return state.Equal;
}


XXAPI uint32 xrtTypedListCount(xtlist pList)
{
	return pList != NULL ? xrtListCount(&pList->List) : 0;
}


XXAPI const xrt_type_desc* xrtTypedListItemType(xtlist pList)
{
	return pList != NULL ? pList->ItemType : NULL;
}


// ------------------------------------ typed set ------------------------------------

XXAPI xtset xrtTypedSetCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtset pSet = (xtset)xrtMalloc(sizeof(xrt_typed_set_struct));
	if ( pSet == NULL ) {
		return NULL;
	}
	xrtTypedSetInit(pSet, pItemType, iMode);
	pSet->RefCount = 1;
	pSet->HeapOwned = TRUE;
	return pSet;
}


XXAPI xtset xrtTypedSetRetain(xtset pSet)
{
	if ( pSet != NULL && pSet->HeapOwned && pSet->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pSet->RefCount, 1);
	}
	return pSet;
}


XXAPI void xrtTypedSetRelease(xtset pSet)
{
	if ( pSet == NULL || !pSet->HeapOwned || pSet->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pSet->RefCount, -1) == 0 ) {
		xrtTypedSetUnit(pSet);
		xrtFree(pSet);
	}
}


XXAPI void xrtTypedSetDestroy(xtset pSet)
{
	xrtTypedSetRelease(pSet);
}


XXAPI void xrtTypedSetInit(xtset pSet, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pSet == NULL ) {
		return;
	}
	pSet->ItemType = pItemType;
	pSet->ItemLength = __xrtTypedItemLength(pItemType);
	pSet->RefCount = 0;
	pSet->HeapOwned = FALSE;
	xrtSetInit(&pSet->Set, pItemType, iMode);
}


XXAPI void xrtTypedSetUnit(xtset pSet)
{
	if ( pSet == NULL ) {
		return;
	}
	xrtSetUnit(&pSet->Set);
	pSet->ItemType = NULL;
	pSet->ItemLength = 0;
}


XXAPI bool xrtTypedSetAdd(xtset pSet, const ptr pItem)
{
	if ( pSet == NULL ) {
		return FALSE;
	}
	return xrtSetAdd(&pSet->Set, pItem);
}


XXAPI bool xrtTypedSetAddValue(xtset pSet, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pSet == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pSet->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedSetAdd(pSet, pItem);
	__xrtTypedFreeTemp(pSet->ItemType, pItem);
	return bOk;
}


XXAPI bool xrtTypedSetExistsValue(xtset pSet, xvalue pVal)
{
	ptr pItem;
	bool bOk;
	if ( pSet == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pSet->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedSetExists(pSet, pItem);
	__xrtTypedFreeTemp(pSet->ItemType, pItem);
	return bOk;
}


XXAPI bool xrtTypedSetRemoveValue(xtset pSet, xvalue pVal)
{
	ptr pItem;
	bool bOk;
	if ( pSet == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pSet->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedSetRemove(pSet, pItem);
	__xrtTypedFreeTemp(pSet->ItemType, pItem);
	return bOk;
}


XXAPI ptr xrtTypedSetGet(xtset pSet, const ptr pItem)
{
	if ( pSet == NULL ) {
		return NULL;
	}
	return xrtSetGet(&pSet->Set, pItem);
}


XXAPI bool xrtTypedSetExists(xtset pSet, const ptr pItem)
{
	if ( pSet == NULL ) {
		return FALSE;
	}
	return xrtSetExists(&pSet->Set, pItem);
}


XXAPI bool xrtTypedSetRemove(xtset pSet, const ptr pItem)
{
	if ( pSet == NULL ) {
		return FALSE;
	}
	return xrtSetRemove(&pSet->Set, pItem);
}


XXAPI ptr xrtTypedSetItemAt(xtset pSet, uint32 iIndex)
{
	if ( pSet == NULL ) {
		return NULL;
	}
	return xrtSetItemAt(&pSet->Set, iIndex);
}


XXAPI bool xrtTypedSetClear(xtset pSet)
{
	if ( pSet == NULL ) {
		return FALSE;
	}
	return xrtSetClear(&pSet->Set);
}


XXAPI bool xrtTypedSetMerge(xtset pTarget, xtset pSource)
{
	if ( pTarget == NULL || pSource == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pTarget->ItemType, pSource->ItemType) ) {
		return FALSE;
	}
	return xrtSetMerge(&pTarget->Set, &pSource->Set);
}


XXAPI xtset xrtTypedSetCloneEx(xtset pSet, uint32 iMode)
{
	xtset pRet;
	if ( pSet == NULL ) {
		return NULL;
	}
	pRet = xrtTypedSetCreate(pSet->ItemType, iMode);
	if ( pRet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedSetMerge(pRet, pSet) ) {
		xrtTypedSetDestroy(pRet);
		return NULL;
	}
	return pRet;
}


XXAPI xtset xrtTypedSetMoveToShared(xtset pSet)
{
	xtset pResult;
	if ( pSet == NULL || xrtOwnerGetMode(&pSet->Set.Owner) == XRT_OBJMODE_SHARED ) {
		return pSet;
	}
	pResult = xrtTypedSetCloneEx(pSet, XRT_OBJMODE_SHARED);
	if ( pResult != NULL ) {
		if ( pSet->HeapOwned ) {
			xrtTypedSetDestroy(pSet);
		} else {
			xrtTypedSetUnit(pSet);
		}
	}
	return pResult;
}


XXAPI xtset xrtTypedSetClone(xtset pSet)
{
	return pSet != NULL ?
		xrtTypedSetCloneEx(pSet, xrtOwnerGetMode(&pSet->Set.Owner)) : NULL;
}


static xtset __xrtTypedSetFromRaw(xtset pBase, xset pRaw)
{
	xtset pRet;
	if ( pBase == NULL || pRaw == NULL ) {
		if ( pRaw != NULL ) {
			xrtSetDestroy(pRaw);
		}
		return NULL;
	}
	pRet = xrtTypedSetCreate(pBase->ItemType, XRT_OBJMODE_LOCAL);
	if ( pRet == NULL ) {
		xrtSetDestroy(pRaw);
		return NULL;
	}
	if ( !xrtSetMerge(&pRet->Set, pRaw) ) {
		xrtTypedSetDestroy(pRet);
		xrtSetDestroy(pRaw);
		return NULL;
	}
	xrtSetDestroy(pRaw);
	return pRet;
}


XXAPI xtset xrtTypedSetUnion(xtset pLeft, xtset pRight)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(pLeft, xrtSetUnion(&pLeft->Set, &pRight->Set));
}


XXAPI xtset xrtTypedSetIntersection(xtset pLeft, xtset pRight)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(pLeft, xrtSetIntersection(&pLeft->Set, &pRight->Set));
}


XXAPI xtset xrtTypedSetDifference(xtset pLeft, xtset pRight)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(pLeft, xrtSetDifference(&pLeft->Set, &pRight->Set));
}


XXAPI xtset xrtTypedSetSymmetricDifference(xtset pLeft, xtset pRight)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return NULL;
	}
	return __xrtTypedSetFromRaw(pLeft, xrtSetSymmetricDifference(&pLeft->Set, &pRight->Set));
}


XXAPI bool xrtTypedSetIsSubset(xtset pLeft, xtset pRight, bool bProper)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	return xrtSetIsSubset(&pLeft->Set, &pRight->Set, bProper);
}


XXAPI bool xrtTypedSetIsSuperset(xtset pLeft, xtset pRight, bool bProper)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	return xrtSetIsSuperset(&pLeft->Set, &pRight->Set, bProper);
}


XXAPI bool xrtTypedSetEquals(xtset pLeft, xtset pRight)
{
	if ( pLeft == NULL || pRight == NULL || !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	return xrtSetEquals(&pLeft->Set, &pRight->Set);
}


XXAPI uint32 xrtTypedSetCount(xtset pSet)
{
	return pSet != NULL ? xrtSetCount(&pSet->Set) : 0;
}


XXAPI const xrt_type_desc* xrtTypedSetItemType(xtset pSet)
{
	return pSet != NULL ? pSet->ItemType : NULL;
}


// ------------------------------------ typed dict ------------------------------------

static bool __xrtTypedDictDropProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xtdict pDict = (xtdict)pArg;
	(void)pKey;
	if ( pDict != NULL ) {
		__xrtTypedDropItem(pDict->ItemType, pVal);
	}
	return FALSE;
}


static uint32 __xrtTypedDictKeyLen(const ptr sKey, uint32 iKeyLen)
{
	if ( sKey != NULL && iKeyLen == 0 ) {
		return (uint32)strlen((const char*)sKey);
	}
	return iKeyLen;
}


typedef struct __xrt_typed_dict_merge_arg {
	xtdict Target;
	bool Failed;
} __xrt_typed_dict_merge_arg;


static bool __xrtTypedDictMergeProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	__xrt_typed_dict_merge_arg* pState = (__xrt_typed_dict_merge_arg*)pArg;
	if ( pState == NULL || pState->Target == NULL || pKey == NULL ) {
		return TRUE;
	}
	if ( xrtTypedDictSet(pState->Target, pKey->Key, pKey->KeyLen, pVal) == NULL ) {
		pState->Failed = TRUE;
		return TRUE;
	}
	return FALSE;
}


typedef struct __xrt_typed_dict_equals_arg {
	xtdict Right;
	const xrt_type_desc* ItemType;
	bool Equal;
} __xrt_typed_dict_equals_arg;


static bool __xrtTypedDictEqualsProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	ptr pRightVal;
	__xrt_typed_dict_equals_arg* pState = (__xrt_typed_dict_equals_arg*)pArg;
	if ( pState == NULL || pState->Right == NULL || pKey == NULL ) {
		return TRUE;
	}
	pRightVal = xrtTypedDictGet(pState->Right, pKey->Key, pKey->KeyLen);
	if ( pRightVal == NULL || __xrtTypedCompareItem(pState->ItemType, pVal, pRightVal) != 0 ) {
		pState->Equal = FALSE;
		return TRUE;
	}
	return FALSE;
}


XXAPI xtdict xrtTypedDictCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtdict pDict = (xtdict)xrtMalloc(sizeof(xrt_typed_dict_struct));
	if ( pDict == NULL ) {
		return NULL;
	}
	xrtTypedDictInit(pDict, pItemType, iMode);
	pDict->RefCount = 1;
	pDict->HeapOwned = TRUE;
	return pDict;
}


XXAPI xtdict xrtTypedDictRetain(xtdict pDict)
{
	if ( pDict != NULL && pDict->HeapOwned && pDict->RefCount > 0 ) {
		__xrtAtomicAddFetch32(&pDict->RefCount, 1);
	}
	return pDict;
}


XXAPI void xrtTypedDictRelease(xtdict pDict)
{
	if ( pDict == NULL || !pDict->HeapOwned || pDict->RefCount <= 0 ) {
		return;
	}
	if ( __xrtAtomicAddFetch32(&pDict->RefCount, -1) == 0 ) {
		xrtTypedDictUnit(pDict);
		xrtFree(pDict);
	}
}


XXAPI void xrtTypedDictDestroy(xtdict pDict)
{
	xrtTypedDictRelease(pDict);
}


XXAPI void xrtTypedDictInit(xtdict pDict, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pDict == NULL ) {
		return;
	}
	pDict->ItemType = pItemType;
	pDict->ItemLength = __xrtTypedItemLength(pItemType);
	pDict->RefCount = 0;
	pDict->HeapOwned = FALSE;
	xrtDictInit(&pDict->Dict, pDict->ItemLength, iMode);
}


XXAPI void xrtTypedDictUnit(xtdict pDict)
{
	if ( pDict == NULL ) {
		return;
	}
	xrtDictWalk(&pDict->Dict, __xrtTypedDictDropProc, pDict);
	xrtDictUnit(&pDict->Dict);
	pDict->ItemType = NULL;
	pDict->ItemLength = 0;
}


XXAPI ptr xrtTypedDictSet(xtdict pDict, const ptr sKey, uint32 iKeyLen, const ptr pItem)
{
	bool bNew = FALSE;
	ptr pSlot;
	if ( pDict == NULL || sKey == NULL ) {
		return NULL;
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	pSlot = xrtDictSet(&pDict->Dict, (ptr)sKey, iKeyLen, &bNew);
	if ( pSlot == NULL ) {
		return NULL;
	}
	if ( !bNew ) {
		__xrtTypedDropItem(pDict->ItemType, pSlot);
	}
	__xrtTypedCopyItem(pDict->ItemType, pSlot, pItem);
	return pSlot;
}


XXAPI bool xrtTypedDictSetValue(xtdict pDict, const ptr sKey, uint32 iKeyLen, xvalue pVal)
{
	ptr pItem;
	bool bOk;

	if ( pDict == NULL ) {
		return FALSE;
	}
	pItem = __xrtTypedUnboxTemp(pDict->ItemType, pVal);
	if ( pItem == NULL ) {
		return FALSE;
	}
	bOk = xrtTypedDictSet(pDict, sKey, iKeyLen, pItem) != NULL;
	__xrtTypedFreeTemp(pDict->ItemType, pItem);
	return bOk;
}


XXAPI ptr xrtTypedDictGet(xtdict pDict, const ptr sKey, uint32 iKeyLen)
{
	if ( pDict == NULL || sKey == NULL ) {
		return NULL;
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	return xrtDictGet(&pDict->Dict, (ptr)sKey, iKeyLen);
}


XXAPI ptr xrtTypedDictItemAt(xtdict pDict, uint32 iIndex, const char** psKey, uint32* piKeyLen)
{
	uint32 i;
	Dict_Key* pData;
	ptr pItem;

	if ( pDict == NULL ) {
		return NULL;
	}

	i = 0;
	pItem = NULL;
	xrtDictIterBegin(&pDict->Dict);
	for ( pData = (Dict_Key*)xrtDictIterNext(&pDict->Dict);
	      pData != NULL;
	      pData = (Dict_Key*)xrtDictIterNext(&pDict->Dict) ) {
		if ( i == iIndex ) {
			if ( psKey != NULL ) {
				*psKey = (const char*)pData->Key;
			}
			if ( piKeyLen != NULL ) {
				*piKeyLen = pData->KeyLen;
			}
			pItem = (ptr)(&pData[1]);
			break;
		}
		i++;
	}
	xrtDictIterEnd(&pDict->Dict);
	return pItem;
}


XXAPI bool xrtTypedDictExists(xtdict pDict, const ptr sKey, uint32 iKeyLen)
{
	if ( pDict == NULL || sKey == NULL ) {
		return FALSE;
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	return xrtDictExists(&pDict->Dict, (ptr)sKey, iKeyLen);
}


XXAPI bool xrtTypedDictRemove(xtdict pDict, const ptr sKey, uint32 iKeyLen)
{
	ptr pSlot;
	if ( pDict == NULL || sKey == NULL ) {
		return FALSE;
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	pSlot = xrtDictGet(&pDict->Dict, (ptr)sKey, iKeyLen);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	__xrtTypedDropItem(pDict->ItemType, pSlot);
	return xrtDictRemove(&pDict->Dict, (ptr)sKey, iKeyLen);
}


XXAPI bool xrtTypedDictTake(xtdict pDict, const ptr sKey, uint32 iKeyLen, ptr pOut)
{
	ptr pSlot;
	if ( pDict == NULL || sKey == NULL ) {
		return FALSE;
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	pSlot = xrtDictGet(&pDict->Dict, (ptr)sKey, iKeyLen);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	if ( pOut != NULL ) {
		__xrtTypedCopyItem(pDict->ItemType, pOut, pSlot);
	}
	__xrtTypedDropItem(pDict->ItemType, pSlot);
	return xrtDictRemove(&pDict->Dict, (ptr)sKey, iKeyLen);
}


XXAPI xvalue xrtTypedDictTakeValue(xtdict pDict, const ptr sKey, uint32 iKeyLen)
{
	ptr pSlot;
	xvalue pRet;
	if ( pDict == NULL || sKey == NULL ) {
		return xvoCreateNull();
	}
	iKeyLen = __xrtTypedDictKeyLen(sKey, iKeyLen);
	pSlot = xrtDictGet(&pDict->Dict, (ptr)sKey, iKeyLen);
	if ( pSlot == NULL ) {
		return xvoCreateNull();
	}
	pRet = __xrtTypedBoxItem(pDict->ItemType, pSlot);
	__xrtTypedDropItem(pDict->ItemType, pSlot);
	xrtDictRemove(&pDict->Dict, (ptr)sKey, iKeyLen);
	return pRet;
}


XXAPI bool xrtTypedDictClear(xtdict pDict)
{
	const xrt_type_desc* pType;
	uint32 iLen;
	if ( pDict == NULL ) {
		return FALSE;
	}
	pType = pDict->ItemType;
	iLen = pDict->ItemLength;
	xrtDictWalk(&pDict->Dict, __xrtTypedDictDropProc, pDict);
	xrtDictUnit(&pDict->Dict);
	xrtDictInit(&pDict->Dict, iLen, XRT_OBJMODE_LOCAL);
	pDict->ItemType = pType;
	pDict->ItemLength = iLen;
	return TRUE;
}


XXAPI bool xrtTypedDictMerge(xtdict pTarget, xtdict pSource)
{
	__xrt_typed_dict_merge_arg state;
	xtdict pSnapshot = NULL;
	if ( pTarget == NULL || pSource == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pTarget->ItemType, pSource->ItemType) ) {
		return FALSE;
	}
	if ( pTarget == pSource ) {
		pSnapshot = xrtTypedDictClone(pSource);
		if ( pSnapshot == NULL ) {
			return FALSE;
		}
		pSource = pSnapshot;
	}
	state.Target = pTarget;
	state.Failed = FALSE;
	xrtDictWalk(&pSource->Dict, __xrtTypedDictMergeProc, &state);
	if ( pSnapshot != NULL ) {
		xrtTypedDictDestroy(pSnapshot);
	}
	return state.Failed == FALSE;
}


XXAPI xtdict xrtTypedDictCloneEx(xtdict pDict, uint32 iMode)
{
	xtdict pRet;
	if ( pDict == NULL ) {
		return NULL;
	}
	pRet = xrtTypedDictCreate(pDict->ItemType, iMode);
	if ( pRet == NULL ) {
		return NULL;
	}
	if ( !xrtTypedDictMerge(pRet, pDict) ) {
		xrtTypedDictDestroy(pRet);
		return NULL;
	}
	return pRet;
}


XXAPI xtdict xrtTypedDictMoveToShared(xtdict pDict)
{
	xtdict pResult;
	if ( pDict == NULL || xrtOwnerGetMode(&pDict->Dict.Owner) == XRT_OBJMODE_SHARED ) {
		return pDict;
	}
	pResult = xrtTypedDictCloneEx(pDict, XRT_OBJMODE_SHARED);
	if ( pResult != NULL ) {
		if ( pDict->HeapOwned ) {
			xrtTypedDictDestroy(pDict);
		} else {
			xrtTypedDictUnit(pDict);
		}
	}
	return pResult;
}


XXAPI xtdict xrtTypedDictClone(xtdict pDict)
{
	return pDict != NULL ?
		xrtTypedDictCloneEx(pDict, xrtOwnerGetMode(&pDict->Dict.Owner)) : NULL;
}


XXAPI bool xrtTypedDictEquals(xtdict pLeft, xtdict pRight)
{
	__xrt_typed_dict_equals_arg state;
	if ( pLeft == NULL || pRight == NULL ) {
		return FALSE;
	}
	if ( !__xrtTypedSameItemType(pLeft->ItemType, pRight->ItemType) ) {
		return FALSE;
	}
	if ( xrtDictCount(&pLeft->Dict) != xrtDictCount(&pRight->Dict) ) {
		return FALSE;
	}
	state.Right = pRight;
	state.ItemType = pLeft->ItemType;
	state.Equal = TRUE;
	xrtDictWalk(&pLeft->Dict, __xrtTypedDictEqualsProc, &state);
	return state.Equal;
}


XXAPI uint32 xrtTypedDictCount(xtdict pDict)
{
	return pDict != NULL ? xrtDictCount(&pDict->Dict) : 0;
}


XXAPI const xrt_type_desc* xrtTypedDictItemType(xtdict pDict)
{
	return pDict != NULL ? pDict->ItemType : NULL;
}
