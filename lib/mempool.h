


// 创建内存池
XXAPI xmempool xrtMemPoolCreate(int iCustom, uint32 iMode)
{
	xmempool objMP = xrtMalloc(sizeof(xmempool_struct));
	if ( objMP ) {
		xrtMemPoolInit(objMP, iCustom, iMode);
	}
	return objMP;
}

// 销毁内存池
XXAPI void xrtMemPoolDestroy(xmempool objMP)
{
	if ( objMP ) {
		if ( !xrtOwnerCheckMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
			return;
		}
		xrtMemPoolUnit(objMP);
		xrtFree(objMP);
	}
}

// 初始化内存池（对自维护结构体指针使用）
static inline uint32 __xrtMemPoolResolveCutoff(int iCustom)
{
	if ( iCustom <= 0 ) {
		return XRT_MEMPOOL_CUTOFF_DEFAULT;
	}
	return (uint32)iCustom;
}

static inline uint32 __xrtMemPoolBucketCount(uint32 iCutoff)
{
	if ( iCutoff == 0 ) {
		return 0;
	}
	return (uint32)((iCutoff + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE);
}

static inline void __xrtMemPoolInitBucket(FSB_Item* pBucket, uint32 iSizeMin, uint32 iSizeMax)
{
	pBucket->MinLength = iSizeMin;
	pBucket->MaxLength = iSizeMax;
	pBucket->LL_Idle = NULL;
	pBucket->LL_Full = NULL;
	pBucket->LL_Null = NULL;
	pBucket->LL_Free = NULL;
	pBucket->left = NULL;
	pBucket->right = NULL;
}

static inline bool __xrtMemPoolBuildBucketPlan(xmempool objMP, uint32 iCutoff)
{
	uint32 iBucketCount;
	uint32 iBucket;
	uint32 iSize;

	objMP->FSB_Memory = NULL;
	objMP->FSB_RootNode = NULL;
	objMP->FSB_Lut = NULL;
	objMP->iBucketStep = XRT_MEMPOOL_STEP_SIZE;
	objMP->iBucketCount = 0;
	objMP->iFallbackCutoff = iCutoff;

	if ( iCutoff == 0 ) {
		return TRUE;
	}

	iBucketCount = __xrtMemPoolBucketCount(iCutoff);
	objMP->FSB_Memory = xrtCalloc(iBucketCount, sizeof(FSB_Item));
	if ( objMP->FSB_Memory == NULL ) {
		return FALSE;
	}

	objMP->FSB_Lut = xrtMalloc(sizeof(uint32) * (iCutoff + 1));
	if ( objMP->FSB_Lut == NULL ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
		return FALSE;
	}

	memset(objMP->FSB_Lut, 0, sizeof(uint32) * (iCutoff + 1));
	objMP->iBucketCount = iBucketCount;
	objMP->FSB_RootNode = &objMP->FSB_Memory[0];

	for ( iBucket = 0; iBucket < iBucketCount; iBucket++ ) {
		uint32 iMin = (iBucket * XRT_MEMPOOL_STEP_SIZE) + 1;
		uint32 iMax = iMin + XRT_MEMPOOL_STEP_SIZE - 1;
		if ( iMax > iCutoff ) {
			iMax = iCutoff;
		}
		__xrtMemPoolInitBucket(&objMP->FSB_Memory[iBucket], iMin, iMax);
	}

	for ( iSize = 1; iSize <= iCutoff; iSize++ ) {
		uint32 iClass = (uint32)((iSize - 1) / XRT_MEMPOOL_STEP_SIZE);
		if ( iClass >= iBucketCount ) {
			iClass = iBucketCount - 1;
		}
		objMP->FSB_Lut[iSize] = iClass;
	}

	return TRUE;
}

static inline FSB_Item* __xrtMemPoolGetBucketBySize(xmempool objMP, uint32 iSize)
{
	if ( objMP == NULL || objMP->FSB_Memory == NULL || objMP->FSB_Lut == NULL ) {
		return NULL;
	}
	if ( iSize == 0 || iSize > objMP->iFallbackCutoff ) {
		return NULL;
	}
	return &objMP->FSB_Memory[objMP->FSB_Lut[iSize]];
}

static inline FSB_Item* __xrtMemPoolGetBucketByMMU(xmempool objMP, xmemunit objMMU)
{
	uint32 iSize;

	if ( objMP == NULL || objMMU == NULL ) {
		return NULL;
	}
	iSize = objMMU->ItemLength - sizeof(MMU_Value);
	if ( iSize == 0 || iSize > objMP->iFallbackCutoff ) {
		return NULL;
	}
	return __xrtMemPoolGetBucketBySize(objMP, iSize);
}

XXAPI void xrtMemPoolInit(xmempool objMP, int iCustom, uint32 iMode)
{
	xrtOwnerInitMode(&objMP->Owner, iMode);
	if ( iMode == XRT_OBJMODE_SHARED ) {
		xrtOwnerActivateShared(&objMP->Owner);
	}
	xrtBsmmInit(&objMP->arrMMU, sizeof(MMU_LLNode), iMode);
	xrtBsmmInit(&objMP->BigMM, sizeof(MP_BigInfoLL), iMode);
	objMP->LL_BigFree = NULL;
	if ( !__xrtMemPoolBuildBucketPlan(objMP, __xrtMemPoolResolveCutoff(iCustom)) ) {
		xrtSetError("Memory Pool : build bucket plan failed.", FALSE);
	}
}

// 释放内存池（对自维护结构体指针使用）
XXAPI void xrtMemPoolUnit(xmempool objMP)
{
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	for ( int i = 0; i < objMP->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, i);
		if ( pNode->objMMU ) {
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
		}
	}
	xrtBsmmUnit(&objMP->arrMMU);
	if ( objMP->FSB_Lut ) {
		xrtFree(objMP->FSB_Lut);
		objMP->FSB_Lut = NULL;
	}
	if ( objMP->FSB_Memory ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
	}
	objMP->FSB_RootNode = NULL;
	objMP->iBucketStep = XRT_MEMPOOL_STEP_SIZE;
	objMP->iBucketCount = 0;
	objMP->iFallbackCutoff = 0;
	for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
		if ( pInfo->Ptr ) {
			xrtFree(pInfo->Ptr);
		}
	}
	xrtBsmmUnit(&objMP->BigMM);
	objMP->LL_BigFree = NULL;
	xrtOwnerEndMutable(&objMP->Owner);
}

