


// 内部函数：获取内存全局 align 大小
static inline size_t __xrtMemGlobalAlignSize(size_t iSize)
{
	size_t iAlign = sizeof(ptr);
	return (iSize + (iAlign - 1)) & ~(iAlign - 1);
}


// 内部函数：获取内存全局头部大小
static inline size_t __xrtMemGlobalHeaderSize()
{
	return __xrtMemGlobalAlignSize(sizeof(xrtMemBlockHeader));
}


// 指针相关处理
static inline ptr (*__xrtMemGlobalProcMalloc())(size_t)
{
	return xCore.malloc ? xCore.malloc : malloc;
}


// 指针相关处理
static inline ptr (*__xrtMemGlobalProcCalloc())(size_t, size_t)
{
	return xCore.calloc ? xCore.calloc : calloc;
}


// 指针相关处理
static inline ptr (*__xrtMemGlobalProcRealloc())(ptr, size_t)
{
	return xCore.realloc ? xCore.realloc : realloc;
}


// void 相关处理
static inline void (*__xrtMemGlobalProcFree())(ptr)
{
	return xCore.free ? xCore.free : free;
}


// 内部函数：锁定内存全局
static inline void __xrtMemGlobalLock(volatile long* pLock)
{
	uint32 iSpin = 0;
	while ( __xrtAtomicCompareExchange32(pLock, 1, 0) != 0 ) {
		iSpin++;
		if ( (iSpin & 0x3FFu) == 0u ) {
			#if defined(_WIN32) || defined(_WIN64)
				SwitchToThread();
			#else
				sched_yield();
			#endif
		}
	}
}


// 内部函数：解锁内存全局
static inline void __xrtMemGlobalUnlock(volatile long* pLock)
{
	__xrtAtomicExchange32(pLock, 0);
}


// 内部函数：初始化内存全局计划
static inline void __xrtMemGlobalInitPlan(xrtMemGlobalPool* pPool)
{
	uint32 i;

	if ( pPool == NULL ) {
		return;
	}

	memset(pPool, 0, sizeof(xrtMemGlobalPool));
	pPool->iClassCount = XRT_MEMPOOL_CLASS_COUNT_DEFAULT;
	pPool->iClassStep = XRT_MEMPOOL_STEP_SIZE;
	pPool->iCutoff = XRT_MEMPOOL_CUTOFF_DEFAULT;

	for ( i = 0; i < XRT_MEMPOOL_CLASS_COUNT_DEFAULT; i++ ) {
		pPool->arrClassDesc[i].iBlockSize = (uint16)((i + 1) * XRT_MEMPOOL_STEP_SIZE);
	}

	for ( i = 0; i <= XRT_MEMPOOL_CUTOFF_DEFAULT; i++ ) {
		if ( i == 0 ) {
			pPool->arrSizeClassLut[i] = 0;
		} else {
			pPool->arrSizeClassLut[i] = (uint8)(((i + (XRT_MEMPOOL_STEP_SIZE - 1)) / XRT_MEMPOOL_STEP_SIZE) - 1);
		}
	}
}


// 内部函数：确保内存全局计划
static inline void __xrtMemGlobalEnsurePlan()
{
	if ( xCore.MemGlobal.iClassCount == 0 ) {
		__xrtMemGlobalInitPlan(&xCore.MemGlobal);
	}
}


// 内部函数：获取内存全局分类 index
static inline uint32 __xrtMemGlobalClassIndex(const xrtMemGlobalPool* pPool, size_t iSize)
{
	if ( pPool == NULL || iSize == 0 || iSize > pPool->iCutoff ) {
		return (uint32)-1;
	}
	return pPool->arrSizeClassLut[iSize];
}


// 内部函数：获取内存全局分类块大小
static inline uint32 __xrtMemGlobalClassBlockSize(const xrtMemGlobalPool* pPool, uint32 iClass)
{
	if ( pPool == NULL || iClass >= pPool->iClassCount ) {
		return 0;
	}
	return pPool->arrClassDesc[iClass].iBlockSize;
}


// 内部函数：获取内存全局调试 tail 大小
static inline size_t __xrtMemGlobalDebugTailSize()
{
	#ifdef XRT_MEM_DEBUG
		return sizeof(uint32);
	#else
		return 0;
	#endif
}


// 内部函数：分配内存全局 payload 大小
static inline size_t __xrtMemGlobalAllocPayloadSize(size_t iRequestSize)
{
	size_t iPayload = iRequestSize ? iRequestSize : 1;
	return iPayload + __xrtMemGlobalDebugTailSize();
}


// 内部函数：__xrtMemGlobalClassIndexForRequest
static inline uint32 __xrtMemGlobalClassIndexForRequest(const xrtMemGlobalPool* pPool, size_t iRequestSize)
{
	return __xrtMemGlobalClassIndex(pPool, __xrtMemGlobalAllocPayloadSize(iRequestSize));
}


// 内部函数：内存调试 now 毫秒相关处理
static inline uint64 __xrtMemDebugNowMs()
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec timer;
		clock_gettime(CLOCK_MONOTONIC, &timer);
		return ((uint64)timer.tv_sec * 1000ULL) + (uint64)(timer.tv_nsec / 1000000ULL);
	#endif
}


// 内部函数：__xrtMemGlobalHeaderFromUser
static inline xrtMemBlockHeader* __xrtMemGlobalHeaderFromUser(ptr pUser)
{
	if ( pUser == NULL ) {
		return NULL;
	}
	return (xrtMemBlockHeader*)((char*)pUser - __xrtMemGlobalHeaderSize());
}


// 内部函数：获取内存全局 user from 头部
static inline ptr __xrtMemGlobalUserFromHeader(xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return NULL;
	}
	return (ptr)((char*)pHeader + __xrtMemGlobalHeaderSize());
}


// 内部函数：判断全局内存头部是否有效
static inline bool __xrtMemGlobalHeaderValid(const xrtMemBlockHeader* pHeader)
{
	return pHeader != NULL && pHeader->iMagic == XRT_MEMBLOCK_MAGIC;
}


// 内部函数：写入内存全局头部
static inline void __xrtMemGlobalWriteHeader(xrtMemBlockHeader* pHeader, uint32 iClassIndex, uint16 iFlags, uint32 iRequestSize)
{
	pHeader->iMagic = XRT_MEMBLOCK_MAGIC;
	pHeader->iClassIndex = (uint16)iClassIndex;
	pHeader->iFlags = iFlags;
	pHeader->iRequestSize = iRequestSize;
	pHeader->iReserved = 0;
	#ifdef XRT_MEM_DEBUG
		pHeader->sAllocFile = NULL;
		pHeader->iAllocLine = 0;
		pHeader->sFreeFile = NULL;
		pHeader->iFreeLine = 0;
		pHeader->iAllocThreadId = 0;
		pHeader->iAllocSeq = 0;
		pHeader->iAllocTimeMs = 0;
		pHeader->iFreeTimeMs = 0;
		pHeader->pDebugPrev = NULL;
		pHeader->pDebugNext = NULL;
		pHeader->iFrontCanary = XRT_MEMDEBUG_CANARY_HEAD ^ (uint32)(uintptr_t)pHeader;
		pHeader->iDebugState = 0;
	#endif
}

#ifdef XRT_MEM_DEBUG

// 内部函数：获取内存全局 tail canary 值
static inline uint32 __xrtMemGlobalTailCanaryValue(const xrtMemBlockHeader* pHeader)
{
	return XRT_MEMDEBUG_CANARY_TAIL ^ (uint32)pHeader->iRequestSize ^ 0xA5A5A5A5u;
}


