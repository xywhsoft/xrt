


// 内部函数：__xrtArrayUnit_NoLock
static inline void __xrtArrayUnit_NoLock(xarray pArr)
{
	if ( pArr->Memory ) {
		xrtFree(pArr->Memory);
		pArr->Memory = NULL;
	}
	pArr->Count = 0;
	pArr->AllocCount = 0;
}


// 内部函数：__xrtArrayAlloc_NoLock
static inline bool __xrtArrayAlloc_NoLock(xarray pArr, uint32 iCount)
{
	if ( (pArr->ItemLength != 0) && ((size_t)iCount > (SIZE_MAX / (size_t)pArr->ItemLength)) ) {
		return FALSE;
	}
	if ( iCount > pArr->AllocCount ) {
		// 增量
		ptr pNew = xrtRealloc(pArr->Memory, (size_t)iCount * (size_t)pArr->ItemLength);
		if ( pNew ) {
			pArr->AllocCount = iCount;
			pArr->Memory = pNew;
			return TRUE;
		}
	} else if ( iCount < pArr->AllocCount ) {
		// 裁剪
		ptr pNew = xrtRealloc(pArr->Memory, (size_t)iCount * (size_t)pArr->ItemLength);
		if ( pNew ) {
			pArr->AllocCount = iCount;
			pArr->Memory = pNew;
			if ( pArr->Count > iCount ) {
				// 需要裁剪数据
				pArr->Count = iCount;
			}
			return TRUE;
		}
	} else if ( iCount == 0 ) {
		// 清空
		__xrtArrayUnit_NoLock(pArr);
		return TRUE;
	} else {
		// 不变
		return TRUE;
	}
	return FALSE;
}

// 创建数组
XXAPI xarray xrtArrayCreate(uint32 iItemLength, uint32 iMode)
{
	xarray pArr = xrtMalloc(sizeof(xarray_struct));
	if ( pArr == NULL ) {
		return NULL;
	}
	xrtArrayInit(pArr, iItemLength, iMode);
	return pArr;
}

// 销毁数组
XXAPI void xrtArrayDestroy(xarray pArr)
{
	if ( pArr ) {
		if ( !xrtOwnerCheckMutable(&pArr->Owner, "array belongs to another thread.") ) {
			return;
		}
		xrtArrayUnit(pArr);
		xrtFree(pArr);
	}
}

// 初始化数组的数据结构 ( 用于内嵌数组的对象使用 )
XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength, uint32 iMode)
{
	pArr->Memory = NULL;
	pArr->ItemLength = iItemLength;
	pArr->Count = 0;
	pArr->AllocCount = 0;
	pArr->AllocStep = XARRAY_PREASSIGNSTEP;
	xrtOwnerInitMode(&pArr->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&pArr->Owner);
	}
}

// 释放数组的数据结构 ( 但不会释放数组结构体本身的内存，用于内嵌数组的对象使用 )
XXAPI void xrtArrayUnit(xarray pArr)
{
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return;
	}
	__xrtArrayUnit_NoLock(pArr);
	xrtOwnerEndMutable(&pArr->Owner);
}

#ifdef XRT_MEM_DEBUG
// 创建数组调试
XXAPI xarray xrtArrayCreateDbg(uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xarray pArr = xrtArrayCreate(iItemLength, iMode);
	__xrtMemDebugRegisterObject(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return pArr;
}


// 初始化数组调试
XXAPI void xrtArrayInitDbg(xarray pArr, uint32 iItemLength, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtArrayInit(pArr, iItemLength, iMode);
	__xrtMemDebugRegisterObject(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁数组调试
XXAPI void xrtArrayDestroyDbg(xarray pArr, const char* sFile, uint32 iLine)
{
	if ( pArr ) {
		if ( !__xrtMemDebugObjectGuardDestroy(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&pArr->Owner, "array belongs to another thread.") ) {
			return;
		}
		xrtArrayUnit(pArr);
		__xrtMemDebugUnregisterObject(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, sFile, iLine);
		xrtFreeDbg(pArr, sFile, iLine);
	}
}


// 释放数组调试
XXAPI void xrtArrayUnitDbg(xarray pArr, const char* sFile, uint32 iLine)
{
	if ( pArr == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return;
	}
	__xrtArrayUnit_NoLock(pArr);
	xrtOwnerEndMutable(&pArr->Owner);
	__xrtMemDebugUnregisterObject(pArr, XRT_MEMDEBUG_OBJECT_ARRAY, sFile, iLine);
}
#endif

// 锁定数组
XXAPI bool xrtArrayLock(xarray pArr)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	return xrtOwnerLock(&pArr->Owner, "array belongs to another thread.");
}


// 解锁数组
XXAPI void xrtArrayUnlock(xarray pArr)
{
	if ( pArr == NULL ) {
		return;
	}
	xrtOwnerUnlock(&pArr->Owner);
}

// 分配内存
XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount)
{
	bool bRet;
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return FALSE;
	}
	bRet = __xrtArrayAlloc_NoLock(pArr, iCount);
	xrtOwnerEndMutable(&pArr->Owner);
	return bRet;
}

