

static inline void __xrtPtrArrayUnit_NoLock(xparray pObject)
{
	if ( pObject->Memory ) {
		xrtFree(pObject->Memory);
		pObject->Memory = NULL;
	}
	pObject->Count = 0;
	pObject->AllocCount = 0;
}

static inline bool __xrtPtrArrayMalloc_NoLock(xparray pObject, uint32 iCount)
{
	if ( iCount > pObject->AllocCount ) {
		// 增量
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(ptr));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			return TRUE;
		}
	} else if ( iCount < pObject->AllocCount ) {
		// 裁剪
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(ptr));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			if ( pObject->Count > iCount ) {
				// 需要裁剪数据
				pObject->Count = iCount;
			}
			return TRUE;
		}
	} else if ( iCount == 0 ) {
		// 清空
		__xrtPtrArrayUnit_NoLock(pObject);
		return TRUE;
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 创建指针内存管理器
XXAPI xparray xrtPtrArrayCreate(uint32 iMode)
{
	xparray ObjPtr = xrtMalloc(sizeof(xparray_struct));
	if ( ObjPtr ) {
		xrtPtrArrayInit(ObjPtr, iMode);
	}
	return ObjPtr;
}

// 销毁指针内存管理器
XXAPI void xrtPtrArrayDestroy(xparray pObject)
{
	if ( pObject ) {
		if ( !xrtOwnerCheckMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
			return;
		}
		xrtPtrArrayUnit(pObject);
		xrtFree(pObject);
	}
}

// 初始化内存管理器（对自维护结构体指针使用）
XXAPI void xrtPtrArrayInit(xparray pObject, uint32 iMode)
{
	pObject->Memory = NULL;
	pObject->Count = 0;
	pObject->AllocCount = 0;
	pObject->AllocStep = XPARRAY_PREASSIGNSTEP;
	xrtOwnerInitMode(&pObject->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&pObject->Owner);
	}
}

// 释放内存管理器（对自维护结构体指针使用）
XXAPI void xrtPtrArrayUnit(xparray pObject)
{
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return;
	}
	__xrtPtrArrayUnit_NoLock(pObject);
	xrtOwnerEndMutable(&pObject->Owner);
}

XXAPI bool xrtPtrArrayLock(xparray pObject)
{
	if ( pObject == NULL ) {
		return FALSE;
	}
	return xrtOwnerLock(&pObject->Owner, "pointer array belongs to another thread.");
}

XXAPI void xrtPtrArrayUnlock(xparray pObject)
{
	if ( pObject == NULL ) {
		return;
	}
	xrtOwnerUnlock(&pObject->Owner);
}

// 分配内存
XXAPI bool xrtPtrArrayMalloc(xparray pObject, uint32 iCount)
{
	bool bRet;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return FALSE;
	}
	bRet = __xrtPtrArrayMalloc_NoLock(pObject, iCount);
	xrtOwnerEndMutable(&pObject->Owner);
	return bRet;
}

// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
XXAPI uint32 xrtPtrArrayInsert(xparray pObject, uint32 iPos, ptr pVal)
{
	uint32 iRet = 0;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return 0;
	}
	// 分配内存
	if ( pObject->Count >= pObject->AllocCount ) {
		if ( __xrtPtrArrayMalloc_NoLock(pObject, pObject->Count + pObject->AllocStep) == 0 ) {
			xrtOwnerEndMutable(&pObject->Owner);
			return 0;
		}
	}
	if ( iPos < pObject->Count ) {
		// 插入模式（需要移动内存）
		ptr* src = &(pObject->Memory[iPos]);
		memmove(src + 1, src, (pObject->Count - iPos) * sizeof(ptr));
		pObject->Memory[iPos] = pVal;
		pObject->Count++;
		iRet = iPos + 1;
	} else {
		// 追加模式
		pObject->Memory[pObject->Count] = pVal;
		pObject->Count++;
		iRet = pObject->Count;
	}
	xrtOwnerEndMutable(&pObject->Owner);
	return iRet;
}

// 末尾添加成员
XXAPI uint32 xrtPtrArrayAppend(xparray pObject, ptr pVal)
{
	return xrtPtrArrayInsert(pObject, pObject->Count, pVal);
}