// 内部函数：__xrtMemGlobalTailCanaryPtr
static inline uint32* __xrtMemGlobalTailCanaryPtr(xrtMemBlockHeader* pHeader)
{
	size_t iPayload = pHeader->iRequestSize ? pHeader->iRequestSize : 1;
	return (uint32*)((char*)__xrtMemGlobalUserFromHeader(pHeader) + iPayload);
}


// 内部函数：__xrtMemGlobalWriteTailCanary
static inline void __xrtMemGlobalWriteTailCanary(xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return;
	}
	*__xrtMemGlobalTailCanaryPtr(pHeader) = __xrtMemGlobalTailCanaryValue(pHeader);
}


// 内部函数：__xrtMemGlobalCheckFrontCanary
static inline bool __xrtMemGlobalCheckFrontCanary(const xrtMemBlockHeader* pHeader)
{
	if ( pHeader == NULL ) {
		return FALSE;
	}
	return pHeader->iFrontCanary == (XRT_MEMDEBUG_CANARY_HEAD ^ (uint32)(uintptr_t)pHeader);
}


// 内部函数：__xrtMemGlobalCheckTailCanary
static inline bool __xrtMemGlobalCheckTailCanary(const xrtMemBlockHeader* pHeader)
{
	uint32* pTail;
	if ( pHeader == NULL ) {
		return FALSE;
	}
	pTail = __xrtMemGlobalTailCanaryPtr((xrtMemBlockHeader*)pHeader);
	return *pTail == __xrtMemGlobalTailCanaryValue(pHeader);
}


// 内部函数：__xrtMemDebugEnabled
static inline bool __xrtMemDebugEnabled()
{
	return xCore.MemDebug.bEnabled != 0;
}

static inline const char* __xrtMemDebugAllocatorName(uint32 iAllocatorKind);


// 内部函数：__xrtMemDebugImmediateObjectTypeName
static inline const char* __xrtMemDebugImmediateObjectTypeName(uint32 iObjectType)
{
	switch ( iObjectType ) {
		case XRT_MEMDEBUG_OBJECT_ARRAY:
			return "array";
		case XRT_MEMDEBUG_OBJECT_DICT:
			return "dict";
		case XRT_MEMDEBUG_OBJECT_LIST:
			return "list";
		case XRT_MEMDEBUG_OBJECT_AVLTREE:
			return "avltree";
		case XRT_MEMDEBUG_OBJECT_DYNSTACK:
			return "dynstack";
		case XRT_MEMDEBUG_OBJECT_MEMPOOL:
			return "mempool";
		case XRT_MEMDEBUG_OBJECT_FSMEMPOOL:
			return "fsmempool";
		default:
			return "unknown";
	}
}


// 内部函数：获取内存调试 immediate 事件名称
static inline const char* __xrtMemDebugImmediateEventName(uint32 iType)
{
	switch ( iType ) {
		case XRT_MEMDEBUG_EVENT_DOUBLE_FREE:
			return "DOUBLE_FREE";
		case XRT_MEMDEBUG_EVENT_INVALID_FREE:
			return "INVALID_FREE";
		case XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE:
			return "WRONG_ALLOCATOR_FREE";
		case XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT:
			return "BUFFER_OVERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT:
			return "BUFFER_UNDERFLOW_SUSPECT";
		case XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT:
			return "USE_AFTER_FREE_SUSPECT";
		case XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY:
			return "OBJECT_DOUBLE_DESTROY";
		default:
			return "UNKNOWN";
	}
}


// 内部函数：__xrtMemDebugShouldEmitImmediate
static inline bool __xrtMemDebugShouldEmitImmediate(uint32 iType)
{
	return iType == XRT_MEMDEBUG_EVENT_DOUBLE_FREE
		|| iType == XRT_MEMDEBUG_EVENT_INVALID_FREE
		|| iType == XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE
		|| iType == XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT
		|| iType == XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY;
}


// 内部函数：__xrtMemDebugEmitConsoleLine
static inline void __xrtMemDebugEmitConsoleLine(const char* sText, size_t iLen)
{
	if ( sText == NULL || iLen == 0 ) {
		return;
	}

	#if defined(_WIN32) || defined(_WIN64)
		{
			HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
			DWORD iWritten = 0;
			if ( hErr && hErr != INVALID_HANDLE_VALUE ) {
				WriteFile(hErr, sText, (DWORD)iLen, &iWritten, NULL);
			}
			OutputDebugStringA(sText);
		}
	#else
		(void)write(2, sText, iLen);
	#endif
}


// 内部函数：__xrtMemDebugEmitImmediateNoLock
static inline void __xrtMemDebugEmitImmediateNoLock(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	char sText[640];
	const char* sKind;
	int iWritten;

	if ( !__xrtMemDebugShouldEmitImmediate(iType) ) {
		return;
	}

	if ( iType == XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY ) {
		sKind = __xrtMemDebugImmediateObjectTypeName(iAllocatorKind);
	} else {
		sKind = __xrtMemDebugAllocatorName(iAllocatorKind);
	}

	iWritten = snprintf(
		sText,
		sizeof(sText),
		"[XRT_MEM_DEBUG][%s] time_ms=%llu ptr=%p size=%llu kind=%s site=%s:%u thread=%llu\n",
		__xrtMemDebugImmediateEventName(iType),
		(unsigned long long)__xrtMemDebugNowMs(),
		pAddress,
		(unsigned long long)iSize,
		sKind ? sKind : "unknown",
		sFile ? sFile : "(unknown)",
		(unsigned int)iLine,
		(unsigned long long)xrtThreadGetCurrentId()
	);
	if ( iWritten <= 0 ) {
		return;
	}
	if ( (size_t)iWritten >= sizeof(sText) ) {
		iWritten = (int)(sizeof(sText) - 1);
	}
	__xrtMemDebugEmitConsoleLine(sText, (size_t)iWritten);
}


// 内部函数：锁定内存调试
static inline void __xrtMemDebugLock()
{
	__xrtMemGlobalLock(&xCore.MemDebug.iLock);
}


// 内部函数：解锁内存调试
static inline void __xrtMemDebugUnlock()
{
	__xrtMemGlobalUnlock(&xCore.MemDebug.iLock);
}


// 内部函数：获取内存调试 allocator 名称
static inline const char* __xrtMemDebugAllocatorName(uint32 iAllocatorKind)
{
	switch ( iAllocatorKind ) {
		case XRT_MEMDEBUG_ALLOCATOR_GLOBAL:
			return "global";
		case XRT_MEMDEBUG_ALLOCATOR_MEMPOOL:
			return "mempool";
		case XRT_MEMDEBUG_ALLOCATOR_FSMEMPOOL:
			return "fsmempool";
		default:
			return "unknown";
	}
}


// 内部函数：__xrtMemDebugFindSiteStatNoLock
static inline xrtMemDebugSiteStat* __xrtMemDebugFindSiteStatNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind)
{
	xrtMemDebugSiteStat* pNode = xCore.MemDebug.pSiteStats;
	while ( pNode ) {
		if ( pNode->sFile == sFile && pNode->iLine == iLine && pNode->iAllocatorKind == iAllocatorKind ) {
			return pNode;
		}
		pNode = pNode->pNext;
	}
	return NULL;
}


// 内部函数：__xrtMemDebugEnsureSiteStatNoLock
static inline xrtMemDebugSiteStat* __xrtMemDebugEnsureSiteStatNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind)
{
	xrtMemDebugSiteStat* pNode = __xrtMemDebugFindSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pNode ) {
		return pNode;
	}
	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugSiteStat));
	if ( pNode == NULL ) {
		return NULL;
	}
	pNode->sFile = sFile;
	pNode->iLine = iLine;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->pNext = xCore.MemDebug.pSiteStats;
	xCore.MemDebug.pSiteStats = pNode;
	return pNode;
}


