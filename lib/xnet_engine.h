#ifndef XRT_XNET_ENGINE_H
#define XRT_XNET_ENGINE_H

/*
	XRT mainline network engine and worker runtime.

	This header owns:
	  - engine and worker lifecycle
	  - worker threads and cross-thread command posting
	  - timer-wheel driven delayed work
	  - dispatch of port completions into stream, datagram, and listener layers

	The engine is the shared runtime core used by async transport, protocol
	modules, and sync facades built on top of futures.
*/


/* ============================== Internal queue and platform types ============================== */

typedef void (*__xnet_task_discard_fn)(ptr pArg);

typedef struct __xnet_engine_cmd {
	struct __xnet_engine_cmd* pNext;
	uint32 iType;
	uint32 iDelayMs;
	uint64 iTimerId;
	xnet_task_fn pfnTask;
	__xnet_task_discard_fn pfnDiscard;
	ptr pArg;
} __xnet_engine_cmd;

typedef struct __xnet_engine_timer {
	struct __xnet_engine_timer* pNext;
	uint64 iTimerId;
	uint64 iDueTick;
	xnet_task_fn pfnTask;
	__xnet_task_discard_fn pfnDiscard;
	ptr pArg;
} __xnet_engine_timer;

typedef void (*xnet_port_event_fn)(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);

#if defined(XXRTL_CORE) && !defined(XRT_NO_THREAD)
	#define __XNET_ENGINE_USE_XRT_THREAD	1
#else
	#define __XNET_ENGINE_USE_XRT_THREAD	0
#endif

typedef struct {
	xmpscq_struct tQueue;
	volatile long iFreeLock;
	__xnet_engine_cmd* pFreeList;
	uint32 iFreeCount;
	uint32 iFreeLimit;
} __xnet_engine_cmdq;

typedef struct {
	uint32 iTickMs;
	uint32 iSlotCount;
	uint64 iCurrentTick;
	uint64 iNextDueTick;
	uint32 iTimerCount;
	__xnet_engine_timer** arrSlots;
} __xnet_engine_timerwheel;

typedef void (*__xnet_resolve_done_fn)(xnetworker* pWorker, ptr pArg,
	xnet_result iStatus, const xnetaddr* pAddr, int iSysErr);
typedef void (*__xnet_resolve_all_done_fn)(xnetworker* pWorker, ptr pArg,
	xnet_result iStatus, xnetaddrlist* pList, int iSysErr);

typedef struct __xnet_resolver_job {
	struct __xnet_resolver_job* pNext;
	xnetengine* pEngine;
	uint32 iAffinityKey;
	uint16 iPort;
	uint16 iFamily;
	xnet_result iStatus;
	int iSysErr;
	xnetaddr tAddr;
	xnetaddrlist* pAddrList;
	__xnet_resolve_done_fn pfnDone;
	__xnet_resolve_all_done_fn pfnDoneAll;
	ptr pArg;
	bool bResolveAll;
	char sHost[256];
} __xnet_resolver_job;

typedef struct {
	bool bUsed;
	uint16 iFamily;
	uint64 iExpiresMs;
	uint64 iLastUseMs;
	xnetaddr tAddr;
	char sHost[256];
} __xnet_resolver_cache_entry;

typedef struct {
	xnetengine* pEngine;
	xmutex pLock;
	xcond pCond;
	xthread* arrThreads;
	uint32 iThreadCount;
	uint32 iStartedThreads;
	uint32 iQueueLimit;
	uint32 iQueued;
	uint32 iCacheCapacity;
	uint32 iCacheTtlMs;
	bool bStopping;
	__xnet_resolver_job* pHead;
	__xnet_resolver_job* pTail;
	__xnet_resolver_cache_entry* arrCache;
} __xnet_resolver;

#define __XNET_ENGINE_CMD_TASK       1u
#define __XNET_ENGINE_CMD_TIMER_ADD  2u
#define __XNET_ENGINE_CMD_TIMER_CANCEL 3u
#define __XNET_ENGINE_TIMER_PULSE_ID UINT64_C(0xffffffffffffffff)
#define __XNET_ENGINE_HARVEST_BATCH  128u
#define __XNET_ENGINE_COMMAND_BUDGET 256u
#define __XNET_ENGINE_IDLE_WAIT_MS   1000u
#define __XNET_ENGINE_CMDQ_DEFAULT_CAPACITY 65536u
#define __XNET_ENGINE_CMDQ_DEFAULT_CACHE_LIMIT 256u



/* ============================== Public object layout ============================== */

struct xrt_net_worker {
	xnetengine* pEngine;
	uint32 iId;
	ptr hThread;
	uint64 iThreadId;
	volatile long bRunning;
	volatile long bStopRequested;
	xnetport tPort;
	xnetmemctx tMemCtx;
	ptr pCmdQ;
	ptr pTimerWheel;
	ptr pStreamTable;
	ptr pDgramTable;
};

struct xrt_net_engine {
	xnetengineconfig tConfig;
	xnetworker* arrWorkers;
	uint32 iWorkerCount;
	uint64 iNextStreamId;
	uint64 iNextTimerId;
	xnet_port_event_fn pfnOnPortEvent;
	xnet_port_event_fn pfnOnPortEvent2;
	ptr pResolver;
	volatile long bRunning;
	volatile long iLiveObjects;
};



/* ============================== Internal platform helpers ============================== */

static uint32 __xnetEngineDetectWorkers(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		SYSTEM_INFO tInfo;
		GetSystemInfo(&tInfo);
		return tInfo.dwNumberOfProcessors > 0 ? (uint32)tInfo.dwNumberOfProcessors : 1u;
	#else
		long iCount = sysconf(_SC_NPROCESSORS_ONLN);
		return iCount > 0 ? (uint32)iCount : 1u;
	#endif
}


// 内部函数：引擎默认端口 ops相关处理
static const xnetportops* __xnetEngineDefaultPortOps(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return xrtNetPortIOCPOps();
	#elif defined(__linux__)
		#if defined(XNET_FORCE_EPOLL)
			return xrtNetPortEpollOps();
		#elif defined(XNET_FORCE_SELECT)
			return xrtNetPortSelectOps();
		#endif
		return xrtNetPortUringOps();
	#elif defined(__APPLE__) && defined(__MACH__)
		#if defined(XNET_FORCE_SELECT)
			return xrtNetPortSelectOps();
		#endif
		return xrtNetPortKqueueOps();
	#else
		return xrtNetPortSelectOps();
	#endif
}


// 内部函数：休眠引擎毫秒
static void __xnetEngineSleepMs(uint32 iMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iMs);
	#else
		usleep((useconds_t)iMs * 1000u);
	#endif
}


// 内部函数：判断是否为引擎当前工作线程
static bool __xnetEngineIsCurrentWorker(xnetworker* pWorker)
{
	if ( !pWorker || __xnetAtomicLoad32(&pWorker->bRunning) == 0 || !pWorker->hThread ) { return false; }
	#if __XNET_ENGINE_USE_XRT_THREAD
		return xrtThreadGetCurrentId() == pWorker->iThreadId;
	#elif defined(_WIN32) || defined(_WIN64)
		return (uint64)GetCurrentThreadId() == pWorker->iThreadId;
	#else
		return (uint64)(uintptr_t)pthread_self() == pWorker->iThreadId;
	#endif
}


// 内部函数：分配下一个 stream/dgram id
static uint64 __xnetEngineAllocStreamId(xnetengine* pEngine)
{
	uint64 iId;
	if ( !pEngine ) { return 0; }
	iId = (uint64)__xnetAtomicAddFetch64((volatile int64*)&pEngine->iNextStreamId, 1) - 1u;
	if ( iId == 0 ) {
		iId = (uint64)__xnetAtomicAddFetch64((volatile int64*)&pEngine->iNextStreamId, 1) - 1u;
	}
	return iId;
}


