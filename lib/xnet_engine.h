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

typedef struct __xnet_engine_cmd {
	struct __xnet_engine_cmd* pNext;
	uint32 iType;
	uint32 iDelayMs;
	xnet_task_fn pfnTask;
	ptr pArg;
} __xnet_engine_cmd;

typedef struct __xnet_engine_timer {
	struct __xnet_engine_timer* pNext;
	uint64 iDueTick;
	xnet_task_fn pfnTask;
	ptr pArg;
} __xnet_engine_timer;

typedef void (*xnet_port_event_fn)(xnetworker* pWorker, const xnetportevent* pEvents, uint32 iCount);

#if defined(_WIN32) || defined(_WIN64)
	typedef CRITICAL_SECTION __xnet_mutex;
#else
	typedef pthread_mutex_t __xnet_mutex;
#endif

#if defined(XXRTL_CORE) && !defined(XRT_NO_THREAD)
	#define __XNET_ENGINE_USE_XRT_THREAD	1
#else
	#define __XNET_ENGINE_USE_XRT_THREAD	0
#endif

typedef struct {
	__xnet_engine_cmd* pHead;
	__xnet_engine_cmd* pTail;
	__xnet_mutex tLock;
} __xnet_engine_cmdq;

typedef struct {
	uint32 iTickMs;
	uint32 iSlotCount;
	uint64 iCurrentTick;
	__xnet_engine_timer** arrSlots;
} __xnet_engine_timerwheel;

#define __XNET_ENGINE_CMD_TASK       1u
#define __XNET_ENGINE_CMD_TIMER_ADD  2u
#define __XNET_ENGINE_TIMER_PULSE_ID UINT64_C(0xffffffffffffffff)
#define __XNET_ENGINE_HARVEST_BATCH  128u



/* ============================== Public object layout ============================== */

struct xrt_net_worker {
	xnetengine* pEngine;
	uint32 iId;
	ptr hThread;
	uint64 iThreadId;
	volatile bool bRunning;
	volatile bool bStopRequested;
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
	xnet_port_event_fn pfnOnPortEvent;
	xnet_port_event_fn pfnOnPortEvent2;
	volatile bool bRunning;
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

static const xnetportops* __xnetEngineDefaultPortOps(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return xrtNetPortIOCPOps();
	#elif defined(__linux__)
		return xrtNetPortUringOps();
	#else
		return NULL;
	#endif
}

static void __xnetEngineSleepMs(uint32 iMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iMs);
	#else
		usleep((useconds_t)iMs * 1000u);
	#endif
}

static bool __xnetEngineIsCurrentWorker(xnetworker* pWorker)
{
	if ( !pWorker || !pWorker->bRunning || !pWorker->hThread ) return false;
	#if __XNET_ENGINE_USE_XRT_THREAD
		return xrtThreadGetCurrentId() == pWorker->iThreadId;
	#elif defined(_WIN32) || defined(_WIN64)
		return (uint64)GetCurrentThreadId() == pWorker->iThreadId;
	#else
		return (uint64)(uintptr_t)pthread_self() == pWorker->iThreadId;
	#endif
}

static void __xnetEngineInitMemConfig(xnetmemconfig* pMemCfg, const xnetengineconfig* pCfg)
{
	if ( !pMemCfg || !pCfg ) return;
	xrtNetMemConfigInit(pMemCfg);
	pMemCfg->iSmallBlockSize = pCfg->iSmallBlockSize;
	pMemCfg->iMediumBlockSize = pCfg->iMediumBlockSize;
	pMemCfg->iLargeBlockSize = pCfg->iLargeBlockSize;
	pMemCfg->iSmallCacheLimit = pCfg->iBlockCachePerWorker;
	pMemCfg->iMediumCacheLimit = pCfg->iBlockCachePerWorker / 2u;
	pMemCfg->iLargeCacheLimit = pCfg->iBlockCachePerWorker / 4u;
	if ( pMemCfg->iMediumCacheLimit == 0 ) pMemCfg->iMediumCacheLimit = 1;
	if ( pMemCfg->iLargeCacheLimit == 0 ) pMemCfg->iLargeCacheLimit = 1;
}