// 内部函数：__xrtMemDebugSiteOnAllocNoLock
static inline void __xrtMemDebugSiteOnAllocNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind, size_t iSize)
{
	xrtMemDebugSiteStat* pSite = __xrtMemDebugEnsureSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pSite == NULL ) {
		return;
	}
	pSite->iAllocCount++;
	pSite->iAllocBytes += iSize;
	pSite->iLiveCount++;
	pSite->iLiveBytes += iSize;
	if ( pSite->iLiveCount > pSite->iPeakLiveCount ) {
		pSite->iPeakLiveCount = pSite->iLiveCount;
	}
	if ( pSite->iLiveBytes > pSite->iPeakLiveBytes ) {
		pSite->iPeakLiveBytes = pSite->iLiveBytes;
	}
}


// 内部函数：__xrtMemDebugSiteOnFreeNoLock
static inline void __xrtMemDebugSiteOnFreeNoLock(const char* sFile, uint32 iLine, uint32 iAllocatorKind, size_t iSize)
{
	xrtMemDebugSiteStat* pSite = __xrtMemDebugEnsureSiteStatNoLock(sFile, iLine, iAllocatorKind);
	if ( pSite == NULL ) {
		return;
	}
	pSite->iFreeCount++;
	pSite->iFreeBytes += iSize;
	if ( pSite->iLiveCount > 0 ) {
		pSite->iLiveCount--;
	}
	if ( pSite->iLiveBytes >= iSize ) {
		pSite->iLiveBytes -= iSize;
	} else {
		pSite->iLiveBytes = 0;
	}
}


// 内部函数：锁定内存调试记录事件 no
static inline void __xrtMemDebugRecordEventNoLock(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugEvent* pEvent = &xCore.MemDebug.arrEvents[xCore.MemDebug.iEventCursor];
	memset(pEvent, 0, sizeof(xrtMemDebugEvent));
	pEvent->iType = iType;
	pEvent->iLine = iLine;
	pEvent->iAllocatorKind = iAllocatorKind;
	pEvent->iThreadId = xrtThreadGetCurrentId();
	pEvent->iTimeMs = __xrtMemDebugNowMs();
	pEvent->iSize = (uint64)iSize;
	pEvent->pAddress = pAddress;
	pEvent->sFile = sFile;
	xCore.MemDebug.iEventCursor = (xCore.MemDebug.iEventCursor + 1) % XRT_MEMDEBUG_EVENT_CAPACITY;
	if ( xCore.MemDebug.iEventCount < XRT_MEMDEBUG_EVENT_CAPACITY ) {
		xCore.MemDebug.iEventCount++;
	}
}


// 内部函数：__xrtMemDebugAttachLiveNoLock
static inline void __xrtMemDebugAttachLiveNoLock(xrtMemBlockHeader* pHeader)
{
	pHeader->pDebugPrev = xCore.MemDebug.pLiveTail;
	pHeader->pDebugNext = NULL;
	if ( xCore.MemDebug.pLiveTail ) {
		xCore.MemDebug.pLiveTail->pDebugNext = pHeader;
	} else {
		xCore.MemDebug.pLiveHead = pHeader;
	}
	xCore.MemDebug.pLiveTail = pHeader;
	xCore.MemDebug.iLiveAllocCount++;
	xCore.MemDebug.iLiveAllocBytes += pHeader->iRequestSize;
	if ( xCore.MemDebug.iLiveAllocCount > xCore.MemDebug.iPeakLiveAllocCount ) {
		xCore.MemDebug.iPeakLiveAllocCount = xCore.MemDebug.iLiveAllocCount;
	}
	if ( xCore.MemDebug.iLiveAllocBytes > xCore.MemDebug.iPeakLiveAllocBytes ) {
		xCore.MemDebug.iPeakLiveAllocBytes = xCore.MemDebug.iLiveAllocBytes;
	}
}


// 内部函数：__xrtMemDebugDetachLiveNoLock
static inline void __xrtMemDebugDetachLiveNoLock(xrtMemBlockHeader* pHeader)
{
	if ( pHeader->pDebugPrev ) {
		pHeader->pDebugPrev->pDebugNext = pHeader->pDebugNext;
	} else {
		xCore.MemDebug.pLiveHead = pHeader->pDebugNext;
	}
	if ( pHeader->pDebugNext ) {
		pHeader->pDebugNext->pDebugPrev = pHeader->pDebugPrev;
	} else {
		xCore.MemDebug.pLiveTail = pHeader->pDebugPrev;
	}
	pHeader->pDebugPrev = NULL;
	pHeader->pDebugNext = NULL;
	if ( xCore.MemDebug.iLiveAllocCount > 0 ) {
		xCore.MemDebug.iLiveAllocCount--;
	}
	if ( xCore.MemDebug.iLiveAllocBytes >= pHeader->iRequestSize ) {
		xCore.MemDebug.iLiveAllocBytes -= pHeader->iRequestSize;
	} else {
		xCore.MemDebug.iLiveAllocBytes = 0;
	}
}


// 内部函数：__xrtMemDebugFindForeignNoLock
static inline xrtMemDebugForeignAlloc* __xrtMemDebugFindForeignNoLock(ptr pAddress, xrtMemDebugForeignAlloc** ppPrev)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode = xCore.MemDebug.pForeignAllocs;
	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			if ( ppPrev ) {
				*ppPrev = pPrev;
			}
			return pNode;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}
	if ( ppPrev ) {
		*ppPrev = NULL;
	}
	return NULL;
}


// 内部函数：__xrtMemDebugRegisterForeignAlloc
static inline void __xrtMemDebugRegisterForeignAlloc(ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	if ( __xrtMemDebugFindForeignNoLock(pAddress, NULL) != NULL ) {
		__xrtMemDebugUnlock();
		return;
	}
	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugForeignAlloc));
	if ( pNode == NULL ) {
		__xrtMemDebugUnlock();
		return;
	}
	pNode->pAddress = pAddress;
	pNode->iSize = (uint32)iSize;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->pNext = xCore.MemDebug.pForeignAllocs;
	xCore.MemDebug.pForeignAllocs = pNode;
	xCore.MemDebug.iForeignLiveCount++;
	xCore.MemDebug.iForeignLiveBytes += iSize;
	if ( xCore.MemDebug.iForeignLiveCount > xCore.MemDebug.iPeakForeignLiveCount ) {
		xCore.MemDebug.iPeakForeignLiveCount = xCore.MemDebug.iForeignLiveCount;
	}
	if ( xCore.MemDebug.iForeignLiveBytes > xCore.MemDebug.iPeakForeignLiveBytes ) {
		xCore.MemDebug.iPeakForeignLiveBytes = xCore.MemDebug.iForeignLiveBytes;
	}
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, iAllocatorKind, iSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_ALLOC, pAddress, iSize, iAllocatorKind, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：__xrtMemDebugUnregisterForeignAlloc
static inline bool __xrtMemDebugUnregisterForeignAlloc(ptr pAddress, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return FALSE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindForeignNoLock(pAddress, &pPrev);
	if ( pNode == NULL ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pAddress, 0, iAllocatorKind, sFile, iLine);
		xCore.MemDebug.iDoubleFreeCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pAddress, 0, iAllocatorKind, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	if ( pPrev ) {
		pPrev->pNext = pNode->pNext;
	} else {
		xCore.MemDebug.pForeignAllocs = pNode->pNext;
	}
	if ( xCore.MemDebug.iForeignLiveCount > 0 ) {
		xCore.MemDebug.iForeignLiveCount--;
	}
	if ( xCore.MemDebug.iForeignLiveBytes >= pNode->iSize ) {
		xCore.MemDebug.iForeignLiveBytes -= pNode->iSize;
	} else {
		xCore.MemDebug.iForeignLiveBytes = 0;
	}
	__xrtMemDebugSiteOnFreeNoLock(pNode->sAllocFile, pNode->iAllocLine, pNode->iAllocatorKind, pNode->iSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_FREE, pAddress, pNode->iSize, pNode->iAllocatorKind, sFile, iLine);
	__xrtMemGlobalProcFree()(pNode);
	__xrtMemDebugUnlock();
	return TRUE;
}


