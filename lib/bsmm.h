


// 内部函数：__xrtBsmmPageMMUUnit
static inline void __xrtBsmmPageMMUUnit(xbsmm objBSMM)
{
	if ( objBSMM->PageMMU.Memory ) {
		xrtFree(objBSMM->PageMMU.Memory);
		objBSMM->PageMMU.Memory = NULL;
	}
	objBSMM->PageMMU.Count = 0;
	objBSMM->PageMMU.AllocCount = 0;
}


// 内部函数：__xrtBsmmPageMMUAppend
static inline uint32 __xrtBsmmPageMMUAppend(xbsmm objBSMM, ptr pBlock)
{
	if ( objBSMM->PageMMU.Count >= objBSMM->PageMMU.AllocCount ) {
		uint32 iNewCount = objBSMM->PageMMU.Count + objBSMM->PageMMU.AllocStep;
		if ( iNewCount < objBSMM->PageMMU.Count || (size_t)iNewCount > (SIZE_MAX / sizeof(ptr)) ) {
			return 0;
		}
		ptr* pNew = xrtRealloc(objBSMM->PageMMU.Memory, (size_t)iNewCount * sizeof(ptr));
		if ( pNew == NULL ) {
			return 0;
		}
		objBSMM->PageMMU.Memory = pNew;
		objBSMM->PageMMU.AllocCount = iNewCount;
	}
	objBSMM->PageMMU.Memory[objBSMM->PageMMU.Count] = pBlock;
	objBSMM->PageMMU.Count++;
	return objBSMM->PageMMU.Count;
}


// 内部函数：检查指针是否属于 BSMM
static inline bool __xrtBsmmOwnsPtr(xbsmm objBSMM, ptr p)
{
	size_t iBlockSize;
	if ( objBSMM == NULL || p == NULL || objBSMM->ItemLength == 0 ) {
		return FALSE;
	}
	if ( objBSMM->ItemLength > (uint32)(SIZE_MAX / 256u) ) {
		return FALSE;
	}
	iBlockSize = (size_t)objBSMM->ItemLength * 256u;
	for ( uint32 i = 0; i < objBSMM->PageMMU.Count; i++ ) {
		uint8* pBlock = (uint8*)objBSMM->PageMMU.Memory[i];
		uint8* pValue = (uint8*)p;
		if ( pBlock == NULL ) {
			continue;
		}
		if ( pValue >= pBlock && (size_t)(pValue - pBlock) < iBlockSize ) {
			return (((size_t)(pValue - pBlock)) % objBSMM->ItemLength) == 0;
		}
	}
	return FALSE;
}

// 创建数据块结构内存管理器
XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength, uint32 iMode)
{
	xbsmm objBSMM = xrtMalloc(sizeof(xbsmm_struct));
	if ( objBSMM ) {
		xrtBsmmInit(objBSMM, iItemLength, iMode);
	}
	return objBSMM;
}

// 销毁数据块结构内存管理器
XXAPI void xrtBsmmDestroy(xbsmm objBSMM)
{
	if ( objBSMM ) {
		if ( !xrtOwnerCheckMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
			return;
		}
		xrtBsmmUnit(objBSMM);
		xrtFree(objBSMM);
	}
}

// 初始化数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Create 功能类似）
XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength, uint32 iMode)
{
	xrtOwnerInitMode(&objBSMM->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objBSMM->Owner);
	}
	objBSMM->ItemLength = iItemLength < sizeof(MemPtr_LLNode) ? (uint32)sizeof(MemPtr_LLNode) : iItemLength;
	objBSMM->Count = 0;
	xrtPtrArrayInit(&objBSMM->PageMMU, iMode);
	objBSMM->LL_Free = NULL;
}

// 释放数据块结构内存管理器（对自维护结构体指针使用，和 BSMM_Destroy 功能类似）
XXAPI void xrtBsmmUnit(xbsmm objBSMM)
{
	if ( !xrtOwnerBeginMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
		return;
	}
	objBSMM->Count = 0;
	// 循环释放 PageMMU 中的内存页
	for ( uint32 i = 0; i < objBSMM->PageMMU.Count; i++ ) {
		xrtFree(objBSMM->PageMMU.Memory[i]);
		objBSMM->PageMMU.Memory[i] = NULL;
	}
	__xrtBsmmPageMMUUnit(objBSMM);
	objBSMM->LL_Free = NULL;
	xrtOwnerEndMutable(&objBSMM->Owner);
}

// 申请结构体内存
XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM)
{
	ptr pResult = NULL;
	if ( objBSMM == NULL ) {
		return NULL;
	}
	if ( !xrtOwnerBeginMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
		return NULL;
	}
	if ( objBSMM->LL_Free ) {
		// 有空闲内存块先用空闲的
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		objBSMM->LL_Free = pNode->Next;
		pResult = pNode;
	} else {
		// 需要申请新的内存块
		if ( (uint64)objBSMM->Count >= ((uint64)objBSMM->PageMMU.Count << 8) ) {
			ptr pBlock;
			if ( objBSMM->ItemLength > (uint32)(SIZE_MAX / 256u) ) {
				xrtOwnerEndMutable(&objBSMM->Owner);
				return NULL;
			}
			pBlock = xrtMalloc((size_t)objBSMM->ItemLength * 256u);
			if ( pBlock == NULL ) {
				xrtOwnerEndMutable(&objBSMM->Owner);
				return NULL;
			}
			uint iIdx = __xrtBsmmPageMMUAppend(objBSMM, pBlock);
			if ( iIdx == 0 ) {
				xrtFree(pBlock);
				xrtOwnerEndMutable(&objBSMM->Owner);
				return NULL;
			}
		}
		// 从内存块中分配值
		uint32 iBlock = objBSMM->Count >> 8;
		uint32 iPos = objBSMM->Count & 0xFF;
		str pBlock = xrtPtrArrayGet_Inline(&objBSMM->PageMMU, iBlock + 1);
		objBSMM->Count++;
		pResult = &pBlock[iPos * objBSMM->ItemLength];
	}
	xrtOwnerEndMutable(&objBSMM->Owner);
	return pResult;
}

// 释放结构体内存
XXAPI void xrtBsmmFree(xbsmm objBSMM, ptr p)
{
	if ( objBSMM == NULL || p == NULL ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
		return;
	}
	if ( __xrtBsmmOwnsPtr(objBSMM, p) == FALSE ) {
		xrtSetError("BSMM free failed: invalid pointer.", FALSE);
		xrtOwnerEndMutable(&objBSMM->Owner);
		return;
	}
	MemPtr_LLNode* pNode = (MemPtr_LLNode*)p;
	pNode->Ptr = p;
	pNode->Next = objBSMM->LL_Free;
	objBSMM->LL_Free = pNode;
	xrtOwnerEndMutable(&objBSMM->Owner);
}


