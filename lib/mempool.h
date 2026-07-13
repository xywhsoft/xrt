


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
			(xrtMemPoolUnit)(objMP);
		xrtFree(objMP);
	}
}

// 初始化内存池（对自维护结构体指针使用）
static inline uint32 __xrtMemPoolResolveCutoff(int iCustom)
{
	size_t iMaxCutoffByLut;
	size_t iMaxBucketCount;
	size_t iMaxCutoffByBucket;
	size_t iMaxCutoff;
	
	// 使用默认值
	if ( iCustom <= 0 ) {
		return XRT_MEMPOOL_CUTOFF_DEFAULT;
	}
	
	// 计算查找表允许的最大 cutoff（受 SIZE_MAX / sizeof(uint32) 限制）
	iMaxCutoffByLut = SIZE_MAX / sizeof(uint32);
	if ( iMaxCutoffByLut > 0 ) {
		iMaxCutoffByLut--;
	}
	
	// 计算桶数组允许的最大 cutoff（受 SIZE_MAX / sizeof(FSB_Item) 限制）
	iMaxBucketCount = SIZE_MAX / sizeof(FSB_Item);
	if ( iMaxBucketCount > 0 ) {
		iMaxBucketCount--;
	}
	iMaxCutoffByBucket = iMaxBucketCount * (size_t)XRT_MEMPOOL_STEP_SIZE;
	
	// 取两者的较小值作为最终上限
	iMaxCutoff = iMaxCutoffByLut < iMaxCutoffByBucket ? iMaxCutoffByLut : iMaxCutoffByBucket;
	if ( iMaxCutoff == 0 ) {
		return XRT_MEMPOOL_CUTOFF_DEFAULT;
	}
	
	// 限制自定义值不超过上限
	if ( (size_t)iCustom > iMaxCutoff ) {
		return (uint32)iMaxCutoff;
	}
	return (uint32)iCustom;
}