// 内部函数：__xrtMemDebugLookupForeignAlloc
static inline bool __xrtMemDebugLookupForeignAlloc(ptr pAddress, uint32* pAllocatorKind, size_t* pSize, const char** psFile, uint32* pLine)
{
	xrtMemDebugForeignAlloc* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return FALSE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindForeignNoLock(pAddress, NULL);
	if ( pNode == NULL ) {
		__xrtMemDebugUnlock();
		return FALSE;
	}
	if ( pAllocatorKind ) {
		*pAllocatorKind = pNode->iAllocatorKind;
	}
	if ( pSize ) {
		*pSize = pNode->iSize;
	}
	if ( psFile ) {
		*psFile = pNode->sAllocFile;
	}
	if ( pLine ) {
		*pLine = pNode->iAllocLine;
	}
	__xrtMemDebugUnlock();
	return TRUE;
}


// 内部函数：__xrtMemDebugFindObjectNoLock
static inline xrtMemDebugObject* __xrtMemDebugFindObjectNoLock(ptr pAddress)
{
	xrtMemDebugObject* pNode = xCore.MemDebug.pObjects;
	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			return pNode;
		}
		pNode = pNode->pNext;
	}
	return NULL;
}


// 内部函数：注册内存调试 object
static inline void __xrtMemDebugRegisterObject(ptr pAddress, uint32 iObjectType, uint32 iOrigin, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode == NULL ) {
		pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugObject));
		if ( pNode == NULL ) {
			__xrtMemDebugUnlock();
			return;
		}
		pNode->pAddress = pAddress;
		pNode->pNext = xCore.MemDebug.pObjects;
		xCore.MemDebug.pObjects = pNode;
	}
	if ( pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		xCore.MemDebug.iLiveObjectCount++;
		if ( xCore.MemDebug.iLiveObjectCount > xCore.MemDebug.iPeakLiveObjectCount ) {
			xCore.MemDebug.iPeakLiveObjectCount = xCore.MemDebug.iLiveObjectCount;
		}
	}
	pNode->iObjectType = iObjectType;
	pNode->iOrigin = iOrigin;
	pNode->iState = XRT_MEMDEBUG_OBJECT_STATE_LIVE;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->sFreeFile = NULL;
	pNode->iFreeLine = 0;
	pNode->iFreeThreadId = 0;
	pNode->iFreeTimeMs = 0;
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_CREATE, pAddress, 0, iObjectType, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：__xrtMemDebugObjectGuardDestroy
static inline bool __xrtMemDebugObjectGuardDestroy(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return TRUE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode && pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
		xCore.MemDebug.iObjectDoubleDestroyCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	__xrtMemDebugUnlock();
	return TRUE;
}


// 内部函数：注销内存调试 object
static inline bool __xrtMemDebugUnregisterObject(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	xrtMemDebugObject* pNode;
	if ( pAddress == NULL || !__xrtMemDebugEnabled() ) {
		return TRUE;
	}
	__xrtMemDebugLock();
	pNode = __xrtMemDebugFindObjectNoLock(pAddress);
	if ( pNode == NULL || pNode->iState != XRT_MEMDEBUG_OBJECT_STATE_LIVE ) {
		__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, iObjectType, sFile, iLine);
		xCore.MemDebug.iObjectDoubleDestroyCount++;
		__xrtMemDebugEmitImmediateNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DOUBLE_DESTROY, pAddress, 0, iObjectType, sFile, iLine);
		__xrtMemDebugUnlock();
		return FALSE;
	}
	pNode->iState = XRT_MEMDEBUG_OBJECT_STATE_DESTROYED;
	pNode->sFreeFile = sFile;
	pNode->iFreeLine = iLine;
	pNode->iFreeThreadId = xrtThreadGetCurrentId();
	pNode->iFreeTimeMs = __xrtMemDebugNowMs();
	if ( xCore.MemDebug.iLiveObjectCount > 0 ) {
		xCore.MemDebug.iLiveObjectCount--;
	}
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_OBJECT_DESTROY, pAddress, 0, pNode->iObjectType ? pNode->iObjectType : iObjectType, sFile, iLine);
	__xrtMemDebugUnlock();
	return TRUE;
}


