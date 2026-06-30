


#define XRT_SET_SLOT_EMPTY		0u
#define XRT_SET_SLOT_USED		1u
#define XRT_SET_SLOT_DELETED	2u
#define XRT_SET_MIN_CAPACITY	16u


// 计算 set 元素地址
static inline ptr __xrtSetItemPtr(xset pSet, uint32 iIndex)
{
	return pSet->Data + ((size_t)iIndex * (size_t)pSet->ItemLength);
}


// 将容量调整为 2 的幂，便于开放寻址取模
static uint32 __xrtSetNormalizeCapacity(uint32 iNeed)
{
	uint32 iCap = XRT_SET_MIN_CAPACITY;
	while ( iCap < iNeed && iCap < 0x80000000u ) {
		iCap <<= 1;
	}
	return iCap;
}


// 计算元素哈希；没有 type ops 时按字节计算
static uint64 __xrtSetHash(xset pSet, const ptr pItem)
{
	if ( pSet->Type && pSet->Type->Ops && pSet->Type->Ops->hash ) {
		return pSet->Type->Ops->hash(pItem);
	}
	if ( pSet->ItemLength == 0 ) {
		return 0;
	}
	return xrtHash64(pItem, pSet->ItemLength);
}


// 比较两个元素；没有 type ops 时按字节比较
static int __xrtSetCompare(xset pSet, const ptr pA, const ptr pB)
{
	if ( pSet->Type && pSet->Type->Ops && pSet->Type->Ops->compare ) {
		return pSet->Type->Ops->compare(pA, pB);
	}
	if ( pSet->ItemLength == 0 ) {
		return 0;
	}
	return memcmp(pA, pB, pSet->ItemLength);
}


// 写入元素；没有 type ops 时直接复制内存
static void __xrtSetCopyItem(xset pSet, ptr pDst, const ptr pSrc)
{
	if ( pSet->Type && pSet->Type->Ops && pSet->Type->Ops->copy ) {
		pSet->Type->Ops->copy(pDst, pSrc);
	} else if ( pSet->ItemLength > 0 ) {
		memcpy(pDst, pSrc, pSet->ItemLength);
	}
}


// 释放元素；没有 type ops 时无需处理
static void __xrtSetDropItem(xset pSet, ptr pItem)
{
	if ( pSet->Type && pSet->Type->Ops && pSet->Type->Ops->drop ) {
		pSet->Type->Ops->drop(pItem);
	}
}


// 查找槽位；返回可写入槽位，bFound 表示是否命中已有元素
static uint32 __xrtSetFindSlot(xset pSet, const ptr pItem, uint64 iHash, bool* bFound)
{
	uint32 iMask = pSet->Capacity - 1u;
	uint32 iIndex = (uint32)iHash & iMask;
	uint32 iFirstDeleted = 0xFFFFFFFFu;

	while ( TRUE ) {
		uint8 iState = pSet->State[iIndex];
		if ( iState == XRT_SET_SLOT_EMPTY ) {
			if ( bFound ) {
				*bFound = FALSE;
			}
			return (iFirstDeleted != 0xFFFFFFFFu) ? iFirstDeleted : iIndex;
		}
		if ( iState == XRT_SET_SLOT_DELETED ) {
			if ( iFirstDeleted == 0xFFFFFFFFu ) {
				iFirstDeleted = iIndex;
			}
		} else if ( pSet->Hash[iIndex] == iHash ) {
			ptr pExist = __xrtSetItemPtr(pSet, iIndex);
			if ( __xrtSetCompare(pSet, pExist, pItem) == 0 ) {
				if ( bFound ) {
					*bFound = TRUE;
				}
				return iIndex;
			}
		}
		iIndex = (iIndex + 1u) & iMask;
	}
}


// 释放 set 内部数组，不释放元素生命周期
static void __xrtSetFreeBuffers(xset pSet)
{
	if ( pSet->State ) {
		xrtFree(pSet->State);
		pSet->State = NULL;
	}
	if ( pSet->Hash ) {
		xrtFree(pSet->Hash);
		pSet->Hash = NULL;
	}
	if ( pSet->Data ) {
		xrtFree(pSet->Data);
		pSet->Data = NULL;
	}
	pSet->Capacity = 0;
}