// 内部函数：初始化引擎内存配置
static void __xnetEngineInitMemConfig(xnetmemconfig* pMemCfg, const xnetengineconfig* pCfg)
{
	if ( !pMemCfg || !pCfg ) { return; }
	xrtNetMemConfigInit(pMemCfg);
	pMemCfg->iSmallBlockSize = pCfg->iSmallBlockSize;
	pMemCfg->iMediumBlockSize = pCfg->iMediumBlockSize;
	pMemCfg->iLargeBlockSize = pCfg->iLargeBlockSize;
	pMemCfg->iSmallCacheLimit = pCfg->iBlockCachePerWorker;
	pMemCfg->iMediumCacheLimit = pCfg->iBlockCachePerWorker / 2u;
	pMemCfg->iLargeCacheLimit = pCfg->iBlockCachePerWorker / 4u;
	if ( pMemCfg->iMediumCacheLimit == 0 ) { pMemCfg->iMediumCacheLimit = 1; }
	if ( pMemCfg->iLargeCacheLimit == 0 ) { pMemCfg->iLargeCacheLimit = 1; }
}


// 内部函数：初始化引擎端口配置
static void __xnetEngineInitPortConfig(xnetportconfig* pPortCfg, const xnetengineconfig* pCfg)
{
	if ( !pPortCfg || !pCfg ) { return; }
	xrtNetPortConfigInit(pPortCfg);
	pPortCfg->iSqEntries = pCfg->iSqEntries;
	pPortCfg->iCqEntries = pCfg->iCqEntries;
	pPortCfg->iAcceptBatch = pCfg->iAcceptBatch;
}



/* ============================== Internal queue helpers ============================== */

static void __xnetCmdQFreeNode(ptr pItem, ptr pUserData)
{
	__xnet_engine_cmd* pCmd = (__xnet_engine_cmd*)pItem;
	(void)pUserData;
	if ( pCmd ) {
		if ( pCmd->pfnDiscard ) { pCmd->pfnDiscard(pCmd->pArg); }
		XNET_FREE(pCmd);
	}
}


// 内部函数：__xnetCmdQResolveFreeLimit
static uint32 __xnetCmdQResolveFreeLimit(uint32 iCapacity)
{
	uint32 iLimit = (iCapacity != 0) ? iCapacity : __XNET_ENGINE_CMDQ_DEFAULT_CAPACITY;
	if ( iLimit > __XNET_ENGINE_CMDQ_DEFAULT_CACHE_LIMIT ) {
		iLimit = __XNET_ENGINE_CMDQ_DEFAULT_CACHE_LIMIT;
	}
	return (iLimit == 0) ? 1u : iLimit;
}


// 内部函数：分配命令 q 节点
static __xnet_engine_cmd* __xnetCmdQAllocNode(__xnet_engine_cmdq* pQ)
{
	__xnet_engine_cmd* pCmd = NULL;

	if ( pQ == NULL ) {
		return NULL;
	}

	// 尝试从空闲链表中取出一个已回收的节点（自旋锁保护）
	__xrtOwnerSpinLock(&pQ->iFreeLock);
	pCmd = pQ->pFreeList;
	if ( pCmd != NULL ) {
		pQ->pFreeList = pCmd->pNext;
		pQ->iFreeCount--;
	}
	__xrtOwnerSpinUnlock(&pQ->iFreeLock);

	if ( pCmd == NULL ) {
		// 空闲链表为空时新分配一个节点
		pCmd = (__xnet_engine_cmd*)XNET_ALLOC(sizeof(__xnet_engine_cmd));
		if ( pCmd == NULL ) {
			return NULL;
		}
	}

	memset(pCmd, 0, sizeof(__xnet_engine_cmd));
	return pCmd;
}


// 内部函数：__xnetCmdQRecycleNode
static void __xnetCmdQRecycleNode(__xnet_engine_cmdq* pQ, __xnet_engine_cmd* pCmd)
{
	int bCached = FALSE;

	if ( pCmd == NULL ) {
		return;
	}

	pCmd->pNext = NULL;
	pCmd->pfnTask = NULL;
	pCmd->pfnDiscard = NULL;
	pCmd->pArg = NULL;
	if ( pQ != NULL && pQ->iFreeLimit != 0 ) {
		__xrtOwnerSpinLock(&pQ->iFreeLock);
		if ( pQ->iFreeCount < pQ->iFreeLimit ) {
			pCmd->pNext = pQ->pFreeList;
			pQ->pFreeList = pCmd;
			pQ->iFreeCount++;
			bCached = TRUE;
		}
		__xrtOwnerSpinUnlock(&pQ->iFreeLock);
	}

	if ( !bCached ) {
		XNET_FREE(pCmd);
	}
}


// 内部函数：__xnetCmdQFreeCached
static void __xnetCmdQFreeCached(__xnet_engine_cmdq* pQ)
{
	__xnet_engine_cmd* pList;

	if ( pQ == NULL ) {
		return;
	}

	__xrtOwnerSpinLock(&pQ->iFreeLock);
	pList = pQ->pFreeList;
	pQ->pFreeList = NULL;
	pQ->iFreeCount = 0;
	__xrtOwnerSpinUnlock(&pQ->iFreeLock);

	while ( pList != NULL ) {
		__xnet_engine_cmd* pNext = pList->pNext;
		XNET_FREE(pList);
		pList = pNext;
	}
}


// 内部函数：__xnetCmdQInit
static bool __xnetCmdQInit(__xnet_engine_cmdq* pQ, uint32 iCapacity)
{
	xqueue_config tCfg;
	if ( !pQ ) { return false; }
	memset(pQ, 0, sizeof(__xnet_engine_cmdq));
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = (iCapacity != 0) ? iCapacity : __XNET_ENGINE_CMDQ_DEFAULT_CAPACITY;
	if ( !xrtMPSCQInit(&pQ->tQueue, &tCfg) ) {
		return false;
	}
	pQ->iFreeLimit = __xnetCmdQResolveFreeLimit(tCfg.iCapacity);
	return true;
}


// 内部函数：__xnetCmdQUnit
static void __xnetCmdQUnit(__xnet_engine_cmdq* pQ)
{
	if ( !pQ ) { return; }
	if ( pQ->tQueue.arrSlots ) {
		xrtMPSCQClose(&pQ->tQueue);
		(void)xrtMPSCQDrain(&pQ->tQueue, __xnetCmdQFreeNode, NULL);
		xrtMPSCQUnit(&pQ->tQueue);
	}
	__xnetCmdQFreeCached(pQ);
}

static xqueue_result __xnetCmdQPushEx(__xnet_engine_cmdq* pQ, uint32 iType, uint32 iDelayMs,
	uint64 iTimerId, xnet_task_fn pfnTask, __xnet_task_discard_fn pfnDiscard, ptr pArg);


// 内部函数：__xnetCmdQPush
static xqueue_result __xnetCmdQPush(__xnet_engine_cmdq* pQ, xnet_task_fn pfnTask, ptr pArg)
{
	return __xnetCmdQPushEx(pQ, __XNET_ENGINE_CMD_TASK, 0, 0, pfnTask, NULL, pArg);
}


// 内部函数：压入命令 q 扩展
static xqueue_result __xnetCmdQPushEx(__xnet_engine_cmdq* pQ, uint32 iType, uint32 iDelayMs,
	uint64 iTimerId, xnet_task_fn pfnTask, __xnet_task_discard_fn pfnDiscard, ptr pArg)
{
	__xnet_engine_cmd* pCmd;
	xqueue_result iRet;
	if ( !pQ || (iType != __XNET_ENGINE_CMD_TIMER_CANCEL && !pfnTask) ) { return XQUEUE_ERROR; }
	pCmd = __xnetCmdQAllocNode(pQ);
	if ( !pCmd ) { return XQUEUE_ERROR; }
	pCmd->iType = iType;
	pCmd->iDelayMs = iDelayMs;
	pCmd->iTimerId = iTimerId;
	pCmd->pfnTask = pfnTask;
	pCmd->pfnDiscard = pfnDiscard;
	pCmd->pArg = pArg;

	iRet = xrtMPSCQTryPush(&pQ->tQueue, pCmd);
	if ( iRet != XQUEUE_OK ) {
		__xnetCmdQRecycleNode(pQ, pCmd);
	}
	return iRet;
}



/* ============================== Internal timer wheel helpers ============================== */

static uint64 __xnetEngineNowMs(void);