// 内部函数：分配内存调试 track
static inline void __xrtMemDebugTrackAlloc(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	pHeader->sAllocFile = sFile;
	pHeader->iAllocLine = iLine;
	pHeader->sFreeFile = NULL;
	pHeader->iFreeLine = 0;
	pHeader->iAllocThreadId = xrtThreadGetCurrentId();
	pHeader->iAllocTimeMs = __xrtMemDebugNowMs();
	pHeader->iAllocSeq = ++xCore.MemDebug.iAllocSeq;
	pHeader->iFreeTimeMs = 0;
	pHeader->iDebugState = XRT_MEMDEBUG_STATE_LIVE;
	__xrtMemDebugAttachLiveNoLock(pHeader);
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_ALLOC, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：释放内存调试 track
static inline void __xrtMemDebugTrackFree(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	if ( pHeader->iDebugState == XRT_MEMDEBUG_STATE_LIVE ) {
		__xrtMemDebugDetachLiveNoLock(pHeader);
		__xrtMemDebugSiteOnFreeNoLock(pHeader->sAllocFile, pHeader->iAllocLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	}
	pHeader->sFreeFile = sFile;
	pHeader->iFreeLine = iLine;
	pHeader->iFreeTimeMs = __xrtMemDebugNowMs();
	pHeader->iDebugState = XRT_MEMDEBUG_STATE_FREED;
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_FREE, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：__xrtMemDebugTrackReallocInPlace
static inline void __xrtMemDebugTrackReallocInPlace(xrtMemBlockHeader* pHeader, size_t iOldSize, const char* sFile, uint32 iLine)
{
	if ( pHeader == NULL || !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	__xrtMemDebugSiteOnFreeNoLock(pHeader->sAllocFile, pHeader->iAllocLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, iOldSize);
	pHeader->sAllocFile = sFile;
	pHeader->iAllocLine = iLine;
	pHeader->iAllocThreadId = xrtThreadGetCurrentId();
	pHeader->iAllocTimeMs = __xrtMemDebugNowMs();
	__xrtMemDebugSiteOnAllocNoLock(sFile, iLine, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, pHeader->iRequestSize);
	__xrtMemDebugRecordEventNoLock(XRT_MEMDEBUG_EVENT_REALLOC, __xrtMemGlobalUserFromHeader(pHeader), pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：内存调试记录 simple 事件相关处理
static inline void __xrtMemDebugRecordSimpleEvent(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	if ( !__xrtMemDebugEnabled() ) {
		return;
	}
	__xrtMemDebugLock();
	__xrtMemDebugRecordEventNoLock(iType, pAddress, iSize, iAllocatorKind, sFile, iLine);
	if ( iType == XRT_MEMDEBUG_EVENT_DOUBLE_FREE ) {
		xCore.MemDebug.iDoubleFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_INVALID_FREE ) {
		xCore.MemDebug.iInvalidFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE ) {
		xCore.MemDebug.iWrongAllocatorFreeCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT ) {
		xCore.MemDebug.iOverflowCount++;
	} else if ( iType == XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT ) {
		xCore.MemDebug.iUnderflowCount++;
	}
	__xrtMemDebugEmitImmediateNoLock(iType, pAddress, iSize, iAllocatorKind, sFile, iLine);
	__xrtMemDebugUnlock();
}


// 内部函数：重置内存调试状态
static inline void __xrtMemDebugResetState(xrtMemDebugState* pState)
{
	xrtMemBlockHeader* pQuarantineHead;
	xrtMemDebugSiteStat* pSiteHead;
	xrtMemDebugForeignAlloc* pForeignHead;
	xrtMemDebugObject* pObjectHead;
	xrtMemDebugSiteStat* pSite;
	xrtMemDebugForeignAlloc* pForeign;
	xrtMemDebugObject* pObject;
	xrtMemBlockHeader* pQuarantine;
	if ( pState == NULL ) {
		return;
	}

	__xrtMemGlobalLock(&pState->iLock);
	pQuarantineHead = pState->pQuarantineHead;
	pSiteHead = pState->pSiteStats;
	pForeignHead = pState->pForeignAllocs;
	pObjectHead = pState->pObjects;
	memset(pState, 0, sizeof(xrtMemDebugState));
	pState->iLock = 1;
	pState->bEnabled = 1;
	__xrtMemGlobalUnlock(&pState->iLock);

	pQuarantine = pQuarantineHead;
	while ( pQuarantine ) {
		xrtMemBlockHeader* pNext = pQuarantine->pDebugNext;
		__xrtMemGlobalProcFree()(pQuarantine);
		pQuarantine = pNext;
	}
	pSite = pSiteHead;
	while ( pSite ) {
		xrtMemDebugSiteStat* pNext = pSite->pNext;
		__xrtMemGlobalProcFree()(pSite);
		pSite = pNext;
	}
	pForeign = pForeignHead;
	while ( pForeign ) {
		xrtMemDebugForeignAlloc* pNext = pForeign->pNext;
		__xrtMemGlobalProcFree()(pForeign);
		pForeign = pNext;
	}
	pObject = pObjectHead;
	while ( pObject ) {
		xrtMemDebugObject* pNext = pObject->pNext;
		__xrtMemGlobalProcFree()(pObject);
		pObject = pNext;
	}
}


// 内部函数：判断是否存在内存调试 leaks
static inline bool __xrtMemDebugHasLeaks()
{
	return xCore.MemDebug.pLiveHead != NULL || xCore.MemDebug.pForeignAllocs != NULL || xCore.MemDebug.iLiveObjectCount != 0;
}

#else

// 内部函数：__xrtMemGlobalWriteTailCanary
static inline void __xrtMemGlobalWriteTailCanary(xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
}


// 内部函数：__xrtMemGlobalCheckFrontCanary
static inline bool __xrtMemGlobalCheckFrontCanary(const xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
	return TRUE;
}


// 内部函数：__xrtMemGlobalCheckTailCanary
static inline bool __xrtMemGlobalCheckTailCanary(const xrtMemBlockHeader* pHeader)
{
	(void)pHeader;
	return TRUE;
}


// 内部函数：__xrtMemDebugFindForeignReleaseNoLock
static inline xrtMemDebugForeignAlloc* __xrtMemDebugFindForeignReleaseNoLock(ptr pAddress, xrtMemDebugForeignAlloc** ppPrev)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode = __xrtMemForeignAllocList;

	while ( pNode ) {
		if ( pNode->pAddress == pAddress ) {
			if ( ppPrev ) {
				*ppPrev = pPrev;
			}
			return pNode;
		}
		pPrev = pNode;
		pNode = pNode->pNext;
	}

	if ( ppPrev ) {
		*ppPrev = NULL;
	}

	return NULL;
}


// 内部函数：__xrtMemDebugRegisterForeignAlloc
static inline void __xrtMemDebugRegisterForeignAlloc(ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pNode;

	if ( pAddress == NULL ) {
		return;
	}

	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	if ( __xrtMemDebugFindForeignReleaseNoLock(pAddress, NULL) != NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return;
	}

	pNode = __xrtMemGlobalProcCalloc()(1, sizeof(xrtMemDebugForeignAlloc));
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return;
	}

	pNode->pAddress = pAddress;
	pNode->iSize = (uint32)iSize;
	pNode->iAllocatorKind = iAllocatorKind;
	pNode->sAllocFile = sFile;
	pNode->iAllocLine = iLine;
	pNode->iAllocThreadId = xrtThreadGetCurrentId();
	pNode->iAllocTimeMs = __xrtMemDebugNowMs();
	pNode->pNext = __xrtMemForeignAllocList;
	__xrtMemForeignAllocList = pNode;
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
}


// 内部函数：__xrtMemDebugUnregisterForeignAlloc
static inline bool __xrtMemDebugUnregisterForeignAlloc(ptr pAddress, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	xrtMemDebugForeignAlloc* pPrev = NULL;
	xrtMemDebugForeignAlloc* pNode;

	(void)iAllocatorKind;
	(void)sFile;
	(void)iLine;

	if ( pAddress == NULL ) {
		return FALSE;
	}

	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pNode = __xrtMemDebugFindForeignReleaseNoLock(pAddress, &pPrev);
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return FALSE;
	}

	if ( pPrev ) {
		pPrev->pNext = pNode->pNext;
	} else {
		__xrtMemForeignAllocList = pNode->pNext;
	}
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	__xrtMemGlobalProcFree()(pNode);
	return TRUE;
}


// 内部函数：__xrtMemDebugLookupForeignAlloc
static inline bool __xrtMemDebugLookupForeignAlloc(ptr pAddress, uint32* pAllocatorKind, size_t* pSize, const char** psFile, uint32* pLine)
{
	xrtMemDebugForeignAlloc* pNode;

	if ( pAddress == NULL ) {
		return FALSE;
	}

	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pNode = __xrtMemDebugFindForeignReleaseNoLock(pAddress, NULL);
	if ( pNode == NULL ) {
		__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
		return FALSE;
	}

	if ( pAllocatorKind ) {
		*pAllocatorKind = pNode->iAllocatorKind;
	}
	if ( pSize ) {
		*pSize = pNode->iSize;
	}
	if ( psFile ) {
		*psFile = pNode->sAllocFile;
	}
	if ( pLine ) {
		*pLine = pNode->iAllocLine;
	}
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	return TRUE;
}


// 内部函数：注册内存调试 object
static inline void __xrtMemDebugRegisterObject(ptr pAddress, uint32 iObjectType, uint32 iOrigin, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)iOrigin;
	(void)sFile;
	(void)iLine;
}


// 内部函数：__xrtMemDebugObjectGuardDestroy
static inline bool __xrtMemDebugObjectGuardDestroy(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)sFile;
	(void)iLine;
	return TRUE;
}


// 内部函数：注销内存调试 object
static inline bool __xrtMemDebugUnregisterObject(ptr pAddress, uint32 iObjectType, const char* sFile, uint32 iLine)
{
	(void)pAddress;
	(void)iObjectType;
	(void)sFile;
	(void)iLine;
	return TRUE;
}


// 内部函数：分配内存调试 track
static inline void __xrtMemDebugTrackAlloc(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)sFile;
	(void)iLine;
}


// 内部函数：释放内存调试 track
static inline void __xrtMemDebugTrackFree(xrtMemBlockHeader* pHeader, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)sFile;
	(void)iLine;
}


// 内部函数：__xrtMemDebugTrackReallocInPlace
static inline void __xrtMemDebugTrackReallocInPlace(xrtMemBlockHeader* pHeader, size_t iOldSize, const char* sFile, uint32 iLine)
{
	(void)pHeader;
	(void)iOldSize;
	(void)sFile;
	(void)iLine;
}


// 内部函数：内存调试记录 simple 事件相关处理
static inline void __xrtMemDebugRecordSimpleEvent(uint32 iType, ptr pAddress, size_t iSize, uint32 iAllocatorKind, const char* sFile, uint32 iLine)
{
	(void)iType;
	(void)pAddress;
	(void)iSize;
	(void)iAllocatorKind;
	(void)sFile;
	(void)iLine;
}


