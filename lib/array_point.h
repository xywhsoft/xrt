

// 创建指针内存管理器
XXAPI xparray xrtPtrArrayCreate()
{
	xparray ObjPtr = xrtMalloc(sizeof(xparray_struct));
	if ( ObjPtr ) {
		xrtPtrArrayInit(ObjPtr);
	}
	return ObjPtr;
}

// 销毁指针内存管理器
XXAPI void xrtPtrArrayDestroy(xparray pObject)
{
	if ( pObject ) {
		xrtPtrArrayUnit(pObject);
		xrtFree(pObject);
	}
}

// 初始化内存管理器（对自维护结构体指针使用，和 PAMM_Create 功能类似）
XXAPI void xrtPtrArrayInit(xparray pObject)
{
	pObject->Memory = NULL;
	pObject->Count = 0;
	pObject->AllocCount = 0;
	pObject->AllocStep = XPARRAY_PREASSIGNSTEP;
}

// 释放内存管理器（对自维护结构体指针使用，和 PAMM_Destroy 功能类似）
XXAPI void xrtPtrArrayUnit(xparray pObject)
{
	if ( pObject->Memory ) { xrtFree(pObject->Memory); pObject->Memory = NULL; }
	pObject->Count = 0;
	pObject->AllocCount = 0;
}

// 分配内存
XXAPI int xrtPtrArrayMalloc(xparray pObject, unsigned int iCount)
{
	if ( iCount > pObject->AllocCount ) {
		// 增量
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(void*));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			return -1;
		}
	} else if ( iCount < pObject->AllocCount ) {
		// 裁剪
		ptr* pNew = xrtRealloc(pObject->Memory, iCount * sizeof(void*));
		if ( pNew ) {
			pObject->AllocCount = iCount;
			pObject->Memory = pNew;
			if ( pObject->Count > iCount ) {
				// 需要裁剪数据
				pObject->Count = iCount;
			}
			return -1;
		}
	} else if ( iCount = 0 ) {
		// 清空
		xrtPtrArrayUnit(pObject);
		return -1;
	} else {
		// 不变
		return -1;
	}
	return 0;
}

// 中间插入成员(0为头部插入，pObject->Count为末尾插入)
XXAPI unsigned int xrtPtrArrayInsert(xparray pObject, unsigned int iPos, void* pVal)
{
	// 分配内存
	if ( pObject->Count >= pObject->AllocCount ) {
		if ( xrtPtrArrayMalloc(pObject, pObject->Count + pObject->AllocStep) == 0 ) {
			return 0;
		}
	}
	if ( iPos < pObject->Count ) {
		// 插入模式（需要移动内存）
		void** src = &(pObject->Memory[iPos]);
		memmove(src + 1, src, (pObject->Count - iPos) * sizeof(void*));
		pObject->Memory[iPos] = pVal;
		pObject->Count++;
		return iPos + 1;
	} else {
		// 追加模式
		pObject->Memory[pObject->Count] = pVal;
		pObject->Count++;
		return pObject->Count;
	}
}

// 末尾添加成员
XXAPI unsigned int xrtPtrArrayAppend(xparray pObject, void* pVal)
{
	return xrtPtrArrayInsert(pObject, pObject->Count, pVal);
}

// 添加成员，自动查找空隙（替换为 NULL 的值）
XXAPI unsigned int xrtPtrArrayAddAlt(xparray pObject, void* pVal)
{
	for ( int i = 0; i < pObject->Count; i++ ) {
		if ( pObject->Memory[i] == NULL ) {
			pObject->Memory[i] = pVal;
			return i + 1;
		}
	}
	return xrtPtrArrayInsert(pObject, pObject->Count, pVal);
}

// 交换成员
XXAPI int xrtPtrArraySwap(xparray pObject, unsigned int iPosA, unsigned int iPosB)
{
	// 范围检查
	if ( iPosA == 0 ) { return 0; }
	if ( iPosA > pObject->Count ) { return 0; }
	if ( iPosB == 0 ) { return 0; }
	if ( iPosB > pObject->Count ) { return 0; }
	if ( iPosA == iPosB ) { return -1; }
	// 交换成员
	iPosA--;
	iPosB--;
	void* pA = pObject->Memory[iPosB];
	pObject->Memory[iPosB] = pObject->Memory[iPosA];
	pObject->Memory[iPosA] = pA;
	return -1;
}

// 删除成员
XXAPI int xrtPtrArrayRemove(xparray pObject, unsigned int iPos, unsigned int iCount)
{
	// 不能添加 0 个成员、不能删除不存在的成员（0号成员也不存在）
	if ( iCount == 0 ) { return 0; }
	if ( iPos == 0 ) { return 0; }
	if ( iPos > pObject->Count ) { return 0; }
	// 删除成员（数量不足时删除后面的所有成员）
	iPos--;
	if ( iPos + iCount < pObject->Count ) {
		// 中段删除
		void** dst = &(pObject->Memory[iPos]);
		memmove(dst, dst + iCount, (pObject->Count - (iPos + iCount)) * sizeof(void*));
		pObject->Count -= iCount;
	} else {
		// 末尾删除
		pObject->Count = iPos;
	}
	return -1;
}

// 获取成员指针
XXAPI void* xrtPtrArrayGet(xparray pObject, unsigned int iPos)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			return pObject->Memory[iPos];
		}
	}
	return 0;
}
XXAPI void* xrtPtrArrayGet_Unsafe(xparray pObject, unsigned int iPos)
{
	return pObject->Memory[iPos - 1];
}

// 设置成员指针
XXAPI int xrtPtrArraySet(xparray pObject, unsigned int iPos, void* pVal)
{
	if ( iPos ) {
		iPos--;
		if ( iPos < pObject->Count ) {
			pObject->Memory[iPos] = pVal;
			return -1;
		}
	}
	return 0;
}
XXAPI void xrtPtrArraySet_Unsafe(xparray pObject, unsigned int iPos, void* pVal)
{
	pObject->Memory[iPos - 1] = pVal;
}

// 成员排序
XXAPI int xrtPtrArraySort(xparray pObject, void* procCompar)
{
	if ( pObject ) {
		qsort(pObject->Memory, pObject->Count, sizeof(void*), procCompar);
		return -1;
	} else {
		return 0;
	}
}


