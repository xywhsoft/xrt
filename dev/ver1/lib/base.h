


// 申请内存
#define __XRT_MEMTELEMETRY_OP_MALLOC 1
#define __XRT_MEMTELEMETRY_OP_CALLOC 2
#define __XRT_MEMTELEMETRY_OP_REALLOC 3

static inline size_t __xrtMemTelemetryMulClamp(size_t iNum, size_t iSize);
static inline uint32 __xrtMemTelemetryClassIndex(size_t iSize);
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize);
static inline void __xrtMemTelemetryRecordFree();
static inline void __xrtMemTelemetryRecordTemp(size_t iSize);


// 内部函数：分配位置
static inline ptr __xrtMallocSite(size_t iSize, const char* sFile, uint32 iLine)
{
	#ifdef XRT_MEM_DEBUG
		__xrtMemDebugPreferSite(&sFile, &iLine);
	#endif
	ptr mem = __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
	if ( mem == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}
	__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_MALLOC, iSize);
	return mem;
}


// 内部函数：分配位置
static inline ptr __xrtCallocSite(size_t iNum, size_t iSize, const char* sFile, uint32 iLine)
{
	size_t iTotal = __xrtMemTelemetryMulClamp(iNum, iSize);
	#ifdef XRT_MEM_DEBUG
		__xrtMemDebugPreferSite(&sFile, &iLine);
	#endif
	ptr mem = __xrtMemGlobalAllocSite(iTotal, TRUE, sFile, iLine);
	if ( mem == NULL ) {
		xrtSetError("class memory allocate failed.", FALSE);
	} else {
		__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_CALLOC, iTotal);
	}
	return mem;
}


// 内部函数：重新分配位置
static inline ptr __xrtReallocSite(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	ptr mem;
	if ( pMem == xCore.sNull ) {
		pMem = NULL;
	}
	#ifdef XRT_MEM_DEBUG
		__xrtMemDebugPreferSite(&sFile, &iLine);
	#endif
	mem = __xrtMemGlobalReallocSite(pMem, iSize, sFile, iLine);
	if ( mem == NULL ) {
		str sError = xrtGetError();
		if ( iSize != 0 && (sError == NULL || sError == xCore.sNull || sError[0] == '\0') ) {
			xrtSetError("memory reallocate failed.", FALSE);
		}
	} else {
		__xrtMemTelemetryRecordSizedOp(__XRT_MEMTELEMETRY_OP_REALLOC, iSize);
	}
	return mem;
}


// 内部函数：释放位置
static inline void __xrtFreeSite(ptr pmem, const char* sFile, uint32 iLine)
{
	if ( pmem && (pmem != xCore.sNull) ) {
		#ifdef XRT_MEM_DEBUG
			__xrtMemDebugPreferSite(&sFile, &iLine);
		#endif
		__xrtMemTelemetryRecordFree();
		__xrtMemGlobalFreeSite(pmem, sFile, iLine);
	}
}

#ifdef XRT_MEM_DEBUG
// 分配调试
XXAPI ptr xrtMallocDbg(size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtMallocSite(iSize, sFile, iLine);
}
#endif

// 分配
XXAPI ptr xrtMalloc(size_t iSize)
{
	#ifdef XRT_MEM_DEBUG
		return __xrtMallocSite(iSize, __FILE__, __LINE__);
	#else
		return __xrtMallocSite(iSize, NULL, 0);
	#endif
}



// 申请类内存
#ifdef XRT_MEM_DEBUG
// 分配调试
XXAPI ptr xrtCallocDbg(size_t iNum, size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtCallocSite(iNum, iSize, sFile, iLine);
}
#endif

// 分配
XXAPI ptr xrtCalloc(size_t iNum, size_t iSize)
{
	#ifdef XRT_MEM_DEBUG
		return __xrtCallocSite(iNum, iSize, __FILE__, __LINE__);
	#else
		return __xrtCallocSite(iNum, iSize, NULL, 0);
	#endif
}