// 内部函数：统计内存内存池桶
static inline uint32 __xrtMemPoolBucketCount(uint32 iCutoff)
{
	if ( iCutoff == 0 ) {
		return 0;
	}
	return (uint32)((iCutoff + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE);
}


// 内部函数：初始化内存内存池桶
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


// 内部函数：构建内存内存池桶计划（根据截止大小构建桶分类和查找表）
static inline bool __xrtMemPoolBuildBucketPlan(xmempool objMP, uint32 iCutoff)
{
	uint32 iBucketCount;
	uint32 iBucket;
	uint32 iSize;
	size_t iLutCount;
	uint64 iBucketCount64;
	uint64 iCutoff64;
	
	// 初始化内存池桶相关字段
	objMP->FSB_Memory = NULL;
	objMP->FSB_RootNode = NULL;
	objMP->FSB_Lut = NULL;
	objMP->iBucketStep = XRT_MEMPOOL_STEP_SIZE;
	objMP->iBucketCount = 0;
	objMP->iFallbackCutoff = iCutoff;
	
	if ( iCutoff == 0 ) {
		return TRUE;
	}
	
	// 计算桶数量并进行溢出检查
	iBucketCount = __xrtMemPoolBucketCount(iCutoff);
	iBucketCount64 = iBucketCount;
	iCutoff64 = iCutoff;
	if ( iBucketCount == 0 ) {
		return FALSE;
	}
	if ( iBucketCount64 > (SIZE_MAX / sizeof(FSB_Item)) ) {
		return FALSE;
	}
	
	// 分配桶数组
	objMP->FSB_Memory = xrtCalloc(iBucketCount, sizeof(FSB_Item));
	if ( objMP->FSB_Memory == NULL ) {
		return FALSE;
	}
	
	// 分配查找表（大小为 cutoff + 1）
	if ( iCutoff64 >= (SIZE_MAX / sizeof(uint32)) ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
		return FALSE;
	}
	iLutCount = (size_t)iCutoff + 1u;
	objMP->FSB_Lut = xrtMalloc(sizeof(uint32) * iLutCount);
	if ( objMP->FSB_Lut == NULL ) {
		xrtFree(objMP->FSB_Memory);
		objMP->FSB_Memory = NULL;
		return FALSE;
	}
	
	// 初始化查找表并设置桶指针
	memset(objMP->FSB_Lut, 0, sizeof(uint32) * iLutCount);
	objMP->iBucketCount = iBucketCount;
	objMP->FSB_RootNode = &objMP->FSB_Memory[0];
	
	// 初始化每个桶的大小范围
	for ( iBucket = 0; iBucket < iBucketCount; iBucket++ ) {
		uint32 iMin = (iBucket * XRT_MEMPOOL_STEP_SIZE) + 1;
		uint32 iMax = iMin + XRT_MEMPOOL_STEP_SIZE - 1;
		if ( iMax > iCutoff ) {
			iMax = iCutoff;
		}
		__xrtMemPoolInitBucket(&objMP->FSB_Memory[iBucket], iMin, iMax);
	}
	
	// 构建大小到桶索引的查找表
	for ( iSize = 1; iSize <= iCutoff; iSize++ ) {
		uint32 iClass = (uint32)((iSize - 1) / XRT_MEMPOOL_STEP_SIZE);
		if ( iClass >= iBucketCount ) {
			iClass = iBucketCount - 1;
		}
		objMP->FSB_Lut[iSize] = iClass;
	}

	return TRUE;
}


// 内部函数：获取内存内存池桶 by 大小
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


// 内部函数：获取内存内存池桶 by mmu
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


// 初始化内存内存池
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
		xrtBsmmUnit(&objMP->arrMMU);
		xrtBsmmUnit(&objMP->BigMM);
		objMP->LL_BigFree = NULL;
		objMP->FSB_Memory = NULL;
		objMP->FSB_RootNode = NULL;
		objMP->FSB_Lut = NULL;
		objMP->iBucketStep = XRT_MEMPOOL_STEP_SIZE;
		objMP->iBucketCount = 0;
		objMP->iFallbackCutoff = 0;
		xrtSetError("Memory Pool : build bucket plan failed.", FALSE);
	}
}

// 释放内存池（对自维护结构体指针使用）
XXAPI void xrtMemPoolUnit(xmempool objMP)
{
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	
	// 遍历所有 MMU 节点，销毁内存单元
	for ( uint32 i = 0; i < objMP->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, i);
		if ( pNode->objMMU ) {
			// 池整体销毁时，活动槽位不会逐个经过 xrtMemPoolFree，必须统一注销。
			for ( int idx = 0; idx < 256; idx++ ) {
				MMU_ValuePtr v = (MMU_ValuePtr)&(pNode->objMMU->Memory[(size_t)pNode->objMMU->ItemLength * idx]);
				if ( v->ItemFlag & MMU_FLAG_USE ) {
					__xrtMemDebugTryUnregisterForeignAllocSilent((ptr)&v[1]);
				}
			}
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
		}
	}
	xrtBsmmUnit(&objMP->arrMMU);
	
	// 释放查找表和桶数组
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
	
	// 遍历所有大块分配记录，释放大块内存
	for ( uint32 i = 0; i < objMP->BigMM.Count; i++ ) {
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
		if ( pInfo->Ptr ) {
			__xrtMemDebugTryUnregisterForeignAllocSilent(&((MP_MemHead*)pInfo->Ptr)[1]);
			xrtFree(pInfo->Ptr);
		}
	}
	xrtBsmmUnit(&objMP->BigMM);
	objMP->LL_BigFree = NULL;
	xrtOwnerEndMutable(&objMP->Owner);
}