#ifdef XRT_MEM_DEBUG
XXAPI xmempool xrtMemPoolCreateDbg(int iCustom, uint32 iMode, const char* sFile, uint32 iLine)
{
	xmempool objMP = xrtMemPoolCreate(iCustom, iMode);
	__xrtMemDebugRegisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objMP;
}
XXAPI void xrtMemPoolInitDbg(xmempool objMP, int iCustom, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemPoolInit(objMP, iCustom, iMode);
	__xrtMemDebugRegisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}
XXAPI void xrtMemPoolDestroyDbg(xmempool objMP, const char* sFile, uint32 iLine)
{
	if ( objMP ) {
		if ( !__xrtMemDebugObjectGuardDestroy(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
			return;
		}
		xrtMemPoolUnit(objMP);
		__xrtMemDebugUnregisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine);
		xrtFreeDbg(objMP, sFile, iLine);
	}
}
XXAPI void xrtMemPoolUnitDbg(xmempool objMP, const char* sFile, uint32 iLine)
{
	if ( objMP == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	for ( int i = 0; i < objMP->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, i);
		if ( pNode->objMMU ) {
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
		}
	}
	xrtBsmmUnit(&objMP->arrMMU);
	if ( objMP->FSB_Lut ) {
		xrtFree(objMP->FSB_Lut);
		objMP->FSB_Lut = NULL;
	}
	if ( objMP->FSB_Memory ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
	}
	objMP->FSB_RootNode = NULL;
	objMP->iBucketStep = XRT_MEMPOOL_STEP_SIZE;
	objMP->iBucketCount = 0;
	objMP->iFallbackCutoff = 0;
	for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
		if ( pInfo->Ptr ) {
			xrtFree(pInfo->Ptr);
		}
	}
	xrtBsmmUnit(&objMP->BigMM);
	objMP->LL_BigFree = NULL;
	xrtOwnerEndMutable(&objMP->Owner);
	__xrtMemDebugUnregisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine);
}
#endif