// 重新申请内存
#ifdef XRT_MEM_DEBUG
// 重新分配调试
XXAPI ptr xrtReallocDbg(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	return __xrtReallocSite(pMem, iSize, sFile, iLine);
}
#endif

// 重新分配
XXAPI ptr xrtRealloc(ptr pMem, size_t iSize)
{
	#ifdef XRT_MEM_DEBUG
		return __xrtReallocSite(pMem, iSize, __FILE__, __LINE__);
	#else
		return __xrtReallocSite(pMem, iSize, NULL, 0);
	#endif
}



// 释放内存（ 会先判断是否为 null ）
#ifdef XRT_MEM_DEBUG
// 释放调试
XXAPI void xrtFreeDbg(ptr pmem, const char* sFile, uint32 iLine)
{
	__xrtFreeSite(pmem, sFile, iLine);
}
#endif

// 释放
XXAPI void xrtFree(ptr pmem)
{
	#ifdef XRT_MEM_DEBUG
		__xrtFreeSite(pmem, __FILE__, __LINE__);
	#else
		__xrtFreeSite(pmem, NULL, 0);
	#endif
}


// 原子保留引用，成功时返回递增后的引用计数
XXAPI int32 xrtAtomicRefRetain(volatile int32* pCount)
{
	uint32 iOld;
	uint32 iNext;

	if ( pCount == NULL ) {
		return -1;
	}
	iOld = __xrtAtomicLoadU32((const volatile uint32*)pCount);
	for ( ;; ) {
		if ( iOld == 0 || iOld >= (uint32)INT32_MAX ) {
			return -1;
		}
		iNext = iOld + 1;
		if ( __xrtAtomicCompareExchangeU32((volatile uint32*)pCount, iNext, iOld) == iOld ) {
			return (int32)iNext;
		}
		iOld = __xrtAtomicLoadU32((const volatile uint32*)pCount);
	}
}


// 原子释放引用，成功时返回递减后的引用计数
XXAPI int32 xrtAtomicRefRelease(volatile int32* pCount)
{
	uint32 iOld;
	uint32 iNext;

	if ( pCount == NULL ) {
		return -1;
	}
	iOld = __xrtAtomicLoadU32((const volatile uint32*)pCount);
	for ( ;; ) {
		if ( iOld == 0 || iOld > (uint32)INT32_MAX ) {
			return -1;
		}
		iNext = iOld - 1;
		if ( __xrtAtomicCompareExchangeU32((volatile uint32*)pCount, iNext, iOld) == iOld ) {
			return (int32)iNext;
		}
		iOld = __xrtAtomicLoadU32((const volatile uint32*)pCount);
	}
}


// 内部函数：获取临时内存区块头部大小
static inline size_t __xrtTempArenaBlockHeaderSize()
{
	return __xrtMemGlobalAlignSize(sizeof(xrtTempArenaBlock));
}


// 内部函数：获取临时内存区块用户区指针
static inline ptr __xrtTempArenaBlockUser(xrtTempArenaBlock* pBlock)
{
	return (ptr)((char*)pBlock + __xrtTempArenaBlockHeaderSize());
}


// 内部函数：分配临时内存区块
static inline xrtTempArenaBlock* __xrtTempArenaAllocBlock(size_t iCapacity)
{
	size_t iTotal;
	size_t iHeaderSize;
	xrtTempArenaBlock* pBlock;

	if ( iCapacity == 0 ) {
		iCapacity = 1;
	}
	iHeaderSize = __xrtTempArenaBlockHeaderSize();
	if ( iCapacity > UINT_MAX || iHeaderSize > (SIZE_MAX - iCapacity) ) {
		xrtSetError("temporary memory block size overflow.", FALSE);
		return NULL;
	}
	iTotal = iHeaderSize + iCapacity;
	pBlock = (xrtTempArenaBlock*)__xrtMemGlobalProcMalloc()(iTotal);
	if ( pBlock == NULL ) {
		return NULL;
	}
	memset(pBlock, 0, iHeaderSize);
	pBlock->iCapacity = (uint32)iCapacity;
	return pBlock;
}