static bool __xnetTimerWheelInit(__xnet_engine_timerwheel* pWheel, uint32 iTickMs, uint32 iSlotCount)
{
	if ( !pWheel ) { return false; }
	memset(pWheel, 0, sizeof(__xnet_engine_timerwheel));
	pWheel->iTickMs = (iTickMs == 0) ? 10u : iTickMs;
	pWheel->iSlotCount = (iSlotCount == 0) ? 256u : iSlotCount;
	pWheel->iCurrentTick = __xnetEngineNowMs() / pWheel->iTickMs;
	pWheel->iNextDueTick = UINT64_MAX;
	pWheel->arrSlots = (__xnet_engine_timer**)XNET_ALLOC(sizeof(__xnet_engine_timer*) * pWheel->iSlotCount);
	if ( !pWheel->arrSlots ) { return false; }
	memset(pWheel->arrSlots, 0, sizeof(__xnet_engine_timer*) * pWheel->iSlotCount);
	return true;
}


// 内部函数：__xnetTimerWheelUnit
static void __xnetTimerWheelUnit(__xnet_engine_timerwheel* pWheel)
{
	if ( !pWheel ) { return; }
	if ( pWheel->arrSlots ) {
		for ( uint32 i = 0; i < pWheel->iSlotCount; ++i ) {
			__xnet_engine_timer* pNode = pWheel->arrSlots[i];
			while ( pNode ) {
				__xnet_engine_timer* pNext = pNode->pNext;
				if ( pNode->pfnDiscard ) { pNode->pfnDiscard(pNode->pArg); }
				XNET_FREE(pNode);
				pNode = pNext;
			}
		}
		XNET_FREE(pWheel->arrSlots);
	}
	memset(pWheel, 0, sizeof(__xnet_engine_timerwheel));
}


// 内部函数：__xnetTimerWheelSchedule
static bool __xnetTimerWheelSchedule(__xnet_engine_timerwheel* pWheel, uint64 iTimerId,
	uint32 iDelayMs, xnet_task_fn pfnTask, __xnet_task_discard_fn pfnDiscard, ptr pArg)
{
	uint64 iNowTick;
	uint64 iDeltaTicks;
	uint64 iDueTick;
	uint32 iSlot;
	__xnet_engine_timer* pTimer;
	if ( !pWheel || !pWheel->arrSlots || iTimerId == 0 || !pfnTask ) { return false; }

	iNowTick = __xnetEngineNowMs() / pWheel->iTickMs;
	if ( iNowTick > pWheel->iCurrentTick ) { pWheel->iCurrentTick = iNowTick; }
	iDeltaTicks = (iDelayMs == 0) ? 1u : ((uint64)iDelayMs + pWheel->iTickMs - 1u) / pWheel->iTickMs;
	if ( iDeltaTicks == 0 ) { iDeltaTicks = 1; }
	iDueTick = pWheel->iCurrentTick + iDeltaTicks;
	iSlot = (uint32)(iDueTick % pWheel->iSlotCount);

	pTimer = (__xnet_engine_timer*)XNET_ALLOC(sizeof(__xnet_engine_timer));
	if ( !pTimer ) { return false; }
	memset(pTimer, 0, sizeof(__xnet_engine_timer));
	pTimer->iTimerId = iTimerId;
	pTimer->iDueTick = iDueTick;
	pTimer->pfnTask = pfnTask;
	pTimer->pfnDiscard = pfnDiscard;
	pTimer->pArg = pArg;
	pTimer->pNext = pWheel->arrSlots[iSlot];
	pWheel->arrSlots[iSlot] = pTimer;
	pWheel->iTimerCount++;
	if ( iDueTick < pWheel->iNextDueTick ) { pWheel->iNextDueTick = iDueTick; }
	return true;
}


// 内部函数：__xnetTimerWheelTick
static void __xnetTimerWheelTick(xnetworker* pWorker)
{
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	__xnet_engine_timer* pList;
	__xnet_engine_timer* pDeferred = NULL;
	uint64 iTargetTick;
	if ( !pWheel || !pWheel->arrSlots || pWheel->iTimerCount == 0 ) { return; }
	iTargetTick = __xnetEngineNowMs() / pWheel->iTickMs;
	if ( iTargetTick < pWheel->iCurrentTick ) { iTargetTick = pWheel->iCurrentTick; }
	if ( pWheel->iNextDueTick > iTargetTick ) { return; }

	// 推进时间轮滴答，取出当前槽位的定时器链表
	pWheel->iCurrentTick = iTargetTick;
	for ( uint32 iScanSlot = 0; iScanSlot < pWheel->iSlotCount; ++iScanSlot ) {
	pList = pWheel->arrSlots[iScanSlot];
	pWheel->arrSlots[iScanSlot] = NULL;

	// 遍历定时器链表：到期则执行任务并释放，未到期则挂入延迟重插链表
	while ( pList ) {
		__xnet_engine_timer* pNext = pList->pNext;
		if ( pList->iDueTick <= pWheel->iCurrentTick ) {
			if ( pList->pfnTask ) {
				pList->pfnTask(pWorker, pList->pArg);
			}
			XNET_FREE(pList);
			if ( pWheel->iTimerCount > 0 ) { pWheel->iTimerCount--; }
		} else {
			pList->pNext = pDeferred;
			pDeferred = pList;
		}
		pList = pNext;
	}

	// 将未到期的定时器重新插入到对应的目标槽位
	while ( pDeferred ) {
		__xnet_engine_timer* pNext = pDeferred->pNext;
		uint32 iSlot = (uint32)(pDeferred->iDueTick % pWheel->iSlotCount);
		pDeferred->pNext = pWheel->arrSlots[iSlot];
		pWheel->arrSlots[iSlot] = pDeferred;
		pDeferred = pNext;
	}
	}

	pWheel->iNextDueTick = UINT64_MAX;
	for ( uint32 i = 0; i < pWheel->iSlotCount; ++i ) {
		for ( __xnet_engine_timer* pNode = pWheel->arrSlots[i]; pNode; pNode = pNode->pNext ) {
			if ( pNode->iDueTick < pWheel->iNextDueTick ) { pWheel->iNextDueTick = pNode->iDueTick; }
		}
	}
}


static void __xnetEngineAddLiveObject(xnetengine* pEngine)
{
	if ( pEngine ) { (void)__xnetAtomicAddFetch32(&pEngine->iLiveObjects, 1); }
}


static void __xnetEngineReleaseLiveObject(xnetengine* pEngine)
{
	if ( pEngine ) { (void)__xnetAtomicAddFetch32(&pEngine->iLiveObjects, -1); }
}


static uint64 __xnetEngineAllocTimerId(xnetengine* pEngine, uint32 iWorkerId)
{
	uint64 iSerial;
	uint64 iId;
	if ( !pEngine || pEngine->iWorkerCount == 0 ) { return 0; }
	iSerial = (uint64)__xnetAtomicAddFetch64((volatile int64*)&pEngine->iNextTimerId, 1) - 1u;
	iId = iSerial * (uint64)pEngine->iWorkerCount + (uint64)iWorkerId;
	if ( iId == 0 ) {
		iSerial = (uint64)__xnetAtomicAddFetch64((volatile int64*)&pEngine->iNextTimerId, 1) - 1u;
		iId = iSerial * (uint64)pEngine->iWorkerCount + (uint64)iWorkerId;
	}
	return iId;
}


static bool __xnetTimerWheelCancel(__xnet_engine_timerwheel* pWheel, uint64 iTimerId)
{
	if ( !pWheel || !pWheel->arrSlots || iTimerId == 0 ) { return false; }
	for ( uint32 i = 0; i < pWheel->iSlotCount; ++i ) {
		__xnet_engine_timer** ppNode = &pWheel->arrSlots[i];
		while ( *ppNode ) {
			__xnet_engine_timer* pNode = *ppNode;
			if ( pNode->iTimerId == iTimerId ) {
				*ppNode = pNode->pNext;
				if ( pNode->pfnDiscard ) { pNode->pfnDiscard(pNode->pArg); }
				XNET_FREE(pNode);
				if ( pWheel->iTimerCount > 0 ) { pWheel->iTimerCount--; }
				pWheel->iNextDueTick = UINT64_MAX;
				for ( uint32 j = 0; j < pWheel->iSlotCount; ++j ) {
					for ( __xnet_engine_timer* pScan = pWheel->arrSlots[j]; pScan; pScan = pScan->pNext ) {
						if ( pScan->iDueTick < pWheel->iNextDueTick ) { pWheel->iNextDueTick = pScan->iDueTick; }
					}
				}
				return true;
			}
			ppNode = &pNode->pNext;
		}
	}
	return false;
}