static void __xnetEngineInitPortConfig(xnetportconfig* pPortCfg, const xnetengineconfig* pCfg)
{
	if ( !pPortCfg || !pCfg ) return;
	xrtNetPortConfigInit(pPortCfg);
	pPortCfg->iSqEntries = pCfg->iSqEntries;
	pPortCfg->iCqEntries = pCfg->iCqEntries;
	pPortCfg->iAcceptBatch = pCfg->iAcceptBatch;
}



/* ============================== Internal queue helpers ============================== */

static bool __xnetCmdQInit(__xnet_engine_cmdq* pQ)
{
	if ( !pQ ) return false;
	memset(pQ, 0, sizeof(__xnet_engine_cmdq));
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pQ->tLock);
		return true;
	#else
		return pthread_mutex_init(&pQ->tLock, NULL) == 0;
	#endif
}

static void __xnetCmdQUnit(__xnet_engine_cmdq* pQ)
{
	__xnet_engine_cmd* pNode;
	if ( !pQ ) return;
	for ( ;; ) {
		pNode = pQ->pHead;
		if ( !pNode ) break;
		pQ->pHead = pNode->pNext;
		XNET_FREE(pNode);
	}
	pQ->pTail = NULL;
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection(&pQ->tLock);
	#else
		pthread_mutex_destroy(&pQ->tLock);
	#endif
}

static void __xnetCmdQLock(__xnet_engine_cmdq* pQ)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pQ->tLock);
	#else
		pthread_mutex_lock(&pQ->tLock);
	#endif
}

static void __xnetCmdQUnlock(__xnet_engine_cmdq* pQ)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pQ->tLock);
	#else
		pthread_mutex_unlock(&pQ->tLock);
	#endif
}

static bool __xnetCmdQPushEx(__xnet_engine_cmdq* pQ, uint32 iType, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);

static bool __xnetCmdQPush(__xnet_engine_cmdq* pQ, xnet_task_fn pfnTask, ptr pArg)
{
	return __xnetCmdQPushEx(pQ, __XNET_ENGINE_CMD_TASK, 0, pfnTask, pArg);
}

static bool __xnetCmdQPushEx(__xnet_engine_cmdq* pQ, uint32 iType, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg)
{
	__xnet_engine_cmd* pCmd;
	if ( !pQ || !pfnTask ) return false;
	pCmd = (__xnet_engine_cmd*)XNET_ALLOC(sizeof(__xnet_engine_cmd));
	if ( !pCmd ) return false;
	memset(pCmd, 0, sizeof(__xnet_engine_cmd));
	pCmd->iType = iType;
	pCmd->iDelayMs = iDelayMs;
	pCmd->pfnTask = pfnTask;
	pCmd->pArg = pArg;

	__xnetCmdQLock(pQ);
	if ( pQ->pTail ) {
		pQ->pTail->pNext = pCmd;
	} else {
		pQ->pHead = pCmd;
	}
	pQ->pTail = pCmd;
	__xnetCmdQUnlock(pQ);
	return true;
}

static __xnet_engine_cmd* __xnetCmdQPopAll(__xnet_engine_cmdq* pQ)
{
	__xnet_engine_cmd* pHead;
	if ( !pQ ) return NULL;
	__xnetCmdQLock(pQ);
	pHead = pQ->pHead;
	pQ->pHead = NULL;
	pQ->pTail = NULL;
	__xnetCmdQUnlock(pQ);
	return pHead;
}



/* ============================== Internal timer wheel helpers ============================== */

static bool __xnetTimerWheelInit(__xnet_engine_timerwheel* pWheel, uint32 iTickMs, uint32 iSlotCount)
{
	if ( !pWheel ) return false;
	memset(pWheel, 0, sizeof(__xnet_engine_timerwheel));
	pWheel->iTickMs = (iTickMs == 0) ? 10u : iTickMs;
	pWheel->iSlotCount = (iSlotCount == 0) ? 256u : iSlotCount;
	pWheel->arrSlots = (__xnet_engine_timer**)XNET_ALLOC(sizeof(__xnet_engine_timer*) * pWheel->iSlotCount);
	if ( !pWheel->arrSlots ) return false;
	memset(pWheel->arrSlots, 0, sizeof(__xnet_engine_timer*) * pWheel->iSlotCount);
	return true;
}