// 分配 set 内部数组
static bool __xrtSetAllocBuffers(xset pSet, uint32 iCapacity)
{
	size_t iDataSize;
	iCapacity = __xrtSetNormalizeCapacity(iCapacity);
	iDataSize = (size_t)iCapacity * (size_t)pSet->ItemLength;
	pSet->State = xrtMalloc(iCapacity * sizeof(uint8));
	pSet->Hash = xrtMalloc(iCapacity * sizeof(uint64));
	pSet->Data = (iDataSize > 0) ? xrtMalloc(iDataSize) : NULL;
	if ( (pSet->State == NULL) || (pSet->Hash == NULL) || ((iDataSize > 0) && (pSet->Data == NULL)) ) {
		__xrtSetFreeBuffers(pSet);
		return FALSE;
	}
	memset(pSet->State, 0, iCapacity * sizeof(uint8));
	pSet->Capacity = iCapacity;
	return TRUE;
}


// 重新分配并搬迁元素
static bool __xrtSetRehash(xset pSet, uint32 iNewCapacity)
{
	xset_struct tOld = *pSet;
	uint32 i;

	pSet->State = NULL;
	pSet->Hash = NULL;
	pSet->Data = NULL;
	pSet->Capacity = 0;
	pSet->Count = 0;
	if ( !__xrtSetAllocBuffers(pSet, iNewCapacity) ) {
		pSet->State = tOld.State;
		pSet->Hash = tOld.Hash;
		pSet->Data = tOld.Data;
		pSet->Capacity = tOld.Capacity;
		pSet->Count = tOld.Count;
		return FALSE;
	}

	for ( i = 0; i < tOld.Capacity; i++ ) {
		if ( tOld.State[i] == XRT_SET_SLOT_USED ) {
			bool bFound = FALSE;
			ptr pOldItem = tOld.Data + ((size_t)i * (size_t)tOld.ItemLength);
			uint32 iSlot = __xrtSetFindSlot(pSet, pOldItem, tOld.Hash[i], &bFound);
			pSet->State[iSlot] = XRT_SET_SLOT_USED;
			pSet->Hash[iSlot] = tOld.Hash[i];
			__xrtSetCopyItem(pSet, __xrtSetItemPtr(pSet, iSlot), pOldItem);
			pSet->Count++;
			__xrtSetDropItem(&tOld, pOldItem);
		}
	}

	if ( tOld.State ) { xrtFree(tOld.State); }
	if ( tOld.Hash ) { xrtFree(tOld.Hash); }
	if ( tOld.Data ) { xrtFree(tOld.Data); }
	return TRUE;
}


// 创建 set
XXAPI xset xrtSetCreate(const xrt_type_desc* pType, uint32 iMode)
{
	xset pSet = xrtMalloc(sizeof(xset_struct));
	if ( pSet == NULL ) {
		return NULL;
	}
	xrtSetInit(pSet, pType, iMode);
	return pSet;
}


// 销毁 set
XXAPI void xrtSetDestroy(xset pSet)
{
	if ( pSet ) {
		if ( !xrtOwnerCheckMutable(&pSet->Owner, "set belongs to another thread.") ) {
			return;
		}
		(xrtSetUnit)(pSet);
		xrtFree(pSet);
	}
}


// 初始化 set
XXAPI void xrtSetInit(xset pSet, const xrt_type_desc* pType, uint32 iMode)
{
	memset(pSet, 0, sizeof(*pSet));
	xrtOwnerInitMode(&pSet->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&pSet->Owner);
	}
	pSet->Type = pType;
	pSet->ItemLength = (pType && pType->Size > 0) ? (uint32)pType->Size : sizeof(ptr);
	__xrtSetAllocBuffers(pSet, XRT_SET_MIN_CAPACITY);
}


// 释放 set
XXAPI void xrtSetUnit(xset pSet)
{
	uint32 i;
	if ( pSet == NULL ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return;
	}
	for ( i = 0; i < pSet->Capacity; i++ ) {
		if ( pSet->State[i] == XRT_SET_SLOT_USED ) {
			__xrtSetDropItem(pSet, __xrtSetItemPtr(pSet, i));
		}
	}
	__xrtSetFreeBuffers(pSet);
	pSet->Count = 0;
	xrtOwnerEndMutable(&pSet->Owner);
}