// 内部函数：记录临时内存区分配调试信息
static inline void __xrtTempArenaDebugOnAlloc(xrtThreadData* pThreadData, size_t iSize, bool bSpill)
{
	#ifdef XRT_MEM_DEBUG
		const char* sFile = __FILE__;
		uint32 iLine = __LINE__;
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugPreferSite(&sFile, &iLine);
		__xrtMemDebugLock();
		pThreadData->tTemp.iCurrentBytes += iSize;
		if ( pThreadData->tTemp.iCurrentBytes > pThreadData->tTemp.iPeakBytes ) {
			pThreadData->tTemp.iPeakBytes = pThreadData->tTemp.iCurrentBytes;
		}
		xCore.MemDebug.iTempCurrentBytes += iSize;
		if ( xCore.MemDebug.iTempCurrentBytes > xCore.MemDebug.iTempPeakBytes ) {
			xCore.MemDebug.iTempPeakBytes = xCore.MemDebug.iTempCurrentBytes;
		}
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_ALLOC, NULL, iSize, bSpill ? XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL : XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
		(void)iSize;
		(void)bSpill;
	#endif
}


// 内部函数：记录临时内存区重置调试信息
static inline void __xrtTempArenaDebugOnReset(xrtThreadData* pThreadData)
{
	#ifdef XRT_MEM_DEBUG
		const char* sFile = __FILE__;
		uint32 iLine = __LINE__;
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugPreferSite(&sFile, &iLine);
		__xrtMemDebugLock();
		if ( xCore.MemDebug.iTempCurrentBytes >= pThreadData->tTemp.iCurrentBytes ) {
			xCore.MemDebug.iTempCurrentBytes -= pThreadData->tTemp.iCurrentBytes;
		} else {
			xCore.MemDebug.iTempCurrentBytes = 0;
		}
		xCore.MemDebug.iTempResetCount++;
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_RESET, NULL, pThreadData->tTemp.iCurrentBytes, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
	#endif
}


// 内部函数：记录临时内存作用域回退信息
static inline void __xrtTempArenaDebugOnRestore(xrtThreadData* pThreadData, uint64 iSavedBytes)
{
	#ifdef XRT_MEM_DEBUG
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugLock();
		if ( pThreadData->tTemp.iCurrentBytes > iSavedBytes ) {
			uint64 iReleased = pThreadData->tTemp.iCurrentBytes - iSavedBytes;
			if ( xCore.MemDebug.iTempCurrentBytes >= iReleased ) {
				xCore.MemDebug.iTempCurrentBytes -= iReleased;
			} else {
				xCore.MemDebug.iTempCurrentBytes = 0;
			}
		}
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
		(void)iSavedBytes;
	#endif
}