// 内部函数：重置内存调试状态
static inline void __xrtMemDebugResetState(ptr pState)
{
	xrtMemDebugForeignAlloc* pHead;
	(void)pState;
	__xrtMemGlobalLock(&__xrtMemForeignAllocLock);
	pHead = __xrtMemForeignAllocList;
	__xrtMemForeignAllocList = NULL;
	__xrtMemGlobalUnlock(&__xrtMemForeignAllocLock);
	while ( pHead ) {
		xrtMemDebugForeignAlloc* pNext = pHead->pNext;
		__xrtMemGlobalProcFree()(pHead);
		pHead = pNext;
	}
}


// 内部函数：判断是否存在内存调试 leaks
static inline bool __xrtMemDebugHasLeaks()
{
	return FALSE;
}

#endif

// 内部函数：获取内存全局线程 cache
static inline xrtMemThreadCache* __xrtMemGlobalGetThreadCache()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	if ( pThreadData == NULL ) {
		return NULL;
	}
	return (xrtMemThreadCache*)pThreadData->tMem.pLocalAlloc;
}


// 内部函数：初始化内存全局线程 cache
static inline bool __xrtMemGlobalInitThreadCache(xrtThreadData* pThreadData)
{
	ptr (*procCalloc)(size_t, size_t) = __xrtMemGlobalProcCalloc();
	xrtMemThreadCache* pCache;

	if ( pThreadData == NULL ) {
		return FALSE;
	}
	if ( pThreadData->tMem.pLocalAlloc ) {
		return TRUE;
	}

	__xrtMemGlobalEnsurePlan();
	pCache = procCalloc(1, sizeof(xrtMemThreadCache));
	if ( pCache == NULL ) {
		return FALSE;
	}

	pCache->iClassCount = xCore.MemGlobal.iClassCount;
	pCache->iCacheLimit = XRT_MEMGLOBAL_CACHE_LIMIT;
	pThreadData->tMem.pLocalAlloc = pCache;
	return TRUE;
}


// 内部函数：压入内存全局 central
static inline void __xrtMemGlobalPushCentral(uint32 iClass, xrtMemFreeNode* pHead, xrtMemFreeNode* pTail, uint32 iCount)
{
	xrtMemGlobalClassDesc* pClass = &xCore.MemGlobal.arrClassDesc[iClass];

	if ( pHead == NULL || pTail == NULL || iCount == 0 ) {
		return;
	}

	__xrtMemGlobalLock(&pClass->iLock);
	pTail->pNext = pClass->pFreeList;
	pClass->pFreeList = pHead;
	pClass->iFreeCount += iCount;
	__xrtMemGlobalUnlock(&pClass->iLock);
}


// 内部函数：弹出内存全局 central
static inline xrtMemFreeNode* __xrtMemGlobalPopCentral(uint32 iClass, uint32 iMaxCount, uint32* pOutCount)
{
	xrtMemGlobalClassDesc* pClass = &xCore.MemGlobal.arrClassDesc[iClass];
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 iCount = 0;

	__xrtMemGlobalLock(&pClass->iLock);
	while ( pClass->pFreeList && iCount < iMaxCount ) {
		xrtMemFreeNode* pNode = pClass->pFreeList;
		pClass->pFreeList = pNode->pNext;
		pNode->pNext = NULL;
		if ( pHead == NULL ) {
			pHead = pNode;
		} else {
			pTail->pNext = pNode;
		}
		pTail = pNode;
		iCount++;
	}
	if ( pClass->iFreeCount >= iCount ) {
		pClass->iFreeCount -= iCount;
	} else {
		pClass->iFreeCount = 0;
	}
	__xrtMemGlobalUnlock(&pClass->iLock);

	if ( pOutCount ) {
		*pOutCount = iCount;
	}
	return pHead;
}


// 内部函数：分配内存全局 span
static inline bool __xrtMemGlobalAllocSpan(uint32 iClass)
{
	ptr (*procMalloc)(size_t) = __xrtMemGlobalProcMalloc();
	size_t iHeaderSize = __xrtMemGlobalHeaderSize();
	size_t iPayloadSize = __xrtMemGlobalClassBlockSize(&xCore.MemGlobal, iClass);
	size_t iStride = __xrtMemGlobalAlignSize(iHeaderSize + iPayloadSize);
	uint32 iBlockCount;
	size_t iBytes;
	xrtMemGlobalSpan* pSpan;
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 i;

	if ( iPayloadSize == 0 ) {
		return FALSE;
	}

	iBlockCount = (uint32)(XRT_MEMGLOBAL_SPAN_TARGET_BYTES / iStride);
	if ( iBlockCount < XRT_MEMGLOBAL_SPAN_MIN_BLOCKS ) {
		iBlockCount = XRT_MEMGLOBAL_SPAN_MIN_BLOCKS;
	}
	if ( iBlockCount > XRT_MEMGLOBAL_SPAN_MAX_BLOCKS ) {
		iBlockCount = XRT_MEMGLOBAL_SPAN_MAX_BLOCKS;
	}

	iBytes = __xrtMemGlobalAlignSize(sizeof(xrtMemGlobalSpan)) + (iStride * iBlockCount);
	pSpan = (xrtMemGlobalSpan*)procMalloc(iBytes);
	if ( pSpan == NULL ) {
		return FALSE;
	}

	pSpan->pMemory = pSpan;
	pSpan->iClassIndex = iClass;
	pSpan->iBlockCount = iBlockCount;

	{
		char* pCursor = (char*)pSpan + __xrtMemGlobalAlignSize(sizeof(xrtMemGlobalSpan));
		for ( i = 0; i < iBlockCount; i++ ) {
			xrtMemBlockHeader* pHeader = (xrtMemBlockHeader*)pCursor;
			xrtMemFreeNode* pNode;

			__xrtMemGlobalWriteHeader(pHeader, iClass, XRT_MEMBLOCK_FLAG_POOLED, (uint32)iPayloadSize);
			pNode = (xrtMemFreeNode*)__xrtMemGlobalUserFromHeader(pHeader);
			pNode->pNext = NULL;
			if ( pHead == NULL ) {
				pHead = pNode;
			} else {
				pTail->pNext = pNode;
			}
			pTail = pNode;
			pCursor += iStride;
		}
	}

	__xrtMemGlobalLock(&xCore.MemGlobal.iSpanLock);
	pSpan->pNext = xCore.MemGlobal.pSpanList;
	xCore.MemGlobal.pSpanList = pSpan;
	__xrtMemGlobalUnlock(&xCore.MemGlobal.iSpanLock);

	xCore.MemGlobal.arrClassDesc[iClass].iSpanCount++;
	__xrtMemGlobalPushCentral(iClass, pHead, pTail, iBlockCount);
	return TRUE;
}


// 内部函数：__xrtMemGlobalRefillThreadCache
static inline bool __xrtMemGlobalRefillThreadCache(xrtMemThreadCache* pCache, uint32 iClass)
{
	uint32 iDesired = pCache ? (uint32)(pCache->iCacheLimit / 2) : 0;
	uint32 iCount = 0;
	xrtMemFreeNode* pHead;
	xrtMemFreeNode* pNode;

	if ( pCache == NULL || iClass >= pCache->iClassCount ) {
		return FALSE;
	}
	if ( iDesired == 0 ) {
		iDesired = 1;
	}

	pHead = __xrtMemGlobalPopCentral(iClass, iDesired, &iCount);
	if ( pHead == NULL ) {
		if ( !__xrtMemGlobalAllocSpan(iClass) ) {
			return FALSE;
		}
		pHead = __xrtMemGlobalPopCentral(iClass, iDesired, &iCount);
		if ( pHead == NULL ) {
			return FALSE;
		}
	}

	pNode = pHead;
	while ( pNode ) {
		xrtMemFreeNode* pNext = pNode->pNext;
		pNode->pNext = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode;
		pCache->arrFreeCount[iClass]++;
		pNode = pNext;
	}

	return pCache->arrFreeCount[iClass] > 0;
}