#ifdef XRT_MEM_DEBUG
// 创建内存内存池调试
XXAPI xmempool xrtMemPoolCreateDbg(int iCustom, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xmempool objMP = xrtMemPoolCreate(iCustom, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objMP;
}


// 初始化内存内存池调试
XXAPI void xrtMemPoolInitDbg(xmempool objMP, int iCustom, uint32 iMode, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xrtMemPoolInit(objMP, iCustom, iMode);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁内存内存池调试
XXAPI void xrtMemPoolDestroyDbg(xmempool objMP, const char* sFile, uint32 iLine)
{
	if ( objMP ) {
		xrtMemDebugSiteScope tScope;
		if ( !__xrtMemDebugObjectGuardDestroy(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine) ) {
			return;
		}
		if ( !xrtOwnerCheckMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
			return;
		}
		tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
		(xrtMemPoolUnit)(objMP);
		__xrtMemDebugLeaveSiteScope(&tScope);
		__xrtMemDebugUnregisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine);
		xrtFreeDbg(objMP, sFile, iLine);
	}
}


// 释放内存内存池调试
XXAPI void xrtMemPoolUnitDbg(xmempool objMP, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope;
	if ( objMP == NULL ) {
		return;
	}
	
	// 检查对象是否已被销毁（防止双重销毁）
	if ( !__xrtMemDebugObjectGuardDestroy(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine) ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	
	// 进入调试位置作用域
	tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	
	// 遍历所有 MMU 节点，清理仍在使用的内存块的 foreign alloc 跟踪并销毁内存单元
	for ( uint32 i = 0; i < objMP->arrMMU.Count; i++ ) {
		MMU_LLNode* pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, i);
		if ( pNode->objMMU ) {
			for ( int idx = 0; idx < 256; idx++ ) {
				MMU_ValuePtr v = (MMU_ValuePtr)&(pNode->objMMU->Memory[(size_t)pNode->objMMU->ItemLength * idx]);
				if ( v->ItemFlag & MMU_FLAG_USE ) {
					__xrtMemDebugTryUnregisterForeignAllocSilent((ptr)&v[1]);
				}
			}
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
		}
	}
	xrtBsmmUnit(&objMP->arrMMU);
	
	// 释放查找表和桶数组
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
	
	// 遍历所有大块分配记录，释放大块内存
	for ( uint32 i = 0; i < objMP->BigMM.Count; i++ ) {
		MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
		if ( pInfo->Ptr ) {
			xrtFree(pInfo->Ptr);
		}
	}
	xrtBsmmUnit(&objMP->BigMM);
	objMP->LL_BigFree = NULL;
	
	// 离开调试位置作用域并注销对象跟踪
	__xrtMemDebugLeaveSiteScope(&tScope);
	xrtOwnerEndMutable(&objMP->Owner);
	__xrtMemDebugUnregisterObject(objMP, XRT_MEMDEBUG_OBJECT_MEMPOOL, sFile, iLine);
}
#endif

// 内部函数：获取内存池可用内存单元（从桶的各链表中获取或创建 MMU）
static inline xmemunit __xrtMemPoolAcquireUnit(xmempool objMP, FSB_Item* objFSB)
{
	xmemunit objMMU = NULL;
	
	if ( objFSB->LL_Idle == NULL ) {
		// Idle 链表为空，尝试从其他链表获取节点
		if ( objFSB->LL_Null ) {
			// 将 Null 链表提升为 Idle 链表
			objMMU = objFSB->LL_Null->objMMU;
			objFSB->LL_Idle = objFSB->LL_Null;
			objFSB->LL_Null = NULL;
		} else if ( objFSB->LL_Free ) {
			// 从 Free 链表取一个节点，创建新的 MMU 并挂载
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
			// 所有链表均为空，创建全新的 MMU 和节点
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
			// 检查 MMU 索引是否溢出
			if ( (objMP->arrMMU.Count - 1u) > (MMU_FLAG_MASK >> 8) ) {
				xrtMemUnitDestroy(objMMU);
				xrtBsmmFree(&objMP->arrMMU, pNode);
				xrtSetError("Memory Pool : MMU index overflow.", FALSE);
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
		// Idle 链表有可用节点
		objMMU = objFSB->LL_Idle->objMMU;
		// 如果 MMU 已满（256个槽位用完），移入 Full 链表
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

// 从内存池中申请一块内存（小块走桶分配，大块走直接分配）
XXAPI void* xrtMemPoolAlloc(xmempool objMP, uint32 iSize)
{
	void* pRet = NULL;
	const char* sDbgFile = NULL;
	uint32 iDbgLine = 0;
	#ifdef XRT_MEM_DEBUG
		sDbgFile = __FILE__;
		iDbgLine = __LINE__;
		__xrtMemDebugPreferSite(&sDbgFile, &iDbgLine);
	#endif

	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return NULL;
	}
	if ( iSize == 0 ) {
		xrtOwnerEndMutable(&objMP->Owner);
		return NULL;
	}

	// 尝试从桶分配（大小在截止阈值内）
	{
		FSB_Item* objFSB = __xrtMemPoolGetBucketBySize(objMP, iSize);
		if ( objFSB ) {
			xmemunit objMMU = __xrtMemPoolAcquireUnit(objMP, objFSB);
			if ( objMMU == NULL ) {
				xrtOwnerEndMutable(&objMP->Owner);
				return NULL;
			}
			pRet = xrtMemUnitAlloc_Inline(objMMU);
			__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, sDbgFile, iDbgLine);
			xrtOwnerEndMutable(&objMP->Owner);
			return pRet;
		}
	}

	// 大块分配：超出桶范围，直接通过全局分配器分配
	{
		MP_MemHead* pHead = xrtMalloc(sizeof(MP_MemHead) + iSize);
		if ( pHead ) {
			// 优先复用空闲的大块信息节点
			if ( objMP->LL_BigFree ) {
				MP_BigInfoLL* pInfo = objMP->LL_BigFree;
				objMP->LL_BigFree = pInfo->Next;
				pInfo->Next = NULL;
				pHead->Index = pInfo->Index;
				pHead->Flag = MMU_FLAG_EXT;
				pInfo->Size = iSize;
				pInfo->Ptr = pHead;
				pRet = &pHead[1];
				__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, sDbgFile, iDbgLine);
				xrtOwnerEndMutable(&objMP->Owner);
				return pRet;
			} else {
				// 没有可复用的节点，分配新的大块信息节点
				MP_BigInfoLL* pInfo = xrtBsmmAlloc(&objMP->BigMM);
				if ( pInfo ) {
					pInfo->Index = objMP->BigMM.Count - 1;
					pInfo->Next = NULL;
					pHead->Index = pInfo->Index;
					pHead->Flag = MMU_FLAG_EXT;
					pInfo->Size = iSize;
					pInfo->Ptr = pHead;
					pRet = &pHead[1];
					__xrtMemDebugRegisterForeignAlloc(pRet, iSize, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, sDbgFile, iDbgLine);
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

// 将内存池申请的内存释放掉（检查并清理空 MMU 节点的链表状态）
static inline void MP256_LLNode_ClearCheck(FSB_Item* objFSB, MMU_LLNode* pNode, int bLL_Full)
{
	// 仅当 MMU 中已无使用中的槽位时才执行清理
	if ( pNode->objMMU->Count == 0 ) {
		if ( objFSB->LL_Null ) {
			// Null 链表已有节点，销毁 MMU 并将节点移入 Free 链表
			xrtMemUnitDestroy(pNode->objMMU);
			pNode->objMMU = NULL;
			
			// 从当前链表中移除节点
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
			// Null 链表为空，将节点移入 Null 链表保留 MMU 以备复用
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


// 内部函数：根据标记获取并校验 MMU 节点
static inline MMU_LLNode* __xrtMemPoolGetNodeByFlag(xmempool objMP, uint32 iFlag, MMU_ValuePtr v)
{
	uint32 iMMU;
	uint8 idx;
	MMU_LLNode* pNode;
	if ( objMP == NULL || v == NULL ) {
		return NULL;
	}
	iMMU = (iFlag & MMU_FLAG_MASK) >> 8;
	idx = (uint8)(iFlag & 0xFF);
	if ( iMMU >= objMP->arrMMU.Count ) {
		return NULL;
	}
	pNode = xrtBsmmGetPtr_Inline(&objMP->arrMMU, iMMU);
	if ( pNode == NULL || pNode->objMMU == NULL ) {
		return NULL;
	}
	if ( v != (MMU_ValuePtr)&pNode->objMMU->Memory[(size_t)pNode->objMMU->ItemLength * idx] ) {
		return NULL;
	}
	return pNode;
}


// 释放内存内存池（区分大块释放和桶内释放，并维护链表状态）
XXAPI void xrtMemPoolFree(xmempool objMP, void* ptr)
{
	if ( objMP == NULL || ptr == NULL ) {
		return;
	}
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}
	
	// 注销外部分配跟踪，失败则可能是无效指针
	if ( !__xrtMemDebugUnregisterForeignAlloc(ptr, XRT_MEMDEBUG_ALLOCATOR_MEMPOOL, NULL, 0) ) {
		#ifdef XRT_MEM_DEBUG
			if ( xrtMemDebugIsEnabled() ) {
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
		#endif
	}

	{
		// 获取内存块前方的 MMU_Value 标记以判断类型
		MMU_ValuePtr v = (MMU_ValuePtr)((char*)ptr - sizeof(MMU_Value));
		if ( (v->ItemFlag & MMU_FLAG_MASK) == MMU_FLAG_MASK ) {
			// 大块内存释放
			MP_MemHead* pHead = (MP_MemHead*)((char*)ptr - sizeof(MP_MemHead));
			if ( pHead->Index >= objMP->BigMM.Count ) {
				xrtSetError("Memory Pool : invalid pointer.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, pHead->Index);
			if ( pInfo == NULL || pInfo->Ptr == NULL || pInfo->Ptr != pHead ) {
				xrtSetError("Memory Pool : BigMM item cannot be null.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			// 释放大块内存并将信息节点回收到大块空闲链表
			pHead->Flag = 0;
			xrtFree(pInfo->Ptr);
			pInfo->Ptr = NULL;
			pInfo->Next = objMP->LL_BigFree;
			objMP->LL_BigFree = pInfo;
		} else if ( v->ItemFlag & MMU_FLAG_USE ) {
			// 桶内内存释放
			uint8 idx = v->ItemFlag & 0xFF;
			MMU_LLNode* pNode = __xrtMemPoolGetNodeByFlag(objMP, v->ItemFlag, v);
			FSB_Item* objFSB;
			if ( pNode == NULL ) {
				xrtSetError("Memory Pool : invalid pointer.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			// 查找对应的桶并释放槽位
			objFSB = __xrtMemPoolGetBucketByMMU(objMP, pNode->objMMU);
			if ( objFSB == NULL ) {
				xrtSetError("Memory Pool : find bucket error.", FALSE);
				xrtOwnerEndMutable(&objMP->Owner);
				return;
			}
			xrtMemUnitFreeIdx_Inline(pNode->objMMU, idx);
			v->ItemFlag = 0;
			// 如果 MMU 之前是满的，移回 Idle 链表
			if ( pNode->objMMU->Count >= 255 ) {
				MP256_LLNode_IdleCheck(objFSB, pNode);
			}
			// 检查并清理空的 MMU 节点
			MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		}
	}

	xrtOwnerEndMutable(&objMP->Owner);
}

// 注销一轮 GC 即将回收的桶内分配记录
static inline void __xrtMemPoolUnregisterUnitWillFree(xmemunit objMMU, bool bFreeMark)
{
	if ( objMMU == NULL || objMMU->Count == 0 ) {
		return;
	}
	for ( int idx = 0; idx < 256; idx++ ) {
		MMU_ValuePtr v = (MMU_ValuePtr)&objMMU->Memory[(size_t)objMMU->ItemLength * idx];
		bool bWillFree;
		if ( (v->ItemFlag & MMU_FLAG_USE) == 0 ) {
			continue;
		}
		bWillFree = bFreeMark
			? (v->ItemFlag & MMU_FLAG_GC) != 0
			: (v->ItemFlag & MMU_FLAG_GC) == 0;
		if ( bWillFree ) {
			__xrtMemDebugTryUnregisterForeignAllocSilent((ptr)&v[1]);
		}
	}
}


// 进行一轮 GC，将 标记 或 未标记 的内存全部回收（按桶执行 GC，先回收后整理链表）
static inline void MP256_GC_Bucket(FSB_Item* objFSB, bool bFreeMark)
{
	MMU_LLNode* pNode;
	
	// 第一阶段：对 Idle 和 Full 链表中的所有 MMU 执行 GC 回收
	pNode = objFSB->LL_Idle;
	while ( pNode ) {
		__xrtMemPoolUnregisterUnitWillFree(pNode->objMMU, bFreeMark);
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	pNode = objFSB->LL_Full;
	while ( pNode ) {
		__xrtMemPoolUnregisterUnitWillFree(pNode->objMMU, bFreeMark);
		xrtMemUnitGC(pNode->objMMU, bFreeMark);
		pNode = pNode->Next;
	}
	
	// 第二阶段：清理 Idle 链表中已清空的节点
	pNode = objFSB->LL_Idle;
	while ( pNode ) {
		MMU_LLNode* pNext = pNode->Next;
		MP256_LLNode_ClearCheck(objFSB, pNode, 0);
		pNode = pNext;
	}
	
	// 第三阶段：整理 Full 链表，满的移入 Idle，空的执行清理
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


// xrtMemPoolGC 相关处理（全局 GC，支持按标记回收和未标记回收两种模式）
XXAPI void xrtMemPoolGC(xmempool objMP, bool bFreeMark)
{
	if ( !xrtOwnerBeginMutable(&objMP->Owner, "memory pool belongs to another thread.") ) {
		return;
	}

	// 对所有桶执行 GC
	for ( uint32 i = 0; i < objMP->iBucketCount; i++ ) {
		MP256_GC_Bucket(&objMP->FSB_Memory[i], bFreeMark);
	}

	if ( bFreeMark ) {
		// 释放标记模式：只回收带有 GC 标记的大块内存
		for ( uint32 i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			if ( pInfo == NULL || pInfo->Ptr == NULL ) {
				continue;
			}
			{
				MP_MemHead* pHead = pInfo->Ptr;
				if ( (pHead->Flag & MMU_FLAG_USE) && (pHead->Flag & MMU_FLAG_GC) ) {
					pHead->Flag = 0;
					__xrtMemDebugTryUnregisterForeignAllocSilent(&pHead[1]);
					xrtFree(pInfo->Ptr);
					pInfo->Ptr = NULL;
					pInfo->Next = objMP->LL_BigFree;
					objMP->LL_BigFree = pInfo;
				}
			}
		}
	} else {
		// 释放未标记模式：回收没有 GC 标记的大块内存，并清除已有标记
		for ( uint32 i = 0; i < objMP->BigMM.Count; i++ ) {
			MP_BigInfoLL* pInfo = xrtBsmmGetPtr_Inline(&objMP->BigMM, i);
			if ( pInfo == NULL || pInfo->Ptr == NULL ) {
				continue;
			}
			{
				MP_MemHead* pHead = pInfo->Ptr;
				if ( pHead->Flag & MMU_FLAG_USE ) {
					if ( pHead->Flag & MMU_FLAG_GC ) {
						// 有标记：清除标记（保留存活）
						pHead->Flag &= ~MMU_FLAG_GC;
					} else {
						// 无标记：释放内存
						pHead->Flag = 0;
						__xrtMemDebugTryUnregisterForeignAllocSilent(&pHead[1]);
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