// 内部函数：确保临时内存区当前
static inline bool __xrtTempArenaEnsureCurrent(xrtThreadData* pThreadData, size_t iNeed)
{
	xrtTempArenaBlock* pBlock;
	size_t iCapacity;

	if ( pThreadData == NULL ) {
		return FALSE;
	}
	// 1. 检查当前区块剩余空间是否足够
	if ( pThreadData->tTemp.pCurrent && ((size_t)pThreadData->tTemp.pCurrent->iUsed + iNeed) <= pThreadData->tTemp.pCurrent->iCapacity ) {
		return TRUE;
	}
	// 2. 在当前区块的后续链表中寻找可用区块
	if ( pThreadData->tTemp.pCurrent && pThreadData->tTemp.pCurrent->pNext ) {
		xrtTempArenaBlock* pNext = pThreadData->tTemp.pCurrent->pNext;
		while ( pNext ) {
			if ( ((size_t)pNext->iUsed + iNeed) <= pNext->iCapacity ) {
				pThreadData->tTemp.pCurrent = pNext;
				return TRUE;
			}
			pNext = pNext->pNext;
		}
	}
	// 3. 若没有当前区块但有区块链表，从头开始查找
	if ( pThreadData->tTemp.pCurrent == NULL && pThreadData->tTemp.pBlocks ) {
		xrtTempArenaBlock* pNext = pThreadData->tTemp.pBlocks;
		while ( pNext ) {
			if ( ((size_t)pNext->iUsed + iNeed) <= pNext->iCapacity ) {
				pThreadData->tTemp.pCurrent = pNext;
				return TRUE;
			}
			pNext = pNext->pNext;
		}
	}
	// 4. 所有现有区块均不足，分配新区块
	iCapacity = pThreadData->tTemp.iBlockSize ? pThreadData->tTemp.iBlockSize : XRT_TEMP_ARENA_BLOCK_SIZE;
	if ( iNeed > iCapacity ) {
		iCapacity = iNeed;
	}
	pBlock = __xrtTempArenaAllocBlock(iCapacity);
	if ( pBlock == NULL ) {
		return FALSE;
	}
	// 5. 将新区块挂入链表
	if ( pThreadData->tTemp.pBlocks == NULL ) {
		pThreadData->tTemp.pBlocks = pBlock;
	} else if ( pThreadData->tTemp.pCurrent ) {
		pThreadData->tTemp.pCurrent->pNext = pBlock;
	} else {
		xrtTempArenaBlock* pTail = pThreadData->tTemp.pBlocks;
		while ( pTail->pNext ) {
			pTail = pTail->pNext;
		}
		pTail->pNext = pBlock;
	}
	pThreadData->tTemp.pCurrent = pBlock;
	return TRUE;
}


// 内部函数：重置临时内存区线程
static inline void __xrtTempArenaResetThread(xrtThreadData* pThreadData)
{
	xrtTempArenaBlock* pBlock;
	xrtTempArenaBlock* pSpill;

	if ( pThreadData == NULL ) {
		return;
	}
	// 1. 记录调试信息
	__xrtTempArenaDebugOnReset(pThreadData);
	// 2. 重置所有常规区块的已用字节数（ 不释放内存 ）
	for ( pBlock = pThreadData->tTemp.pBlocks; pBlock; pBlock = pBlock->pNext ) {
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pBlock), 0xE7, pBlock->iCapacity);
		#endif
		pBlock->iUsed = 0;
	}
	// 3. 释放所有溢出区块（ 超大临时分配 ）
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pSpill), 0xE7, pSpill->iCapacity);
		#endif
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	// 4. 恢复初始状态
	pThreadData->tTemp.pCurrent = pThreadData->tTemp.pBlocks;
	pThreadData->tTemp.pSpill = NULL;
	pThreadData->tTemp.iCurrentBytes = 0;
	pThreadData->tTemp.iResetCount++;
}


// 内部函数：释放临时内存区全部线程
static inline void __xrtTempArenaFreeAllThread(xrtThreadData* pThreadData)
{
	xrtTempArenaBlock* pBlock;
	xrtTempArenaBlock* pSpill;

	if ( pThreadData == NULL ) {
		return;
	}
	__xrtTempArenaDebugOnReset(pThreadData);
	// 释放所有常规区块
	pBlock = pThreadData->tTemp.pBlocks;
	while ( pBlock ) {
		xrtTempArenaBlock* pNext = pBlock->pNext;
		__xrtMemGlobalProcFree()(pBlock);
		pBlock = pNext;
	}
	// 释放所有溢出区块
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	// 重置状态为初始值
	memset(&pThreadData->tTemp, 0, sizeof(xrtTempArenaState));
	pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
}