// 内部函数：__xrtMemGlobalDrainThreadCache
static inline void __xrtMemGlobalDrainThreadCache(xrtMemThreadCache* pCache, uint32 iClass, uint32 iKeepCount)
{
	xrtMemFreeNode* pHead = NULL;
	xrtMemFreeNode* pTail = NULL;
	uint32 iCount = 0;

	if ( pCache == NULL || iClass >= pCache->iClassCount ) {
		return;
	}

	while ( pCache->arrFreeCount[iClass] > iKeepCount ) {
		xrtMemFreeNode* pNode = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode->pNext;
		pCache->arrFreeCount[iClass]--;
		pNode->pNext = NULL;
		if ( pHead == NULL ) {
			pHead = pNode;
		} else {
			pTail->pNext = pNode;
		}
		pTail = pNode;
		iCount++;
	}

	if ( iCount > 0 ) {
		__xrtMemGlobalPushCentral(iClass, pHead, pTail, iCount);
	}
}


// 内部函数：释放内存全局线程 cache
static inline void __xrtMemGlobalUnitThreadCache(xrtThreadData* pThreadData)
{
	void (*procFree)(ptr) = __xrtMemGlobalProcFree();
	xrtMemThreadCache* pCache;
	uint32 i;

	if ( pThreadData == NULL || pThreadData->tMem.pLocalAlloc == NULL ) {
		return;
	}

	pCache = (xrtMemThreadCache*)pThreadData->tMem.pLocalAlloc;
	for ( i = 0; i < pCache->iClassCount; i++ ) {
		__xrtMemGlobalDrainThreadCache(pCache, i, 0);
	}

	procFree(pCache);
	pThreadData->tMem.pLocalAlloc = NULL;
}


// 内部函数：释放内存全局计划
static inline void __xrtMemGlobalUnitPlan(xrtMemGlobalPool* pPool)
{
	void (*procFree)(ptr) = __xrtMemGlobalProcFree();
	xrtMemGlobalSpan* pSpan;

	if ( pPool == NULL ) {
		return;
	}

	pSpan = pPool->pSpanList;
	while ( pSpan ) {
		xrtMemGlobalSpan* pNext = pSpan->pNext;
		procFree(pSpan->pMemory);
		pSpan = pNext;
	}

	memset(pPool, 0, sizeof(xrtMemGlobalPool));
}


// 内部函数：分配内存全局 pooled
static inline ptr __xrtMemGlobalAllocPooled(uint32 iClass, size_t iRequestSize, bool bZero)
{
	xrtMemThreadCache* pCache = __xrtMemGlobalGetThreadCache();
	xrtMemFreeNode* pNode = NULL;
	xrtMemBlockHeader* pHeader;
	uint32 iBlockSize;

	if ( pCache && iClass < pCache->iClassCount ) {
		if ( pCache->arrFreeList[iClass] == NULL ) {
			if ( !__xrtMemGlobalRefillThreadCache(pCache, iClass) ) {
				return NULL;
			}
		}
		pNode = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
		pCache->arrFreeList[iClass] = pNode->pNext;
		pCache->arrFreeCount[iClass]--;
	} else {
		uint32 iCount = 0;
		pNode = __xrtMemGlobalPopCentral(iClass, 1, &iCount);
		if ( pNode == NULL ) {
			if ( !__xrtMemGlobalAllocSpan(iClass) ) {
				return NULL;
			}
			pNode = __xrtMemGlobalPopCentral(iClass, 1, &iCount);
			if ( pNode == NULL ) {
				return NULL;
			}
		}
	}

	pHeader = __xrtMemGlobalHeaderFromUser(pNode);
	iBlockSize = __xrtMemGlobalClassBlockSize(&xCore.MemGlobal, iClass);
	__xrtMemGlobalWriteHeader(pHeader, iClass, XRT_MEMBLOCK_FLAG_POOLED, (uint32)iRequestSize);
	if ( bZero ) {
		memset(pNode, 0, iBlockSize);
	#ifdef XRT_MEM_DEBUG
	} else {
		memset(pNode, 0xCD, iBlockSize);
	#endif
	}
	__xrtMemGlobalWriteTailCanary(pHeader);
	return pNode;
}


// 内部函数：分配内存全局 backing
static inline ptr __xrtMemGlobalAllocBacking(size_t iRequestSize, bool bZero)
{
	ptr pRaw;
	xrtMemBlockHeader* pHeader;
	size_t iAllocPayload = __xrtMemGlobalAllocPayloadSize(iRequestSize);
	size_t iAllocSize = __xrtMemGlobalHeaderSize() + iAllocPayload;

	if ( bZero ) {
		pRaw = __xrtMemGlobalProcCalloc()(1, iAllocSize);
	} else {
		pRaw = __xrtMemGlobalProcMalloc()(iAllocSize);
	}
	if ( pRaw == NULL ) {
		return NULL;
	}

	pHeader = (xrtMemBlockHeader*)pRaw;
	__xrtMemGlobalWriteHeader(pHeader, 0xFFFFu, XRT_MEMBLOCK_FLAG_BACKING, (uint32)iRequestSize);
	#ifdef XRT_MEM_DEBUG
		if ( !bZero ) {
			memset(__xrtMemGlobalUserFromHeader(pHeader), 0xCD, iAllocPayload);
		}
	#endif
	__xrtMemGlobalWriteTailCanary(pHeader);
	return __xrtMemGlobalUserFromHeader(pHeader);
}


// 内部函数：分配内存全局位置
static inline ptr __xrtMemGlobalAllocSite(size_t iSize, bool bZero, const char* sFile, uint32 iLine)
{
	uint32 iClass;
	ptr pMem;

	__xrtMemGlobalEnsurePlan();

	iClass = __xrtMemGlobalClassIndexForRequest(&xCore.MemGlobal, iSize);
	if ( iClass != (uint32)-1 ) {
		pMem = __xrtMemGlobalAllocPooled(iClass, iSize, bZero);
	} else {
		pMem = __xrtMemGlobalAllocBacking(iSize, bZero);
	}
	if ( pMem ) {
		__xrtMemDebugTrackAlloc(__xrtMemGlobalHeaderFromUser(pMem), sFile, iLine);
	}
	return pMem;
}


// 内部函数：分配内存全局
static inline ptr __xrtMemGlobalAlloc(size_t iSize, bool bZero)
{
	return __xrtMemGlobalAllocSite(iSize, bZero, NULL, 0);
}


// 内部函数：释放内存全局 free
static inline void __xrtMemGlobalFreeRelease(ptr pMem)
{
	xrtMemBlockHeader* pHeader;

	if ( pMem == NULL ) {
		return;
	}

	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		__xrtMemGlobalProcFree()(pMem);
		return;
	}

	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED ) {
		uint32 iClass = pHeader->iClassIndex;
		xrtMemThreadCache* pCache = __xrtMemGlobalGetThreadCache();
		xrtMemFreeNode* pNode = (xrtMemFreeNode*)pMem;

		if ( pCache && iClass < pCache->iClassCount ) {
			if ( pCache->arrFreeCount[iClass] >= pCache->iCacheLimit ) {
				__xrtMemGlobalDrainThreadCache(pCache, iClass, pCache->iCacheLimit / 2);
			}
			pNode->pNext = (xrtMemFreeNode*)pCache->arrFreeList[iClass];
			pCache->arrFreeList[iClass] = pNode;
			pCache->arrFreeCount[iClass]++;
		} else {
			pNode->pNext = NULL;
			__xrtMemGlobalPushCentral(iClass, pNode, pNode, 1);
		}
		return;
	}

	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
		__xrtMemGlobalProcFree()(pHeader);
		return;
	}

	__xrtMemGlobalProcFree()(pHeader);
}