static uint32 __xnetTimerWheelGetWaitMs(const __xnet_engine_timerwheel* pWheel)
{
	uint64 iNowMs;
	uint64 iDueMs;
	uint64 iWaitMs;
	if ( !pWheel || pWheel->iTimerCount == 0 || pWheel->iNextDueTick == UINT64_MAX ) {
		return __XNET_ENGINE_IDLE_WAIT_MS;
	}
	iNowMs = __xnetEngineNowMs();
	iDueMs = pWheel->iNextDueTick * (uint64)pWheel->iTickMs;
	if ( iDueMs <= iNowMs ) { return 0; }
	iWaitMs = iDueMs - iNowMs;
	return (uint32)(iWaitMs > UINT32_MAX ? UINT32_MAX : iWaitMs);
}


// 内部函数：处理引擎端口 events
static void __xnetEngineHandlePortEvents(xnetworker* pWorker, xnetportevent* pEvents, uint32 iCount)
{
	if ( !pWorker || !pEvents || iCount == 0 ) { return; }
	if ( pWorker->pEngine && pWorker->pEngine->pfnOnPortEvent ) {
		pWorker->pEngine->pfnOnPortEvent(pWorker, pEvents, iCount);
	}
	if ( pWorker->pEngine && pWorker->pEngine->pfnOnPortEvent2 &&
		pWorker->pEngine->pfnOnPortEvent2 != pWorker->pEngine->pfnOnPortEvent ) {
		pWorker->pEngine->pfnOnPortEvent2(pWorker, pEvents, iCount);
	}
}


/* ============================== Internal worker helpers ============================== */

static uint32 __xnetEngineDrainCommands(xnetworker* pWorker, uint32 iBudget)
{
	__xnet_engine_cmdq* pQ = pWorker ? (__xnet_engine_cmdq*)pWorker->pCmdQ : NULL;
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	__xnet_engine_cmd* pNode;
	ptr pItem = NULL;
	xqueue_result iRet;
	uint32 iProcessed = 0;
	if ( !pQ || iBudget == 0 ) { return 0; }
	while ( iProcessed < iBudget ) {
		iRet = xrtMPSCQTryPop(&pQ->tQueue, &pItem);
		if ( iRet != XQUEUE_OK ) {
			break;
		}
		pNode = (__xnet_engine_cmd*)pItem;
		if ( !pNode ) {
			continue;
		}
		if ( pNode->iType == __XNET_ENGINE_CMD_TIMER_ADD ) {
			if ( __xnetTimerWheelSchedule(pWheel, pNode->iTimerId, pNode->iDelayMs,
				pNode->pfnTask, pNode->pfnDiscard, pNode->pArg) ) {
				pNode->pfnDiscard = NULL;
				pNode->pArg = NULL;
			} else if ( pNode->pfnDiscard ) {
				pNode->pfnDiscard(pNode->pArg);
				pNode->pfnDiscard = NULL;
				pNode->pArg = NULL;
			}
		} else if ( pNode->iType == __XNET_ENGINE_CMD_TIMER_CANCEL ) {
			(void)__xnetTimerWheelCancel(pWheel, pNode->iTimerId);
		} else if ( pNode->pfnTask ) {
			pNode->pfnTask(pWorker, pNode->pArg);
		}
		__xnetCmdQRecycleNode(pQ, pNode);
		iProcessed++;
	}
	return iProcessed;
}


static void __xnetEngineHandleQuiesceEvents(ptr pOwner, xnetportevent* pEvents, uint32 iCount)
{
	__xnetEngineHandlePortEvents((xnetworker*)pOwner, pEvents, iCount);
}


static uint64 __xnetEngineNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) { return 0; }
		return (uint64)tNow.tv_sec * 1000u + (uint64)tNow.tv_nsec / 1000000u;
	#endif
}


static void __xnetResolverJobComplete(xnetworker* pWorker, ptr pArg)
{
	__xnet_resolver_job* pJob = (__xnet_resolver_job*)pArg;
	if ( !pJob ) { return; }
	if ( pJob->bResolveAll && pJob->pfnDoneAll ) {
		pJob->pfnDoneAll(pWorker, pJob->pArg, pJob->iStatus,
			pJob->iStatus == XRT_NET_OK ? pJob->pAddrList : NULL, pJob->iSysErr);
		if ( pJob->iStatus == XRT_NET_OK ) { pJob->pAddrList = NULL; }
	} else if ( pJob->pfnDone ) {
		pJob->pfnDone(pWorker, pJob->pArg, pJob->iStatus,
			pJob->iStatus == XRT_NET_OK ? &pJob->tAddr : NULL, pJob->iSysErr);
	}
	if ( pJob->pAddrList ) { xrtNetAddrListDestroy(pJob->pAddrList); }
	XNET_FREE(pJob);
}


static bool __xnetResolverCacheGetLocked(__xnet_resolver* pResolver, const char* sHost,
	uint16 iPort, uint16 iFamily, xnetaddr* pAddr)
{
	uint64 iNowMs;
	uint32 i;
	if ( !pResolver || !sHost || !pAddr || !pResolver->arrCache ) { return false; }
	iNowMs = __xnetEngineNowMs();
	for ( i = 0; i < pResolver->iCacheCapacity; ++i ) {
		__xnet_resolver_cache_entry* pEntry = &pResolver->arrCache[i];
		if ( !pEntry->bUsed || pEntry->iFamily != iFamily ||
			strcmp(pEntry->sHost, sHost) != 0 ) { continue; }
		if ( pEntry->iExpiresMs <= iNowMs ) {
			pEntry->bUsed = false;
			continue;
		}
		*pAddr = pEntry->tAddr;
		pAddr->iPort = iPort;
		pEntry->iLastUseMs = iNowMs;
		return true;
	}
	return false;
}


static void __xnetResolverCachePut(__xnet_resolver* pResolver, const char* sHost,
	uint16 iFamily, const xnetaddr* pAddr)
{
	uint64 iNowMs;
	uint64 iOldestUse = UINT64_MAX;
	uint32 i;
	uint32 iSlot = 0;
	if ( !pResolver || !sHost || !pAddr || !pResolver->arrCache || pResolver->iCacheCapacity == 0 ) { return; }
	iNowMs = __xnetEngineNowMs();
	xrtMutexLock(pResolver->pLock);
	for ( i = 0; i < pResolver->iCacheCapacity; ++i ) {
		__xnet_resolver_cache_entry* pEntry = &pResolver->arrCache[i];
		if ( pEntry->bUsed && pEntry->iFamily == iFamily && strcmp(pEntry->sHost, sHost) == 0 ) {
			iSlot = i;
			break;
		}
		if ( !pEntry->bUsed || pEntry->iExpiresMs <= iNowMs ) {
			iSlot = i;
			iOldestUse = 0;
			break;
		}
		if ( pEntry->iLastUseMs < iOldestUse ) {
			iOldestUse = pEntry->iLastUseMs;
			iSlot = i;
		}
	}
	{
		__xnet_resolver_cache_entry* pEntry = &pResolver->arrCache[iSlot];
		memset(pEntry, 0, sizeof(*pEntry));
		pEntry->bUsed = true;
		pEntry->iLastUseMs = iNowMs;
		pEntry->iExpiresMs = iNowMs + pResolver->iCacheTtlMs;
		pEntry->iFamily = iFamily;
		pEntry->tAddr = *pAddr;
		__xnetCopyFixedString(pEntry->sHost, sizeof(pEntry->sHost), sHost);
	}
	xrtMutexUnlock(pResolver->pLock);
}


static bool __xnetResolverPostCompletion(__xnet_resolver_job* pJob)
{
	xnet_result iPostResult;
	if ( !pJob || !pJob->pEngine ) { return false; }
	do {
		iPostResult = xrtNetEnginePost(pJob->pEngine, pJob->iAffinityKey,
			__xnetResolverJobComplete, pJob);
		if ( iPostResult != XRT_NET_AGAIN ) { break; }
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(1);
		#else
			usleep(1000);
		#endif
	} while ( __xnetAtomicLoad32(&pJob->pEngine->bRunning) != 0 );
	return iPostResult == XRT_NET_OK;
}