// 内部函数：释放已经脱离线程的临时内存区状态
static inline void __xrtTempArenaFreeDetached(xrtTempArenaState* pState)
{
	xrtThreadData tDetached;

	if ( pState == NULL ) {
		return;
	}
	memset(&tDetached, 0, sizeof(tDetached));
	tDetached.tTemp = *pState;
	__xrtTempArenaFreeAllThread(&tDetached);
	*pState = tDetached.tTemp;
}



// 申请无需主动释放的临时内存（线程级）
XXAPI ptr xrtTempMemory(size_t iSize)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	size_t iNeed;
	bool bSpill;
	xrtTempArenaBlock* pBlock;
	ptr pRet;

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return NULL;
	}

	// 申请内存
	if ( pThreadData->tTemp.iBlockSize == 0 ) {
		pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	}
	if ( pThreadData->tTemp.iSpillCutoff == 0 ) {
		pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
	}
	iNeed = __xrtMemGlobalAlignSize(iSize ? iSize : 1);
	bSpill = iNeed > pThreadData->tTemp.iSpillCutoff;
	if ( bSpill ) {
		pBlock = __xrtTempArenaAllocBlock(iNeed);
		if ( pBlock == NULL ) {
			xrtSetError("temporary memory allocate failed.", FALSE);
			return NULL;
		}
		pBlock->iUsed = (uint32)iNeed;
		pBlock->iFlags = 1u;
		pBlock->pNext = pThreadData->tTemp.pSpill;
		pThreadData->tTemp.pSpill = pBlock;
		pRet = __xrtTempArenaBlockUser(pBlock);
	} else {
		if ( !__xrtTempArenaEnsureCurrent(pThreadData, iNeed) ) {
			xrtSetError("temporary memory allocate failed.", FALSE);
			return NULL;
		}
		pBlock = pThreadData->tTemp.pCurrent;
		pRet = (ptr)((char*)__xrtTempArenaBlockUser(pBlock) + pBlock->iUsed);
		pBlock->iUsed += (uint32)iNeed;
	}

	__xrtMemTelemetryRecordTemp(iSize);
	__xrtTempArenaDebugOnAlloc(pThreadData, iNeed, bSpill);

	return pRet;
}



// 释放当前线程的所有临时内存
XXAPI void xrtFreeTempMemory()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		return;
	}
	if ( pThreadData->tTemp.iScopeDepth != 0 ) {
		xrtSetError("cannot reset temporary memory while a temp scope is active.", FALSE);
		return;
	}

	__xrtTempArenaResetThread(pThreadData);
}



// 建立可嵌套的临时内存作用域
XXAPI xrtTempScope xrtTempScopeBegin()
{
	xrtTempScope tScope;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	memset(&tScope, 0, sizeof(tScope));
	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return tScope;
	}
	tScope.pThreadData = pThreadData;
	tScope.pCurrent = pThreadData->tTemp.pCurrent;
	tScope.pSpill = pThreadData->tTemp.pSpill;
	tScope.iUsed = tScope.pCurrent ? tScope.pCurrent->iUsed : 0;
	tScope.iCurrentBytes = pThreadData->tTemp.iCurrentBytes;
	tScope.iDepth = ++pThreadData->tTemp.iScopeDepth;
	tScope.bActive = TRUE;
	return tScope;
}