// 中间插入成员
XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount)
{
	uint32 iRet = 0;
	uint64 iNeedCount;
	uint64 iAllocCount;
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return 0;
	}
	// 不能添加 0 个成员
	if ( iCount == 0 ) { iCount = 1; }
	iNeedCount = (uint64)pArr->Count + (uint64)iCount;
	iAllocCount = iNeedCount + (uint64)pArr->AllocStep;
	if ( iNeedCount > UINT32_MAX || iAllocCount > UINT32_MAX ) {
		xrtOwnerEndMutable(&pArr->Owner);
		return 0;
	}
	// 分配内存
	if ( iNeedCount > pArr->AllocCount ) {
		if ( __xrtArrayAlloc_NoLock(pArr, (uint32)iAllocCount) == FALSE ) {
			xrtOwnerEndMutable(&pArr->Owner);
			return 0;
		}
	}
	if ( iPos < pArr->Count ) {
		// 插入模式（需要移动内存）
		ptr dst = pArr->Memory + ((iPos + iCount) * pArr->ItemLength);
		ptr src = pArr->Memory + (iPos * pArr->ItemLength);
		memmove(dst, src, (pArr->Count - iPos) * pArr->ItemLength);
		pArr->Count += iCount;
		iRet = iPos + 1;
	} else {
		// 追加模式
		pArr->Count += iCount;
		iRet = pArr->Count - iCount + 1;
	}
	xrtOwnerEndMutable(&pArr->Owner);
	return iRet;
}

// 末尾添加成员
XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount)
{
	return xrtArrayInsert(pArr, pArr->Count, iCount);
}

// 交换成员
XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return FALSE;
	}
	// 范围检查
	if ( iPosA == 0 || iPosA > pArr->Count || iPosB == 0 || iPosB > pArr->Count ) {
		xrtOwnerEndMutable(&pArr->Owner);
		return FALSE;
	}
	if ( iPosA == iPosB ) {
		xrtOwnerEndMutable(&pArr->Owner);
		return TRUE;
	}
	// 交换成员
	iPosA--;
	iPosB--;
	ptr pTemp = xrtMalloc(pArr->ItemLength);
	if ( pTemp == NULL ) {
		xrtOwnerEndMutable(&pArr->Owner);
		return FALSE;
	}
	memmove(pTemp, pArr->Memory + (iPosA * pArr->ItemLength), pArr->ItemLength);
	memmove(pArr->Memory + (iPosA * pArr->ItemLength), pArr->Memory + (iPosB * pArr->ItemLength), pArr->ItemLength);
	memmove(pArr->Memory + (iPosB * pArr->ItemLength), pTemp, pArr->ItemLength);
	xrtFree(pTemp);
	bRet = TRUE;
	xrtOwnerEndMutable(&pArr->Owner);
	return bRet;
}

// 删除成员
XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount)
{
	bool bRet = FALSE;
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return FALSE;
	}
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 || iPos == 0 || iPos > pArr->Count ) {
		xrtOwnerEndMutable(&pArr->Owner);
		return FALSE;
	}
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pArr->Count ) {
		// 中段删除
		ptr dst = pArr->Memory + (iPos * pArr->ItemLength);
		ptr src = pArr->Memory + ((iPos + iCount) * pArr->ItemLength);
		memmove(dst, src, (pArr->Count - (iPos + iCount)) * pArr->ItemLength);
		pArr->Count -= iCount;
	} else {
		// 末尾删除
		pArr->Count = iPos;
	}
	bRet = TRUE;
	xrtOwnerEndMutable(&pArr->Owner);
	return bRet;
}

// 获取成员指针
XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos)
{
	ptr pRet = NULL;
	if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
		return NULL;
	}
	if ( iPos ) {
		iPos--;
		if ( iPos < pArr->Count ) {
			pRet = &pArr->Memory[iPos * pArr->ItemLength];
		}
	}
	xrtOwnerEndMutable(&pArr->Owner);
	return pRet;
}


// xrtArrayGet_Unsafe 相关处理
XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos)
{
	if ( pArr == NULL || iPos == 0 ) { return NULL; }
	return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
}

// 成员排序
XXAPI bool xrtArraySort(xarray pArr, ptr procCompar)
{
	if ( pArr ) {
		if ( procCompar == NULL ) {
			return FALSE;
		}
		if ( !xrtOwnerBeginMutable(&pArr->Owner, "array belongs to another thread.") ) {
			return FALSE;
		}
		qsort(pArr->Memory, pArr->Count, pArr->ItemLength, procCompar);
		xrtOwnerEndMutable(&pArr->Owner);
		return TRUE;
	} else {
		return FALSE;
	}
}