static void __xnetTimerWheelUnit(__xnet_engine_timerwheel* pWheel)
{
	if ( !pWheel ) return;
	if ( pWheel->arrSlots ) {
		for ( uint32 i = 0; i < pWheel->iSlotCount; ++i ) {
			__xnet_engine_timer* pNode = pWheel->arrSlots[i];
			while ( pNode ) {
				__xnet_engine_timer* pNext = pNode->pNext;
				XNET_FREE(pNode);
				pNode = pNext;
			}
		}
		XNET_FREE(pWheel->arrSlots);
	}
	memset(pWheel, 0, sizeof(__xnet_engine_timerwheel));
}

static bool __xnetTimerWheelSchedule(__xnet_engine_timerwheel* pWheel, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg)
{
	uint64 iDeltaTicks;
	uint64 iDueTick;
	uint32 iSlot;
	__xnet_engine_timer* pTimer;
	if ( !pWheel || !pWheel->arrSlots || !pfnTask ) return false;

	iDeltaTicks = (iDelayMs == 0) ? 1u : ((uint64)iDelayMs + pWheel->iTickMs - 1u) / pWheel->iTickMs;
	if ( iDeltaTicks == 0 ) iDeltaTicks = 1;
	iDueTick = pWheel->iCurrentTick + iDeltaTicks;
	iSlot = (uint32)(iDueTick % pWheel->iSlotCount);

	pTimer = (__xnet_engine_timer*)XNET_ALLOC(sizeof(__xnet_engine_timer));
	if ( !pTimer ) return false;
	memset(pTimer, 0, sizeof(__xnet_engine_timer));
	pTimer->iDueTick = iDueTick;
	pTimer->pfnTask = pfnTask;
	pTimer->pArg = pArg;
	pTimer->pNext = pWheel->arrSlots[iSlot];
	pWheel->arrSlots[iSlot] = pTimer;
	return true;
}

static void __xnetTimerWheelTick(xnetworker* pWorker)
{
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	__xnet_engine_timer* pList;
	__xnet_engine_timer* pDeferred = NULL;
	if ( !pWheel || !pWheel->arrSlots ) return;

	pWheel->iCurrentTick++;
	pList = pWheel->arrSlots[pWheel->iCurrentTick % pWheel->iSlotCount];
	pWheel->arrSlots[pWheel->iCurrentTick % pWheel->iSlotCount] = NULL;

	while ( pList ) {
		__xnet_engine_timer* pNext = pList->pNext;
		if ( pList->iDueTick <= pWheel->iCurrentTick ) {
			if ( pList->pfnTask ) {
				pList->pfnTask(pWorker, pList->pArg);
			}
			XNET_FREE(pList);
		} else {
			pList->pNext = pDeferred;
			pDeferred = pList;
		}
		pList = pNext;
	}

	while ( pDeferred ) {
		__xnet_engine_timer* pNext = pDeferred->pNext;
		uint32 iSlot = (uint32)(pDeferred->iDueTick % pWheel->iSlotCount);
		pDeferred->pNext = pWheel->arrSlots[iSlot];
		pWheel->arrSlots[iSlot] = pDeferred;
		pDeferred = pNext;
	}
}