// 内部函数：释放内存全局位置
static inline void __xrtMemGlobalFreeSite(ptr pMem, const char* sFile, uint32 iLine)
{
	xrtMemBlockHeader* pHeader;

	if ( pMem == NULL ) {
		return;
	}

	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		uint32 iAllocatorKind = 0;
		size_t iForeignSize = 0;
		const char* sForeignFile = NULL;
		uint32 iForeignLine = 0;
		if ( __xrtMemDebugLookupForeignAlloc(pMem, &iAllocatorKind, &iForeignSize, &sForeignFile, &iForeignLine) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE, pMem, iForeignSize, iAllocatorKind, sFile, iLine);
			xrtSetError("memory belongs to an explicit pool allocator.", FALSE);
			return;
		}
		#ifdef XRT_MEM_DEBUG
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_INVALID_FREE, pMem, 0, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("invalid free detected.", FALSE);
			return;
		#else
			__xrtMemGlobalProcFree()(pMem);
			return;
		#endif
	}

	#ifdef XRT_MEM_DEBUG
		if ( pHeader->iDebugState != XRT_MEMDEBUG_STATE_LIVE ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_DOUBLE_FREE, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("double free detected.", FALSE);
			return;
		}
		if ( !__xrtMemGlobalCheckFrontCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		if ( !__xrtMemGlobalCheckTailCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		__xrtMemDebugTrackFree(pHeader, sFile, iLine);
		memset(pMem, 0xDD, __xrtMemGlobalAllocPayloadSize(pHeader->iRequestSize));
		if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
			__xrtMemDebugLock();
			pHeader->iDebugState = XRT_MEMDEBUG_STATE_QUARANTINE;
			pHeader->pDebugPrev = xCore.MemDebug.pQuarantineTail;
			pHeader->pDebugNext = NULL;
			if ( xCore.MemDebug.pQuarantineTail ) {
				xCore.MemDebug.pQuarantineTail->pDebugNext = pHeader;
			} else {
				xCore.MemDebug.pQuarantineHead = pHeader;
			}
			xCore.MemDebug.pQuarantineTail = pHeader;
			xCore.MemDebug.iQuarantineCount++;
			xCore.MemDebug.iQuarantineBytes += pHeader->iRequestSize;
			while ( xCore.MemDebug.iQuarantineCount > XRT_MEMDEBUG_QUARANTINE_LIMIT && xCore.MemDebug.pQuarantineHead ) {
				xrtMemBlockHeader* pOld = xCore.MemDebug.pQuarantineHead;
				xCore.MemDebug.pQuarantineHead = pOld->pDebugNext;
				if ( xCore.MemDebug.pQuarantineHead ) {
					xCore.MemDebug.pQuarantineHead->pDebugPrev = NULL;
				} else {
					xCore.MemDebug.pQuarantineTail = NULL;
				}
				pOld->pDebugPrev = NULL;
				pOld->pDebugNext = NULL;
				if ( xCore.MemDebug.iQuarantineCount > 0 ) {
					xCore.MemDebug.iQuarantineCount--;
				}
				if ( xCore.MemDebug.iQuarantineBytes >= pOld->iRequestSize ) {
					xCore.MemDebug.iQuarantineBytes -= pOld->iRequestSize;
				} else {
					xCore.MemDebug.iQuarantineBytes = 0;
				}
				__xrtMemGlobalProcFree()(pOld);
			}
			__xrtMemDebugUnlock();
			return;
		}
	#endif

	__xrtMemGlobalFreeRelease(pMem);
}


// 内部函数：释放内存全局
static inline void __xrtMemGlobalFree(ptr pMem)
{
	__xrtMemGlobalFreeSite(pMem, NULL, 0);
}


// 内部函数：重新分配内存全局位置
static inline ptr __xrtMemGlobalReallocSite(ptr pMem, size_t iSize, const char* sFile, uint32 iLine)
{
	xrtMemBlockHeader* pHeader;
	size_t iOldSize;

	if ( pMem == NULL ) {
		return __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
	}
	if ( iSize == 0 ) {
		__xrtMemGlobalFreeSite(pMem, sFile, iLine);
		return NULL;
	}

	pHeader = __xrtMemGlobalHeaderFromUser(pMem);
	if ( !__xrtMemGlobalHeaderValid(pHeader) ) {
		uint32 iAllocatorKind = 0;
		size_t iForeignSize = 0;
		if ( __xrtMemDebugLookupForeignAlloc(pMem, &iAllocatorKind, &iForeignSize, NULL, NULL) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_WRONG_ALLOCATOR_FREE, pMem, iForeignSize, iAllocatorKind, sFile, iLine);
			xrtSetError("memory belongs to an explicit pool allocator.", FALSE);
			return NULL;
		}
		return __xrtMemGlobalProcRealloc()(pMem, iSize);
	}

	#ifdef XRT_MEM_DEBUG
		if ( pHeader->iDebugState != XRT_MEMDEBUG_STATE_LIVE ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_USE_AFTER_FREE_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
			xrtSetError("realloc on freed memory detected.", FALSE);
			return NULL;
		}
		if ( !__xrtMemGlobalCheckFrontCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_UNDERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
		if ( !__xrtMemGlobalCheckTailCanary(pHeader) ) {
			__xrtMemDebugRecordSimpleEvent(XRT_MEMDEBUG_EVENT_BUFFER_OVERFLOW_SUSPECT, pMem, pHeader->iRequestSize, XRT_MEMDEBUG_ALLOCATOR_GLOBAL, sFile, iLine);
		}
	#endif

	iOldSize = pHeader->iRequestSize;
	if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_POOLED ) {
		uint32 iOldClass = pHeader->iClassIndex;
		uint32 iNewClass = __xrtMemGlobalClassIndexForRequest(&xCore.MemGlobal, iSize);

		if ( iNewClass == iOldClass ) {
			pHeader->iRequestSize = (uint32)iSize;
			__xrtMemGlobalWriteTailCanary(pHeader);
			__xrtMemDebugTrackReallocInPlace(pHeader, iOldSize, sFile, iLine);
			return pMem;
		}
	} else if ( pHeader->iFlags & XRT_MEMBLOCK_FLAG_BACKING ) {
		#ifndef XRT_MEM_DEBUG
		if ( iSize > xCore.MemGlobal.iCutoff ) {
			size_t iAllocPayload = __xrtMemGlobalAllocPayloadSize(iSize);
			size_t iAllocSize = __xrtMemGlobalHeaderSize() + iAllocPayload;
			xrtMemBlockHeader* pNewHeader = __xrtMemGlobalProcRealloc()(pHeader, iAllocSize);
			if ( pNewHeader == NULL ) {
				return NULL;
			}
			__xrtMemGlobalWriteHeader(pNewHeader, 0xFFFFu, XRT_MEMBLOCK_FLAG_BACKING, (uint32)iSize);
			__xrtMemGlobalWriteTailCanary(pNewHeader);
			return __xrtMemGlobalUserFromHeader(pNewHeader);
		}
		#endif
	}

	{
		ptr pNewMem = __xrtMemGlobalAllocSite(iSize, FALSE, sFile, iLine);
		if ( pNewMem == NULL ) {
			return NULL;
		}
		memcpy(pNewMem, pMem, iOldSize < iSize ? iOldSize : iSize);
		__xrtMemGlobalFreeSite(pMem, sFile, iLine);
		return pNewMem;
	}
}


// 内部函数：重新分配内存全局
static inline ptr __xrtMemGlobalRealloc(ptr pMem, size_t iSize)
{
	return __xrtMemGlobalReallocSite(pMem, iSize, NULL, 0);
}