// 添加成员，自动查找空隙（替换为 NULL 的值）
XXAPI uint32 xrtPtrArrayAddAlt(xparray pObject, ptr pVal)
{
	uint32 iRet = 0;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return 0;
	}
	for ( uint32 i = 0; i < pObject->Count; i++ ) {
		if ( pObject->Memory[i] == NULL ) {
			pObject->Memory[i] = pVal;
			iRet = i + 1;
			xrtOwnerEndMutable(&pObject->Owner);
			return iRet;
		}
	}
	if ( pObject->Count >= pObject->AllocCount ) {
		if ( __xrtPtrArrayMalloc_NoLock(pObject, pObject->Count + pObject->AllocStep) == 0 ) {
			xrtOwnerEndMutable(&pObject->Owner);
			return 0;
		}
	}
	pObject->Memory[pObject->Count] = pVal;
	pObject->Count++;
	iRet = pObject->Count;
	xrtOwnerEndMutable(&pObject->Owner);
	return iRet;
}

// 交换成员
XXAPI bool xrtPtrArraySwap(xparray pObject, uint32 iPosA, uint32 iPosB)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return FALSE;
	}
	// 范围检查
	if ( iPosA == 0 || iPosA > pObject->Count || iPosB == 0 || iPosB > pObject->Count ) {
		xrtOwnerEndMutable(&pObject->Owner);
		return FALSE;
	}
	if ( iPosA == iPosB ) {
		xrtOwnerEndMutable(&pObject->Owner);
		return TRUE;
	}
	// 交换成员
	iPosA--;
	iPosB--;
	ptr pA = pObject->Memory[iPosB];
	pObject->Memory[iPosB] = pObject->Memory[iPosA];
	pObject->Memory[iPosA] = pA;
	bRet = TRUE;
	xrtOwnerEndMutable(&pObject->Owner);
	return bRet;
}

// 删除成员
XXAPI bool xrtPtrArrayRemove(xparray pObject, uint32 iPos, uint32 iCount)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return FALSE;
	}
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 || iPos == 0 || iPos > pObject->Count ) {
		xrtOwnerEndMutable(&pObject->Owner);
		return FALSE;
	}
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pObject->Count ) {
		// 中段删除
		ptr* dst = &(pObject->Memory[iPos]);
		memmove(dst, dst + iCount, (pObject->Count - (iPos + iCount)) * sizeof(ptr));
		pObject->Count -= iCount;
	} else {
		// 末尾删除
		pObject->Count = iPos;
	}
	bRet = TRUE;
	xrtOwnerEndMutable(&pObject->Owner);
	return bRet;
}

// 获取成员指针
XXAPI ptr xrtPtrArrayGet(xparray pObject, uint32 iPos)
{
	ptr pRet = NULL;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return NULL;
	}
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			pRet = pObject->Memory[iPos];
		}
	}
	xrtOwnerEndMutable(&pObject->Owner);
	return pRet;
}
XXAPI ptr xrtPtrArrayGet_Unsafe(xparray pObject, uint32 iPos)
{
	return pObject->Memory[iPos - 1];
}

// 设置成员指针
XXAPI bool xrtPtrArraySet(xparray pObject, uint32 iPos, ptr pVal)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return FALSE;
	}
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			pObject->Memory[iPos] = pVal;
			bRet = TRUE;
		}
	}
	xrtOwnerEndMutable(&pObject->Owner);
	return bRet;
}
XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, uint32 iPos, ptr pVal)
{
	if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
		return;
	}
	pObject->Memory[iPos - 1] = pVal;
	xrtOwnerEndMutable(&pObject->Owner);
}

// 成员排序
XXAPI bool xrtPtrArraySort(xparray pObject, ptr procCompar)
{
	if ( pObject ) {
		if ( procCompar == NULL ) {
			return FALSE;
		}
		if ( !xrtOwnerBeginMutable(&pObject->Owner, "pointer array belongs to another thread.") ) {
			return FALSE;
		}
		qsort(pObject->Memory, pObject->Count, sizeof(ptr), procCompar);
		xrtOwnerEndMutable(&pObject->Owner);
		return TRUE;
	} else {
		return FALSE;
	}
}