static void __xnetEngineHandlePortEvents(xnetworker* pWorker, xnetportevent* pEvents, uint32 iCount)
{
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	if ( !pWorker || !pEvents || iCount == 0 ) return;
	for ( uint32 i = 0; i < iCount; ++i ) {
		if ( pEvents[i].iType == XNET_PORT_EVENT_TIMER && pEvents[i].iOpId == __XNET_ENGINE_TIMER_PULSE_ID ) {
			__xnetTimerWheelTick(pWorker);
			if ( !pWorker->bStopRequested && pWheel ) {
				(void)xrtNetPortArmTimer(&pWorker->tPort, __XNET_ENGINE_TIMER_PULSE_ID, pWheel->iTickMs);
			}
		}
	}
	if ( pWorker->pEngine && pWorker->pEngine->pfnOnPortEvent ) {
		pWorker->pEngine->pfnOnPortEvent(pWorker, pEvents, iCount);
	}
	if ( pWorker->pEngine && pWorker->pEngine->pfnOnPortEvent2 &&
		pWorker->pEngine->pfnOnPortEvent2 != pWorker->pEngine->pfnOnPortEvent ) {
		pWorker->pEngine->pfnOnPortEvent2(pWorker, pEvents, iCount);
	}
}


/* ============================== Internal worker helpers ============================== */

static void __xnetEngineDrainCommands(xnetworker* pWorker)
{
	__xnet_engine_cmdq* pQ = pWorker ? (__xnet_engine_cmdq*)pWorker->pCmdQ : NULL;
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	__xnet_engine_cmd* pNode;
	if ( !pQ ) return;
	pNode = __xnetCmdQPopAll(pQ);
	while ( pNode ) {
		__xnet_engine_cmd* pNext = pNode->pNext;
		if ( pNode->iType == __XNET_ENGINE_CMD_TIMER_ADD ) {
			(void)__xnetTimerWheelSchedule(pWheel, pNode->iDelayMs, pNode->pfnTask, pNode->pArg);
		} else if ( pNode->pfnTask ) {
			pNode->pfnTask(pWorker, pNode->pArg);
		}
		XNET_FREE(pNode);
		pNode = pNext;
	}
}

