


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
	return __xrtMallocSite(iSize, NULL, 0);
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
	return __xrtCallocSite(iNum, iSize, NULL, 0);
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
	return __xrtReallocSite(pMem, iSize, NULL, 0);
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
	__xrtFreeSite(pmem, NULL, 0);
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
	xrtTempArenaBlock* pBlock;

	if ( iCapacity == 0 ) {
		iCapacity = 1;
	}
	iTotal = __xrtTempArenaBlockHeaderSize() + iCapacity;
	pBlock = (xrtTempArenaBlock*)__xrtMemGlobalProcMalloc()(iTotal);
	if ( pBlock == NULL ) {
		return NULL;
	}
	memset(pBlock, 0, __xrtTempArenaBlockHeaderSize());
	pBlock->iCapacity = (uint32)iCapacity;
	return pBlock;
}


// 内部函数：记录临时内存区分配调试信息
static inline void __xrtTempArenaDebugOnAlloc(xrtThreadData* pThreadData, size_t iSize, bool bSpill)
{
	#ifdef XRT_MEM_DEBUG
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugLock();
		pThreadData->tTemp.iCurrentBytes += iSize;
		if ( pThreadData->tTemp.iCurrentBytes > pThreadData->tTemp.iPeakBytes ) {
			pThreadData->tTemp.iPeakBytes = pThreadData->tTemp.iCurrentBytes;
		}
		xCore.MemDebug.iTempCurrentBytes += iSize;
		if ( xCore.MemDebug.iTempCurrentBytes > xCore.MemDebug.iTempPeakBytes ) {
			xCore.MemDebug.iTempPeakBytes = xCore.MemDebug.iTempCurrentBytes;
		}
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_ALLOC, NULL, iSize, bSpill ? XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL : XRT_MEMDEBUG_ALLOCATOR_GLOBAL, NULL, 0);
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
		if ( pThreadData == NULL || !__xrtMemDebugEnabled() ) {
			return;
		}
		__xrtMemDebugLock();
		if ( xCore.MemDebug.iTempCurrentBytes >= pThreadData->tTemp.iCurrentBytes ) {
			xCore.MemDebug.iTempCurrentBytes -= pThreadData->tTemp.iCurrentBytes;
		} else {
			xCore.MemDebug.iTempCurrentBytes = 0;
		}
		xCore.MemDebug.iTempResetCount++;
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_TEMP_RESET, NULL, pThreadData->tTemp.iCurrentBytes, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, NULL, 0);
		__xrtMemDebugUnlock();
	#else
		(void)pThreadData;
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
	if ( pThreadData->tTemp.pCurrent && ((size_t)pThreadData->tTemp.pCurrent->iUsed + iNeed) <= pThreadData->tTemp.pCurrent->iCapacity ) {
		return TRUE;
	}
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
	iCapacity = pThreadData->tTemp.iBlockSize ? pThreadData->tTemp.iBlockSize : XRT_TEMP_ARENA_BLOCK_SIZE;
	if ( iNeed > iCapacity ) {
		iCapacity = iNeed;
	}
	pBlock = __xrtTempArenaAllocBlock(iCapacity);
	if ( pBlock == NULL ) {
		return FALSE;
	}
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
	__xrtTempArenaDebugOnReset(pThreadData);
	for ( pBlock = pThreadData->tTemp.pBlocks; pBlock; pBlock = pBlock->pNext ) {
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pBlock), 0xE7, pBlock->iCapacity);
		#endif
		pBlock->iUsed = 0;
	}
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		#ifdef XRT_MEM_DEBUG
			memset(__xrtTempArenaBlockUser(pSpill), 0xE7, pSpill->iCapacity);
		#endif
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
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
	pBlock = pThreadData->tTemp.pBlocks;
	while ( pBlock ) {
		xrtTempArenaBlock* pNext = pBlock->pNext;
		__xrtMemGlobalProcFree()(pBlock);
		pBlock = pNext;
	}
	pSpill = pThreadData->tTemp.pSpill;
	while ( pSpill ) {
		xrtTempArenaBlock* pNext = pSpill->pNext;
		__xrtMemGlobalProcFree()(pSpill);
		pSpill = pNext;
	}
	memset(&pThreadData->tTemp, 0, sizeof(xrtTempArenaState));
	pThreadData->tTemp.iBlockSize = XRT_TEMP_ARENA_BLOCK_SIZE;
	pThreadData->tTemp.iSpillCutoff = XRT_TEMP_ARENA_SPILL_CUTOFF;
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

	__xrtTempArenaResetThread(pThreadData);
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
	if ( iNum > ((size_t)-1) / iSize ) {
		return (size_t)-1;
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


// 内部函数：__xrtMemTelemetryRecordSizedOp
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize)
{
	xrtMemTelemetryState* pState = &xCore.MemTelemetry;
	uint32 iIdx;

	if ( pState->bEnabled == 0 ) {
		return;
	}

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
#define __XRT_MEMTELEMETRY_OP_MALLOC 1
#define __XRT_MEMTELEMETRY_OP_CALLOC 2
#define __XRT_MEMTELEMETRY_OP_REALLOC 3

static inline size_t __xrtMemTelemetryMulClamp(size_t iNum, size_t iSize);
static inline uint32 __xrtMemTelemetryClassIndex(size_t iSize);
static inline void __xrtMemTelemetryRecordSizedOp(uint32 iOp, size_t iSize);
static inline void __xrtMemTelemetryRecordFree();
static inline void __xrtMemTelemetryRecordTemp(size_t iSize);