// 内部函数：结束临时内存作用域
static inline bool __xrtTempScopeEndInternal(xrtTempScope* pScope)
{
	xrtThreadData* pThreadData;
	xrtTempArenaBlock* pBlock;
	xrtTempArenaBlock* pSpill;

	if ( pScope == NULL || !pScope->bActive ) {
		return TRUE;
	}
	pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL || pThreadData != pScope->pThreadData ||
		 pThreadData->tTemp.iScopeDepth != pScope->iDepth ) {
		xrtSetError("temporary memory scopes must end on the owning thread in LIFO order.", FALSE);
		return FALSE;
	}

	// 溢出块采用头插法，只释放本作用域新增的部分。
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill != pScope->pSpill ) {
		xrtTempArenaBlock* pNext;
		if ( pSpill == NULL ) {
			xrtSetError("temporary memory spill chain is corrupted.", FALSE);
			return FALSE;
		}
		pNext = pSpill->pNext;
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	pThreadData->tTemp.pSpill = pScope->pSpill;

	// 常规区块保留给后续复用，只回退已用位置。
	if ( pScope->pCurrent != NULL ) {
		pScope->pCurrent->iUsed = pScope->iUsed;
		pBlock = pScope->pCurrent->pNext;
	} else {
		pBlock = pThreadData->tTemp.pBlocks;
	}
	while ( pBlock ) {
		pBlock->iUsed = 0;
		pBlock = pBlock->pNext;
	}
	__xrtTempArenaDebugOnRestore(pThreadData, pScope->iCurrentBytes);
	pThreadData->tTemp.pCurrent = pScope->pCurrent != NULL ? pScope->pCurrent : pThreadData->tTemp.pBlocks;
	pThreadData->tTemp.iCurrentBytes = pScope->iCurrentBytes;
	pThreadData->tTemp.iScopeDepth--;
	pScope->bActive = FALSE;
	return TRUE;
}



// 结束临时内存作用域，仅回收该作用域内的分配
XXAPI void xrtTempScopeEnd(xrtTempScope* pScope)
{
	(void)__xrtTempScopeEndInternal(pScope);
}



// 将需要 xrtFree 的字符串移入当前临时内存区
XXAPI str xrtTempStringTake(str sText, size_t iSize)
{
	str sRet;
	if ( sText == NULL ) {
		return xCore.sNull;
	}
	if ( iSize == 0 ) {
		iSize = strlen((const char*)sText);
	}
	if ( iSize == 0 ) {
		xrtFree(sText);
		return xCore.sNull;
	}
	sRet = (str)xrtTempMemory(iSize + 1);
	if ( sRet == NULL ) {
		xrtFree(sText);
		return xCore.sNull;
	}
	memcpy(sRet, sText, iSize);
	sRet[iSize] = 0;
	xrtFree(sText);
	return sRet;
}



// 复制字符串到父作用域并结束当前作用域
XXAPI str xrtTempScopeEndString(xrtTempScope* pScope, str sText, size_t iSize)
{
	str sOwned;
	if ( sText == NULL ) {
		xrtTempScopeEnd(pScope);
		return xCore.sNull;
	}
	if ( iSize == 0 ) {
		iSize = strlen((const char*)sText);
	}
	if ( iSize == 0 ) {
		xrtTempScopeEnd(pScope);
		return xCore.sNull;
	}
	sOwned = (str)xrtMalloc(iSize + 1);
	if ( sOwned == NULL ) {
		xrtTempScopeEnd(pScope);
		return xCore.sNull;
	}
	memcpy(sOwned, sText, iSize);
	sOwned[iSize] = 0;
	if ( !__xrtTempScopeEndInternal(pScope) ) {
		xrtFree(sOwned);
		return xCore.sNull;
	}
	return xrtTempStringTake(sOwned, iSize);
}



// 设置错误（线程级）
XXAPI void xrtSetError(const void* sError, bool bFree)
{
	str sErrorText = (str)sError;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	// 回调通知
	if ( xCore.OnError ) {
		xCore.OnError(sErrorText);
	}

	if ( pThreadData == NULL ) {
		return;
	}

	// 释放旧的错误信息
	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = sErrorText;
	pThreadData->bFreeLastError = bFree;
}


// 设置错误 u 16
XXAPI void xrtSetErrorU16(u16str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF16to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}


// 设置错误 u 32
XXAPI void xrtSetErrorU32(u32str sError, size_t iSize, bool bFree)
{
	str sErrorU8 = xrtUTF32to8(sError, iSize, NULL);
	if ( bFree ) {
		xrtFree(sError);
	}
	xrtSetError(sErrorU8, TRUE);
}