static uint32 __xnetResolverThreadMain(ptr pArg)
{
	__xnet_resolver* pResolver = (__xnet_resolver*)pArg;
	if ( !pResolver ) { return 0; }
	for (;;) {
		__xnet_resolver_job* pJob;
		xrtMutexLock(pResolver->pLock);
		while ( !pResolver->bStopping && pResolver->pHead == NULL ) {
			xrtCondWait(pResolver->pCond, pResolver->pLock);
		}
		if ( pResolver->pHead == NULL && pResolver->bStopping ) {
			xrtMutexUnlock(pResolver->pLock);
			break;
		}
		pJob = pResolver->pHead;
		pResolver->pHead = pJob->pNext;
		if ( !pResolver->pHead ) { pResolver->pTail = NULL; }
		if ( pResolver->iQueued > 0 ) { pResolver->iQueued--; }
		xrtMutexUnlock(pResolver->pLock);

		memset(&pJob->tAddr, 0, sizeof(pJob->tAddr));
		pJob->tAddr.iPort = pJob->iPort;
		pJob->tAddr.iFamily = pJob->iFamily;
		if ( pJob->bResolveAll ) {
			pJob->pAddrList = xrtNetResolveAll(pJob->sHost, pJob->iPort, pJob->iFamily);
			pJob->iStatus = pJob->pAddrList ? XRT_NET_OK : XRT_NET_ERROR;
		} else {
			pJob->iStatus = xrtNetResolve(pJob->sHost, &pJob->tAddr);
		}
		pJob->iSysErr = (pJob->iStatus == XRT_NET_OK) ? 0 : -1;
		if ( pJob->iStatus == XRT_NET_OK && !pJob->bResolveAll ) {
			__xnetResolverCachePut(pResolver, pJob->sHost, pJob->iFamily, &pJob->tAddr);
		} else if ( pJob->iStatus == XRT_NET_OK && pJob->pAddrList &&
			xrtNetAddrListCount(pJob->pAddrList) > 0u ) {
			const xnetaddr* pCacheAddr = xrtNetAddrListGet(pJob->pAddrList, 0u);
			if ( pJob->iFamily == AF_UNSPEC ) {
				uint32 iAddr;
				for ( iAddr = 0u; iAddr < xrtNetAddrListCount(pJob->pAddrList); ++iAddr ) {
					const xnetaddr* pCandidate = xrtNetAddrListGet(pJob->pAddrList, iAddr);
					if ( pCandidate && pCandidate->iFamily == AF_INET ) {
						pCacheAddr = pCandidate;
						break;
					}
				}
			}
			__xnetResolverCachePut(pResolver, pJob->sHost, pJob->iFamily, pCacheAddr);
		}
		if ( !__xnetResolverPostCompletion(pJob) ) {
			if ( pJob->bResolveAll && pJob->pAddrList ) {
				xrtNetAddrListDestroy(pJob->pAddrList);
				pJob->pAddrList = NULL;
			}
			if ( pJob->pfnDone ) {
				pJob->pfnDone(NULL, pJob->pArg, XRT_NET_CLOSED, NULL, pJob->iSysErr);
			} else if ( pJob->pfnDoneAll ) {
				pJob->pfnDoneAll(NULL, pJob->pArg, XRT_NET_CLOSED, NULL, pJob->iSysErr);
			}
			XNET_FREE(pJob);
		}
	}
	return 0;
}


static void __xnetResolverUnit(__xnet_resolver* pResolver)
{
	uint32 i;
	if ( !pResolver ) { return; }
	if ( pResolver->pLock ) {
		xrtMutexLock(pResolver->pLock);
		pResolver->bStopping = true;
		if ( pResolver->pCond ) { xrtCondBroadcast(pResolver->pCond); }
		xrtMutexUnlock(pResolver->pLock);
	}
	for ( i = 0; i < pResolver->iStartedThreads; ++i ) {
		if ( pResolver->arrThreads[i] ) {
			xrtThreadWait(pResolver->arrThreads[i]);
			xrtThreadDestroy(pResolver->arrThreads[i]);
		}
	}
	while ( pResolver->pHead ) {
		__xnet_resolver_job* pJob = pResolver->pHead;
		pResolver->pHead = pJob->pNext;
		if ( pJob->pfnDone ) { pJob->pfnDone(NULL, pJob->pArg, XRT_NET_CLOSED, NULL, 0); }
		else if ( pJob->pfnDoneAll ) { pJob->pfnDoneAll(NULL, pJob->pArg, XRT_NET_CLOSED, NULL, 0); }
		if ( pJob->pAddrList ) { xrtNetAddrListDestroy(pJob->pAddrList); }
		XNET_FREE(pJob);
	}
	if ( pResolver->pCond ) { xrtCondDestroy(pResolver->pCond); }
	if ( pResolver->pLock ) { xrtMutexDestroy(pResolver->pLock); }
	XNET_FREE(pResolver->arrThreads);
	XNET_FREE(pResolver->arrCache);
	XNET_FREE(pResolver);
}


static bool __xnetResolverStart(xnetengine* pEngine)
{
	__xnet_resolver* pResolver;
	uint32 i;
	uint32 iThreadCount;
	if ( !pEngine ) { return false; }
	iThreadCount = pEngine->tConfig.iResolverThreads;
	if ( iThreadCount == 0 ) { iThreadCount = 2; }
	if ( iThreadCount > 32 ) { iThreadCount = 32; }
	pResolver = (__xnet_resolver*)XNET_ALLOC(sizeof(*pResolver));
	if ( !pResolver ) { return false; }
	memset(pResolver, 0, sizeof(*pResolver));
	pResolver->pEngine = pEngine;
	pResolver->iThreadCount = iThreadCount;
	pResolver->iQueueLimit = pEngine->tConfig.iResolverQueueLimit ? pEngine->tConfig.iResolverQueueLimit : 4096;
	pResolver->iCacheCapacity = pEngine->tConfig.iResolverCacheEntries;
	pResolver->iCacheTtlMs = pEngine->tConfig.iResolverCacheTtlMs ? pEngine->tConfig.iResolverCacheTtlMs : 60000;
	pResolver->pLock = xrtMutexCreate();
	pResolver->pCond = xrtCondCreate();
	pResolver->arrThreads = (xthread*)XNET_ALLOC(sizeof(xthread) * iThreadCount);
	if ( pResolver->iCacheCapacity > 0 ) {
		pResolver->arrCache = (__xnet_resolver_cache_entry*)XNET_ALLOC(
			sizeof(__xnet_resolver_cache_entry) * pResolver->iCacheCapacity);
	}
	if ( !pResolver->pLock || !pResolver->pCond || !pResolver->arrThreads ||
		(pResolver->iCacheCapacity > 0 && !pResolver->arrCache) ) {
		__xnetResolverUnit(pResolver);
		return false;
	}
	memset(pResolver->arrThreads, 0, sizeof(xthread) * iThreadCount);
	if ( pResolver->arrCache ) {
		memset(pResolver->arrCache, 0, sizeof(__xnet_resolver_cache_entry) * pResolver->iCacheCapacity);
	}
	pEngine->pResolver = pResolver;
	for ( i = 0; i < iThreadCount; ++i ) {
		pResolver->arrThreads[i] = xrtThreadCreate((ptr)__xnetResolverThreadMain, pResolver, 0);
		if ( !pResolver->arrThreads[i] ) {
			pEngine->pResolver = NULL;
			__xnetResolverUnit(pResolver);
			return false;
		}
		pResolver->iStartedThreads++;
	}
	return true;
}


static void __xnetResolverStop(xnetengine* pEngine)
{
	__xnet_resolver* pResolver;
	if ( !pEngine ) { return; }
	pResolver = (__xnet_resolver*)pEngine->pResolver;
	pEngine->pResolver = NULL;
	__xnetResolverUnit(pResolver);
}