static inline xmemunit __xrtMemPoolAcquireUnit(xmempool objMP, FSB_Item* objFSB)
{
	xmemunit objMMU = NULL;

	if ( objFSB->LL_Idle == NULL ) {
		if ( objFSB->LL_Null ) {
			objMMU = objFSB->LL_Null->objMMU;
			objFSB->LL_Idle = objFSB->LL_Null;
			objFSB->LL_Null = NULL;
		} else if ( objFSB->LL_Free ) {
			MMU_LLNode* pNode = objFSB->LL_Free;
			objMMU = xrtMemUnitCreate(objFSB->MaxLength, xrtOwnerGetMode(&objMP->Owner));
			if ( objMMU == NULL ) {
				return NULL;
			}
			objMMU->Flag = pNode->Flag;
			pNode->objMMU = objMMU;
			if ( pNode->Next ) {
				pNode->Next->Prev = NULL;
			}
			objFSB->LL_Free = pNode->Next;
			pNode->Prev = NULL;
			pNode->Next = NULL;
			objFSB->LL_Idle = pNode;
		} else {
			MMU_LLNode* pNode;
			objMMU = xrtMemUnitCreate(objFSB->MaxLength, xrtOwnerGetMode(&objMP->Owner));
			if ( objMMU == NULL ) {
				return NULL;
			}
			pNode = xrtBsmmAlloc(&objMP->arrMMU);
			if ( pNode == NULL ) {
				xrtMemUnitDestroy(objMMU);
				xrtSetError("Memory Pool : add memory unit failed.", FALSE);
				return NULL;
			}
			pNode->objMMU = objMMU;
			pNode->Prev = NULL;
			pNode->Next = NULL;
			pNode->Flag = MMU_FLAG_USE | ((objMP->arrMMU.Count - 1) << 8);
			objMMU->Flag = pNode->Flag;
			objFSB->LL_Idle = pNode;
		}
	} else {
		objMMU = objFSB->LL_Idle->objMMU;
		if ( objMMU->Count >= 255 ) {
			MMU_LLNode* pNode = objFSB->LL_Idle;
			if ( pNode->Next ) {
				pNode->Next->Prev = NULL;
			}
			objFSB->LL_Idle = pNode->Next;
			pNode->Prev = NULL;
			pNode->Next = objFSB->LL_Full;
			if ( objFSB->LL_Full ) {
				objFSB->LL_Full->Prev = pNode;
			}
			objFSB->LL_Full = pNode;
		}
	}

	return objMMU;
}

// 从内存池中申请一块内存
XXAPI void* xrtMemPoolAlloc(xmempool objMP, uint32 iSize)
{
	void* pRet = NULL;

	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return NULL;
	}
	if ( iSize == 0 ) {
		xrtOwnerEndMutable(&objMP->Owner);
		return NULL;
	}

	{
		FSB_Item* objFSB = __xrtMemPoolGetBucketBySize(objMP, iSize);
		if ( objFSB ) {
			xmemunit objMMU = __xrtMemPoolAcquireUnit(objMP, objFSB);
			if ( objMMU == NULL ) {
				xrtOwnerEndMutable(&objMP->Owner);
				return NULL;
			}
			pRet = xrtMemUnitAlloc_Inline(objMMU);
			__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, NULL, 0);
			xrtOwnerEndMutable(&objMP->Owner);
			return pRet;
		}
	}

	{
		MP_MemHead* pHead = xrtMalloc(sizeof(MP_MemHead) + iSize);
		if ( pHead ) {
			if ( objMP->LL_BigFree ) {
				MP_BigInfoLL* pInfo = objMP->LL_BigFree;
				objMP->LL_BigFree = pInfo->Next;
				pInfo->Next = NULL;
				pHead->Index = pInfo->Index;
				pHead->Flag = MMU_FLAG_EXT;
				pInfo->Size = iSize;
				pInfo->Ptr = pHead;
				pRet = &pHead[1];
				__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, NULL, 0);
				xrtOwnerEndMutable(&objMP->Owner);
				return pRet;
			} else {
				MP_BigInfoLL* pInfo = xrtBsmmAlloc(&objMP->BigMM);
				if ( pInfo ) {
					pInfo->Index = objMP->BigMM.Count - 1;
					pInfo->Next = NULL;
					pHead->Index = pInfo->Index;
					pHead->Flag = MMU_FLAG_EXT;
					pInfo->Size = iSize;
					pInfo->Ptr = pHead;
					pRet = &pHead[1];
					__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, NULL, 0);
					xrtOwnerEndMutable(&objMP->Owner);
					return pRet;
				}
			}
			xrtFree(pHead);
		}
	}

	xrtOwnerEndMutable(&objMP->Owner);
	return NULL;
}

// 将内存池申请的内存释放掉
static inline void MP256_LLNode_ClearCheck(FSB_Item* objFSB, MMU_LLNode* pNode, int bLL_Full)
{
	if ( pNode->objMMU->Count == 0 ) {
		if ( objFSB->LL_Null ) {
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objFSB->LL_Full = pNode->Next;
				} else {
					objFSB->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			pNode->Prev = NULL;
			pNode->Next = objFSB->LL_Free;
			if ( objFSB->LL_Free ) {
				objFSB->LL_Free->Prev = pNode;
			}
			objFSB->LL_Free = pNode;
		} else {
			if ( pNode->Prev ) {
				pNode->Prev->Next = pNode->Next;
			} else {
				if ( bLL_Full ) {
					objFSB->LL_Full = pNode->Next;
				} else {
					objFSB->LL_Idle = pNode->Next;
				}
			}
			if ( pNode->Next ) {
				pNode->Next->Prev = pNode->Prev;
			}
			objFSB->LL_Null = pNode;
			pNode->Prev = NULL;
			pNode->Next = NULL;
		}
	}
}