static void __xnetEngineStopWorkerResources(xnetworker* pWorker)
{
	if ( !pWorker ) return;
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
static void* __xnetEngineWorkerMain(void* pArg)
#endif
{
	xnetworker* pWorker = (xnetworker*)pArg;
	__xnet_engine_timerwheel* pWheel = pWorker ? (__xnet_engine_timerwheel*)pWorker->pTimerWheel : NULL;
	xnetportevent arrEvents[__XNET_ENGINE_HARVEST_BATCH];
	uint32 iEventCount;
	if ( !pWorker ) {
		#if __XNET_ENGINE_USE_XRT_THREAD || defined(_WIN32) || defined(_WIN64)
		return 0;
		#else
		return NULL;
		#endif
	}

	#if __XNET_ENGINE_USE_XRT_THREAD
	pWorker->iThreadId = xrtThreadGetCurrentId();
	#elif defined(_WIN32) || defined(_WIN64)
		pWorker->iThreadId = (uint64)GetCurrentThreadId();
	#else
		pWorker->iThreadId = (uint64)(uintptr_t)pthread_self();
	#endif

	while ( !pWorker->bStopRequested ) {
		__xnetEngineDrainCommands(pWorker);
		iEventCount = xrtNetPortHarvest(&pWorker->tPort, arrEvents, __XNET_ENGINE_HARVEST_BATCH, pWheel ? pWheel->iTickMs : 50u);
		__xnetEngineHandlePortEvents(pWorker, arrEvents, iEventCount);
		__xnetEngineDrainCommands(pWorker);
	}

	__xnetEngineDrainCommands(pWorker);
	#if __XNET_ENGINE_USE_XRT_THREAD || defined(_WIN32) || defined(_WIN64)
	return 0;
	#else
		return NULL;
	#endif
}

static bool __xnetEngineStartWorkerThread(xnetworker* pWorker)
{
	if ( !pWorker ) return false;
	#if __XNET_ENGINE_USE_XRT_THREAD
		xthread pThread = xrtThreadCreate((ptr)__xnetEngineWorkerMain, pWorker, 0);
		if ( !pThread ) return false;
		pWorker->hThread = (ptr)pThread;
		pWorker->iThreadId = pThread->TID;
		return true;
	#elif defined(_WIN32) || defined(_WIN64)
		DWORD iThreadId = 0;
		HANDLE hThread = CreateThread(NULL, 0, __xnetEngineWorkerMain, pWorker, 0, &iThreadId);
		if ( !hThread ) return false;
		pWorker->hThread = (ptr)hThread;
		pWorker->iThreadId = (uint64)iThreadId;
		return true;
	#else
		pthread_t* hThread = (pthread_t*)XNET_ALLOC(sizeof(pthread_t));
		if ( !hThread ) return false;
		if ( pthread_create(hThread, NULL, __xnetEngineWorkerMain, pWorker) != 0 ) {
			XNET_FREE(hThread);
			return false;
		}
		pWorker->hThread = (ptr)hThread;
		pWorker->iThreadId = (uint64)(uintptr_t)(*hThread);
		return true;
	#endif
}

static void __xnetEngineJoinWorkerThread(xnetworker* pWorker)
{
	if ( !pWorker || !pWorker->hThread ) return;
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

static xnet_result __xnetEngineStartWorker(xnetworker* pWorker, const xnetengineconfig* pEngineCfg, const xnetportops* pOps, const xnetportconfig* pPortCfg, const xnetmemconfig* pMemCfg)
{
	__xnet_engine_timerwheel* pWheel;
	if ( !pWorker || !pEngineCfg || !pOps || !pPortCfg || !pMemCfg ) return XRT_NET_ERROR;

	pWorker->pCmdQ = XNET_ALLOC(sizeof(__xnet_engine_cmdq));
	if ( !pWorker->pCmdQ ) return XRT_NET_ERROR;
	if ( !__xnetCmdQInit((__xnet_engine_cmdq*)pWorker->pCmdQ) ) {
		XNET_FREE(pWorker->pCmdQ);
		pWorker->pCmdQ = NULL;
		return XRT_NET_ERROR;
	}

	xrtNetMemCtxInit(&pWorker->tMemCtx, pMemCfg);
	if ( xrtNetPortInit(&pWorker->tPort, pOps, pPortCfg, pWorker) != XRT_NET_OK ) {
		__xnetCmdQUnit((__xnet_engine_cmdq*)pWorker->pCmdQ);
		XNET_FREE(pWorker->pCmdQ);
		pWorker->pCmdQ = NULL;
		xrtNetMemCtxUnit(&pWorker->tMemCtx);
		return XRT_NET_ERROR;
	}

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
	if ( xrtNetPortArmTimer(&pWorker->tPort, __XNET_ENGINE_TIMER_PULSE_ID, pWheel->iTickMs) != XRT_NET_OK ) {
		__xnetEngineStopWorkerResources(pWorker);
		return XRT_NET_ERROR;
	}

	pWorker->bStopRequested = false;
	pWorker->bRunning = true;
	if ( !__xnetEngineStartWorkerThread(pWorker) ) {
		pWorker->bRunning = false;
		__xnetEngineStopWorkerResources(pWorker);
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}

static void __xnetEngineStopWorker(xnetworker* pWorker)
{
	if ( !pWorker || !pWorker->bRunning ) return;
	pWorker->bStopRequested = true;
	(void)xrtNetPortWake(&pWorker->tPort);
	__xnetEngineJoinWorkerThread(pWorker);
	__xnetEngineStopWorkerResources(pWorker);
	pWorker->bRunning = false;
}



/* ============================== Public engine helpers ============================== */

XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg)
{
	xnetengineconfig tCfg;
	xnetengine* pEngine;
	uint32 iWorkerCount;

	if ( pCfg ) {
		tCfg = *pCfg;
	} else {
		xrtNetEngineConfigInit(&tCfg);
	}

	iWorkerCount = (tCfg.iWorkerCount != 0) ? tCfg.iWorkerCount : __xnetEngineDetectWorkers();
	if ( iWorkerCount == 0 ) iWorkerCount = 1;

	pEngine = (xnetengine*)XNET_ALLOC(sizeof(xnetengine));
	if ( !pEngine ) return NULL;
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

	for ( uint32 i = 0; i < iWorkerCount; ++i ) {
		pEngine->arrWorkers[i].pEngine = pEngine;
		pEngine->arrWorkers[i].iId = i;
	}

	return pEngine;
}

XXAPI void xrtNetEngineDestroy(xnetengine* pEngine)
{
	if ( !pEngine ) return;
	if ( pEngine->bRunning ) {
		for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
			__xnetEngineStopWorker(&pEngine->arrWorkers[i]);
		}
		pEngine->bRunning = false;
	}
	if ( pEngine->arrWorkers ) {
		XNET_FREE(pEngine->arrWorkers);
	}
	XNET_FREE(pEngine);
}

XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine)
{
	const xnetportops* pOps;
	xnetportconfig tPortCfg;
	xnetmemconfig tMemCfg;

	if ( !pEngine ) return XRT_NET_ERROR;
	if ( pEngine->bRunning ) return XRT_NET_OK;

	pOps = __xnetEngineDefaultPortOps();
	if ( !pOps ) return XRT_NET_ERROR;

	__xnetEngineInitPortConfig(&tPortCfg, &pEngine->tConfig);
	__xnetEngineInitMemConfig(&tMemCfg, &pEngine->tConfig);

	for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
		if ( __xnetEngineStartWorker(&pEngine->arrWorkers[i], &pEngine->tConfig, pOps, &tPortCfg, &tMemCfg) != XRT_NET_OK ) {
			for ( uint32 j = 0; j < i; ++j ) {
				__xnetEngineStopWorker(&pEngine->arrWorkers[j]);
			}
			return XRT_NET_ERROR;
		}
	}

	pEngine->bRunning = true;
	return XRT_NET_OK;
}