static xnet_result __xnetEngineResolveAsyncEx(xnetengine* pEngine, uint32 iAffinityKey,
	const char* sHost, uint16 iPort, uint16 iFamily,
	__xnet_resolve_done_fn pfnDone, ptr pArg)
{
	__xnet_resolver* pResolver;
	__xnet_resolver_job* pJob;
	xnetaddr tCached;
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 ||
		!pfnDone || !sHost || !sHost[0] || iPort == 0 ) { return XRT_NET_ERROR; }
	if ( iFamily != AF_UNSPEC && iFamily != AF_INET && iFamily != AF_INET6 ) { return XRT_NET_ERROR; }
	pResolver = (__xnet_resolver*)pEngine->pResolver;
	if ( !pResolver ) { return XRT_NET_ERROR; }
	pJob = (__xnet_resolver_job*)XNET_ALLOC(sizeof(*pJob));
	if ( !pJob ) { return XRT_NET_ERROR; }
	memset(pJob, 0, sizeof(*pJob));
	pJob->pEngine = pEngine;
	pJob->iAffinityKey = iAffinityKey;
	pJob->iPort = iPort;
	pJob->iFamily = iFamily;
	pJob->pfnDone = pfnDone;
	pJob->pArg = pArg;
	__xnetCopyFixedString(pJob->sHost, sizeof(pJob->sHost), sHost);
	if ( strlen(sHost) >= sizeof(pJob->sHost) ) {
		XNET_FREE(pJob);
		return XRT_NET_ERROR;
	}

	xrtMutexLock(pResolver->pLock);
	if ( __xnetResolverCacheGetLocked(pResolver, sHost, iPort, iFamily, &tCached) ) {
		xrtMutexUnlock(pResolver->pLock);
		pJob->iStatus = XRT_NET_OK;
		pJob->tAddr = tCached;
		if ( !__xnetResolverPostCompletion(pJob) ) {
			XNET_FREE(pJob);
			return XRT_NET_CLOSED;
		}
		return XRT_NET_OK;
	}
	if ( pResolver->bStopping ) {
		xrtMutexUnlock(pResolver->pLock);
		XNET_FREE(pJob);
		return XRT_NET_CLOSED;
	}
	if ( pResolver->iQueued >= pResolver->iQueueLimit ) {
		xrtMutexUnlock(pResolver->pLock);
		XNET_FREE(pJob);
		return XRT_NET_AGAIN;
	}
	if ( pResolver->pTail ) { pResolver->pTail->pNext = pJob; }
	else { pResolver->pHead = pJob; }
	pResolver->pTail = pJob;
	pResolver->iQueued++;
	xrtCondSignal(pResolver->pCond);
	xrtMutexUnlock(pResolver->pLock);
	return XRT_NET_OK;
}


static xnet_result __xnetEngineResolveAllAsync(xnetengine* pEngine, uint32 iAffinityKey,
	const char* sHost, uint16 iPort, uint16 iFamily,
	__xnet_resolve_all_done_fn pfnDone, ptr pArg)
{
	__xnet_resolver* pResolver;
	__xnet_resolver_job* pJob;
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 ||
		!pfnDone || !sHost || !sHost[0] || iPort == 0u ) { return XRT_NET_ERROR; }
	if ( iFamily != AF_UNSPEC && iFamily != AF_INET && iFamily != AF_INET6 ) {
		return XRT_NET_ERROR;
	}
	pResolver = (__xnet_resolver*)pEngine->pResolver;
	if ( !pResolver ) { return XRT_NET_ERROR; }
	pJob = (__xnet_resolver_job*)XNET_ALLOC(sizeof(*pJob));
	if ( !pJob ) { return XRT_NET_ERROR; }
	memset(pJob, 0, sizeof(*pJob));
	pJob->pEngine = pEngine;
	pJob->iAffinityKey = iAffinityKey;
	pJob->iPort = iPort;
	pJob->iFamily = iFamily;
	pJob->pfnDoneAll = pfnDone;
	pJob->pArg = pArg;
	pJob->bResolveAll = true;
	__xnetCopyFixedString(pJob->sHost, sizeof(pJob->sHost), sHost);
	if ( strlen(sHost) >= sizeof(pJob->sHost) ) {
		XNET_FREE(pJob);
		return XRT_NET_ERROR;
	}
	xrtMutexLock(pResolver->pLock);
	if ( pResolver->bStopping ) {
		xrtMutexUnlock(pResolver->pLock);
		XNET_FREE(pJob);
		return XRT_NET_CLOSED;
	}
	if ( pResolver->iQueued >= pResolver->iQueueLimit ) {
		xrtMutexUnlock(pResolver->pLock);
		XNET_FREE(pJob);
		return XRT_NET_AGAIN;
	}
	if ( pResolver->pTail ) { pResolver->pTail->pNext = pJob; }
	else { pResolver->pHead = pJob; }
	pResolver->pTail = pJob;
	pResolver->iQueued++;
	xrtCondSignal(pResolver->pCond);
	xrtMutexUnlock(pResolver->pLock);
	return XRT_NET_OK;
}


// 内部函数：停止引擎工作线程 resources
static void __xnetEngineStopWorkerResources(xnetworker* pWorker)
{
	if ( !pWorker ) { return; }
	xrtNetPortUnit(&pWorker->tPort);
	if ( pWorker->pTimerWheel ) {
		__xnetTimerWheelUnit((__xnet_engine_timerwheel*)pWorker->pTimerWheel);
		XNET_FREE(pWorker->pTimerWheel);
		pWorker->pTimerWheel = NULL;
	}
	xrtNetMemCtxUnit(&pWorker->tMemCtx);
	if ( pWorker->pCmdQ ) {
		__xnetCmdQUnit((__xnet_engine_cmdq*)pWorker->pCmdQ);
		XNET_FREE(pWorker->pCmdQ);
		pWorker->pCmdQ = NULL;
	}
}

#if __XNET_ENGINE_USE_XRT_THREAD
	static uint32 __xnetEngineWorkerMain(ptr pArg)
#elif defined(_WIN32) || defined(_WIN64)
	static DWORD WINAPI __xnetEngineWorkerMain(LPVOID pArg)
#else
	// 内部函数：__xnetEngineWorkerMain
	static void* __xnetEngineWorkerMain(void* pArg)
#endif
{
	xnetworker* pWorker = (xnetworker*)pArg;
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	xnetportevent arrEvents[__XNET_ENGINE_HARVEST_BATCH];
	uint32 iEventCount;
	uint32 iWaitMs;
	if ( !pWorker ) {
		#if __XNET_ENGINE_USE_XRT_THREAD || defined(_WIN32) || defined(_WIN64)
		return 0;
		#else
		return NULL;
		#endif
	}

	// 记录工作线程 ID
	#if __XNET_ENGINE_USE_XRT_THREAD
	pWorker->iThreadId = xrtThreadGetCurrentId();
	#elif defined(_WIN32) || defined(_WIN64)
		pWorker->iThreadId = (uint64)GetCurrentThreadId();
	#else
		pWorker->iThreadId = (uint64)(uintptr_t)pthread_self();
	#endif

	// 工作线程主循环：处理命令 -> 收集端口事件 -> 分发事件 -> 再次处理命令
	while ( __xnetAtomicLoad32(&pWorker->bStopRequested) == 0 ) {
		(void)__xnetEngineDrainCommands(pWorker, __XNET_ENGINE_COMMAND_BUDGET);
		__xnetTimerWheelTick(pWorker);
		iWaitMs = __xnetTimerWheelGetWaitMs(pWheel);
		iEventCount = xrtNetPortHarvest(&pWorker->tPort, arrEvents, __XNET_ENGINE_HARVEST_BATCH, iWaitMs);
		__xnetEngineHandlePortEvents(pWorker, arrEvents, iEventCount);
		__xnetTimerWheelTick(pWorker);
		(void)__xnetEngineDrainCommands(pWorker, __XNET_ENGINE_COMMAND_BUDGET);
	}

	// 停止后做最后一次命令排空
	while ( __xnetEngineDrainCommands(pWorker, UINT32_MAX) != 0 ) {}
	#if __XNET_ENGINE_USE_XRT_THREAD || defined(_WIN32) || defined(_WIN64)
	return 0;
	#else
		return NULL;
	#endif
}