// 添加元素
XXAPI bool xrtSetAdd(xset pSet, const ptr pItem)
{
	uint64 iHash;
	uint32 iSlot;
	bool bFound = FALSE;

	if ( (pSet == NULL) || (pItem == NULL) ) {
		return FALSE;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return FALSE;
	}
	if ( ((uint64)pSet->Count + 1u) * 100u > ((uint64)pSet->Capacity * 70u) ) {
		if ( !__xrtSetRehash(pSet, pSet->Capacity ? pSet->Capacity * 2u : XRT_SET_MIN_CAPACITY) ) {
			xrtOwnerEndMutable(&pSet->Owner);
			return FALSE;
		}
	}
	iHash = __xrtSetHash(pSet, pItem);
	iSlot = __xrtSetFindSlot(pSet, pItem, iHash, &bFound);
	if ( !bFound ) {
		pSet->State[iSlot] = XRT_SET_SLOT_USED;
		pSet->Hash[iSlot] = iHash;
		__xrtSetCopyItem(pSet, __xrtSetItemPtr(pSet, iSlot), pItem);
		pSet->Count++;
	}
	xrtOwnerEndMutable(&pSet->Owner);
	return TRUE;
}


// 获取元素指针
XXAPI ptr xrtSetGet(xset pSet, const ptr pItem)
{
	uint64 iHash;
	uint32 iSlot;
	bool bFound = FALSE;

	if ( (pSet == NULL) || (pItem == NULL) || (pSet->Capacity == 0) ) {
		return NULL;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return NULL;
	}
	iHash = __xrtSetHash(pSet, pItem);
	iSlot = __xrtSetFindSlot(pSet, pItem, iHash, &bFound);
	xrtOwnerEndMutable(&pSet->Owner);
	return bFound ? __xrtSetItemPtr(pSet, iSlot) : NULL;
}


// 判断元素是否存在
XXAPI bool xrtSetExists(xset pSet, const ptr pItem)
{
	return xrtSetGet(pSet, pItem) != NULL;
}


// 按逻辑序号获取元素指针
XXAPI ptr xrtSetItemAt(xset pSet, uint32 iIndex)
{
	uint32 i;
	uint32 iSeen = 0;
	ptr pRet = NULL;

	if ( pSet == NULL ) {
		return NULL;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return NULL;
	}
	for ( i = 0; i < pSet->Capacity; i++ ) {
		if ( pSet->State[i] != XRT_SET_SLOT_USED ) {
			continue;
		}
		if ( iSeen == iIndex ) {
			pRet = __xrtSetItemPtr(pSet, i);
			break;
		}
		iSeen++;
	}
	xrtOwnerEndMutable(&pSet->Owner);
	return pRet;
}


// 删除元素
XXAPI bool xrtSetRemove(xset pSet, const ptr pItem)
{
	uint64 iHash;
	uint32 iSlot;
	bool bFound = FALSE;

	if ( (pSet == NULL) || (pItem == NULL) || (pSet->Capacity == 0) ) {
		return FALSE;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return FALSE;
	}
	iHash = __xrtSetHash(pSet, pItem);
	iSlot = __xrtSetFindSlot(pSet, pItem, iHash, &bFound);
	if ( bFound ) {
		__xrtSetDropItem(pSet, __xrtSetItemPtr(pSet, iSlot));
		pSet->State[iSlot] = XRT_SET_SLOT_DELETED;
		pSet->Count--;
	}
	xrtOwnerEndMutable(&pSet->Owner);
	return bFound;
}


// 获取元素数量
XXAPI uint32 xrtSetCount(xset pSet)
{
	if ( pSet == NULL ) {
		return 0;
	}
	return pSet->Count;
}


// 遍历 set
XXAPI void xrtSetWalk(xset pSet, xset_each_proc procEach, ptr pArg)
{
	uint32 i;
	if ( (pSet == NULL) || (procEach == NULL) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&pSet->Owner, "set belongs to another thread.") ) {
		return;
	}
	for ( i = 0; i < pSet->Capacity; i++ ) {
		if ( pSet->State[i] == XRT_SET_SLOT_USED ) {
			if ( procEach(__xrtSetItemPtr(pSet, i), pArg) ) {
				break;
			}
		}
	}
	xrtOwnerEndMutable(&pSet->Owner);
}

#undef XRT_SET_SLOT_EMPTY
#undef XRT_SET_SLOT_USED
#undef XRT_SET_SLOT_DELETED
#undef XRT_SET_MIN_CAPACITY