static inline void MP256_LLNode_IdleCheck(FSB_Item* objFSB, MMU_LLNode* pNode)
{
	if ( pNode->objMMU->Count < 256 ) {
		if ( pNode->Prev ) {
			pNode->Prev->Next = pNode->Next;
		} else {
			objFSB->LL_Full = pNode->Next;
		}
		if ( pNode->Next ) {
			pNode->Next->Prev = pNode->Prev;
		}
		pNode->Prev = NULL;
		pNode->Next = objFSB->LL_Idle;
		if ( objFSB->LL_Idle ) {
			objFSB->LL_Idle->Prev = pNode;
		}
		objFSB->LL_Idle = pNode;
	}
}

XXAPI void xrtMemPoolFree(xmempool objMP, void* ptr)
{
	if ( ptr == NULL ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	if ( !__xrtMemDebugUnregisterForeignAlloc(ptr, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, NULL, 0) ) {
		#ifdef XRT_MEM_DEBUG
			if ( xrtMemDebugIsEnabled() ) {
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
		#endif
	}

	{
		MMU_ValuePtr v = (MMU_ValuePtr)((char*)ptr - sizeof(MMU_Value));
		if ( (v->ItemFlag & MMU_FLAG_MASK) == MMU_FLAG_MASK ) {
			MP_MemHead* pHead = (MP_MemHead*)((char*)ptr - sizeof(MP_MemHead));
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, pHead->Index);
			if ( pInfo == NULL || pInfo->Ptr == NULL ) {
				xrtSetError("Memory Pool : BigMM item cannot be null.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			pHead->Flag = 0;
			xrtFree(pInfo->Ptr);
			pInfo->Ptr = NULL;
			pInfo->Next = objMP->LL_BigFree;
			objMP->LL_BigFree = pInfo;
		} else if ( v->ItemFlag & MMU_FLAG_USE ) {
			int iMMU = (v->ItemFlag & MMU_FLAG_MASK) >> 8;
			uint8 idx = v->ItemFlag & 0xFF;
			MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, iMMU);
			FSB_Item* objFSB;
			if ( pNode->objMMU == NULL ) {
				xrtSetError("Memory Pool : MMU cannot be null.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			objFSB = __xrtMemPoolGetBucketByMMU(objMP, pNode->objMMU);
			if ( objFSB == NULL ) {
				xrtSetError("Memory Pool : find bucket error.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			xrtMemUnitFreeIdx_Inline(pNode->objMMU, idx);
			v->ItemFlag = 0;
			if ( pNode->objMMU->Count >= 255 ) {
				MP256_LLNode_IdleCheck(objFSB, pNode);
			}
			MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		}
	}

	xrtOwnerEndMutable(&objMP->Owner);
}

// 进行一轮 GC，将 标记 或 未标记 的内存全部回收
static inline void MP256_GC_Bucket(FSB_Item* objFSB, bool bFreeMark)
{
	MMU_LLNode* pNode = objFSB->LL_Idle;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	pNode = objFSB->LL_Full;
	while ( pNode ) {
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}

	pNode = objFSB->LL_Idle;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		pNode = pNext;
	}
	pNode = objFSB->LL_Full;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		if ( pNode->objMMU->Count == 0 ) {
			MP256_LLNode_ClearCheck(objFSB, pNode, -1);
		} else {
			MP256_LLNode_IdleCheck(objFSB, pNode);
		}
		pNode = pNext;
	}
}

XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark)
{
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}

	for ( uint32 i = 0; i < objMP->iBucketCount; i++ ) {
		MP256_GC_Bucket(&objMP->FSB_Memory[i], bFreeMark);
	}

	if ( bFreeMark ) {
		for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			if ( pInfo == NULL || pInfo->Ptr == NULL ) {
				continue;
			}
			{
				MP_MemHead* pHead = pInfo->Ptr;
				if ( (pHead->Flag & MMU_FLAG_USE) && (pHead->Flag & MMU_FLAG_GC) ) {
					pHead->Flag = 0;
					xrtFree(pInfo->Ptr);
					pInfo->Ptr = NULL;
					pInfo->Next = objMP->LL_BigFree;
					objMP->LL_BigFree = pInfo;
				}
			}
		}
	} else {
		for ( int i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			if ( pInfo == NULL || pInfo->Ptr == NULL ) {
				continue;
			}
			{
				MP_MemHead* pHead = pInfo->Ptr;
				if ( pHead->Flag & MMU_FLAG_USE ) {
					if ( pHead->Flag & MMU_FLAG_GC ) {
						pHead->Flag &= ~MMU_FLAG_GC;
					} else {
						pHead->Flag = 0;
						xrtFree(pInfo->Ptr);
						pInfo->Ptr = NULL;
						pInfo->Next = objMP->LL_BigFree;
						objMP->LL_BigFree = pInfo;
					}
				}
			}
		}
	}

	xrtOwnerEndMutable(&objMP->Owner);
}