// 内部函数：启动引擎工作线程线程
static bool __xnetEngineStartWorkerThread(xnetworker* pWorker)
{
	if ( !pWorker ) { return false; }
	// 根据编译平台选择线程创建方式，保存句柄和线程 ID
	#if __XNET_ENGINE_USE_XRT_THREAD
		xthread pThread = xrtThreadCreate((ptr)__xnetEngineWorkerMain, pWorker, 0);
		if ( !pThread ) { return false; }
		pWorker->hThread = (ptr)pThread;
		pWorker->iThreadId = pThread->TID;
		return true;
	#elif defined(_WIN32) || defined(_WIN64)
		DWORD iThreadId = 0;
		HANDLE hThread = CreateThread(NULL, 0, __xnetEngineWorkerMain, pWorker, 0, &iThreadId);
		if ( !hThread ) { return false; }
		pWorker->hThread = (ptr)hThread;
		pWorker->iThreadId = (uint64)iThreadId;
		return true;
	#else
		pthread_t* hThread = (pthread_t*)XNET_ALLOC(sizeof(pthread_t));
		if ( !hThread ) { return false; }
		if ( pthread_create(hThread, NULL, __xnetEngineWorkerMain, pWorker) != 0 ) {
			XNET_FREE(hThread);
			return false;
		}
		pWorker->hThread = (ptr)hThread;
		pWorker->iThreadId = (uint64)(uintptr_t)(*hThread);
		return true;
	#endif
}


// 内部函数：加入引擎工作线程线程
static void __xnetEngineJoinWorkerThread(xnetworker* pWorker)
{
	if ( !pWorker || !pWorker->hThread ) { return; }
	#if __XNET_ENGINE_USE_XRT_THREAD
		xrtThreadWait((xthread)pWorker->hThread);
		xrtThreadDestroy((xthread)pWorker->hThread);
	#elif defined(_WIN32) || defined(_WIN64)
		WaitForSingleObject((HANDLE)pWorker->hThread, INFINITE);
		CloseHandle((HANDLE)pWorker->hThread);
	#else
		pthread_join(*(pthread_t*)pWorker->hThread, NULL);
		XNET_FREE(pWorker->hThread);
	#endif
	pWorker->hThread = NULL;
	memset(&pWorker->iThreadId, 0, sizeof(pWorker->iThreadId));
}


// 内部函数：启动引擎工作线程
static xnet_result __xnetEngineStartWorker(xnetworker* pWorker, const xnetengineconfig* pEngineCfg, const xnetportops* pOps, const xnetportconfig* pPortCfg, const xnetmemconfig* pMemCfg)
{
	const xnetportops* pStartOps;
	__xnet_engine_timerwheel* pWheel;
	if ( !pWorker || !pEngineCfg || !pOps || !pPortCfg || !pMemCfg ) { return XRT_NET_ERROR; }

	// 初始化命令队列
	pWorker->pCmdQ = XNET_ALLOC(sizeof(__xnet_engine_cmdq));
	if ( !pWorker->pCmdQ ) { return XRT_NET_ERROR; }
	if ( !__xnetCmdQInit((__xnet_engine_cmdq*)pWorker->pCmdQ, pEngineCfg->iCmdQueueSize) ) {
		XNET_FREE(pWorker->pCmdQ);
		pWorker->pCmdQ = NULL;
		return XRT_NET_ERROR;
	}

	// 初始化内存上下文和 I/O 端口
	xrtNetMemCtxInit(&pWorker->tMemCtx, pMemCfg);
	pStartOps = pOps;
	if ( xrtNetPortInit(&pWorker->tPort, pStartOps, pPortCfg, pWorker) != XRT_NET_OK ) {
		#if defined(__linux__)
			if ( pStartOps == xrtNetPortUringOps() && xrtNetPortEpollOps() != NULL ) {
				pStartOps = xrtNetPortEpollOps();
				if ( xrtNetPortInit(&pWorker->tPort, pStartOps, pPortCfg, pWorker) == XRT_NET_OK ) {
					goto __xnet_engine_port_ready;
				}
			}
		#endif
		if ( pStartOps != xrtNetPortSelectOps() && xrtNetPortSelectOps() != NULL ) {
			pStartOps = xrtNetPortSelectOps();
			if ( xrtNetPortInit(&pWorker->tPort, pStartOps, pPortCfg, pWorker) == XRT_NET_OK ) {
				goto __xnet_engine_port_ready;
			}
		}
		__xnetCmdQUnit((__xnet_engine_cmdq*)pWorker->pCmdQ);
		XNET_FREE(pWorker->pCmdQ);
		pWorker->pCmdQ = NULL;
		xrtNetMemCtxUnit(&pWorker->tMemCtx);
		return XRT_NET_ERROR;
	}
	__xnet_engine_port_ready:
	pWorker->tPort.pfnQuiesceEvents = __xnetEngineHandleQuiesceEvents;

	// 初始化定时器轮并挂载周期定时脉冲
	pWheel = (__xnet_engine_timerwheel*)XNET_ALLOC(sizeof(__xnet_engine_timerwheel));
	if ( !pWheel ) {
		__xnetEngineStopWorkerResources(pWorker);
		return XRT_NET_ERROR;
	}
	if ( !__xnetTimerWheelInit(pWheel, pEngineCfg->iTimerTickMs, pEngineCfg->iTimerWheelSlots) ) {
		XNET_FREE(pWheel);
		__xnetEngineStopWorkerResources(pWorker);
		return XRT_NET_ERROR;
	}
	pWorker->pTimerWheel = pWheel;

	// 设置运行标志并启动工作线程
	(void)__xnetAtomicExchange32(&pWorker->bStopRequested, 0);
	(void)__xnetAtomicExchange32(&pWorker->bRunning, 1);
	if ( !__xnetEngineStartWorkerThread(pWorker) ) {
		(void)__xnetAtomicExchange32(&pWorker->bRunning, 0);
		__xnetEngineStopWorkerResources(pWorker);
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}


// 内部函数：停止引擎工作线程
static void __xnetEngineStopWorker(xnetworker* pWorker)
{
	if ( !pWorker || __xnetAtomicLoad32(&pWorker->bRunning) == 0 ) { return; }
	(void)__xnetAtomicExchange32(&pWorker->bStopRequested, 1);
	(void)xrtNetPortWake(&pWorker->tPort);
	__xnetEngineJoinWorkerThread(pWorker);
	__xnetEngineStopWorkerResources(pWorker);
	(void)__xnetAtomicExchange32(&pWorker->bRunning, 0);
}



/* ============================== Public engine helpers ============================== */

XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg)
{
	xnetengineconfig tCfg;
	xnetengine* pEngine;
	uint32 iWorkerCount;

	xrtNetEngineConfigInit(&tCfg);
	if ( pCfg ) {
		if ( !__xnetConfigCopyKnown(&tCfg, sizeof(tCfg), pCfg, pCfg->iSize,
			pCfg->iVersion, XNET_ENGINE_CONFIG_V1_SIZE) ) {
			__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_NONE, XNET_PHASE_VALIDATE, XNET_BACKEND_NONE, 0,
				"invalid network engine config version or size.");
			return NULL;
		}
		tCfg.iSize = (uint32)sizeof(tCfg);
	}

	// 检测或使用配置中的工作线程数量
	iWorkerCount = (tCfg.iWorkerCount != 0) ? tCfg.iWorkerCount : __xnetEngineDetectWorkers();
	if ( iWorkerCount == 0 ) { iWorkerCount = 1; }

	// 分配引擎和工作线程数组
	pEngine = (xnetengine*)XNET_ALLOC(sizeof(xnetengine));
	if ( !pEngine ) { return NULL; }
	memset(pEngine, 0, sizeof(xnetengine));
	pEngine->arrWorkers = (xnetworker*)XNET_ALLOC(sizeof(xnetworker) * iWorkerCount);
	if ( !pEngine->arrWorkers ) {
		XNET_FREE(pEngine);
		return NULL;
	}

	memset(pEngine->arrWorkers, 0, sizeof(xnetworker) * iWorkerCount);
	pEngine->tConfig = tCfg;
	pEngine->iWorkerCount = iWorkerCount;
	pEngine->iNextStreamId = 1;
	pEngine->iNextTimerId = 1;

	// 建立工作线程到引擎的反向引用
	for ( uint32 i = 0; i < iWorkerCount; ++i ) {
		pEngine->arrWorkers[i].pEngine = pEngine;
		pEngine->arrWorkers[i].iId = i;
	}

	return pEngine;
}


