


static inline void __xrtBsmmPageMMUUnit(xbsmm objBSMM)
{
	if ( objBSMM->PageMMU.Memory ) {
		xrtFree(objBSMM->PageMMU.Memory);
		objBSMM->PageMMU.Memory = NULL;
	}
	objBSMM->PageMMU.Count = 0;
	objBSMM->PageMMU.AllocCount = 0;
}

static inline uint32 __xrtBsmmPageMMUAppend(xbsmm objBSMM, ptr pBlock)
{
	if ( objBSMM->PageMMU.Count >= objBSMM->PageMMU.AllocCount ) {
		uint32 iNewCount = objBSMM->PageMMU.Count + objBSMM->PageMMU.AllocStep;
		ptr* pNew = xrtRealloc(objBSMM->PageMMU.Memory, iNewCount * sizeof(ptr));
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

// 创建数据块结构内存管理器
XXAPI xbsmm xrtBsmmCreate(uint32 iItemLength)
{
	return xrtBsmmCreateEx(iItemLength, XRT_OBJMODE_LOCAL);
}
XXAPI xbsmm xrtBsmmCreateEx(uint32 iItemLength, uint32 iMode)
{
	xbsmm objBSMM = xrtMalloc(sizeof(xbsmm_struct));
	if ( objBSMM ) {
		xrtBsmmInitEx(objBSMM, iItemLength, iMode);
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
XXAPI void xrtBsmmInit(xbsmm objBSMM, uint32 iItemLength)
{
	xrtBsmmInitEx(objBSMM, iItemLength, XRT_OBJMODE_LOCAL);
}
XXAPI void xrtBsmmInitEx(xbsmm objBSMM, uint32 iItemLength, uint32 iMode)
{
	xrtOwnerInitMode(&objBSMM->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objBSMM->Owner);
	}
	objBSMM->ItemLength = iItemLength;
	objBSMM->Count = 0;
	xrtPtrArrayInitEx(&objBSMM->PageMMU, iMode);
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
	for ( int i = 0; i < objBSMM->PageMMU.Count; i++ ) {
		xrtFree(objBSMM->PageMMU.Memory[i]);
		objBSMM->PageMMU.Memory[i] = NULL;
	}
	__xrtBsmmPageMMUUnit(objBSMM);
	// 循环释放空闲内存块链表
	MemPtr_LLNode* pNode = objBSMM->LL_Free;
	while ( pNode ) {
		MemPtr_LLNode* pNext = pNode->Next;
		xrtFree(pNode);
		pNode = pNext;
	}
	objBSMM->LL_Free = NULL;
	xrtOwnerEndMutable(&objBSMM->Owner);
}

// 申请结构体内存
XXAPI ptr xrtBsmmAlloc(xbsmm objBSMM)
{
	ptr pResult = NULL;
	if ( !xrtOwnerBeginMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
		return NULL;
	}
	if ( objBSMM->LL_Free ) {
		// 有空闲内存块先用空闲的
		ptr Ptr = objBSMM->LL_Free->Ptr;
		MemPtr_LLNode* pNext = objBSMM->LL_Free->Next;
		xrtFree(objBSMM->LL_Free);
		objBSMM->LL_Free = pNext;
		pResult = Ptr;
	} else {
		// 需要申请新的内存块
		if ( objBSMM->Count >= (objBSMM->PageMMU.Count * 256) ) {
			ptr pBlock = xrtMalloc(objBSMM->ItemLength * 256);
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
	if ( !xrtOwnerBeginMutable(&objBSMM->Owner, "bsmm belongs to another thread.") ) {
		return;
	}
	MemPtr_LLNode* pNode = xrtMalloc(sizeof(MemPtr_LLNode));
	if ( pNode == NULL ) {
		xrtSetError("BSMM free failed: out of memory.", FALSE);
		xrtOwnerEndMutable(&objBSMM->Owner);
		return;
	}
	pNode->Ptr = p;
	pNode->Next = objBSMM->LL_Free;
	objBSMM->LL_Free = pNode;
	xrtOwnerEndMutable(&objBSMM->Owner);
}