// 清除当前线程错误
XXAPI void xrtClearError()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		return;
	}

	if ( pThreadData->bFreeLastError && pThreadData->LastError && pThreadData->LastError != xCore.sNull ) {
		xrtFree(pThreadData->LastError);
	}
	pThreadData->LastError = xCore.sNull;
	pThreadData->bFreeLastError = FALSE;
}


// 内部函数：内存遥测乘法钳制相关处理
static inline size_t __xrtMemTelemetryMulClamp(size_t iNum, size_t iSize)
{
	if ( iNum == 0 || iSize == 0 ) {
		return 0;
	}
	if ( iNum > (SIZE_MAX / iSize) ) {
		return SIZE_MAX;
	}
	return iNum * iSize;
}


// 内部函数：获取内存遥测分类 index
static inline uint32 __xrtMemTelemetryClassIndex(size_t iSize)
{
	if ( xCore.MemGlobal.iClassCount == XRT_MEMPOOL_CLASS_COUNT_DEFAULT && iSize <= xCore.MemGlobal.iCutoff ) {
		if ( iSize == 0 ) {
			return 0;
		}
		return xCore.MemGlobal.arrSizeClassLut[iSize];
	}
	if ( iSize <= 1 ) {
		return 0;
	}
	if ( iSize >= XRT_MEMPOOL_CUTOFF_DEFAULT ) {
		return XRT_MEMPOOL_CLASS_COUNT_DEFAULT - 1;
	}
	return (uint32)(((iSize + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE) - 1);
}


// 内部函数：内存遥测记录分配操作
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	uint32 iIdx;

	if ( pState->bEnabled == 0 ) {
		return;
	}

	// 按操作类型累计调用次数和字节数
	switch ( iOp ) {
		case __XRT_MEMTELEMETRY_OP_MALLOC:
			__xrtAtomicAddFetch64(&pState->iMallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iMallocBytes, (int64)iSize);
			break;
		case __XRT_MEMTELEMETRY_OP_CALLOC:
			__xrtAtomicAddFetch64(&pState->iCallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iCallocBytes, (int64)iSize);
			break;
		case __XRT_MEMTELEMETRY_OP_REALLOC:
			__xrtAtomicAddFetch64(&pState->iReallocCalls, 1);
			__xrtAtomicAddFetch64(&pState->iReallocBytes, (int64)iSize);
			break;
		default:
			return;
	}

	// 区分池化候选和回退通道统计
	if ( iSize <= XRT_MEMPOOL_CUTOFF_DEFAULT ) {
		iIdx = __xrtMemTelemetryClassIndex(iSize);
		__xrtAtomicAddFetch64(&pState->iPooledCandidateCalls, 1);
		__xrtAtomicAddFetch64(&pState->iPooledCandidateBytes, (int64)iSize);
		__xrtAtomicAddFetch64(&pState->arrClassCalls[iIdx], 1);
		__xrtAtomicAddFetch64(&pState->arrClassBytes[iIdx], (int64)iSize);
	} else {
		__xrtAtomicAddFetch64(&pState->iFallbackCalls, 1);
		__xrtAtomicAddFetch64(&pState->iFallbackBytes, (int64)iSize);
	}
}


// 内部函数：释放内存遥测记录
static inline void __xrtMemTelemetryRecordFree()
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;

	if ( pState->bEnabled == 0 ) {
		return;
	}

	__xrtAtomicAddFetch64(&pState->iFreeCalls, 1);
}


// 内部函数：内存遥测记录临时相关处理
static inline void __xrtMemTelemetryRecordTemp(size_t iSize)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;

	if ( pState->bEnabled == 0 ) {
		return;
	}

	// temp arena 单独统计，不折算进 malloc/free/pooled 候选通道。
	__xrtAtomicAddFetch64(&pState->iTempCalls, 1);
	__xrtAtomicAddFetch64(&pState->iTempBytes, (int64)iSize);
}