XXAPI void xrtNetEngineStop(xnetengine* pEngine)
{
	if ( !pEngine || !pEngine->bRunning ) return;
	for ( uint32 i = 0; i < pEngine->iWorkerCount; ++i ) {
		__xnetEngineStopWorker(&pEngine->arrWorkers[i]);
	}
	pEngine->bRunning = false;
}

XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine)
{
	return pEngine ? pEngine->iWorkerCount : 0;
}

XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey, xnet_task_fn pfnTask, ptr pArg)
{
	xnetworker* pWorker;
	__xnet_engine_cmdq* pQ;
	if ( !pEngine || !pEngine->bRunning || !pfnTask || pEngine->iWorkerCount == 0 ) {
		return XRT_NET_ERROR;
	}

	pWorker = &pEngine->arrWorkers[iAffinityKey % pEngine->iWorkerCount];
	if ( !pWorker->bRunning || !pWorker->pCmdQ ) {
		return XRT_NET_ERROR;
	}

	pQ = (__xnet_engine_cmdq*)pWorker->pCmdQ;
	if ( !__xnetCmdQPush(pQ, pfnTask, pArg) ) {
		return XRT_NET_ERROR;
	}

	if ( xrtNetPortWake(&pWorker->tPort) != XRT_NET_OK ) {
		/*
		    The queue already owns the task node at this point.
		    Fall back to harvest timeout progress instead of rolling back.
		*/
	}

	return XRT_NET_OK;
}

XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg)
{
	xnetworker* pWorker;
	__xnet_engine_cmdq* pQ;
	if ( !pEngine || !pEngine->bRunning || !pfnTask || pEngine->iWorkerCount == 0 ) {
		return XRT_NET_ERROR;
	}

	pWorker = &pEngine->arrWorkers[iAffinityKey % pEngine->iWorkerCount];
	if ( !pWorker->bRunning || !pWorker->pCmdQ || !pWorker->pTimerWheel ) {
		return XRT_NET_ERROR;
	}

	pQ = (__xnet_engine_cmdq*)pWorker->pCmdQ;
	if ( !__xnetCmdQPushEx(pQ, __XNET_ENGINE_CMD_TIMER_ADD, iDelayMs, pfnTask, pArg) ) {
		return XRT_NET_ERROR;
	}

	(void)xrtNetPortWake(&pWorker->tPort);
	return XRT_NET_OK;
}


#endif