// 销毁网络引擎
XXAPI void xrtNetEngineDestroy(xnetengine* pEngine)
{
	if ( !pEngine ) { return; }
	if ( __xnetAtomicLoad32(&pEngine->bRunning) != 0 ) {
		__xnetResolverStop(pEngine);
		for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
			__xnetEngineStopWorker(&pEngine->arrWorkers[i]);
		}
		(void)__xnetAtomicExchange32(&pEngine->bRunning, 0);
	}
	if ( __xnetAtomicLoad32(&pEngine->iLiveObjects) != 0 ) {
		__xnetErrorSetEx(XRT_NET_ERROR, XNET_OP_CLOSE, XNET_PHASE_VALIDATE,
			XNET_BACKEND_NONE, 0, "cannot destroy network engine while network objects are still alive.");
		return;
	}
	if ( pEngine->arrWorkers ) {
		XNET_FREE(pEngine->arrWorkers);
	}
	XNET_FREE(pEngine);
}


// 启动网络引擎
XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine)
{
	const xnetportops* pOps;
	xnetportconfig tPortCfg;
	xnetmemconfig tMemCfg;

	if ( !pEngine ) { return XRT_NET_ERROR; }
	if ( __xnetAtomicLoad32(&pEngine->bRunning) != 0 ) { return XRT_NET_OK; }

	// 获取平台默认端口操作接口（IOCP/io_uring）
	pOps = __xnetEngineDefaultPortOps();
	if ( !pOps ) { return XRT_NET_ERROR; }

	// 初始化端口和内存配置
	__xnetEngineInitPortConfig(&tPortCfg, &pEngine->tConfig);
	__xnetEngineInitMemConfig(&tMemCfg, &pEngine->tConfig);

	// 依次启动所有工作线程，失败时回滚已启动的线程
	for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
		if ( __xnetEngineStartWorker(&pEngine->arrWorkers[i], &pEngine->tConfig, pOps, &tPortCfg, &tMemCfg) != XRT_NET_OK ) {
			for ( uint32 j = 0; j < i; ++j ) {
				__xnetEngineStopWorker(&pEngine->arrWorkers[j]);
			}
			return XRT_NET_ERROR;
		}
	}

	(void)__xnetAtomicExchange32(&pEngine->bRunning, 1);
	if ( !__xnetResolverStart(pEngine) ) {
		for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
			__xnetEngineStopWorker(&pEngine->arrWorkers[i]);
		}
		(void)__xnetAtomicExchange32(&pEngine->bRunning, 0);
		return XRT_NET_ERROR;
	}
	return XRT_NET_OK;
}


// 停止网络引擎
XXAPI void xrtNetEngineStop(xnetengine* pEngine)
{
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 ) { return; }
	__xnetResolverStop(pEngine);
	for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
		__xnetEngineStopWorker(&pEngine->arrWorkers[i]);
	}
	(void)__xnetAtomicExchange32(&pEngine->bRunning, 0);
}


// 统计网络引擎 get 工作线程
XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine)
{
	return pEngine ? pEngine->iWorkerCount : 0;
}


// xrtNetEnginePost 相关处理
XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg)
{
	xnetworker* pWorker;
	__xnet_engine_cmdq* pQ;
	xqueue_result iPushRet;
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 || !pfnTask || pEngine->iWorkerCount == 0 ) {
		return XRT_NET_ERROR;
	}

	// 根据亲和键取模选择目标工作线程
	pWorker = &pEngine->arrWorkers[iAffinityKey % pEngine->iWorkerCount];
	if ( __xnetAtomicLoad32(&pWorker->bRunning) == 0 || !pWorker->pCmdQ ) {
		return XRT_NET_ERROR;
	}

	// 将任务压入命令队列
	pQ = (__xnet_engine_cmdq*)pWorker->pCmdQ;
	iPushRet = __xnetCmdQPush(pQ, pfnTask, pArg);
	if ( iPushRet == XQUEUE_FULL ) {
		return XRT_NET_AGAIN;
	}
	if ( iPushRet != XQUEUE_OK ) {
		return XRT_NET_ERROR;
	}

	// 唤醒工作线程端口以加速任务处理
	if ( xrtNetPortWake(&pWorker->tPort) != XRT_NET_OK ) {
		/*
		    The queue already owns the task node at this point.
		    Fall back to harvest timeout progress instead of rolling back.
		*/
	}

	return XRT_NET_OK;
}


// 网络引擎 post 延迟相关处理
static xnet_result __xnetEngineScheduleTimerOwned(xnetengine* pEngine, uint32 iAffinityKey,
	uint32 iDelayMs, xnet_task_fn pfnTask, __xnet_task_discard_fn pfnDiscard,
	ptr pArg, uint64* pTimerId)
{
	xnetworker* pWorker;
	__xnet_engine_cmdq* pQ;
	xqueue_result iPushRet;
	uint64 iNewTimerId;
	if ( pTimerId ) { *pTimerId = 0; }
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 || !pfnTask || pEngine->iWorkerCount == 0 ) {
		return XRT_NET_ERROR;
	}

	// 根据亲和键取模选择目标工作线程
	pWorker = &pEngine->arrWorkers[iAffinityKey % pEngine->iWorkerCount];
	if ( __xnetAtomicLoad32(&pWorker->bRunning) == 0 || !pWorker->pCmdQ || !pWorker->pTimerWheel ) {
		return XRT_NET_ERROR;
	}

	// 压入定时器添加命令到命令队列
	pQ = (__xnet_engine_cmdq*)pWorker->pCmdQ;
	iNewTimerId = __xnetEngineAllocTimerId(pEngine, pWorker->iId);
	if ( iNewTimerId == 0 ) { return XRT_NET_ERROR; }
	iPushRet = __xnetCmdQPushEx(pQ, __XNET_ENGINE_CMD_TIMER_ADD, iDelayMs, iNewTimerId,
		pfnTask, pfnDiscard, pArg);
	if ( iPushRet == XQUEUE_FULL ) {
		return XRT_NET_AGAIN;
	}
	if ( iPushRet != XQUEUE_OK ) {
		return XRT_NET_ERROR;
	}

	(void)xrtNetPortWake(&pWorker->tPort);
	if ( pTimerId ) { *pTimerId = iNewTimerId; }
	return XRT_NET_OK;
}


XXAPI xnet_result xrtNetEngineScheduleTimer(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs,
	xnet_task_fn pfnTask, ptr pArg, uint64* pTimerId)
{
	return __xnetEngineScheduleTimerOwned(pEngine, iAffinityKey, iDelayMs,
		pfnTask, NULL, pArg, pTimerId);
}


XXAPI uint32 xrtNetEngineGetLiveObjectCount(const xnetengine* pEngine)
{
	long iCount = pEngine ? __xnetAtomicLoad32(&pEngine->iLiveObjects) : 0;
	return iCount > 0 ? (uint32)iCount : 0u;
}


XXAPI xnet_result xrtNetEngineCancelTimer(xnetengine* pEngine, uint64 iTimerId)
{
	xnetworker* pWorker;
	__xnet_engine_cmdq* pQ;
	xqueue_result iPushRet;
	uint32 iWorkerId;
	if ( !pEngine || __xnetAtomicLoad32(&pEngine->bRunning) == 0 || iTimerId == 0 || pEngine->iWorkerCount == 0 ) {
		return XRT_NET_ERROR;
	}
	iWorkerId = (uint32)(iTimerId % (uint64)pEngine->iWorkerCount);
	pWorker = &pEngine->arrWorkers[iWorkerId];
	if ( __xnetAtomicLoad32(&pWorker->bRunning) == 0 || !pWorker->pCmdQ || !pWorker->pTimerWheel ) {
		return XRT_NET_ERROR;
	}
	pQ = (__xnet_engine_cmdq*)pWorker->pCmdQ;
	iPushRet = __xnetCmdQPushEx(pQ, __XNET_ENGINE_CMD_TIMER_CANCEL, 0, iTimerId, NULL, NULL, NULL);
	if ( iPushRet == XQUEUE_FULL ) { return XRT_NET_AGAIN; }
	if ( iPushRet != XQUEUE_OK ) { return XRT_NET_ERROR; }
	(void)xrtNetPortWake(&pWorker->tPort);
	return XRT_NET_OK;
}


XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs,
	xnet_task_fn pfnTask, ptr pArg)
{
	return xrtNetEngineScheduleTimer(pEngine, iAffinityKey, iDelayMs, pfnTask, pArg, NULL);
}


#endif
