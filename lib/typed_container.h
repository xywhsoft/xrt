

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


// ------------------------------------ typed array ------------------------------------

XXAPI xtarray xrtTypedArrayCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtarray pArray = (xtarray)xrtMalloc(sizeof(xrt_typed_array_struct));
	if ( pArray == NULL ) {
		return NULL;
	}
	xrtTypedArrayInit(pArray, pItemType, iMode);
	return pArray;
}


XXAPI void xrtTypedArrayDestroy(xtarray pArray)
{
	if ( pArray != NULL ) {
		xrtTypedArrayUnit(pArray);
		xrtFree(pArray);
	}
}


XXAPI void xrtTypedArrayInit(xtarray pArray, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pArray == NULL ) {
		return;
	}
	pArray->ItemType = pItemType;
	pArray->ItemLength = __xrtTypedItemLength(pItemType);
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


XXAPI xtlist xrtTypedListCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtlist pList = (xtlist)xrtMalloc(sizeof(xrt_typed_list_struct));
	if ( pList == NULL ) {
		return NULL;
	}
	xrtTypedListInit(pList, pItemType, iMode);
	return pList;
}


XXAPI void xrtTypedListDestroy(xtlist pList)
{
	if ( pList != NULL ) {
		xrtTypedListUnit(pList);
		xrtFree(pList);
	}
}


XXAPI void xrtTypedListInit(xtlist pList, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pList == NULL ) {
		return;
	}
	pList->ItemType = pItemType;
	pList->ItemLength = __xrtTypedItemLength(pItemType);
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
	return pSet;
}


XXAPI void xrtTypedSetDestroy(xtset pSet)
{
	if ( pSet != NULL ) {
		xrtTypedSetUnit(pSet);
		xrtFree(pSet);
	}
}


XXAPI void xrtTypedSetInit(xtset pSet, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pSet == NULL ) {
		return;
	}
	pSet->ItemType = pItemType;
	pSet->ItemLength = __xrtTypedItemLength(pItemType);
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


XXAPI xtdict xrtTypedDictCreate(const xrt_type_desc* pItemType, uint32 iMode)
{
	xtdict pDict = (xtdict)xrtMalloc(sizeof(xrt_typed_dict_struct));
	if ( pDict == NULL ) {
		return NULL;
	}
	xrtTypedDictInit(pDict, pItemType, iMode);
	return pDict;
}


XXAPI void xrtTypedDictDestroy(xtdict pDict)
{
	if ( pDict != NULL ) {
		xrtTypedDictUnit(pDict);
		xrtFree(pDict);
	}
}


XXAPI void xrtTypedDictInit(xtdict pDict, const xrt_type_desc* pItemType, uint32 iMode)
{
	if ( pDict == NULL ) {
		return;
	}
	pDict->ItemType = pItemType;
	pDict->ItemLength = __xrtTypedItemLength(pItemType);
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


XXAPI uint32 xrtTypedDictCount(xtdict pDict)
{
	return pDict != NULL ? xrtDictCount(&pDict->Dict) : 0;
}


XXAPI const xrt_type_desc* xrtTypedDictItemType(xtdict pDict)
{
	return pDict != NULL ? pDict->ItemType : NULL;
}
