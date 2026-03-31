#ifndef XRT_XNET_SYNC_H
#define XRT_XNET_SYNC_H

/*
    XRT mainline synchronous wait and convenience runtime.

    This header provides:
      - future objects with thread and coroutine wait support
      - monotonic timed-wait semantics for sync wrappers
      - wait-source helpers used by stream, listener, and datagram sync paths
      - a hidden convenience engine used by synchronous network facades
*/


/* ============================== Local sync primitives ============================== */

#define XNET_WAIT_INFINITE UINT32_C(0xffffffff)

#if defined(_WIN32) || defined(_WIN64)
	typedef CRITICAL_SECTION __xnet_sync_mutex;
	typedef CONDITION_VARIABLE __xnet_sync_cond;
#else
	typedef pthread_mutex_t __xnet_sync_mutex;
	typedef pthread_cond_t __xnet_sync_cond;
#if defined(CLOCK_MONOTONIC)
	#define __XNET_SYNC_WAIT_CLOCK CLOCK_MONOTONIC
#else
	#define __XNET_SYNC_WAIT_CLOCK CLOCK_REALTIME
#endif
#endif

typedef struct {
	volatile long iLock;
	xnetengine* pEngine;
} __xnet_sync_hidden_state;

typedef bool (*__xnet_future_pending_cancel_fn)(struct xrt_net_future* pFuture);
typedef void (*__xnet_future_pending_cleanup_fn)(struct xrt_net_future* pFuture);

struct xrt_net_future {
	__xnet_sync_mutex hLock;
	__xnet_sync_cond hCond;
	volatile bool bDone;
	volatile long iRefCount;
	xnet_result iStatus;
	ptr pValue;
	const char* sError;
	uint32 iResultFlags;
	uint64 iCreateTimeMs;
	uint64 iCompleteTimeMs;
	const char* sDebugName;
	volatile long iPendingContCount;
	int iGroupSourceIndex;
	xfuture* pGroupSource;
	volatile long iAsyncHoldCount;
	ptr pPendingCtx;
	__xnet_future_pending_cancel_fn pfnPendingCancel;
	__xnet_future_pending_cleanup_fn pfnPendingCleanup;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCoEvent;
		volatile long iCoWaitActive;
	#endif
	#if defined(XXRTL_CORE)
		struct xrt_future_cont* pContHead;
		struct xrt_future_cont* pContTail;
	#endif
};

#if defined(XXRTL_CORE)
struct xrt_promise {
	xfuture* pFuture;
	volatile long bCompleted;
};

struct xrt_task_group {
	xmutex pLock;
	xfuture** arrFuture;
	int iCount;
	int iCapacity;
	bool bClosed;
	int iParentBindCount;
	xfuture* pScopeFuture;
	xpromise* pScopePromise;
	xfuture* pJoinFuture;
	xpromise* pJoinPromise;
	volatile long iRefCount;
};

struct xrt_task {
	xtask_state iState;
	xfuture* pFuture;
	xpromise* pPromise;
	xtask_engine_fn pfnEngineTask;
	xtask_thread_fn pfnThreadTask;
	xtask_co_fn pfnCoTask;
	ptr pArg;
	xthread pThread;
	xcoro pCo;
};
#endif

#if !defined(XRT_BUILD_CORE)
typedef xnet_result (*xnet_future_task_fn)(xnetworker* pWorker, ptr pArg, ptr* ppValue);
#endif

typedef struct {
	xnetfuture* pFuture;
	xnet_future_task_fn pfnTask;
	ptr pArg;
} __xnet_future_task_ctx;

#if defined(XXRTL_CORE)
typedef struct {
	xtask* pTask;
} __xnet_task_ctx;

typedef struct {
	xtask* pTask;
} __xnet_task_thread_ctx;

typedef struct {
	xtask* pTask;
} __xnet_task_co_ctx;

typedef enum {
	__XNET_FCONT_THEN = 1,
	__XNET_FCONT_CATCH = 2,
	__XNET_FCONT_FINALLY = 3
} __xnet_future_cont_kind;

typedef enum {
	__XNET_FCONT_EXEC_INLINE = 1,
	__XNET_FCONT_EXEC_CURRENT = 2,
	__XNET_FCONT_EXEC_ENGINE = 3,
	__XNET_FCONT_EXEC_CO = 4
} __xnet_future_cont_exec;

typedef struct xrt_future_cont {
	struct xrt_future_cont* pNext;
	__xnet_future_cont_kind iKind;
	__xnet_future_cont_exec iExec;
	xfuture* pSource;
	xpromise* pPromise;
	xfuture_cont_fn pfnCont;
	xfuture_finally_fn pfnFinally;
	ptr pArg;
	xnetengine* pEngine;
	uint32 iAffinityKey;
	xcosched* pSched;
	size_t iStackSize;
	xfuture_result tInput;
} xrt_future_cont;

typedef struct {
	xrt_future_cont* pCont;
} __xnet_future_cont_ctx;

typedef enum {
	__XNET_FGROUP_ANY = 1,
	__XNET_FGROUP_ALL = 2,
	__XNET_FGROUP_RACE = 3
} __xnet_future_group_mode;

typedef struct __xnet_future_group_ctx __xnet_future_group_ctx;
typedef struct {
	xtaskgroup* pGroup;
} __xnet_task_group_parent_ctx;

typedef struct {
	xtaskgroup* pGroup;
} __xnet_task_group_future_ctx;

typedef struct {
	__xnet_future_group_ctx* pGroup;
	int iIndex;
} __xnet_future_group_item;

struct __xnet_future_group_ctx {
	__xnet_future_group_mode iMode;
	xmutex pLock;
	int iCount;
	int iRemaining;
	volatile long iRefCount;
	bool bCompleted;
	bool bHasFailure;
	xfuture_result tFirstFailure;
	xpromise* pPromise;
	xfuture** arrSources;
	__xnet_future_group_item* arrItems;
};

static void __xnetFutureAllValueFree(xfuture_all_value* pAll);
static xfuture_all_value* __xnetFutureAllValueCreate(__xnet_future_group_ctx* pGroup);
static bool __xnetFutureContCopyInput(xrt_future_cont* pCont, const xfuture_result* pInput);
static void __xnetFutureDispatchDetachedList(xrt_future_cont* pHead);
static void __xnetFutureCurrentCleanup(xrtThreadData* pThreadData, ptr pArg);
static bool __xnetFutureEnqueueCurrent(xrt_future_cont* pCont);
static xtaskgroup* __xnetTaskGroupAddRef(xtaskgroup* pGroup);
static void __xnetTaskGroupRelease(xtaskgroup* pGroup);
static void __xnetTaskGroupMaybeResolveJoin(xtaskgroup* pGroup);
static xfuture* __xnetTaskGroupGetScopeFuture(xtaskgroup* pGroup);
#endif

#if !defined(XRT_BUILD_CORE)

typedef struct {
	uint32 iKind;
	union {
		xnetfuture* pFuture;
		struct {
			xnetstream* pStream;
			uint32 iWaitKind;
		} tStream;
		xdgramsock* pDgram;
		xnetlistener* pListener;
	} u;
} xnetwaitsrc;

#endif /* !XRT_BUILD_CORE */

typedef struct {
	xnetfuture* pFuture;
	xnetstream* pStream;
	uint32 iWaitKind;
	volatile long iState;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCancelDoneEvent;
	#endif
} __xnet_stream_future_wait_ctx;

typedef struct {
	xnetfuture* pFuture;
	xdgramsock* pSock;
	volatile long iState;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCancelDoneEvent;
	#endif
} __xnet_dgram_future_wait_ctx;

typedef struct {
	xnetfuture* pFuture;
	xnetlistener* pListener;
	volatile long iState;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCancelDoneEvent;
	#endif
} __xnet_listener_future_wait_ctx;

#define __XNET_SYNC_STREAM_WAIT_POSTED            0l
#define __XNET_SYNC_STREAM_WAIT_REGISTERED        1l
#define __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED  2l
#define __XNET_SYNC_STREAM_WAIT_FINISHED          3l

#define XNET_WAITSRC_NONE   0u
#define XNET_WAITSRC_FUTURE 1u
#define XNET_WAITSRC_STREAM 2u
#define XNET_WAITSRC_DGRAM  3u
#define XNET_WAITSRC_LISTENER 4u

#ifndef XFUTURE_RESULT_F_NONE
	#define XFUTURE_RESULT_F_NONE        0x00000000u
	#define XFUTURE_RESULT_F_OWN_VALUE   0x00000001u
	#define XFUTURE_RESULT_F_OWN_ERROR   0x00000002u
	#define XFUTURE_RESULT_F_SYS_ERROR   0x00000004u
	#define XFUTURE_RESULT_F_TIMEOUT     0x00000008u
	#define XFUTURE_RESULT_F_CANCELLED   0x00000010u
	#define XFUTURE_RESULT_F_CLOSED      0x00000020u
#endif

typedef struct {
	__xnet_stream_sync_wait_fn pfnOnReady;
	const char* sRegisterError;
} __xnet_stream_wait_ops;

static const __xnet_stream_wait_ops* __xnetSyncGetStreamWaitOps(uint32 iWaitKind);
static xnetengine* __xnetSyncResolveEngine(xnetengine* pEngine);

#if defined(XXRTL_CORE)
// 内部函数：确保任务组容量
static bool __xnetTaskGroupEnsureCapacity(xtaskgroup* pGroup, int iNeed)
{
	xfuture** arrNew = NULL;
	int iNewCapacity = 0;

	if ( pGroup == NULL || iNeed <= 0 ) {
		return false;
	}
	if ( pGroup->iCapacity >= iNeed ) {
		return true;
	}

	iNewCapacity = (pGroup->iCapacity > 0) ? pGroup->iCapacity : 4;
	while ( iNewCapacity < iNeed ) {
		if ( iNewCapacity > (INT_MAX / 2) ) {
			iNewCapacity = iNeed;
			break;
		}
		iNewCapacity *= 2;
	}

	arrNew = (xfuture**)XNET_ALLOC(sizeof(xfuture*) * (size_t)iNewCapacity);
	if ( arrNew == NULL ) {
		return false;
	}
	memset(arrNew, 0, sizeof(xfuture*) * (size_t)iNewCapacity);

	if ( pGroup->arrFuture && pGroup->iCount > 0 ) {
		memcpy(arrNew, pGroup->arrFuture, sizeof(xfuture*) * (size_t)pGroup->iCount);
	}
	if ( pGroup->arrFuture ) {
		XNET_FREE(pGroup->arrFuture);
	}

	pGroup->arrFuture = arrNew;
	pGroup->iCapacity = iNewCapacity;
	return true;
}


// 内部函数：__xnetTaskGroupReapCompletedUnlocked
static int __xnetTaskGroupReapCompletedUnlocked(xtaskgroup* pGroup)
{
	int iWrite = 0;
	int iRead = 0;
	int iReaped = 0;

	if ( pGroup == NULL || pGroup->arrFuture == NULL || pGroup->iCount <= 0 ) {
		return 0;
	}

	for ( iRead = 0; iRead < pGroup->iCount; ++iRead ) {
		xfuture* pFuture = pGroup->arrFuture[iRead];
		xfuture_state iState;

		if ( pFuture == NULL ) {
			continue;
		}

		iState = xFutureState(pFuture);
		if ( iState != XFUTURE_PENDING ) {
			xFutureRelease(pFuture);
			pGroup->arrFuture[iRead] = NULL;
			iReaped++;
			continue;
		}

		if ( iWrite != iRead ) {
			pGroup->arrFuture[iWrite] = pFuture;
			pGroup->arrFuture[iRead] = NULL;
		}
		iWrite++;
	}

	for ( ; iWrite < pGroup->iCount; ++iWrite ) {
		pGroup->arrFuture[iWrite] = NULL;
	}

	pGroup->iCount -= iReaped;
	if ( pGroup->iCount < 0 ) {
		pGroup->iCount = 0;
	}

	return iReaped;
}


// 内部函数：__xnetTaskGroupCountPendingUnlocked
static int __xnetTaskGroupCountPendingUnlocked(xtaskgroup* pGroup)
{
	int iCount = 0;
	int i;

	if ( pGroup == NULL || pGroup->arrFuture == NULL || pGroup->iCount <= 0 ) {
		return 0;
	}

	for ( i = 0; i < pGroup->iCount; ++i ) {
		xfuture* pFuture = pGroup->arrFuture[i];
		if ( pFuture && xFutureState(pFuture) == XFUTURE_PENDING ) {
			iCount++;
		}
	}

	return iCount;
}


// 内部函数：__xnetTaskGroupAddRef
static xtaskgroup* __xnetTaskGroupAddRef(xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		return NULL;
	}

	(void)__xnetAtomicAddFetch32(&pGroup->iRefCount, 1);
	return pGroup;
}


// 内部函数：释放任务组
static void __xnetTaskGroupFree(xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}

	if ( pGroup->pJoinPromise ) {
		xPromiseDestroy(pGroup->pJoinPromise);
		pGroup->pJoinPromise = NULL;
	}
	if ( pGroup->pJoinFuture ) {
		xFutureRelease(pGroup->pJoinFuture);
		pGroup->pJoinFuture = NULL;
	}
	if ( pGroup->pScopePromise ) {
		xPromiseDestroy(pGroup->pScopePromise);
		pGroup->pScopePromise = NULL;
	}
	if ( pGroup->pScopeFuture ) {
		xFutureRelease(pGroup->pScopeFuture);
		pGroup->pScopeFuture = NULL;
	}

	if ( pGroup->pLock ) {
		xrtMutexDestroy(pGroup->pLock);
		pGroup->pLock = NULL;
	}
	if ( pGroup->arrFuture ) {
		XNET_FREE(pGroup->arrFuture);
		pGroup->arrFuture = NULL;
	}
	XNET_FREE(pGroup);
}


// 内部函数：释放任务组
static void __xnetTaskGroupRelease(xtaskgroup* pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}

	if ( __xnetAtomicAddFetch32(&pGroup->iRefCount, -1) == 0 ) {
		__xnetTaskGroupFree(pGroup);
	}
}


// 内部函数：__xnetTaskGroupMaybeResolveJoin
static void __xnetTaskGroupMaybeResolveJoin(xtaskgroup* pGroup)
{
	xpromise* pPromise = NULL;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return;
	}

	__xnetTaskGroupAddRef(pGroup);
	xrtMutexLock(pGroup->pLock);
	if (
		pGroup->bClosed &&
		pGroup->pJoinPromise != NULL &&
		pGroup->pJoinFuture != NULL &&
		xFutureState(pGroup->pJoinFuture) == XFUTURE_PENDING &&
		__xnetTaskGroupCountPendingUnlocked(pGroup) == 0
	) {
		pPromise = pGroup->pJoinPromise;
	}
	xrtMutexUnlock(pGroup->pLock);

	if ( pPromise ) {
		(void)xPromiseResolve(pPromise, NULL);
	}

	__xnetTaskGroupRelease(pGroup);
}


// 内部函数：获取任务组 scope Future
static xfuture* __xnetTaskGroupGetScopeFuture(xtaskgroup* pGroup)
{
	xfuture* pFuture = NULL;
	xfuture* pOutFuture = NULL;
	xpromise* pPromise = NULL;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return NULL;
	}

	xrtMutexLock(pGroup->pLock);
	if ( pGroup->pScopeFuture ) {
		pOutFuture = xFutureAddRef(pGroup->pScopeFuture);
		xrtMutexUnlock(pGroup->pLock);
		return pOutFuture;
	}
	xrtMutexUnlock(pGroup->pLock);

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}
	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}

	xrtMutexLock(pGroup->pLock);
	if ( pGroup->pScopeFuture == NULL && pGroup->pScopePromise == NULL ) {
		pGroup->pScopeFuture = pFuture;
		pGroup->pScopePromise = pPromise;
		pOutFuture = xFutureAddRef(pFuture);
		xrtMutexUnlock(pGroup->pLock);
		return pOutFuture;
	}

	pOutFuture = pGroup->pScopeFuture ? xFutureAddRef(pGroup->pScopeFuture) : NULL;
	xrtMutexUnlock(pGroup->pLock);
	xPromiseDestroy(pPromise);
	xFutureRelease(pFuture);
	return pOutFuture;
}
#endif



/* ============================== Local platform helpers ============================== */

static void __xnetSyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__xnetSyncNowMs
static uint64 __xnetSyncNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) return 0;
		return ((uint64)tNow.tv_sec * 1000ULL) + ((uint64)tNow.tv_nsec / 1000000ULL);
	#endif
}


// 内部函数：__xnetSyncAtomicCompareExchange
static long __xnetSyncAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}


// 内部函数：__xnetSyncAtomicStore
static void __xnetSyncAtomicStore(volatile long* pValue, long iValue)
{
	(void)__xnetAtomicExchange32(pValue, iValue);
}


// 内部函数：__xnetSyncSpinLock
static void __xnetSyncSpinLock(volatile long* pLock)
{
	while ( __xnetSyncAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xnetSyncSleepMs(1);
	}
}


// 内部函数：__xnetSyncSpinUnlock
static void __xnetSyncSpinUnlock(volatile long* pLock)
{
	__xnetSyncAtomicStore(pLock, 0);
}


// 内部函数：设置同步错误
static void __xnetSyncSetError(const char* sError)
{
	#if defined(XXRTL_CORE)
		xrtSetError((str)sError, FALSE);
	#else
		(void)sError;
	#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
// 内部函数：构建同步 abs 超时
static bool __xnetSyncMakeAbsTimeout(struct timespec* pTs, uint32 iTimeoutMs)
{
	uint64 iNs;
	if ( !pTs ) return false;
	if ( clock_gettime(__XNET_SYNC_WAIT_CLOCK, pTs) != 0 ) return false;
	iNs = (uint64)pTs->tv_nsec + ((uint64)iTimeoutMs * 1000000ULL);
	pTs->tv_sec += (time_t)(iNs / 1000000000ULL);
	pTs->tv_nsec = (long)(iNs % 1000000000ULL);
	return true;
}


// 内部函数：__xnetSyncInitCond
static bool __xnetSyncInitCond(__xnet_sync_cond* pCond)
{
	pthread_condattr_t tAttr;
	if ( !pCond ) return false;
	if ( pthread_condattr_init(&tAttr) != 0 ) return false;
	#if defined(CLOCK_MONOTONIC)
		(void)pthread_condattr_setclock(&tAttr, CLOCK_MONOTONIC);
	#endif
	if ( pthread_cond_init(pCond, &tAttr) != 0 ) {
		pthread_condattr_destroy(&tAttr);
		return false;
	}
	pthread_condattr_destroy(&tAttr);
	return true;
}
#endif

// 内部函数：__xnetFuturePrimitiveInit
static bool __xnetFuturePrimitiveInit(xnetfuture* pFuture)
{
	if ( !pFuture ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pFuture->hLock);
		InitializeConditionVariable(&pFuture->hCond);
		return true;
	#else
		if ( pthread_mutex_init(&pFuture->hLock, NULL) != 0 ) return false;
		if ( !__xnetSyncInitCond(&pFuture->hCond) ) {
			pthread_mutex_destroy(&pFuture->hLock);
			return false;
		}
		return true;
	#endif
}


// 内部函数：__xnetFuturePrimitiveUnit
static void __xnetFuturePrimitiveUnit(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	#if defined(_WIN32) || defined(_WIN64)
		DeleteCriticalSection(&pFuture->hLock);
	#else
		pthread_cond_destroy(&pFuture->hCond);
		pthread_mutex_destroy(&pFuture->hLock);
	#endif
}


// 内部函数：锁定 Future
static void __xnetFutureLock(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pFuture->hLock);
	#else
		pthread_mutex_lock(&pFuture->hLock);
	#endif
}


// 内部函数：解锁 Future
static void __xnetFutureUnlock(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pFuture->hLock);
	#else
		pthread_mutex_unlock(&pFuture->hLock);
	#endif
}


// 内部函数：唤醒 Future 全部
static void __xnetFutureWakeAll(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pFuture->hCond);
	#else
		pthread_cond_broadcast(&pFuture->hCond);
	#endif
}


// 内部函数：__xnetFutureWaitOnce
static bool __xnetFutureWaitOnce(xnetfuture* pFuture, uint32 iTimeoutMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
			return SleepConditionVariableCS(&pFuture->hCond, &pFuture->hLock, INFINITE) != 0;
		}
		return SleepConditionVariableCS(&pFuture->hCond, &pFuture->hLock, iTimeoutMs) != 0;
	#else
		if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
			return pthread_cond_wait(&pFuture->hCond, &pFuture->hLock) == 0;
		} else {
			struct timespec tAbs;
			if ( !__xnetSyncMakeAbsTimeout(&tAbs, iTimeoutMs) ) return false;
			return pthread_cond_timedwait(&pFuture->hCond, &pFuture->hLock, &tAbs) == 0;
		}
	#endif
}


// 内部函数：推进 Future 当前 auto
static int __xnetFuturePumpCurrentAuto(void)
{
	#if defined(XXRTL_CORE)
		return xFuturePumpCurrentContinuations(0);
	#else
		return 0;
	#endif
}


// 内部函数：初始化 Future
static bool __xnetFutureInit(xnetfuture* pFuture)
{
	if ( !pFuture ) return false;
	memset(pFuture, 0, sizeof(xnetfuture));
	if ( !__xnetFuturePrimitiveInit(pFuture) ) return false;
	pFuture->bDone = false;
	pFuture->iRefCount = 1;
	pFuture->iStatus = XRT_NET_AGAIN;
	pFuture->pValue = NULL;
	pFuture->sError = NULL;
	pFuture->iResultFlags = 0;
	pFuture->iGroupSourceIndex = -1;
	pFuture->iAsyncHoldCount = 0;
	return true;
}

static void __xnetFutureUnit(xnetfuture* pFuture);


// 内部函数：获取 Future dup 错误
static const char* __xnetFutureDupError(const char* sError)
{
	size_t iLen;
	char* sCopy;
	if ( !sError || !sError[0] ) return NULL;
	iLen = strlen(sError);
	sCopy = (char*)XNET_ALLOC(iLen + 1u);
	if ( !sCopy ) return NULL;
	memcpy(sCopy, sError, iLen + 1u);
	return sCopy;
}


// 内部函数：释放 Future 错误
static void __xnetFutureFreeError(const char* sError, uint32 iFlags)
{
	if ( sError && (iFlags & XFUTURE_RESULT_F_OWN_ERROR) ) {
		XNET_FREE((ptr)sError);
	}
}


// 内部函数：__xnetFutureAddRefInternal
static void __xnetFutureAddRefInternal(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->iRefCount++;
	__xnetFutureUnlock(pFuture);
}


// 内部函数：__xnetFutureFinalizeFree
static void __xnetFutureFinalizeFree(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	if ( pFuture->pfnPendingCleanup ) {
		pFuture->pfnPendingCleanup(pFuture);
	}
	__xnetFutureUnit(pFuture);
	XNET_FREE(pFuture);
}


// 内部函数：__xnetFutureReleaseRefInternal
static void __xnetFutureReleaseRefInternal(xnetfuture* pFuture)
{
	bool bFree = false;
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	if ( pFuture->iRefCount > 0 ) {
		pFuture->iRefCount--;
	}
	if ( pFuture->iRefCount == 0 && pFuture->iAsyncHoldCount == 0 ) {
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
			if ( pFuture->iCoWaitActive == 0 ) {
				bFree = true;
			}
		#else
			bFree = true;
		#endif
	}
	__xnetFutureUnlock(pFuture);
	if ( bFree ) {
		__xnetFutureFinalizeFree(pFuture);
	}
}


// 内部函数：异步增加 Future hold
static void __xnetFutureAddAsyncHold(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->iAsyncHoldCount++;
	pFuture->iRefCount++;
	__xnetFutureUnlock(pFuture);
}


// 内部函数：异步释放 Future hold
static void __xnetFutureReleaseAsyncHold(xnetfuture* pFuture)
{
	bool bReleaseRef = false;
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	if ( pFuture->iAsyncHoldCount > 0 ) {
		pFuture->iAsyncHoldCount--;
		bReleaseRef = true;
	}
	__xnetFutureUnlock(pFuture);
	if ( bReleaseRef ) {
		__xnetFutureReleaseRefInternal(pFuture);
	}
}


// 内部函数：释放 Future
static void __xnetFutureUnit(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureFreeError(pFuture->sError, pFuture->iResultFlags);
	pFuture->sError = NULL;
	__xnetFutureFreeError(pFuture->sDebugName, XFUTURE_RESULT_F_OWN_ERROR);
	pFuture->sDebugName = NULL;
	pFuture->iResultFlags = 0;
	pFuture->iCreateTimeMs = 0;
	pFuture->iCompleteTimeMs = 0;
	pFuture->iPendingContCount = 0;
	pFuture->iGroupSourceIndex = -1;
	if ( pFuture->pGroupSource ) {
		xFutureRelease(pFuture->pGroupSource);
		pFuture->pGroupSource = NULL;
	}
	#if defined(XXRTL_CORE)
		pFuture->pContHead = NULL;
		pFuture->pContTail = NULL;
	#endif
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->pCoEvent ) {
			xrtCoEventDestroy(pFuture->pCoEvent);
			pFuture->pCoEvent = NULL;
		}
	#endif
	__xnetFuturePrimitiveUnit(pFuture);
}


// 内部函数：重置 Future
static void UNUSED_ATTR __xnetFutureReset(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->bDone = false;
	pFuture->iStatus = XRT_NET_AGAIN;
	pFuture->pValue = NULL;
	__xnetFutureFreeError(pFuture->sError, pFuture->iResultFlags);
	pFuture->sError = NULL;
	pFuture->iResultFlags = 0;
	pFuture->iGroupSourceIndex = -1;
	if ( pFuture->pGroupSource ) {
		xFutureRelease(pFuture->pGroupSource);
		pFuture->pGroupSource = NULL;
	}
	pFuture->iCompleteTimeMs = 0;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->pCoEvent ) {
			xrtCoEventReset(pFuture->pCoEvent);
		}
	#endif
	__xnetFutureUnlock(pFuture);
}


// 内部函数：__xnetFutureCompleteEx
static bool __xnetFutureCompleteEx(xnetfuture* pFuture, xnet_result iStatus, ptr pValue, const char* sError, uint32 iFlags)
{
	bool bResolved = false;
	#if defined(XXRTL_CORE)
		xrt_future_cont* pDispatchHead = NULL;
		xfuture_result tDispatchInput;
	#endif
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCoEvent = NULL;
	#endif
	const char* sOwnedError = NULL;
	if ( !pFuture ) return false;
	#if defined(XXRTL_CORE)
		memset(&tDispatchInput, 0, sizeof(tDispatchInput));
	#endif
	if ( sError && sError[0] && (iFlags & XFUTURE_RESULT_F_OWN_ERROR) ) {
		sOwnedError = __xnetFutureDupError(sError);
		if ( sOwnedError == NULL ) {
			iFlags &= ~XFUTURE_RESULT_F_OWN_ERROR;
		}
	} else {
		sOwnedError = sError;
	}
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		pFuture->bDone = true;
		pFuture->iStatus = iStatus;
		pFuture->pValue = pValue;
		pFuture->sError = sOwnedError;
		pFuture->iResultFlags = iFlags;
		pFuture->iCompleteTimeMs = __xnetSyncNowMs();
		bResolved = true;
		#if defined(XXRTL_CORE)
			tDispatchInput.iStatus = iStatus;
			tDispatchInput.pValue = pValue;
			tDispatchInput.sError = (str)sOwnedError;
			tDispatchInput.iFlags = iFlags;
			pDispatchHead = pFuture->pContHead;
			pFuture->pContHead = NULL;
			pFuture->pContTail = NULL;
			{
				xrt_future_cont* pIt = pDispatchHead;
				while ( pIt != NULL ) {
					(void)__xnetFutureContCopyInput(pIt, &tDispatchInput);
					pIt = pIt->pNext;
				}
			}
		#endif
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
			pCoEvent = pFuture->pCoEvent;
		#endif
		__xnetFutureWakeAll(pFuture);
	}
	__xnetFutureUnlock(pFuture);
	if ( !bResolved && sOwnedError && (iFlags & XFUTURE_RESULT_F_OWN_ERROR) ) {
		XNET_FREE((ptr)sOwnedError);
	}
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( bResolved && pCoEvent ) {
			xrtCoEventSet(pCoEvent);
		}
	#endif
	#if defined(XXRTL_CORE)
		if ( bResolved && pDispatchHead ) {
			__xnetFutureDispatchDetachedList(pDispatchHead);
		}
	#endif
	return bResolved;
}


// 内部函数：解析 Future
static bool __xnetFutureResolve(xnetfuture* pFuture, xnet_result iStatus, ptr pValue)
{
	return __xnetFutureCompleteEx(pFuture, iStatus, pValue, NULL, XFUTURE_RESULT_F_NONE);
}

#if defined(XXRTL_CORE)
// 内部函数：获取 Promise complete 结果
static bool __xnetPromiseCompleteResult(xpromise* pPromise, const xfuture_result* pResult)
{
	int32 iStatus;
	ptr pValue;
	const char* sError;
	uint32 iFlags;

	if ( !pPromise ) return false;
	if ( __xnetSyncAtomicCompareExchange(&pPromise->bCompleted, 1, 0) != 0 ) {
		return false;
	}

	iStatus = pResult ? pResult->iStatus : XRT_NET_OK;
	pValue = pResult ? pResult->pValue : NULL;
	sError = pResult ? (const char*)pResult->sError : NULL;
	iFlags = pResult ? pResult->iFlags : XFUTURE_RESULT_F_NONE;

	if ( iStatus == 0 ) iStatus = XRT_NET_OK;
	if ( iStatus == XRT_NET_AGAIN ) iStatus = XRT_NET_ERROR;
	return __xnetFutureCompleteEx(pPromise->pFuture, (xnet_result)iStatus, pValue, sError, iFlags);
}


// 内部函数：释放 Future 结果
static void __xnetFutureResultFree(xfuture_result* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->pValue && (pResult->iFlags & XFUTURE_RESULT_F_OWN_VALUE) ) {
		if ( pResult->iFlags & XFUTURE_RESULT_F_GROUP_ALL ) {
			__xnetFutureAllValueFree((xfuture_all_value*)pResult->pValue);
		}
	}
	pResult->pValue = NULL;
	pResult->iFlags &= ~XFUTURE_RESULT_F_OWN_VALUE;
	__xnetFutureFreeError((const char*)pResult->sError, pResult->iFlags);
	pResult->sError = NULL;
	pResult->iFlags &= ~XFUTURE_RESULT_F_OWN_ERROR;
}


// 内部函数：复制 Future 结果
static bool __xnetFutureResultCopy(xfuture_result* pDst, const xfuture_result* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) {
		return false;
	}
	memset(pDst, 0, sizeof(*pDst));
	pDst->iStatus = pSrc->iStatus;
	pDst->pValue = pSrc->pValue;
	pDst->iFlags = pSrc->iFlags & ~(XFUTURE_RESULT_F_OWN_ERROR | XFUTURE_RESULT_F_OWN_VALUE);
	if ( pSrc->sError && pSrc->sError[0] ) {
		pDst->sError = (str)__xnetFutureDupError((const char*)pSrc->sError);
		if ( pDst->sError ) {
			pDst->iFlags |= XFUTURE_RESULT_F_OWN_ERROR;
		}
		else {
			pDst->sError = pSrc->sError;
		}
	}
	return true;
}


// 内部函数：释放 Future cont 输入
static void __xnetFutureContFreeInput(xrt_future_cont* pCont)
{
	if ( pCont == NULL ) {
		return;
	}
	__xnetFutureResultFree(&pCont->tInput);
}


// 内部函数：复制 Future cont 输入
static bool __xnetFutureContCopyInput(xrt_future_cont* pCont, const xfuture_result* pInput)
{
	if ( pCont == NULL || pInput == NULL ) {
		return false;
	}
	return __xnetFutureResultCopy(&pCont->tInput, pInput);
}


// 内部函数：__xnetFutureContCaptureInputLocked
static bool __xnetFutureContCaptureInputLocked(xfuture* pFuture, xrt_future_cont* pCont)
{
	xfuture_result tInput;

	if ( pFuture == NULL || pCont == NULL || !pFuture->bDone ) {
		return false;
	}
	memset(&tInput, 0, sizeof(tInput));
	tInput.iStatus = pFuture->iStatus;
	tInput.pValue = pFuture->pValue;
	tInput.sError = (str)pFuture->sError;
	tInput.iFlags = pFuture->iResultFlags;
	return __xnetFutureContCopyInput(pCont, &tInput);
}


// 内部函数：__xnetFutureContShouldRun
static bool __xnetFutureContShouldRun(xrt_future_cont* pCont)
{
	if ( pCont == NULL ) {
		return false;
	}
	if ( pCont->iKind == __XNET_FCONT_THEN ) {
		return pCont->tInput.iStatus == XRT_NET_OK;
	}
	if ( pCont->iKind == __XNET_FCONT_CATCH ) {
		return pCont->tInput.iStatus != XRT_NET_OK;
	}
	return true;
}


// 内部函数：__xnetFutureContCompletePassThrough
static void __xnetFutureContCompletePassThrough(xrt_future_cont* pCont)
{
	xfuture_result tPass;

	if ( pCont == NULL ) {
		return;
	}
	memset(&tPass, 0, sizeof(tPass));
	tPass = pCont->tInput;
	tPass.iFlags &= ~XFUTURE_RESULT_F_OWN_VALUE;
	(void)__xnetPromiseCompleteResult(pCont->pPromise, &tPass);
}


// 内部函数：完成 Future cont 节点
static void __xnetFutureContFinalizeNode(xrt_future_cont* pCont)
{
	if ( pCont == NULL ) {
		return;
	}
	if ( pCont->pSource ) {
		(void)__xnetAtomicAddFetch32(&pCont->pSource->iPendingContCount, -1);
	}
	if ( pCont->pPromise ) {
		xPromiseDestroy(pCont->pPromise);
		pCont->pPromise = NULL;
	}
	if ( pCont->pSource ) {
		xFutureRelease(pCont->pSource);
		pCont->pSource = NULL;
	}
	__xnetFutureContFreeInput(pCont);
	XNET_FREE(pCont);
}


// 内部函数：__xnetFutureContRun
static void __xnetFutureContRun(xrt_future_cont* pCont)
{
	xfuture_result tOut;
	int32 iStatus = XRT_NET_ERROR;

	if ( pCont == NULL ) {
		return;
	}

	if ( !__xnetFutureContShouldRun(pCont) ) {
		__xnetFutureContCompletePassThrough(pCont);
		__xnetFutureContFinalizeNode(pCont);
		return;
	}

	if ( pCont->iKind == __XNET_FCONT_FINALLY ) {
		if ( pCont->pfnFinally ) {
			pCont->pfnFinally(&pCont->tInput, pCont->pArg);
		}
		__xnetFutureContCompletePassThrough(pCont);
		__xnetFutureContFinalizeNode(pCont);
		return;
	}

	memset(&tOut, 0, sizeof(tOut));
	if ( pCont->pfnCont != NULL ) {
		iStatus = pCont->pfnCont(&pCont->tInput, pCont->pArg, &tOut);
		tOut.iStatus = iStatus;
	}
	else {
		tOut.iStatus = XRT_NET_ERROR;
		tOut.sError = (str)"future continuation callback is null.";
		tOut.iFlags = XFUTURE_RESULT_F_NONE;
	}
	(void)__xnetPromiseCompleteResult(pCont->pPromise, &tOut);
	__xnetFutureContFinalizeNode(pCont);
}


// 内部函数：分发 Future cont 引擎
static void __xnetFutureContEngineDispatch(xnetworker* pWorker, ptr pArg)
{
	__xnet_future_cont_ctx* pCtx = (__xnet_future_cont_ctx*)pArg;
	xrt_future_cont* pCont = NULL;

	(void)pWorker;
	if ( pCtx == NULL ) {
		return;
	}
	pCont = pCtx->pCont;
	XNET_FREE(pCtx);
	__xnetFutureContRun(pCont);
}

#if !defined(XRT_NO_COROUTINE)
// 内部函数：分发 Future cont 协程
static void __xnetFutureContCoDispatch(ptr pArg)
{
	__xnet_future_cont_ctx* pCtx = (__xnet_future_cont_ctx*)pArg;
	xrt_future_cont* pCont = NULL;

	if ( pCtx == NULL ) {
		return;
	}
	pCont = pCtx->pCont;
	XNET_FREE(pCtx);
	__xnetFutureContRun(pCont);
}
#endif

// 内部函数：分发 Future 延续回调
static bool __xnetFutureDispatchContinuation(xrt_future_cont* pCont)
{
	__xnet_future_cont_ctx* pCtx = NULL;
	xnetengine* pEngine = NULL;
	xnet_result iPostResult;

	if ( pCont == NULL ) {
		return false;
	}

	if ( pCont->iExec == __XNET_FCONT_EXEC_INLINE ) {
		__xnetFutureContRun(pCont);
		return true;
	}

	if ( pCont->iExec == __XNET_FCONT_EXEC_CURRENT ) {
		return __xnetFutureEnqueueCurrent(pCont);
	}

	pCtx = (__xnet_future_cont_ctx*)XNET_ALLOC(sizeof(__xnet_future_cont_ctx));
	if ( pCtx == NULL ) {
		return false;
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pCont = pCont;

	if ( pCont->iExec == __XNET_FCONT_EXEC_ENGINE ) {
		pEngine = __xnetSyncResolveEngine(pCont->pEngine);
		if ( pEngine == NULL ) {
			XNET_FREE(pCtx);
			return false;
		}
		iPostResult = xrtNetEnginePost(pEngine, pCont->iAffinityKey, __xnetFutureContEngineDispatch, pCtx);
		if ( iPostResult != XRT_NET_OK ) {
			XNET_FREE(pCtx);
			return false;
		}
		return true;
	}

	#if !defined(XRT_NO_COROUTINE)
	if ( pCont->iExec == __XNET_FCONT_EXEC_CO ) {
		if ( pCont->pSched == NULL ) {
			XNET_FREE(pCtx);
			return false;
		}
		if ( xrtCoSchedSpawn(pCont->pSched, __xnetFutureContCoDispatch, pCtx, pCont->iStackSize) == NULL ) {
			XNET_FREE(pCtx);
			return false;
		}
		return true;
	}
	#endif

	XNET_FREE(pCtx);
	return false;
}


// 内部函数：分发 Future detached 列表
static void __xnetFutureDispatchDetachedList(xrt_future_cont* pHead)
{
	xrt_future_cont* pCont = pHead;

	while ( pCont != NULL ) {
		xrt_future_cont* pNext = pCont->pNext;
		pCont->pNext = NULL;
		if ( !__xnetFutureDispatchContinuation(pCont) ) {
			xfuture_result tError;
			memset(&tError, 0, sizeof(tError));
			tError.iStatus = XRT_NET_ERROR;
			tError.sError = (str)"future continuation could not be scheduled.";
			tError.iFlags = XFUTURE_RESULT_F_NONE;
			(void)__xnetPromiseCompleteResult(pCont->pPromise, &tError);
			__xnetFutureContFinalizeNode(pCont);
		}
		pCont = pNext;
	}
}


// 内部函数：确保 Future 当前队列 registered
static bool __xnetFutureEnsureCurrentQueueRegistered(void)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		return false;
	}
	if ( pThreadData->bFutureDeferredCleanupRegistered ) {
		return true;
	}
	if ( !xrtThreadPushCleanup(__xnetFutureCurrentCleanup, NULL) ) {
		return false;
	}
	pThreadData->bFutureDeferredCleanupRegistered = TRUE;
	return true;
}


// 内部函数：入队 Future 当前
static bool __xnetFutureEnqueueCurrent(xrt_future_cont* pCont)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	xrt_future_cont* pTail = NULL;

	if ( pCont == NULL || pThreadData == NULL ) {
		return false;
	}
	if ( !__xnetFutureEnsureCurrentQueueRegistered() ) {
		return false;
	}

	pCont->pNext = NULL;
	pTail = (xrt_future_cont*)pThreadData->pFutureDeferredTail;
	if ( pTail ) {
		pTail->pNext = pCont;
	}
	else {
		pThreadData->pFutureDeferredHead = pCont;
	}
	pThreadData->pFutureDeferredTail = pCont;
	return true;
}


// 推进 Future 当前延续回调
XXAPI int xFuturePumpCurrentContinuations(int iMaxCount)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	int iPumped = 0;

	if ( pThreadData == NULL ) {
		return 0;
	}

	while ( pThreadData->pFutureDeferredHead ) {
		xrt_future_cont* pCont = (xrt_future_cont*)pThreadData->pFutureDeferredHead;

		pThreadData->pFutureDeferredHead = pCont->pNext;
		if ( pThreadData->pFutureDeferredHead == NULL ) {
			pThreadData->pFutureDeferredTail = NULL;
		}

		pCont->pNext = NULL;
		__xnetFutureContRun(pCont);
		iPumped++;

		if ( iMaxCount > 0 && iPumped >= iMaxCount ) {
			break;
		}
	}

	if ( pThreadData->pFutureDeferredHead == NULL && pThreadData->bFutureDeferredCleanupRegistered ) {
		(void)xrtThreadPopCleanup(__xnetFutureCurrentCleanup, NULL);
		pThreadData->bFutureDeferredCleanupRegistered = FALSE;
	}

	return iPumped;
}


// 内部函数：__xnetFutureCurrentCleanup
static void __xnetFutureCurrentCleanup(xrtThreadData* pThreadData, ptr pArg)
{
	(void)pThreadData;
	(void)pArg;
	(void)xFuturePumpCurrentContinuations(0);
}


// 内部函数：设置 Future 组源
static void __xnetFutureSetGroupSource(xfuture* pFuture, xfuture* pSource, int iIndex)
{
	if ( pFuture == NULL ) {
		return;
	}

	__xnetFutureLock(pFuture);
	if ( pFuture->pGroupSource ) {
		xFutureRelease(pFuture->pGroupSource);
		pFuture->pGroupSource = NULL;
	}
	pFuture->iGroupSourceIndex = iIndex;
	if ( pSource ) {
		pFuture->pGroupSource = xFutureAddRef(pSource);
	}
	__xnetFutureUnlock(pFuture);
}


// 内部函数：释放 Future 全部值
static void __xnetFutureAllValueFree(xfuture_all_value* pAll)
{
	if ( pAll == NULL ) {
		return;
	}

	if ( pAll->arrValue ) {
		XNET_FREE(pAll->arrValue);
		pAll->arrValue = NULL;
	}
	XNET_FREE(pAll);
}


// 内部函数：创建 Future 全部值
static xfuture_all_value* __xnetFutureAllValueCreate(__xnet_future_group_ctx* pGroup)
{
	xfuture_all_value* pAll = NULL;
	ptr* arrValue = NULL;
	int i;

	if ( pGroup == NULL || pGroup->iCount <= 0 || pGroup->arrSources == NULL ) {
		return NULL;
	}

	pAll = (xfuture_all_value*)XNET_ALLOC(sizeof(xfuture_all_value));
	if ( pAll == NULL ) {
		return NULL;
	}
	memset(pAll, 0, sizeof(*pAll));

	arrValue = (ptr*)XNET_ALLOC(sizeof(ptr) * (size_t)pGroup->iCount);
	if ( arrValue == NULL ) {
		XNET_FREE(pAll);
		return NULL;
	}
	memset(arrValue, 0, sizeof(ptr) * (size_t)pGroup->iCount);

	for ( i = 0; i < pGroup->iCount; i++ ) {
		arrValue[i] = xFutureValue(pGroup->arrSources[i]);
	}

	pAll->iCount = pGroup->iCount;
	pAll->arrValue = arrValue;
	return pAll;
}


// 内部函数：释放 Future 组
static void __xnetFutureGroupFree(__xnet_future_group_ctx* pGroup)
{
	int i;

	if ( pGroup == NULL ) {
		return;
	}
	if ( pGroup->pPromise ) {
		xPromiseDestroy(pGroup->pPromise);
		pGroup->pPromise = NULL;
	}
	if ( pGroup->pLock ) {
		xrtMutexDestroy(pGroup->pLock);
		pGroup->pLock = NULL;
	}
	if ( pGroup->arrSources ) {
		for ( i = 0; i < pGroup->iCount; i++ ) {
			if ( pGroup->arrSources[i] ) {
				xFutureRelease(pGroup->arrSources[i]);
				pGroup->arrSources[i] = NULL;
			}
		}
		XNET_FREE(pGroup->arrSources);
		pGroup->arrSources = NULL;
	}
	if ( pGroup->arrItems ) {
		XNET_FREE(pGroup->arrItems);
		pGroup->arrItems = NULL;
	}
	__xnetFutureResultFree(&pGroup->tFirstFailure);
	XNET_FREE(pGroup);
}


// 内部函数：__xnetFutureGroupOnSourceDone
static void __xnetFutureGroupOnSourceDone(const xfuture_result* pIn, ptr pArg)
{
	__xnet_future_group_item* pItem = (__xnet_future_group_item*)pArg;
	__xnet_future_group_ctx* pGroup = NULL;
	xfuture_result tFailure;
	bool bDoComplete = false;
	bool bDoCancelLosers = false;
	bool bCompleteFailure = false;
	bool bCompleteAllOk = false;
	int iWinner = -1;
	int i;

	if ( pItem == NULL || pIn == NULL ) {
		return;
	}
	pGroup = pItem->pGroup;
	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return;
	}
	memset(&tFailure, 0, sizeof(tFailure));

	xrtMutexLock(pGroup->pLock);
	if ( !pGroup->bCompleted ) {
		if ( pGroup->iMode == __XNET_FGROUP_ANY ) {
			pGroup->bCompleted = true;
			bDoComplete = true;
			iWinner = pItem->iIndex;
		}
		else if ( pGroup->iMode == __XNET_FGROUP_RACE ) {
			pGroup->bCompleted = true;
			bDoComplete = true;
			bDoCancelLosers = true;
			iWinner = pItem->iIndex;
		}
		else if ( pGroup->iMode == __XNET_FGROUP_ALL ) {
			if ( !pGroup->bHasFailure && pIn->iStatus != XRT_NET_OK ) {
				pGroup->bHasFailure = true;
				__xnetFutureSetGroupSource(pGroup->pPromise->pFuture, pGroup->arrSources ? pGroup->arrSources[pItem->iIndex] : NULL, pItem->iIndex);
				(void)__xnetFutureResultCopy(&pGroup->tFirstFailure, pIn);
			}
		}
	}

	pGroup->iRemaining--;
	if ( pGroup->iRemaining <= 0 ) {
		if ( pGroup->iMode == __XNET_FGROUP_ALL && !pGroup->bCompleted ) {
			pGroup->bCompleted = true;
			if ( pGroup->bHasFailure ) {
				(void)__xnetFutureResultCopy(&tFailure, &pGroup->tFirstFailure);
				bCompleteFailure = true;
			}
			else {
				bCompleteAllOk = true;
			}
		}
	}
	xrtMutexUnlock(pGroup->pLock);

	if ( bDoComplete ) {
		__xnetFutureSetGroupSource(pGroup->pPromise->pFuture, pGroup->arrSources ? pGroup->arrSources[iWinner] : NULL, iWinner);
		(void)__xnetPromiseCompleteResult(pGroup->pPromise, pIn);
	}
	if ( bDoCancelLosers ) {
		for ( i = 0; i < pGroup->iCount; i++ ) {
			if ( i == iWinner ) continue;
			if ( pGroup->arrSources && pGroup->arrSources[i] ) {
				(void)xFutureRequestCancel(pGroup->arrSources[i]);
			}
		}
	}
	if ( bCompleteFailure ) {
		(void)__xnetPromiseCompleteResult(pGroup->pPromise, &tFailure);
		__xnetFutureResultFree(&tFailure);
	}
	if ( bCompleteAllOk ) {
		xfuture_result tOk;
		memset(&tOk, 0, sizeof(tOk));
		tOk.iStatus = XRT_NET_OK;
		tOk.pValue = __xnetFutureAllValueCreate(pGroup);
		if ( tOk.pValue ) {
			tOk.iFlags = XFUTURE_RESULT_F_OWN_VALUE | XFUTURE_RESULT_F_GROUP_ALL;
		}
		(void)__xnetPromiseCompleteResult(pGroup->pPromise, &tOk);
	}
	if ( __xnetAtomicAddFetch32(&pGroup->iRefCount, -1) == 0 ) {
		__xnetFutureGroupFree(pGroup);
	}
}


// 内部函数：创建 Future 组
static xfuture* __xnetFutureCreateGroup(xfuture** arrFuture, int iCount, __xnet_future_group_mode iMode)
{
	xfuture* pOut = NULL;
	xpromise* pPromise = NULL;
	__xnet_future_group_ctx* pGroup = NULL;
	int i;

	if ( arrFuture == NULL || iCount <= 0 ) {
		__xnetSyncSetError("future group requires at least one source future.");
		return NULL;
	}

	pOut = xFutureCreate();
	if ( pOut == NULL ) {
		return NULL;
	}

	pPromise = xPromiseCreate(pOut);
	if ( pPromise == NULL ) {
		xFutureRelease(pOut);
		return NULL;
	}

	pGroup = (__xnet_future_group_ctx*)XNET_ALLOC(sizeof(__xnet_future_group_ctx));
	if ( pGroup == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pOut);
		return NULL;
	}
	memset(pGroup, 0, sizeof(*pGroup));
	pGroup->iMode = iMode;
	pGroup->iCount = iCount;
	pGroup->iRemaining = iCount;
	pGroup->iRefCount = iCount;
	pGroup->pPromise = pPromise;
	pGroup->pLock = xrtMutexCreate();
	if ( pGroup->pLock == NULL ) {
		__xnetFutureGroupFree(pGroup);
		xFutureRelease(pOut);
		return NULL;
	}
	pGroup->arrSources = (xfuture**)XNET_ALLOC(sizeof(xfuture*) * (size_t)iCount);
	pGroup->arrItems = (__xnet_future_group_item*)XNET_ALLOC(sizeof(__xnet_future_group_item) * (size_t)iCount);
	if ( pGroup->arrSources == NULL || pGroup->arrItems == NULL ) {
		__xnetFutureGroupFree(pGroup);
		xFutureRelease(pOut);
		return NULL;
	}
	memset(pGroup->arrSources, 0, sizeof(xfuture*) * (size_t)iCount);
	memset(pGroup->arrItems, 0, sizeof(__xnet_future_group_item) * (size_t)iCount);

	for ( i = 0; i < iCount; i++ ) {
		xfuture* pChild;
		if ( arrFuture[i] == NULL ) {
			__xnetSyncSetError("future group source is null.");
			__xnetFutureGroupFree(pGroup);
			xFutureRelease(pOut);
			return NULL;
		}
		pGroup->arrSources[i] = xFutureAddRef(arrFuture[i]);
		pGroup->arrItems[i].pGroup = pGroup;
		pGroup->arrItems[i].iIndex = i;
		pChild = xFutureFinallyEngine(arrFuture[i], NULL, 0, __xnetFutureGroupOnSourceDone, &pGroup->arrItems[i]);
		if ( pChild == NULL ) {
			__xnetFutureGroupFree(pGroup);
			xFutureRelease(pOut);
			return NULL;
		}
		xFutureRelease(pChild);
	}

	return pOut;
}
#endif

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：确保 Future 协程事件
static xcoevent __xnetFutureEnsureCoEvent(xnetfuture* pFuture)
{
	xcoevent pEvent = NULL;
	xcoevent pNewEvent = NULL;

	if ( !pFuture ) return NULL;

	__xnetFutureLock(pFuture);
	pEvent = pFuture->pCoEvent;
	__xnetFutureUnlock(pFuture);
	if ( pEvent ) return pEvent;

	pNewEvent = xrtCoEventCreate(TRUE, FALSE);
	if ( !pNewEvent ) return NULL;

	__xnetFutureLock(pFuture);
	if ( pFuture->pCoEvent == NULL ) {
		pFuture->pCoEvent = pNewEvent;
		pEvent = pNewEvent;
		pNewEvent = NULL;
	} else {
		pEvent = pFuture->pCoEvent;
	}
	__xnetFutureUnlock(pFuture);

	if ( pNewEvent ) {
		xrtCoEventDestroy(pNewEvent);
	}

	return pEvent;
}
#endif

// 内部函数：获取同步 hidden 状态
static __xnet_sync_hidden_state* __xnetSyncHiddenState(void)
{
	static __xnet_sync_hidden_state tState;
	return &tState;
}



/* ============================== Public future helpers ============================== */

XXAPI xnetfuture* xrtNetFutureCreate(void)
{
	xnetfuture* pFuture = (xnetfuture*)XNET_ALLOC(sizeof(xnetfuture));
	if ( !pFuture ) return NULL;
	if ( !__xnetFutureInit(pFuture) ) {
		XNET_FREE(pFuture);
		return NULL;
	}
	return pFuture;
}


// 创建空等待源
XXAPI xnetwaitsrc xrtNetWaitSourceNone(void)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_NONE;
	return tSrc;
}


// 创建 Future 等待源
XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_FUTURE;
	tSrc.u.pFuture = pFuture;
	return tSrc;
}


// 创建流等待源
XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_STREAM;
	tSrc.u.tStream.pStream = pStream;
	tSrc.u.tStream.iWaitKind = iWaitKind;
	return tSrc;
}


// 创建数据报接收等待源
XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_DGRAM;
	tSrc.u.pDgram = pSock;
	return tSrc;
}


// 创建监听器接受等待源
XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_LISTENER;
	tSrc.u.pListener = pListener;
	return tSrc;
}


// 内部函数：__xnetFutureDestroyCore
static bool __xnetFutureDestroyCore(xnetfuture* pFuture)
{
	bool bDestroy = false;
	if ( !pFuture ) return true;
	__xnetFutureLock(pFuture);
	if ( pFuture->iAsyncHoldCount != 0 || pFuture->iRefCount != 1 ) {
		__xnetFutureUnlock(pFuture);
		return false;
	}
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->iCoWaitActive != 0 ) {
			__xnetFutureUnlock(pFuture);
			return false;
		}
	#endif
	pFuture->iRefCount = 0;
	bDestroy = true;
	__xnetFutureUnlock(pFuture);
	if ( bDestroy ) {
		__xnetFutureFinalizeFree(pFuture);
	}
	return true;
}


// 销毁网络 Future
XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	if ( !__xnetFutureDestroyCore(pFuture) ) {
		__xnetSyncSetError("cannot destroy future while an async waiter or task still holds it.");
	}
}

#if defined(XXRTL_CORE)
// 内部函数：获取 Future map 状态
static xfuture_state __xnetFutureMapState(xfuture* pFuture)
{
	int32 iStatus;
	if ( !pFuture ) return XFUTURE_REJECTED;
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		__xnetFutureUnlock(pFuture);
		return XFUTURE_PENDING;
	}
	iStatus = pFuture->iStatus;
	__xnetFutureUnlock(pFuture);
	if ( iStatus == XRT_NET_OK ) return XFUTURE_RESOLVED;
	if ( iStatus == XRT_NET_CANCELLED ) return XFUTURE_CANCELLED;
	if ( iStatus == XRT_NET_CLOSED ) return XFUTURE_CLOSED;
	return XFUTURE_REJECTED;
}


// 创建 Future
XXAPI xfuture* xFutureCreate(void)
{
	xfuture* pFuture = xrtNetFutureCreate();

	if ( pFuture ) {
		pFuture->iCreateTimeMs = __xnetSyncNowMs();
	}
	return pFuture;
}


// xFutureAddRef 相关处理
XXAPI xfuture* xFutureAddRef(xfuture* pFuture)
{
	__xnetFutureAddRefInternal(pFuture);
	return pFuture;
}


// 释放 Future
XXAPI void xFutureRelease(xfuture* pFuture)
{
	__xnetFutureReleaseRefInternal(pFuture);
}


// 获取 Future 状态
XXAPI xfuture_state xFutureState(xfuture* pFuture)
{
	return __xnetFutureMapState(pFuture);
}


// 获取 Future 状态
XXAPI int32 xFutureStatus(xfuture* pFuture)
{
	return (int32)xrtNetFutureStatus(pFuture);
}


// 获取 Future 值
XXAPI ptr xFutureValue(xfuture* pFuture)
{
	return xrtNetFutureValue(pFuture);
}


// 获取 Future 错误
XXAPI str xFutureError(xfuture* pFuture)
{
	str sError = NULL;
	if ( !pFuture ) return NULL;
	__xnetFutureLock(pFuture);
	sError = (str)pFuture->sError;
	__xnetFutureUnlock(pFuture);
	return sError;
}


// 获取 Future 结果
XXAPI bool xFutureGetResult(xfuture* pFuture, xfuture_result* pOut)
{
	if ( !pOut ) return false;
	memset(pOut, 0, sizeof(*pOut));
	if ( !pFuture ) {
		pOut->iStatus = XRT_NET_ERROR;
		return false;
	}
	__xnetFutureLock(pFuture);
	pOut->iStatus = pFuture->bDone ? pFuture->iStatus : XRT_NET_AGAIN;
	pOut->pValue = pFuture->bDone ? pFuture->pValue : NULL;
	pOut->sError = (str)(pFuture->bDone ? pFuture->sError : NULL);
	pOut->iFlags = pFuture->bDone ? pFuture->iResultFlags : XFUTURE_RESULT_F_NONE;
	__xnetFutureUnlock(pFuture);
	return pOut->iStatus == XRT_NET_OK;
}


// 设置 Future 调试名称
XXAPI bool xFutureSetDebugName(xfuture* pFuture, str sDebugName)
{
	const char* sOwned = NULL;

	if ( pFuture == NULL ) {
		return false;
	}
	if ( sDebugName ) {
		sOwned = __xnetFutureDupError((const char*)sDebugName);
		if ( sOwned == NULL ) {
			return false;
		}
	}

	__xnetFutureLock(pFuture);
	__xnetFutureFreeError(pFuture->sDebugName, XFUTURE_RESULT_F_OWN_ERROR);
	pFuture->sDebugName = sOwned;
	__xnetFutureUnlock(pFuture);
	return true;
}


// 获取 Future 调试名称
XXAPI str xFutureGetDebugName(xfuture* pFuture)
{
	str sName = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}

	__xnetFutureLock(pFuture);
	sName = (str)pFuture->sDebugName;
	__xnetFutureUnlock(pFuture);
	return sName;
}


// xFutureGetCreateTimeMs 相关处理
XXAPI uint64 xFutureGetCreateTimeMs(xfuture* pFuture)
{
	uint64 iTime = 0;

	if ( pFuture == NULL ) {
		return 0;
	}

	__xnetFutureLock(pFuture);
	iTime = pFuture->iCreateTimeMs;
	__xnetFutureUnlock(pFuture);
	return iTime;
}


// xFutureGetCompleteTimeMs 相关处理
XXAPI uint64 xFutureGetCompleteTimeMs(xfuture* pFuture)
{
	uint64 iTime = 0;

	if ( pFuture == NULL ) {
		return 0;
	}

	__xnetFutureLock(pFuture);
	iTime = pFuture->iCompleteTimeMs;
	__xnetFutureUnlock(pFuture);
	return iTime;
}


// xFutureGetPendingContinuationCount 相关处理
XXAPI int xFutureGetPendingContinuationCount(xfuture* pFuture)
{
	long iCount = 0;

	if ( pFuture == NULL ) {
		return 0;
	}

	iCount = __xnetAtomicLoad32(&pFuture->iPendingContCount);
	return (iCount > 0) ? (int)iCount : 0;
}


// 获取 Future 组源 index
XXAPI int xFutureGetGroupSourceIndex(xfuture* pFuture)
{
	int iIndex = -1;

	if ( pFuture == NULL ) {
		return -1;
	}

	__xnetFutureLock(pFuture);
	iIndex = pFuture->iGroupSourceIndex;
	__xnetFutureUnlock(pFuture);
	return iIndex;
}


// 获取 Future 组源
XXAPI xfuture* xFutureGetGroupSource(xfuture* pFuture)
{
	xfuture* pSource = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}

	__xnetFutureLock(pFuture);
	pSource = pFuture->pGroupSource ? xFutureAddRef(pFuture->pGroupSource) : NULL;
	__xnetFutureUnlock(pFuture);
	return pSource;
}


// 查看 Future 组源
XXAPI xfuture* xFuturePeekGroupSource(xfuture* pFuture)
{
	xfuture* pSource = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}

	__xnetFutureLock(pFuture);
	pSource = pFuture->pGroupSource;
	__xnetFutureUnlock(pFuture);
	return pSource;
}


// 查看 Future 全部值
XXAPI const xfuture_all_value* xFuturePeekAllValue(xfuture* pFuture)
{
	const xfuture_all_value* pAll = NULL;

	if ( pFuture == NULL ) {
		return NULL;
	}

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone && pFuture->iStatus == XRT_NET_OK && (pFuture->iResultFlags & XFUTURE_RESULT_F_GROUP_ALL) ) {
		pAll = (const xfuture_all_value*)pFuture->pValue;
	}
	__xnetFutureUnlock(pFuture);
	return pAll;
}


// 统计 Future get 全部值
XXAPI int xFutureGetAllValueCount(xfuture* pFuture)
{
	const xfuture_all_value* pAll = xFuturePeekAllValue(pFuture);
	return pAll ? pAll->iCount : 0;
}


// 获取 Future 全部值 item
XXAPI ptr xFutureGetAllValueItem(xfuture* pFuture, int iIndex)
{
	const xfuture_all_value* pAll = xFuturePeekAllValue(pFuture);

	if ( pAll == NULL || pAll->arrValue == NULL || iIndex < 0 || iIndex >= pAll->iCount ) {
		return NULL;
	}

	return pAll->arrValue[iIndex];
}


// xFutureRequestCancel 相关处理
XXAPI bool xFutureRequestCancel(xfuture* pFuture)
{
	__xnet_future_pending_cancel_fn pfnCancel = NULL;
	if ( !pFuture ) return false;
	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		__xnetFutureUnlock(pFuture);
		return pFuture->iStatus == XRT_NET_CANCELLED;
	}
	pfnCancel = pFuture->pfnPendingCancel;
	__xnetFutureUnlock(pFuture);
	if ( pfnCancel ) {
		return pfnCancel(pFuture);
	}
	return __xnetFutureCompleteEx(pFuture, XRT_NET_CANCELLED, NULL, "future cancel requested.", XFUTURE_RESULT_F_OWN_ERROR | XFUTURE_RESULT_F_CANCELLED);
}


// 创建 Promise
XXAPI xpromise* xPromiseCreate(xfuture* pFuture)
{
	xpromise* pPromise;
	if ( !pFuture ) return NULL;
	pPromise = (xpromise*)XNET_ALLOC(sizeof(xpromise));
	if ( !pPromise ) return NULL;
	memset(pPromise, 0, sizeof(*pPromise));
	pPromise->pFuture = xFutureAddRef(pFuture);
	return pPromise;
}


// 销毁 Promise
XXAPI void xPromiseDestroy(xpromise* pPromise)
{
	if ( !pPromise ) return;
	if ( pPromise->pFuture ) {
		xFutureRelease(pPromise->pFuture);
		pPromise->pFuture = NULL;
	}
	XNET_FREE(pPromise);
}


// 获取 Promise Future
XXAPI xfuture* xPromiseGetFuture(xpromise* pPromise)
{
	if ( !pPromise || !pPromise->pFuture ) return NULL;
	return xFutureAddRef(pPromise->pFuture);
}


// 查看 Promise Future
XXAPI xfuture* xPromisePeekFuture(xpromise* pPromise)
{
	if ( !pPromise ) {
		return NULL;
	}
	return pPromise->pFuture;
}


// 解析 Promise
XXAPI bool xPromiseResolve(xpromise* pPromise, ptr pValue)
{
	if ( !pPromise ) return false;
	if ( __xnetSyncAtomicCompareExchange(&pPromise->bCompleted, 1, 0) != 0 ) {
		return false;
	}
	return __xnetFutureCompleteEx(pPromise->pFuture, XRT_NET_OK, pValue, NULL, XFUTURE_RESULT_F_NONE);
}


// xPromiseReject 相关处理
XXAPI bool xPromiseReject(xpromise* pPromise, int32 iStatus, str sError)
{
	if ( !pPromise ) return false;
	if ( __xnetSyncAtomicCompareExchange(&pPromise->bCompleted, 1, 0) != 0 ) {
		return false;
	}
	if ( iStatus == XRT_NET_OK || iStatus == XRT_NET_AGAIN ) {
		iStatus = XRT_NET_ERROR;
	}
	return __xnetFutureCompleteEx(pPromise->pFuture, (xnet_result)iStatus, NULL, (const char*)sError, sError ? XFUTURE_RESULT_F_OWN_ERROR : XFUTURE_RESULT_F_NONE);
}


// 取消 Promise
XXAPI bool xPromiseCancel(xpromise* pPromise, str sError)
{
	if ( !pPromise ) return false;
	if ( __xnetSyncAtomicCompareExchange(&pPromise->bCompleted, 1, 0) != 0 ) {
		return false;
	}
	return __xnetFutureCompleteEx(pPromise->pFuture, XRT_NET_CANCELLED, NULL, (const char*)sError ? (const char*)sError : "promise cancelled.", XFUTURE_RESULT_F_OWN_ERROR | XFUTURE_RESULT_F_CANCELLED);
}


// 关闭 Promise
XXAPI bool xPromiseClose(xpromise* pPromise, str sError)
{
	if ( !pPromise ) return false;
	if ( __xnetSyncAtomicCompareExchange(&pPromise->bCompleted, 1, 0) != 0 ) {
		return false;
	}
	return __xnetFutureCompleteEx(pPromise->pFuture, XRT_NET_CLOSED, NULL, (const char*)sError ? (const char*)sError : "promise closed.", XFUTURE_RESULT_F_OWN_ERROR | XFUTURE_RESULT_F_CLOSED);
}


// 内部函数：__xnetFutureAttachContinuation
static xfuture* __xnetFutureAttachContinuation(
	xfuture* pFuture,
	__xnet_future_cont_kind iKind,
	__xnet_future_cont_exec iExec,
	xfuture_cont_fn pfnCont,
	xfuture_finally_fn pfnFinally,
	ptr pArg,
	xnetengine* pEngine,
	uint32 iAffinityKey,
	xcosched* pSched,
	size_t iStackSize)
{
	xfuture* pOutFuture = NULL;
	xpromise* pPromise = NULL;
	xrt_future_cont* pCont = NULL;
	bool bDispatchNow = false;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( iKind == __XNET_FCONT_FINALLY ) {
		if ( pfnFinally == NULL ) {
			__xnetSyncSetError("future finally callback is null.");
			return NULL;
		}
	}
	else {
		if ( pfnCont == NULL ) {
			__xnetSyncSetError("future continuation callback is null.");
			return NULL;
		}
	}
	if ( iExec == __XNET_FCONT_EXEC_INLINE && xFutureState(pFuture) == XFUTURE_PENDING ) {
		__xnetSyncSetError("inline continuation requires a completed future.");
		return NULL;
	}
	if ( iExec == __XNET_FCONT_EXEC_CURRENT && xrtThreadGetCurrent() == NULL ) {
		__xnetSyncSetError("current-thread continuation requires an attached xrt thread.");
		return NULL;
	}

	pOutFuture = xFutureCreate();
	if ( pOutFuture == NULL ) {
		return NULL;
	}

	pPromise = xPromiseCreate(pOutFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pOutFuture);
		return NULL;
	}

	pCont = (xrt_future_cont*)XNET_ALLOC(sizeof(xrt_future_cont));
	if ( pCont == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pOutFuture);
		return NULL;
	}

	memset(pCont, 0, sizeof(*pCont));
	pCont->iKind = iKind;
	pCont->iExec = iExec;
	pCont->pPromise = pPromise;
	pCont->pfnCont = pfnCont;
	pCont->pfnFinally = pfnFinally;
	pCont->pArg = pArg;
	pCont->pEngine = pEngine;
	pCont->iAffinityKey = iAffinityKey;
	pCont->pSched = pSched;
	pCont->iStackSize = iStackSize;
	pCont->pSource = xFutureAddRef(pFuture);
	(void)__xnetAtomicAddFetch32(&pFuture->iPendingContCount, 1);

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		(void)__xnetFutureContCaptureInputLocked(pFuture, pCont);
		bDispatchNow = true;
	}
	else {
		if ( pFuture->pContTail ) {
			pFuture->pContTail->pNext = pCont;
		}
		else {
			pFuture->pContHead = pCont;
		}
		pFuture->pContTail = pCont;
	}
	__xnetFutureUnlock(pFuture);

	if ( bDispatchNow ) {
		if ( !__xnetFutureDispatchContinuation(pCont) ) {
			xfuture_result tError;
			memset(&tError, 0, sizeof(tError));
			tError.iStatus = XRT_NET_ERROR;
			tError.sError = (str)"future continuation could not be scheduled.";
			(void)__xnetPromiseCompleteResult(pCont->pPromise, &tError);
			__xnetFutureContFinalizeNode(pCont);
		}
	}

	return pOutFuture;
}


// xFutureThenInline 相关处理
XXAPI xfuture* xFutureThenInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_THEN, __XNET_FCONT_EXEC_INLINE, pfnCont, NULL, pArg, NULL, 0, NULL, 0);
}


// xFutureCatchInline 相关处理
XXAPI xfuture* xFutureCatchInline(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_CATCH, __XNET_FCONT_EXEC_INLINE, pfnCont, NULL, pArg, NULL, 0, NULL, 0);
}


// xFutureFinallyInline 相关处理
XXAPI xfuture* xFutureFinallyInline(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_FINALLY, __XNET_FCONT_EXEC_INLINE, NULL, pfnCont, pArg, NULL, 0, NULL, 0);
}


// xFutureThenCurrent 相关处理
XXAPI xfuture* xFutureThenCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_THEN, __XNET_FCONT_EXEC_CURRENT, pfnCont, NULL, pArg, NULL, 0, NULL, 0);
}


// xFutureCatchCurrent 相关处理
XXAPI xfuture* xFutureCatchCurrent(xfuture* pFuture, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_CATCH, __XNET_FCONT_EXEC_CURRENT, pfnCont, NULL, pArg, NULL, 0, NULL, 0);
}


// xFutureFinallyCurrent 相关处理
XXAPI xfuture* xFutureFinallyCurrent(xfuture* pFuture, xfuture_finally_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_FINALLY, __XNET_FCONT_EXEC_CURRENT, NULL, pfnCont, pArg, NULL, 0, NULL, 0);
}


// xFutureThenEngine 相关处理
XXAPI xfuture* xFutureThenEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_THEN, __XNET_FCONT_EXEC_ENGINE, pfnCont, NULL, pArg, pEngine, iAffinityKey, NULL, 0);
}


// xFutureCatchEngine 相关处理
XXAPI xfuture* xFutureCatchEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_cont_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_CATCH, __XNET_FCONT_EXEC_ENGINE, pfnCont, NULL, pArg, pEngine, iAffinityKey, NULL, 0);
}


// xFutureFinallyEngine 相关处理
XXAPI xfuture* xFutureFinallyEngine(xfuture* pFuture, xnetengine* pEngine, uint32 iAffinityKey, xfuture_finally_fn pfnCont, ptr pArg)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_FINALLY, __XNET_FCONT_EXEC_ENGINE, NULL, pfnCont, pArg, pEngine, iAffinityKey, NULL, 0);
}

#if !defined(XRT_NO_COROUTINE)
// xFutureThenCo 相关处理
XXAPI xfuture* xFutureThenCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_THEN, __XNET_FCONT_EXEC_CO, pfnCont, NULL, pArg, NULL, 0, pSched, iStackSize);
}


// xFutureCatchCo 相关处理
XXAPI xfuture* xFutureCatchCo(xfuture* pFuture, xcosched* pSched, xfuture_cont_fn pfnCont, ptr pArg, size_t iStackSize)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_CATCH, __XNET_FCONT_EXEC_CO, pfnCont, NULL, pArg, NULL, 0, pSched, iStackSize);
}


// xFutureFinallyCo 相关处理
XXAPI xfuture* xFutureFinallyCo(xfuture* pFuture, xcosched* pSched, xfuture_finally_fn pfnCont, ptr pArg, size_t iStackSize)
{
	return __xnetFutureAttachContinuation(pFuture, __XNET_FCONT_FINALLY, __XNET_FCONT_EXEC_CO, NULL, pfnCont, pArg, NULL, 0, pSched, iStackSize);
}
#endif

// 创建任意 Future 完成聚合
XXAPI xfuture* xFutureWhenAny(xfuture** arrFuture, int iCount)
{
	return __xnetFutureCreateGroup(arrFuture, iCount, __XNET_FGROUP_ANY);
}


// 创建全部 Future 完成聚合
XXAPI xfuture* xFutureWhenAll(xfuture** arrFuture, int iCount)
{
	return __xnetFutureCreateGroup(arrFuture, iCount, __XNET_FGROUP_ALL);
}


// 创建 Future 竞速聚合
XXAPI xfuture* xFutureRace(xfuture** arrFuture, int iCount)
{
	return __xnetFutureCreateGroup(arrFuture, iCount, __XNET_FGROUP_RACE);
}


// 内部函数：__xnetTaskGroupWaitSnapshot
static bool __xnetTaskGroupWaitSnapshot(xtaskgroup* pGroup, uint32 iMode, uint32 iTimeoutMs, int64 iDeadlineMs)
{
	xfuture** arrSnapshot = NULL;
	xfuture* pWaitFuture = NULL;
	bool bOk = true;
	int iCount = 0;
	int i;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return false;
	}

	xrtMutexLock(pGroup->pLock);
	iCount = pGroup->iCount;
	if ( iCount > 0 ) {
		arrSnapshot = (xfuture**)XNET_ALLOC(sizeof(xfuture*) * (size_t)iCount);
		if ( arrSnapshot == NULL ) {
			xrtMutexUnlock(pGroup->pLock);
			return false;
		}
		memset(arrSnapshot, 0, sizeof(xfuture*) * (size_t)iCount);
		for ( i = 0; i < iCount; ++i ) {
			arrSnapshot[i] = xFutureAddRef(pGroup->arrFuture[i]);
			if ( arrSnapshot[i] == NULL ) {
				bOk = false;
				break;
			}
		}
	}
	xrtMutexUnlock(pGroup->pLock);

	if ( !bOk ) {
		if ( arrSnapshot ) {
			for ( i = 0; i < iCount; ++i ) {
				if ( arrSnapshot[i] ) {
					xFutureRelease(arrSnapshot[i]);
				}
			}
			XNET_FREE(arrSnapshot);
		}
		return false;
	}

	if ( iCount <= 0 ) {
		if ( arrSnapshot ) {
			XNET_FREE(arrSnapshot);
		}
		return true;
	}

	pWaitFuture = xFutureWhenAll(arrSnapshot, iCount);
	for ( i = 0; i < iCount; ++i ) {
		if ( arrSnapshot[i] ) {
			xFutureRelease(arrSnapshot[i]);
		}
	}
	XNET_FREE(arrSnapshot);

	if ( pWaitFuture == NULL ) {
		return false;
	}

	if ( iMode == 0 ) {
		bOk = xFutureWait(pWaitFuture);
	} else if ( iMode == 1 ) {
		bOk = xFutureWaitTimeout(pWaitFuture, iTimeoutMs);
	} else {
		bOk = xFutureWaitUntil(pWaitFuture, iDeadlineMs);
	}

	xFutureRelease(pWaitFuture);
	return bOk;
}


// 内部函数：__xnetTaskGroupOnChildFinally
static void __xnetTaskGroupOnChildFinally(const xfuture_result* pIn, ptr pArg)
{
	__xnet_task_group_future_ctx* pCtx = (__xnet_task_group_future_ctx*)pArg;

	(void)pIn;
	if ( pCtx == NULL ) {
		return;
	}

	__xnetTaskGroupMaybeResolveJoin(pCtx->pGroup);
	__xnetTaskGroupRelease(pCtx->pGroup);
	XNET_FREE(pCtx);
}


// 内部函数：__xnetTaskGroupAttachChildWatcher
static bool __xnetTaskGroupAttachChildWatcher(xtaskgroup* pGroup, xfuture* pFuture)
{
	__xnet_task_group_future_ctx* pCtx = NULL;
	xfuture* pHook = NULL;

	if ( pGroup == NULL || pFuture == NULL ) {
		return false;
	}

	pCtx = (__xnet_task_group_future_ctx*)XNET_ALLOC(sizeof(__xnet_task_group_future_ctx));
	if ( pCtx == NULL ) {
		return false;
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pGroup = __xnetTaskGroupAddRef(pGroup);

	pHook = xFutureFinallyEngine(pFuture, NULL, 0, __xnetTaskGroupOnChildFinally, pCtx);
	if ( pHook == NULL ) {
		__xnetTaskGroupRelease(pCtx->pGroup);
		XNET_FREE(pCtx);
		return false;
	}

	xFutureRelease(pHook);
	return true;
}


// 内部函数：加入任务组 create Future
static xfuture* __xnetTaskGroupCreateJoinFuture(xtaskgroup* pGroup)
{
	xfuture* pWaitFuture = NULL;
	xfuture* pOutFuture = NULL;
	xpromise* pPromise = NULL;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return NULL;
	}

	xrtMutexLock(pGroup->pLock);
	if ( pGroup->pJoinFuture ) {
		pOutFuture = xFutureAddRef(pGroup->pJoinFuture);
		xrtMutexUnlock(pGroup->pLock);
		return pOutFuture;
	}
	xrtMutexUnlock(pGroup->pLock);

	pWaitFuture = xFutureCreate();
	if ( pWaitFuture == NULL ) {
		return NULL;
	}
	pPromise = xPromiseCreate(pWaitFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pWaitFuture);
		return NULL;
	}

	xrtMutexLock(pGroup->pLock);
	if ( pGroup->pJoinFuture == NULL && pGroup->pJoinPromise == NULL ) {
		pGroup->pJoinFuture = pWaitFuture;
		pGroup->pJoinPromise = pPromise;
		pOutFuture = xFutureAddRef(pWaitFuture);
		xrtMutexUnlock(pGroup->pLock);
		__xnetTaskGroupMaybeResolveJoin(pGroup);
		return pOutFuture;
	} else {
		pOutFuture = pGroup->pJoinFuture ? xFutureAddRef(pGroup->pJoinFuture) : NULL;
		xrtMutexUnlock(pGroup->pLock);
		xPromiseDestroy(pPromise);
		xFutureRelease(pWaitFuture);
		return pOutFuture;
	}
}


// 内部函数：__xnetTaskGroupCloseInternal
static void __xnetTaskGroupCloseInternal(xtaskgroup* pGroup)
{
	xpromise* pPromise = NULL;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return;
	}

	xrtMutexLock(pGroup->pLock);
	pGroup->bClosed = true;
	pPromise = pGroup->pScopePromise;
	xrtMutexUnlock(pGroup->pLock);
	if ( pPromise ) {
		(void)xPromiseClose(pPromise, (str)"task group closed");
	}
	__xnetTaskGroupMaybeResolveJoin(pGroup);
}


// 创建任务组
XXAPI xtaskgroup* xTaskGroupCreate(void)
{
	xtaskgroup* pGroup = (xtaskgroup*)XNET_ALLOC(sizeof(xtaskgroup));

	if ( pGroup == NULL ) {
		return NULL;
	}
	memset(pGroup, 0, sizeof(*pGroup));
	pGroup->iRefCount = 1;

	pGroup->pLock = xrtMutexCreate();
	if ( pGroup->pLock == NULL ) {
		XNET_FREE(pGroup);
		return NULL;
	}

	return pGroup;
}


// xTaskGroupCreateChild 相关处理
XXAPI xtaskgroup* xTaskGroupCreateChild(xtaskgroup* pParent)
{
	xtaskgroup* pChild = NULL;
	xfuture* pParentScope = NULL;
	xfuture* pChildJoin = NULL;

	if ( pParent == NULL ) {
		return NULL;
	}

	pChild = xTaskGroupCreate();
	if ( pChild == NULL ) {
		return NULL;
	}

	pParentScope = __xnetTaskGroupGetScopeFuture(pParent);
	if ( pParentScope == NULL ) {
		xTaskGroupDestroy(pChild);
		return NULL;
	}

	if ( !xTaskGroupBindParent(pChild, pParentScope) ) {
		xFutureRelease(pParentScope);
		xTaskGroupDestroy(pChild);
		return NULL;
	}
	xFutureRelease(pParentScope);

	pChildJoin = xTaskGroupJoinFuture(pChild);
	if ( pChildJoin == NULL ) {
		xTaskGroupDestroy(pChild);
		return NULL;
	}

	if ( !xTaskGroupAddFuture(pParent, pChildJoin) ) {
		xFutureRelease(pChildJoin);
		xTaskGroupDestroy(pChild);
		return NULL;
	}

	xFutureRelease(pChildJoin);
	return pChild;
}


// 销毁任务组
XXAPI void xTaskGroupDestroy(xtaskgroup* pGroup)
{
	int i;
	xpromise* pScopePromise = NULL;

	if ( pGroup == NULL ) {
		return;
	}

	if ( pGroup->pLock ) {
		xrtMutexLock(pGroup->pLock);
		pGroup->bClosed = true;
		pScopePromise = pGroup->pScopePromise;
		for ( i = 0; i < pGroup->iCount; ++i ) {
			if ( pGroup->arrFuture && pGroup->arrFuture[i] ) {
				if ( xFutureState(pGroup->arrFuture[i]) == XFUTURE_PENDING ) {
					(void)xFutureRequestCancel(pGroup->arrFuture[i]);
				}
				xFutureRelease(pGroup->arrFuture[i]);
				pGroup->arrFuture[i] = NULL;
			}
		}
		pGroup->iCount = 0;
		xrtMutexUnlock(pGroup->pLock);
	}

	if ( pScopePromise ) {
		(void)xPromiseCancel(pScopePromise, (str)"task group destroyed");
	}

	__xnetTaskGroupMaybeResolveJoin(pGroup);

	__xnetTaskGroupRelease(pGroup);
}


// 关闭任务组
XXAPI void xTaskGroupClose(xtaskgroup* pGroup)
{
	__xnetTaskGroupCloseInternal(pGroup);
}


// 内部函数：__xnetTaskGroupOnParentFinally
static void __xnetTaskGroupOnParentFinally(const xfuture_result* pIn, ptr pArg)
{
	__xnet_task_group_parent_ctx* pCtx = (__xnet_task_group_parent_ctx*)pArg;

	if ( pCtx == NULL ) {
		return;
	}

	if ( pIn && (pIn->iStatus == XRT_NET_CANCELLED || pIn->iStatus == XRT_NET_CLOSED) ) {
		xTaskGroupClose(pCtx->pGroup);
		xTaskGroupCancel(pCtx->pGroup);
	}

	__xnetTaskGroupRelease(pCtx->pGroup);
	XNET_FREE(pCtx);
}


// xTaskGroupBindParent 相关处理
XXAPI bool xTaskGroupBindParent(xtaskgroup* pGroup, xfuture* pParent)
{
	__xnet_task_group_parent_ctx* pCtx = NULL;
	xfuture* pHook = NULL;

	if ( pGroup == NULL || pParent == NULL || pGroup->pLock == NULL ) {
		return false;
	}

	pCtx = (__xnet_task_group_parent_ctx*)XNET_ALLOC(sizeof(__xnet_task_group_parent_ctx));
	if ( pCtx == NULL ) {
		return false;
	}
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->pGroup = __xnetTaskGroupAddRef(pGroup);

	pHook = xFutureFinallyEngine(pParent, NULL, 0, __xnetTaskGroupOnParentFinally, pCtx);
	if ( pHook == NULL ) {
		__xnetTaskGroupRelease(pCtx->pGroup);
		XNET_FREE(pCtx);
		return false;
	}

	xrtMutexLock(pGroup->pLock);
	pGroup->iParentBindCount++;
	xrtMutexUnlock(pGroup->pLock);

	xFutureRelease(pHook);
	return true;
}


// 增加任务组 Future
XXAPI bool xTaskGroupAddFuture(xtaskgroup* pGroup, xfuture* pFuture)
{
	bool bOk = false;
	bool bWatchOk = false;

	if ( pGroup == NULL || pFuture == NULL || pGroup->pLock == NULL ) {
		return false;
	}

	xrtMutexLock(pGroup->pLock);
	if ( !pGroup->bClosed && __xnetTaskGroupEnsureCapacity(pGroup, pGroup->iCount + 1) ) {
		pGroup->arrFuture[pGroup->iCount] = xFutureAddRef(pFuture);
		if ( pGroup->arrFuture[pGroup->iCount] ) {
			pGroup->iCount++;
			bOk = true;
		}
	}
	xrtMutexUnlock(pGroup->pLock);

	if ( !bOk ) {
		return false;
	}

	bWatchOk = __xnetTaskGroupAttachChildWatcher(pGroup, pFuture);
	if ( !bWatchOk ) {
		xrtMutexLock(pGroup->pLock);
		if ( pGroup->iCount > 0 ) {
			int iIndex = pGroup->iCount - 1;
			if ( pGroup->arrFuture[iIndex] == pFuture ) {
				xFutureRelease(pGroup->arrFuture[iIndex]);
				pGroup->arrFuture[iIndex] = NULL;
				pGroup->iCount--;
			}
		}
		xrtMutexUnlock(pGroup->pLock);
		return false;
	}

	__xnetTaskGroupMaybeResolveJoin(pGroup);
	return true;
}


// 统计任务组
XXAPI int xTaskGroupCount(xtaskgroup* pGroup)
{
	int iCount = 0;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return 0;
	}

	xrtMutexLock(pGroup->pLock);
	iCount = pGroup->iCount;
	xrtMutexUnlock(pGroup->pLock);
	return iCount;
}


// xTaskGroupReapCompleted 相关处理
XXAPI int xTaskGroupReapCompleted(xtaskgroup* pGroup)
{
	int iReaped = 0;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return 0;
	}

	xrtMutexLock(pGroup->pLock);
	iReaped = __xnetTaskGroupReapCompletedUnlocked(pGroup);
	xrtMutexUnlock(pGroup->pLock);
	return iReaped;
}


// 加入任务组 Future
XXAPI xfuture* xTaskGroupJoinFuture(xtaskgroup* pGroup)
{
	return __xnetTaskGroupCreateJoinFuture(pGroup);
}


// 等待任务组结束
XXAPI bool xTaskGroupJoin(xtaskgroup* pGroup)
{
	xfuture* pFuture = xTaskGroupJoinFuture(pGroup);
	bool bOk = false;

	if ( pFuture == NULL ) {
		return false;
	}

	xTaskGroupClose(pGroup);
	bOk = xFutureWait(pFuture);
	xFutureRelease(pFuture);
	if ( bOk ) {
		(void)xTaskGroupReapCompleted(pGroup);
	}
	return bOk;
}


// 限时等待任务组结束
XXAPI bool xTaskGroupJoinTimeout(xtaskgroup* pGroup, int64 iTimeoutMs)
{
	xfuture* pFuture = xTaskGroupJoinFuture(pGroup);
	bool bOk = false;

	if ( pFuture == NULL ) {
		return false;
	}

	xTaskGroupClose(pGroup);
	bOk = xFutureWaitTimeout(pFuture, iTimeoutMs);
	xFutureRelease(pFuture);
	if ( bOk ) {
		(void)xTaskGroupReapCompleted(pGroup);
	}
	return bOk;
}


// 等待任务组结束直到指定时刻
XXAPI bool xTaskGroupJoinUntil(xtaskgroup* pGroup, int64 iDeadlineMs)
{
	xfuture* pFuture = xTaskGroupJoinFuture(pGroup);
	bool bOk = false;

	if ( pFuture == NULL ) {
		return false;
	}

	xTaskGroupClose(pGroup);
	bOk = xFutureWaitUntil(pFuture, iDeadlineMs);
	xFutureRelease(pFuture);
	if ( bOk ) {
		(void)xTaskGroupReapCompleted(pGroup);
	}
	return bOk;
}


// 等待任务组
XXAPI bool xTaskGroupWait(xtaskgroup* pGroup)
{
	bool bOk = __xnetTaskGroupWaitSnapshot(pGroup, 0, 0, 0);
	if ( bOk ) {
		(void)xTaskGroupReapCompleted(pGroup);
	}
	return bOk;
}


// 等待任务组超时
XXAPI bool xTaskGroupWaitTimeout(xtaskgroup* pGroup, int64 iTimeoutMs)
{
	uint32 iWaitMs = 0;

	if ( iTimeoutMs < 0 ) {
		return false;
	}
	if ( iTimeoutMs > (int64)UINT32_MAX ) {
		iWaitMs = UINT32_MAX;
	} else {
		iWaitMs = (uint32)iTimeoutMs;
	}

	{
		bool bOk = __xnetTaskGroupWaitSnapshot(pGroup, 1, iWaitMs, 0);
		if ( bOk ) {
			(void)xTaskGroupReapCompleted(pGroup);
		}
		return bOk;
	}
}


// 等待任务组直到指定时刻
XXAPI bool xTaskGroupWaitUntil(xtaskgroup* pGroup, int64 iDeadlineMs)
{
	bool bOk = __xnetTaskGroupWaitSnapshot(pGroup, 2, 0, iDeadlineMs);
	if ( bOk ) {
		(void)xTaskGroupReapCompleted(pGroup);
	}
	return bOk;
}


// 取消任务组
XXAPI void xTaskGroupCancel(xtaskgroup* pGroup)
{
	int i;
	xpromise* pScopePromise = NULL;

	if ( pGroup == NULL || pGroup->pLock == NULL ) {
		return;
	}

	xrtMutexLock(pGroup->pLock);
	pScopePromise = pGroup->pScopePromise;
	for ( i = 0; i < pGroup->iCount; ++i ) {
		if ( pGroup->arrFuture[i] ) {
			(void)xFutureRequestCancel(pGroup->arrFuture[i]);
		}
	}
	xrtMutexUnlock(pGroup->pLock);
	if ( pScopePromise ) {
		(void)xPromiseCancel(pScopePromise, (str)"task group cancelled");
	}
}


// 内部函数：__xnetTaskGroupAdoptFuture
static xfuture* __xnetTaskGroupAdoptFuture(xtaskgroup* pGroup, xfuture* pFuture)
{
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( pGroup == NULL ) {
		return pFuture;
	}
	if ( !xTaskGroupAddFuture(pGroup, pFuture) ) {
		xFutureRelease(pFuture);
		return NULL;
	}
	return pFuture;
}


// xTaskGroupRunEngine 相关处理
XXAPI xfuture* xTaskGroupRunEngine(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg)
{
	return __xnetTaskGroupAdoptFuture(pGroup, xTaskRunEngine(pEngine, iAffinityKey, pfnTask, pArg));
}


// xTaskGroupRunDelayed 相关处理
XXAPI xfuture* xTaskGroupRunDelayed(xtaskgroup* pGroup, xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg)
{
	return __xnetTaskGroupAdoptFuture(pGroup, xTaskRunDelayed(pEngine, iAffinityKey, iDelayMs, pfnTask, pArg));
}


// xTaskGroupRunThread 相关处理
XXAPI xfuture* xTaskGroupRunThread(xtaskgroup* pGroup, xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize)
{
	return __xnetTaskGroupAdoptFuture(pGroup, xTaskRunThread(pfnTask, pArg, iStackSize));
}

#if !defined(XRT_NO_COROUTINE)
// xTaskGroupRunCo 相关处理
XXAPI xfuture* xTaskGroupRunCo(xtaskgroup* pGroup, xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize)
{
	return __xnetTaskGroupAdoptFuture(pGroup, xTaskRunCo(pSched, pfnTask, pArg, iStackSize));
}
#endif
#endif

// 等待网络 Future
XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs)
{
	xnet_result iStatus;
	if ( !pFuture ) return XRT_NET_ERROR;
	__xnetFutureAddRefInternal(pFuture);

	(void)__xnetFuturePumpCurrentAuto();
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		if ( iTimeoutMs == 0 ) {
			__xnetFutureUnlock(pFuture);
			(void)__xnetFuturePumpCurrentAuto();
			__xnetFutureReleaseRefInternal(pFuture);
			return pFuture->bDone ? xrtNetFutureStatus(pFuture) : XRT_NET_TIMEOUT;
		}
		if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
			while ( !pFuture->bDone ) {
				__xnetFutureUnlock(pFuture);
				(void)__xnetFuturePumpCurrentAuto();
				__xnetFutureLock(pFuture);
				if ( pFuture->bDone ) {
					break;
				}
				(void)__xnetFutureWaitOnce(pFuture, XNET_WAIT_INFINITE);
			}
		} else {
			uint64 iDeadlineMs;
				#if defined(_WIN32) || defined(_WIN64)
					iDeadlineMs = GetTickCount64() + (uint64)iTimeoutMs;
				#else
					struct timespec tNow;
					if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) {
						__xnetFutureUnlock(pFuture);
						__xnetFutureReleaseRefInternal(pFuture);
						return XRT_NET_ERROR;
					}
				iDeadlineMs = ((uint64)tNow.tv_sec * 1000ULL) + ((uint64)tNow.tv_nsec / 1000000ULL) + (uint64)iTimeoutMs;
			#endif

			while ( !pFuture->bDone ) {
				uint32 iWaitMs;
				uint64 iNowMs;
				bool bSignaled;
				__xnetFutureUnlock(pFuture);
				(void)__xnetFuturePumpCurrentAuto();
				__xnetFutureLock(pFuture);
				if ( pFuture->bDone ) {
					break;
				}
				#if defined(_WIN32) || defined(_WIN64)
					iNowMs = GetTickCount64();
				#else
					struct timespec tNowLoop;
					if ( clock_gettime(CLOCK_MONOTONIC, &tNowLoop) != 0 ) {
						__xnetFutureUnlock(pFuture);
						__xnetFutureReleaseRefInternal(pFuture);
						return XRT_NET_ERROR;
					}
					iNowMs = ((uint64)tNowLoop.tv_sec * 1000ULL) + ((uint64)tNowLoop.tv_nsec / 1000000ULL);
				#endif
				if ( iNowMs >= iDeadlineMs ) {
					__xnetFutureUnlock(pFuture);
					__xnetFutureReleaseRefInternal(pFuture);
					return XRT_NET_TIMEOUT;
				}
				iWaitMs = (uint32)(iDeadlineMs - iNowMs);
				bSignaled = __xnetFutureWaitOnce(pFuture, iWaitMs);
				if ( !bSignaled && !pFuture->bDone ) {
					__xnetFutureUnlock(pFuture);
					__xnetFutureReleaseRefInternal(pFuture);
					return XRT_NET_TIMEOUT;
				}
			}
		}
	}

	iStatus = pFuture->iStatus;
	__xnetFutureUnlock(pFuture);
	(void)__xnetFuturePumpCurrentAuto();
	__xnetFutureReleaseRefInternal(pFuture);
	return iStatus;
}


// 获取网络 Future 状态
XXAPI xnet_result xrtNetFutureStatus(xnetfuture* pFuture)
{
	xnet_result iStatus;
	if ( !pFuture ) return XRT_NET_ERROR;
	__xnetFutureLock(pFuture);
	iStatus = pFuture->bDone ? pFuture->iStatus : XRT_NET_AGAIN;
	__xnetFutureUnlock(pFuture);
	return iStatus;
}


// 获取网络 Future 值
XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture)
{
	ptr pValue;
	if ( !pFuture ) return NULL;
	__xnetFutureLock(pFuture);
	pValue = pFuture->bDone ? pFuture->pValue : NULL;
	__xnetFutureUnlock(pFuture);
	return pValue;
}


// 等待网络 Future 直到指定时刻
XXAPI xnet_result xrtNetFutureWaitUntil(xnetfuture* pFuture, int64_t iDeadlineMs)
{
	int64_t iNowMs;
	uint64 iRemainMs;

	if ( !pFuture ) return XRT_NET_ERROR;
	iNowMs = (int64_t)__xnetSyncNowMs();
	if ( iNowMs >= iDeadlineMs ) {
		return xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN ? XRT_NET_TIMEOUT : xrtNetFutureWait(pFuture, 0);
	}

	iRemainMs = (uint64)(iDeadlineMs - iNowMs);
	if ( iRemainMs > (uint64)XNET_WAIT_INFINITE ) {
		iRemainMs = (uint64)XNET_WAIT_INFINITE;
	}
	return xrtNetFutureWait(pFuture, (uint32)iRemainMs);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：等待 Future 协程 core
static xnet_result __xnetFutureWaitCoCore(xnetfuture* pFuture, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	xcoevent pEvent = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	bool bWaitOk = false;

	if ( !pFuture ) return XRT_NET_ERROR;
	__xnetFutureAddRefInternal(pFuture);

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		iStatus = pFuture->iStatus;
		__xnetFutureUnlock(pFuture);
		__xnetFutureReleaseRefInternal(pFuture);
		return iStatus;
	}
	__xnetFutureUnlock(pFuture);

	pEvent = __xnetFutureEnsureCoEvent(pFuture);
	if ( !pEvent ) {
		__xnetFutureReleaseRefInternal(pFuture);
		return XRT_NET_ERROR;
	}

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		iStatus = pFuture->iStatus;
		__xnetFutureUnlock(pFuture);
		__xnetFutureReleaseRefInternal(pFuture);
		return iStatus;
	}
	pFuture->iCoWaitActive++;
	__xnetFutureUnlock(pFuture);

	if ( iWaitMode == 0 ) {
		bWaitOk = xrtCoWaitEvent(pEvent);
	}
	else if ( iWaitMode == 1 ) {
		bWaitOk = xrtCoWaitEventTimeout(pEvent, iTimeoutMs);
	}
	else {
		bWaitOk = xrtCoWaitEventUntil(pEvent, iDeadlineMs);
	}

	__xnetFutureLock(pFuture);
	if ( pFuture->iCoWaitActive > 0 ) {
		pFuture->iCoWaitActive--;
	}
	iStatus = pFuture->bDone ? pFuture->iStatus : XRT_NET_AGAIN;
	__xnetFutureUnlock(pFuture);

	if ( !bWaitOk && iStatus == XRT_NET_AGAIN ) {
		if ( xrtCoIsCancelRequested() ) {
			__xnetSyncSetError("coroutine future wait was interrupted before resolve.");
			__xnetFutureReleaseRefInternal(pFuture);
			return XRT_NET_ERROR;
		}
		__xnetFutureReleaseRefInternal(pFuture);
		return XRT_NET_TIMEOUT;
	}

	__xnetFutureReleaseRefInternal(pFuture);
	return (iStatus == XRT_NET_AGAIN) ? XRT_NET_ERROR : iStatus;
}


// 等待网络 Future 协程直到指定时刻
XXAPI xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs)
{
	return __xnetFutureWaitCoCore(pFuture, 2, iDeadlineMs, 0);
}


// 等待网络 Future 协程超时
XXAPI xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs)
{
	if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
		return __xnetFutureWaitCoCore(pFuture, 0, 0, 0);
	}

	return __xnetFutureWaitCoCore(pFuture, 1, 0, iTimeoutMs);
}


// 等待网络 Future 协程
XXAPI xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture)
{
	return __xnetFutureWaitCoCore(pFuture, 0, 0, 0);
}
#endif

#if defined(XXRTL_CORE)
// 等待 Future
XXAPI bool xFutureWait(xfuture* pFuture)
{
	return xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE) == XRT_NET_OK;
}


// 等待 Future 超时
XXAPI bool xFutureWaitTimeout(xfuture* pFuture, int64 iTimeoutMs)
{
	if ( iTimeoutMs < 0 ) return xFutureWait(pFuture);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetFutureWait(pFuture, (uint32)iTimeoutMs) == XRT_NET_OK;
}


// 等待 Future 直到指定时刻
XXAPI bool xFutureWaitUntil(xfuture* pFuture, int64 iDeadlineMs)
{
	return xrtNetFutureWaitUntil(pFuture, iDeadlineMs) == XRT_NET_OK;
}

#if !defined(XRT_NO_COROUTINE)
// 等待 Future 协程
XXAPI bool xFutureWaitCo(xfuture* pFuture)
{
	return xrtNetFutureWaitCo(pFuture) == XRT_NET_OK;
}


// 等待 Future 协程超时
XXAPI bool xFutureWaitCoTimeout(xfuture* pFuture, int64 iTimeoutMs)
{
	if ( iTimeoutMs < 0 ) return xFutureWaitCo(pFuture);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetFutureWaitCoTimeout(pFuture, (uint32)iTimeoutMs) == XRT_NET_OK;
}


// 等待 Future 协程直到指定时刻
XXAPI bool xFutureWaitCoUntil(xfuture* pFuture, int64 iDeadlineMs)
{
	return xrtNetFutureWaitCoUntil(pFuture, iDeadlineMs) == XRT_NET_OK;
}
#endif

// 等待 Future 值
XXAPI ptr xFutureWaitValue(xfuture* pFuture)
{
	return xFutureWait(pFuture) ? xFutureValue(pFuture) : NULL;
}


// 等待 Future 值超时
XXAPI ptr xFutureWaitValueTimeout(xfuture* pFuture, int64 iTimeoutMs)
{
	return xFutureWaitTimeout(pFuture, iTimeoutMs) ? xFutureValue(pFuture) : NULL;
}


// 等待 Future 值直到指定时刻
XXAPI ptr xFutureWaitValueUntil(xfuture* pFuture, int64 iDeadlineMs)
{
	return xFutureWaitUntil(pFuture, iDeadlineMs) ? xFutureValue(pFuture) : NULL;
}

#if !defined(XRT_NO_COROUTINE)
// 等待 Future 协程值
XXAPI ptr xFutureWaitCoValue(xfuture* pFuture)
{
	return xFutureWaitCo(pFuture) ? xFutureValue(pFuture) : NULL;
}


// 等待 Future 协程值超时
XXAPI ptr xFutureWaitCoValueTimeout(xfuture* pFuture, int64 iTimeoutMs)
{
	return xFutureWaitCoTimeout(pFuture, iTimeoutMs) ? xFutureValue(pFuture) : NULL;
}


// 等待 Future 协程值直到指定时刻
XXAPI ptr xFutureWaitCoValueUntil(xfuture* pFuture, int64 iDeadlineMs)
{
	return xFutureWaitCoUntil(pFuture, iDeadlineMs) ? xFutureValue(pFuture) : NULL;
}
#endif
#endif



/* ============================== Hidden convenience engine helpers ============================== */

XXAPI xnetengine* xrtNetSyncGetHiddenEngine(void)
{
	__xnet_sync_hidden_state* pState = __xnetSyncHiddenState();
	xnetengine* pEngine;

	__xnetSyncSpinLock(&pState->iLock);
	pEngine = pState->pEngine;
	if ( pEngine && !pEngine->bRunning ) {
		xrtNetEngineDestroy(pEngine);
		pEngine = NULL;
		pState->pEngine = NULL;
	}

	if ( !pEngine ) {
		xnetengineconfig tCfg;
		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		if ( pEngine ) {
			if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
				xrtNetEngineDestroy(pEngine);
				pEngine = NULL;
			}
		}
		pState->pEngine = pEngine;
	}
	__xnetSyncSpinUnlock(&pState->iLock);
	return pEngine;
}


// xrtNetSyncShutdownHiddenEngine 相关处理
XXAPI void xrtNetSyncShutdownHiddenEngine(void)
{
	__xnet_sync_hidden_state* pState = __xnetSyncHiddenState();
	xnetengine* pEngine;

	__xnetSyncSpinLock(&pState->iLock);
	pEngine = pState->pEngine;
	pState->pEngine = NULL;
	__xnetSyncSpinUnlock(&pState->iLock);

	if ( pEngine ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
}


// 内部函数：解析同步引擎
static xnetengine* __xnetSyncResolveEngine(xnetengine* pEngine)
{
	return pEngine ? pEngine : xrtNetSyncGetHiddenEngine();
}


// 内部函数：分发 Future 任务
static void __xnetFutureTaskDispatch(xnetworker* pWorker, ptr pArg)
{
	__xnet_future_task_ctx* pCtx = (__xnet_future_task_ctx*)pArg;
	xnetfuture* pFuture = NULL;
	xnet_future_task_fn pfnTask = NULL;
	ptr pTaskArg = NULL;
	ptr pValue = NULL;
	xnet_result iStatus = XRT_NET_ERROR;

	if ( pCtx == NULL ) {
		return;
	}

	pFuture = pCtx->pFuture;
	pfnTask = pCtx->pfnTask;
	pTaskArg = pCtx->pArg;
	XNET_FREE(pCtx);

	if ( pfnTask != NULL ) {
		iStatus = pfnTask(pWorker, pTaskArg, &pValue);
	}

	(void)__xnetFutureResolve(pFuture, iStatus, pValue);
	__xnetFutureReleaseAsyncHold(pFuture);
}

#if defined(XXRTL_CORE)
// 内部函数：更新任务状态
static void __xnetTaskUpdateState(xtask* pTask, int32 iStatus)
{
	if ( pTask == NULL ) {
		return;
	}
	if ( iStatus == XRT_NET_CANCELLED ) {
		pTask->iState = XTASK_CANCELLED;
	}
	else if ( iStatus == XRT_NET_CLOSED ) {
		pTask->iState = XTASK_CLOSED;
	}
	else {
		pTask->iState = XTASK_DONE;
	}
}


// 内部函数：完成任务
static void __xnetTaskFinalize(xtask* pTask, xfuture_result* pResult)
{
	if ( pTask == NULL ) {
		return;
	}
	if ( pResult == NULL ) {
		xfuture_result tResult;
		memset(&tResult, 0, sizeof(tResult));
		tResult.iStatus = XRT_NET_ERROR;
		tResult.sError = (str)"task did not provide a result.";
		pResult = &tResult;
		__xnetTaskUpdateState(pTask, tResult.iStatus);
		(void)__xnetPromiseCompleteResult(pTask->pPromise, &tResult);
	}
	else {
		__xnetTaskUpdateState(pTask, pResult->iStatus);
		(void)__xnetPromiseCompleteResult(pTask->pPromise, pResult);
	}
	xPromiseDestroy(pTask->pPromise);
	pTask->pPromise = NULL;
	xFutureRelease(pTask->pFuture);
	pTask->pFuture = NULL;
	XNET_FREE(pTask);
}


// 内部函数：分发任务
static void __xnetTaskDispatch(xnetworker* pWorker, ptr pArg)
{
	__xnet_task_ctx* pCtx = (__xnet_task_ctx*)pArg;
	xtask* pTask = NULL;
	xfuture_result tResult;

	memset(&tResult, 0, sizeof(tResult));
	if ( pCtx == NULL ) {
		return;
	}
	pTask = pCtx->pTask;
	XNET_FREE(pCtx);
	if ( pTask == NULL ) {
		return;
	}

	pTask->iState = XTASK_RUNNING;
	if ( pTask->pfnEngineTask != NULL ) {
		tResult.iStatus = pTask->pfnEngineTask(pWorker, pTask->pArg, &tResult);
	} else {
		tResult.iStatus = XRT_NET_ERROR;
		tResult.sError = (str)"task callback is null.";
		tResult.iFlags = XFUTURE_RESULT_F_NONE;
	}
	__xnetTaskFinalize(pTask, &tResult);
}


// 内部函数：分发任务线程
static uint32 __xnetTaskThreadDispatch(ptr pArg)
{
	__xnet_task_thread_ctx* pCtx = (__xnet_task_thread_ctx*)pArg;
	xtask* pTask = NULL;
	xfuture_result tResult;
	int32 iStatus = XRT_NET_ERROR;

	memset(&tResult, 0, sizeof(tResult));
	if ( pCtx == NULL ) {
		return (uint32)XRT_NET_ERROR;
	}
	pTask = pCtx->pTask;
	XNET_FREE(pCtx);
	if ( pTask == NULL ) {
		return (uint32)XRT_NET_ERROR;
	}

	pTask->iState = XTASK_RUNNING;
	if ( pTask->pfnThreadTask != NULL ) {
		iStatus = pTask->pfnThreadTask(pTask->pArg, &tResult);
		tResult.iStatus = iStatus;
	}
	else {
		tResult.iStatus = XRT_NET_ERROR;
		tResult.sError = (str)"thread task callback is null.";
		tResult.iFlags = XFUTURE_RESULT_F_NONE;
	}
	__xnetTaskFinalize(pTask, &tResult);
	return (uint32)tResult.iStatus;
}


// 内部函数：分发任务协程
static void __xnetTaskCoDispatch(ptr pArg)
{
	__xnet_task_co_ctx* pCtx = (__xnet_task_co_ctx*)pArg;
	xtask* pTask = NULL;
	xfuture_result tResult;
	int32 iStatus = XRT_NET_ERROR;

	memset(&tResult, 0, sizeof(tResult));
	if ( pCtx == NULL ) {
		return;
	}
	pTask = pCtx->pTask;
	XNET_FREE(pCtx);
	if ( pTask == NULL ) {
		return;
	}

	pTask->iState = XTASK_RUNNING;
	if ( pTask->pfnCoTask != NULL ) {
		iStatus = pTask->pfnCoTask(pTask->pArg, &tResult);
		tResult.iStatus = iStatus;
	}
	else {
		tResult.iStatus = XRT_NET_ERROR;
		tResult.sError = (str)"coroutine task callback is null.";
		tResult.iFlags = XFUTURE_RESULT_F_NONE;
	}
	__xnetTaskFinalize(pTask, &tResult);
}


// 内部函数：创建引擎任务 Future
static xfuture* __xnetCreateEngineTaskFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg, bool bDelayed)
{
	xnetengine* pResolvedEngine = NULL;
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	xtask* pTask = NULL;
	__xnet_task_ctx* pCtx = NULL;
	xnet_result iPostResult;

	if ( pfnTask == NULL ) {
		__xnetSyncSetError("task callback is null.");
		return NULL;
	}

	pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( pResolvedEngine == NULL ) {
		__xnetSyncSetError("unable to resolve task engine.");
		return NULL;
	}

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}

	pTask = (xtask*)XNET_ALLOC(sizeof(xtask));
	if ( pTask == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	pCtx = (__xnet_task_ctx*)XNET_ALLOC(sizeof(__xnet_task_ctx));
	if ( pCtx == NULL ) {
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	memset(pTask, 0, sizeof(*pTask));
	memset(pCtx, 0, sizeof(*pCtx));
	pTask->iState = XTASK_QUEUED;
	pTask->pFuture = xFutureAddRef(pFuture);
	pTask->pPromise = pPromise;
	pTask->pfnEngineTask = pfnTask;
	pTask->pArg = pArg;
	pCtx->pTask = pTask;

	if ( bDelayed ) {
		iPostResult = xrtNetEnginePostDelayed(pResolvedEngine, iAffinityKey, iDelayMs, __xnetTaskDispatch, pCtx);
	} else {
		iPostResult = xrtNetEnginePost(pResolvedEngine, iAffinityKey, __xnetTaskDispatch, pCtx);
	}

	if ( iPostResult != XRT_NET_OK ) {
		XNET_FREE(pCtx);
		xFutureRelease(pTask->pFuture);
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	return pFuture;
}


// 内部函数：创建线程任务 Future
static xfuture* __xnetCreateThreadTaskFuture(xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	xtask* pTask = NULL;
	__xnet_task_thread_ctx* pCtx = NULL;
	xthread pThread = NULL;

	if ( pfnTask == NULL ) {
		__xnetSyncSetError("thread task callback is null.");
		return NULL;
	}

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}

	pTask = (xtask*)XNET_ALLOC(sizeof(xtask));
	if ( pTask == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	pCtx = (__xnet_task_thread_ctx*)XNET_ALLOC(sizeof(__xnet_task_thread_ctx));
	if ( pCtx == NULL ) {
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	memset(pTask, 0, sizeof(*pTask));
	memset(pCtx, 0, sizeof(*pCtx));
	pTask->iState = XTASK_QUEUED;
	pTask->pFuture = xFutureAddRef(pFuture);
	pTask->pPromise = pPromise;
	pTask->pfnThreadTask = pfnTask;
	pTask->pArg = pArg;
	pCtx->pTask = pTask;

	pThread = xrtThreadCreate(__xnetTaskThreadDispatch, pCtx, iStackSize);
	if ( pThread == NULL ) {
		XNET_FREE(pCtx);
		xFutureRelease(pTask->pFuture);
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	pThread->bAutoDestroy = TRUE;
	pTask->pThread = pThread;
	return pFuture;
}

#if !defined(XRT_NO_COROUTINE)
// 内部函数：创建协程任务 Future
static xfuture* __xnetCreateCoTaskFuture(xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	xtask* pTask = NULL;
	__xnet_task_co_ctx* pCtx = NULL;
	xcoro pCo = NULL;
	xcosched* pResolvedSched = pSched;

	if ( pfnTask == NULL ) {
		__xnetSyncSetError("coroutine task callback is null.");
		return NULL;
	}

	if ( pResolvedSched == NULL ) {
		pResolvedSched = xrtCoSchedCurrent();
	}
	if ( pResolvedSched == NULL ) {
		__xnetSyncSetError("coroutine task requires a scheduler.");
		return NULL;
	}

	pFuture = xFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pPromise = xPromiseCreate(pFuture);
	if ( pPromise == NULL ) {
		xFutureRelease(pFuture);
		return NULL;
	}

	pTask = (xtask*)XNET_ALLOC(sizeof(xtask));
	if ( pTask == NULL ) {
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	pCtx = (__xnet_task_co_ctx*)XNET_ALLOC(sizeof(__xnet_task_co_ctx));
	if ( pCtx == NULL ) {
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	memset(pTask, 0, sizeof(*pTask));
	memset(pCtx, 0, sizeof(*pCtx));
	pTask->iState = XTASK_QUEUED;
	pTask->pFuture = xFutureAddRef(pFuture);
	pTask->pPromise = pPromise;
	pTask->pfnCoTask = pfnTask;
	pTask->pArg = pArg;
	pCtx->pTask = pTask;

	pCo = xrtCoSchedSpawn(pResolvedSched, __xnetTaskCoDispatch, pCtx, iStackSize);
	if ( pCo == NULL ) {
		XNET_FREE(pCtx);
		xFutureRelease(pTask->pFuture);
		XNET_FREE(pTask);
		xPromiseDestroy(pPromise);
		xFutureRelease(pFuture);
		return NULL;
	}

	pTask->pCo = pCo;
	return pFuture;
}
#endif
#endif

// 内部函数：取消同步 signal 流 Future
static void __xnetSyncSignalStreamFutureCancel(__xnet_stream_future_wait_ctx* pCtx)
{
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx && pCtx->pCancelDoneEvent ) {
			xrtCoEventSet(pCtx->pCancelDoneEvent);
		}
	#else
		(void)pCtx;
	#endif
}


// 内部函数：等待同步 cleanup 流 Future
static void __xnetSyncCleanupStreamFutureWait(xnetfuture* pFuture)
{
	__xnet_stream_future_wait_ctx* pCtx = NULL;

	if ( !pFuture ) return;
	pCtx = (__xnet_stream_future_wait_ctx*)pFuture->pPendingCtx;
	pFuture->pPendingCtx = NULL;
	pFuture->pfnPendingCancel = NULL;
	pFuture->pfnPendingCleanup = NULL;
	if ( !pCtx ) return;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx->pCancelDoneEvent ) {
			xrtCoEventDestroy(pCtx->pCancelDoneEvent);
			pCtx->pCancelDoneEvent = NULL;
		}
	#endif
	XNET_FREE(pCtx);
}


// 内部函数：等待同步 resolve 流 Future
static void __xnetSyncResolveStreamFutureWait(__xnet_stream_future_wait_ctx* pCtx, xnet_result iStatus)
{
	long iPrevState;

	if ( pCtx == NULL ) return;
	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
	if ( iPrevState != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
		(void)__xnetFutureResolve(pCtx->pFuture, iStatus, pCtx->pStream);
	}
	__xnetStreamReleaseAsyncHold(pCtx->pStream);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalStreamFutureCancel(pCtx);
}


// 内部函数：完成同步 cancel 流 Future wait
static void __xnetSyncCancelStreamFutureWaitFinish(__xnet_stream_future_wait_ctx* pCtx)
{
	long iPrevState;

	if ( pCtx == NULL ) return;
	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
	__xnetStreamReleaseAsyncHold(pCtx->pStream);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalStreamFutureCancel(pCtx);
}


// 内部函数：流排空等待回调
static void __xnetSyncOnStreamDrainFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}


// 内部函数：流关闭等待回调
static void __xnetSyncOnStreamCloseFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}


// 内部函数：流可读等待回调
static void __xnetSyncOnStreamReadableFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}


// 内部函数：流可写等待回调
static void __xnetSyncOnStreamWritableFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}


// 内部函数：等待同步 cancel 流 Future
static void __xnetSyncCancelStreamFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_stream_future_wait_ctx* pCtx = (__xnet_stream_future_wait_ctx*)pArg;

	(void)pWorker;
	if ( pCtx == NULL || pCtx->pStream == NULL ) {
		__xnetSyncCancelStreamFutureWaitFinish(pCtx);
		return;
	}
	(void)__xnetStreamCancelSyncWait(pCtx->pStream, pCtx->iWaitKind, pCtx);
	__xnetSyncCancelStreamFutureWaitFinish(pCtx);
}


// 内部函数：等待同步 register 流 Future
static void __xnetSyncRegisterStreamFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_stream_future_wait_ctx* pCtx = (__xnet_stream_future_wait_ctx*)pArg;
	const __xnet_stream_wait_ops* pOps = NULL;
	bool bRegistered = false;
	long iState;

	(void)pWorker;

	if ( pCtx == NULL || pCtx->pFuture == NULL || pCtx->pStream == NULL ) {
		__xnetSyncResolveStreamFutureWait(pCtx, XRT_NET_ERROR);
		return;
	}
	pOps = __xnetSyncGetStreamWaitOps(pCtx->iWaitKind);
	if ( pOps == NULL ) {
		__xnetSyncSetError("invalid stream wait kind.");
		__xnetSyncResolveStreamFutureWait(pCtx, XRT_NET_ERROR);
		return;
	}

	for ( ;; ) {
		iState = __xnetAtomicLoad32(&pCtx->iState);
		if ( iState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
		if ( iState == __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
			__xnetSyncCancelStreamFutureWaitFinish(pCtx);
			return;
		}
		if ( iState == __XNET_SYNC_STREAM_WAIT_REGISTERED ) break;
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_REGISTERED, __XNET_SYNC_STREAM_WAIT_POSTED) == __XNET_SYNC_STREAM_WAIT_POSTED ) {
			break;
		}
	}
	bRegistered = __xnetStreamRegisterSyncWait(pCtx->pStream, pCtx->iWaitKind, pOps->pfnOnReady, pCtx);

	if ( !bRegistered ) {
		__xnetSyncSetError(pOps->sRegisterError);
		__xnetSyncResolveStreamFutureWait(pCtx, XRT_NET_ERROR);
	}
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：取消同步 ensure 流 wait 事件
static xcoevent __xnetSyncEnsureStreamWaitCancelEvent(__xnet_stream_future_wait_ctx* pCtx)
{
	xcoevent pEvent = NULL;

	if ( !pCtx ) return NULL;
	if ( pCtx->pCancelDoneEvent ) return pCtx->pCancelDoneEvent;

	pEvent = xrtCoEventCreate(TRUE, FALSE);
	if ( !pEvent ) return NULL;
	pCtx->pCancelDoneEvent = pEvent;
	return pEvent;
}
#endif

// 内部函数：等待同步 cancel pending 流 Future
static bool __xnetSyncCancelPendingStreamFutureWait(xnetfuture* pFuture)
{
	__xnet_stream_future_wait_ctx* pCtx = NULL;
	xnet_result iPostResult;
	bool bUseCoroWait = false;
	uint64 iDeadlineMs = 0;

	if ( pFuture == NULL ) return false;
	pCtx = (__xnet_stream_future_wait_ctx*)pFuture->pPendingCtx;
	if ( pCtx == NULL ) return true;
	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;

	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( xrtCoGetCurrent() != NULL ) {
			if ( __xnetSyncEnsureStreamWaitCancelEvent(pCtx) == NULL ) {
				__xnetSyncSetError("unable to allocate stream waiter cancellation event.");
				return false;
			}
			bUseCoroWait = true;
		}
	#endif

	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED &&
			__xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		long iObservedState = __xnetAtomicLoad32(&pCtx->iState);
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED, iObservedState) == iObservedState ) {
			break;
		}
	}

	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;

	iPostResult = xrtNetEnginePost(pCtx->pStream->pEngine, pCtx->pStream->pWorker->iId, __xnetSyncCancelStreamFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetSyncSetError("unable to post stream waiter cancellation.");
		return false;
	}

	if ( bUseCoroWait ) {
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( !xrtCoWaitEvent(pCtx->pCancelDoneEvent) ) {
			__xnetSyncSetError("stream waiter cancellation did not complete cleanly.");
			return false;
		}
		#endif
		return true;
	}

	iDeadlineMs = __xnetSyncNowMs() + 5000ULL;
	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( __xnetSyncNowMs() >= iDeadlineMs ) {
			__xnetSyncSetError("stream waiter cancellation timed out.");
			return false;
		}
		__xnetSyncSleepMs(1);
	}
	return true;
}


// 内部函数：__xnetSyncGetStreamWaitOps
static const __xnet_stream_wait_ops* __xnetSyncGetStreamWaitOps(uint32 iWaitKind)
{
	static const __xnet_stream_wait_ops arrOps[__XNET_STREAM_WAIT_COUNT] = {
		{ __xnetSyncOnStreamReadableFuture, "unable to register stream readable waiter." },
		{ __xnetSyncOnStreamWritableFuture, "unable to register stream writable waiter." },
		{ __xnetSyncOnStreamDrainFuture,    "unable to register stream drain waiter." },
		{ __xnetSyncOnStreamCloseFuture,    "unable to register stream close waiter." }
	};

	if ( iWaitKind >= __XNET_STREAM_WAIT_COUNT ) return NULL;
	return &arrOps[iWaitKind];
}


// 内部函数：__xnetSyncListenerWaitCanAccept
static bool __xnetSyncListenerWaitCanAccept(ptr pCtx)
{
	__xnet_listener_future_wait_ctx* pWaitCtx = (__xnet_listener_future_wait_ctx*)pCtx;
	if ( pWaitCtx == NULL ) return false;
	return __xnetAtomicLoad32(&pWaitCtx->iState) != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED &&
		__xnetAtomicLoad32(&pWaitCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED;
}


// 内部函数：取消同步 signal 监听器 Future
static void __xnetSyncSignalListenerFutureCancel(__xnet_listener_future_wait_ctx* pCtx)
{
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx && pCtx->pCancelDoneEvent ) {
			xrtCoEventSet(pCtx->pCancelDoneEvent);
		}
	#else
		(void)pCtx;
	#endif
}


// 内部函数：等待同步 cleanup 监听器 Future
static void __xnetSyncCleanupListenerFutureWait(xnetfuture* pFuture)
{
	__xnet_listener_future_wait_ctx* pCtx = NULL;

	if ( !pFuture ) return;
	pCtx = (__xnet_listener_future_wait_ctx*)pFuture->pPendingCtx;
	pFuture->pPendingCtx = NULL;
	pFuture->pfnPendingCancel = NULL;
	pFuture->pfnPendingCleanup = NULL;
	if ( !pCtx ) return;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx->pCancelDoneEvent ) {
			xrtCoEventDestroy(pCtx->pCancelDoneEvent);
			pCtx->pCancelDoneEvent = NULL;
		}
	#endif
	XNET_FREE(pCtx);
}


// 内部函数：等待同步 resolve 监听器 Future
static void __xnetSyncResolveListenerFutureWait(__xnet_listener_future_wait_ctx* pCtx, xnet_result iStatus, xnetstream* pStream)
{
	long iPrevState;

	if ( pCtx == NULL ) {
		if ( pStream ) {
			__xnetStreamAbandonUnownedAccepted(pStream);
		}
		return;
	}

	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( pStream ) {
			__xnetStreamAbandonUnownedAccepted(pStream);
		}
		return;
	}

	if ( iPrevState != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
		(void)__xnetFutureResolve(pCtx->pFuture, iStatus, pStream);
	} else if ( pStream ) {
		__xnetStreamAbandonUnownedAccepted(pStream);
	}

	__xnetListenerReleaseAsyncHold(pCtx->pListener);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalListenerFutureCancel(pCtx);
}


// 内部函数：完成同步 cancel 监听器 Future wait
static void __xnetSyncCancelListenerFutureWaitFinish(__xnet_listener_future_wait_ctx* pCtx)
{
	long iPrevState;

	if ( pCtx == NULL ) return;
	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
	__xnetListenerReleaseAsyncHold(pCtx->pListener);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalListenerFutureCancel(pCtx);
}


// 内部函数：监听器接受等待回调
static void __xnetSyncOnListenerAcceptFuture(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	__xnet_listener_future_wait_ctx* pWaitCtx = (__xnet_listener_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pListener = pListener;
	}
	__xnetSyncResolveListenerFutureWait(pWaitCtx, iStatus, pStream);
}


// 内部函数：等待同步 cancel 监听器 Future
static void __xnetSyncCancelListenerFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_listener_future_wait_ctx* pCtx = (__xnet_listener_future_wait_ctx*)pArg;

	(void)pWorker;
	if ( pCtx == NULL || pCtx->pListener == NULL ) {
		__xnetSyncCancelListenerFutureWaitFinish(pCtx);
		return;
	}
	(void)__xnetListenerCancelSyncAcceptWait(pCtx->pListener, pCtx);
	__xnetSyncCancelListenerFutureWaitFinish(pCtx);
}


// 内部函数：等待同步 register 监听器 Future
static void __xnetSyncRegisterListenerFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_listener_future_wait_ctx* pCtx = (__xnet_listener_future_wait_ctx*)pArg;
	bool bRegistered = false;
	long iState;

	(void)pWorker;
	if ( pCtx == NULL || pCtx->pFuture == NULL || pCtx->pListener == NULL ) {
		__xnetSyncResolveListenerFutureWait(pCtx, XRT_NET_ERROR, NULL);
		return;
	}

	for ( ;; ) {
		iState = __xnetAtomicLoad32(&pCtx->iState);
		if ( iState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
		if ( iState == __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
			__xnetSyncCancelListenerFutureWaitFinish(pCtx);
			return;
		}
		if ( iState == __XNET_SYNC_STREAM_WAIT_REGISTERED ) break;
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_REGISTERED, __XNET_SYNC_STREAM_WAIT_POSTED) == __XNET_SYNC_STREAM_WAIT_POSTED ) {
			break;
		}
	}

	bRegistered = __xnetListenerRegisterSyncAcceptWait(
		pCtx->pListener,
		__xnetSyncOnListenerAcceptFuture,
		__xnetSyncListenerWaitCanAccept,
		pCtx);

	if ( !bRegistered ) {
		__xnetSyncSetError("unable to register listener accept waiter.");
		__xnetSyncResolveListenerFutureWait(pCtx, XRT_NET_ERROR, NULL);
	}
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：取消同步 ensure 监听器 wait 事件
static xcoevent __xnetSyncEnsureListenerWaitCancelEvent(__xnet_listener_future_wait_ctx* pCtx)
{
	xcoevent pEvent = NULL;

	if ( !pCtx ) return NULL;
	if ( pCtx->pCancelDoneEvent ) return pCtx->pCancelDoneEvent;

	pEvent = xrtCoEventCreate(TRUE, FALSE);
	if ( !pEvent ) return NULL;
	pCtx->pCancelDoneEvent = pEvent;
	return pEvent;
}
#endif

// 内部函数：等待同步 cancel pending 监听器 Future
static bool __xnetSyncCancelPendingListenerFutureWait(xnetfuture* pFuture)
{
	__xnet_listener_future_wait_ctx* pCtx = NULL;
	xnet_result iPostResult;
	bool bUseCoroWait = false;
	uint64 iDeadlineMs = 0;

	if ( pFuture == NULL ) return false;
	pCtx = (__xnet_listener_future_wait_ctx*)pFuture->pPendingCtx;
	if ( pCtx == NULL ) return true;
	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;

	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( xrtCoGetCurrent() != NULL ) {
			if ( __xnetSyncEnsureListenerWaitCancelEvent(pCtx) == NULL ) {
				__xnetSyncSetError("unable to allocate listener waiter cancellation event.");
				return false;
			}
			bUseCoroWait = true;
		}
	#endif

	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED &&
			__xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		long iObservedState = __xnetAtomicLoad32(&pCtx->iState);
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED, iObservedState) == iObservedState ) {
			break;
		}
	}

	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;
	iPostResult = xrtNetEnginePost(pCtx->pListener->pEngine, pCtx->pListener->pWorker->iId, __xnetSyncCancelListenerFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetSyncSetError("unable to post listener waiter cancellation.");
		return false;
	}

	if ( bUseCoroWait ) {
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( !xrtCoWaitEvent(pCtx->pCancelDoneEvent) ) {
			__xnetSyncSetError("listener waiter cancellation did not complete cleanly.");
			return false;
		}
		#endif
		return true;
	}

	iDeadlineMs = __xnetSyncNowMs() + 5000ULL;
	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( __xnetSyncNowMs() >= iDeadlineMs ) {
			__xnetSyncSetError("listener waiter cancellation timed out.");
			return false;
		}
		__xnetSyncSleepMs(1);
	}
	return true;
}


// 内部函数：等待同步 cleanup 数据报 Future
static void __xnetSyncCleanupDgramFutureWait(xnetfuture* pFuture)
{
	__xnet_dgram_future_wait_ctx* pCtx = NULL;

	if ( !pFuture ) return;
	pCtx = (__xnet_dgram_future_wait_ctx*)pFuture->pPendingCtx;
	pFuture->pPendingCtx = NULL;
	pFuture->pfnPendingCancel = NULL;
	pFuture->pfnPendingCleanup = NULL;
	if ( !pCtx ) return;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx->pCancelDoneEvent ) {
			xrtCoEventDestroy(pCtx->pCancelDoneEvent);
			pCtx->pCancelDoneEvent = NULL;
		}
	#endif
	XNET_FREE(pCtx);
}


// 内部函数：取消同步 signal 数据报 Future
static void __xnetSyncSignalDgramFutureCancel(__xnet_dgram_future_wait_ctx* pCtx)
{
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pCtx && pCtx->pCancelDoneEvent ) {
			xrtCoEventSet(pCtx->pCancelDoneEvent);
		}
	#else
		(void)pCtx;
	#endif
}


// 内部函数：等待同步 resolve 数据报 Future
static void __xnetSyncResolveDgramFutureWait(__xnet_dgram_future_wait_ctx* pCtx, xnet_result iStatus, xnetdgrampkt* pPacket)
{
	long iPrevState;

	if ( pCtx == NULL ) {
		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		return;
	}

	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( pPacket ) xrtNetDgramPacketDestroy(pPacket);
		return;
	}

	if ( iPrevState != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
		(void)__xnetFutureResolve(pCtx->pFuture, iStatus, pPacket);
	} else if ( pPacket ) {
		xrtNetDgramPacketDestroy(pPacket);
	}

	__xnetDgramReleaseAsyncHold(pCtx->pSock);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalDgramFutureCancel(pCtx);
}


// 内部函数：完成同步 cancel 数据报 Future wait
static void __xnetSyncCancelDgramFutureWaitFinish(__xnet_dgram_future_wait_ctx* pCtx)
{
	long iPrevState;

	if ( pCtx == NULL ) return;
	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
	__xnetDgramReleaseAsyncHold(pCtx->pSock);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalDgramFutureCancel(pCtx);
}


// 内部函数：数据报接收等待回调
static void __xnetSyncOnDgramRecvFuture(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket, ptr pCtx)
{
	__xnet_dgram_future_wait_ctx* pWaitCtx = (__xnet_dgram_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pSock = pSock;
	}
	__xnetSyncResolveDgramFutureWait(pWaitCtx, iStatus, pPacket);
}


// 内部函数：等待同步 cancel 数据报 Future
static void __xnetSyncCancelDgramFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_dgram_future_wait_ctx* pCtx = (__xnet_dgram_future_wait_ctx*)pArg;

	(void)pWorker;
	if ( pCtx == NULL || pCtx->pSock == NULL ) {
		__xnetSyncCancelDgramFutureWaitFinish(pCtx);
		return;
	}
	(void)__xnetDgramCancelSyncRecvWait(pCtx->pSock, pCtx);
	__xnetSyncCancelDgramFutureWaitFinish(pCtx);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：取消同步 ensure 数据报 wait 事件
static xcoevent __xnetSyncEnsureDgramWaitCancelEvent(__xnet_dgram_future_wait_ctx* pCtx)
{
	xcoevent pEvent = NULL;

	if ( !pCtx ) return NULL;
	if ( pCtx->pCancelDoneEvent ) return pCtx->pCancelDoneEvent;

	pEvent = xrtCoEventCreate(TRUE, FALSE);
	if ( !pEvent ) return NULL;
	pCtx->pCancelDoneEvent = pEvent;
	return pEvent;
}
#endif

// 内部函数：等待同步 cancel pending 数据报 Future
static bool __xnetSyncCancelPendingDgramFutureWait(xnetfuture* pFuture)
{
	__xnet_dgram_future_wait_ctx* pCtx = NULL;
	xnet_result iPostResult;
	bool bUseCoroWait = false;
	uint64 iDeadlineMs = 0;

	if ( pFuture == NULL ) return false;
	pCtx = (__xnet_dgram_future_wait_ctx*)pFuture->pPendingCtx;
	if ( pCtx == NULL ) return true;
	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;

	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( xrtCoGetCurrent() != NULL ) {
			if ( __xnetSyncEnsureDgramWaitCancelEvent(pCtx) == NULL ) {
				__xnetSyncSetError("unable to allocate datagram waiter cancellation event.");
				return false;
			}
			bUseCoroWait = true;
		}
	#endif

	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED &&
			__xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		long iObservedState = __xnetAtomicLoad32(&pCtx->iState);
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED, iObservedState) == iObservedState ) {
			break;
		}
	}

	if ( __xnetAtomicLoad32(&pCtx->iState) == __XNET_SYNC_STREAM_WAIT_FINISHED ) return true;

	iPostResult = xrtNetEnginePost(pCtx->pSock->pEngine, pCtx->pSock->pWorker->iId, __xnetSyncCancelDgramFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetSyncSetError("unable to post datagram waiter cancellation.");
		return false;
	}

	if ( bUseCoroWait ) {
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( !xrtCoWaitEvent(pCtx->pCancelDoneEvent) ) {
			__xnetSyncSetError("datagram waiter cancellation did not complete cleanly.");
			return false;
		}
		#endif
		return true;
	}

	iDeadlineMs = __xnetSyncNowMs() + 5000ULL;
	while ( __xnetAtomicLoad32(&pCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( __xnetSyncNowMs() >= iDeadlineMs ) {
			__xnetSyncSetError("datagram waiter cancellation timed out.");
			return false;
		}
		__xnetSyncSleepMs(1);
	}
	return true;
}


// 内部函数：等待同步 register 数据报 Future
static void __xnetSyncRegisterDgramFutureWait(xnetworker* pWorker, ptr pArg)
{
	__xnet_dgram_future_wait_ctx* pCtx = (__xnet_dgram_future_wait_ctx*)pArg;
	bool bRegistered = false;
	long iState;

	(void)pWorker;

	if ( pCtx == NULL || pCtx->pFuture == NULL || pCtx->pSock == NULL ) {
		__xnetSyncResolveDgramFutureWait(pCtx, XRT_NET_ERROR, NULL);
		return;
	}

	for ( ;; ) {
		iState = __xnetAtomicLoad32(&pCtx->iState);
		if ( iState == __XNET_SYNC_STREAM_WAIT_FINISHED ) return;
		if ( iState == __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
			__xnetSyncCancelDgramFutureWaitFinish(pCtx);
			return;
		}
		if ( iState == __XNET_SYNC_STREAM_WAIT_REGISTERED ) break;
		if ( __xnetAtomicCompareExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_REGISTERED, __XNET_SYNC_STREAM_WAIT_POSTED) == __XNET_SYNC_STREAM_WAIT_POSTED ) {
			break;
		}
	}

	bRegistered = __xnetDgramRegisterSyncRecvWait(pCtx->pSock, __xnetSyncOnDgramRecvFuture, pCtx);
	if ( !bRegistered ) {
		__xnetSyncSetError("unable to register datagram recv waiter.");
		__xnetSyncResolveDgramFutureWait(pCtx, XRT_NET_ERROR, NULL);
	}
}


// 内部函数：创建同步 posted Future
static xnetfuture* __xnetSyncCreatePostedFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_future_task_fn pfnTask, ptr pArg, bool bDelayed)
{
	xnetengine* pResolvedEngine = NULL;
	xnetfuture* pFuture = NULL;
	__xnet_future_task_ctx* pCtx = NULL;
	xnet_result iPostResult;

	if ( pfnTask == NULL ) {
		__xnetSyncSetError("future task callback is null.");
		return NULL;
	}

	pResolvedEngine = __xnetSyncResolveEngine(pEngine);
	if ( pResolvedEngine == NULL ) {
		__xnetSyncSetError("unable to resolve sync engine.");
		return NULL;
	}

	pFuture = xrtNetFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pCtx = (__xnet_future_task_ctx*)XNET_ALLOC(sizeof(__xnet_future_task_ctx));
	if ( pCtx == NULL ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	memset(pCtx, 0, sizeof(__xnet_future_task_ctx));
	pCtx->pFuture = pFuture;
	pCtx->pfnTask = pfnTask;
	pCtx->pArg = pArg;
	__xnetFutureAddAsyncHold(pFuture);

	if ( bDelayed ) {
		iPostResult = xrtNetEnginePostDelayed(pResolvedEngine, iAffinityKey, iDelayMs, __xnetFutureTaskDispatch, pCtx);
	}
	else {
		iPostResult = xrtNetEnginePost(pResolvedEngine, iAffinityKey, __xnetFutureTaskDispatch, pCtx);
	}

	if ( iPostResult != XRT_NET_OK ) {
		__xnetFutureReleaseAsyncHold(pFuture);
		XNET_FREE(pCtx);
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	return pFuture;
}


// 网络引擎 post Future相关处理
XXAPI xnetfuture* xrtNetEnginePostFuture(xnetengine* pEngine, uint32 iAffinityKey, xnet_future_task_fn pfnTask, ptr pArg)
{
	return __xnetSyncCreatePostedFuture(pEngine, iAffinityKey, 0, pfnTask, pArg, false);
}


// 网络引擎 post 延迟 Future相关处理
XXAPI xnetfuture* xrtNetEnginePostDelayedFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_future_task_fn pfnTask, ptr pArg)
{
	return __xnetSyncCreatePostedFuture(pEngine, iAffinityKey, iDelayMs, pfnTask, pArg, true);
}

#if defined(XXRTL_CORE)
// xTaskRunEngine 相关处理
XXAPI xfuture* xTaskRunEngine(xnetengine* pEngine, uint32 iAffinityKey, xtask_engine_fn pfnTask, ptr pArg)
{
	return __xnetCreateEngineTaskFuture(pEngine, iAffinityKey, 0, pfnTask, pArg, false);
}


// xTaskRunDelayed 相关处理
XXAPI xfuture* xTaskRunDelayed(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xtask_engine_fn pfnTask, ptr pArg)
{
	return __xnetCreateEngineTaskFuture(pEngine, iAffinityKey, iDelayMs, pfnTask, pArg, true);
}


// xTaskRunThread 相关处理
XXAPI xfuture* xTaskRunThread(xtask_thread_fn pfnTask, ptr pArg, size_t iStackSize)
{
	return __xnetCreateThreadTaskFuture(pfnTask, pArg, iStackSize);
}

#if !defined(XRT_NO_COROUTINE)
// xTaskRunCo 相关处理
XXAPI xfuture* xTaskRunCo(xcosched* pSched, xtask_co_fn pfnTask, ptr pArg, size_t iStackSize)
{
	return __xnetCreateCoTaskFuture(pSched, pfnTask, pArg, iStackSize);
}
#endif
#endif

// 内部函数：等待同步 create 流 Future
static xnetfuture* __xnetSyncCreateStreamFutureWait(xnetstream* pStream, uint32 iWaitKind)
{
	xnetfuture* pFuture = NULL;
	__xnet_stream_future_wait_ctx* pCtx = NULL;
	const __xnet_stream_wait_ops* pOps = NULL;
	xnet_result iPostResult;

	if ( pStream == NULL || pStream->pEngine == NULL || pStream->pWorker == NULL ) {
		__xnetSyncSetError("stream is not bound to a running engine worker.");
		return NULL;
	}
	pOps = __xnetSyncGetStreamWaitOps(iWaitKind);
	if ( pOps == NULL ) {
		__xnetSyncSetError("invalid stream wait kind.");
		return NULL;
	}
	if ( iWaitKind == __XNET_STREAM_WAIT_READABLE && !pStream->bReadPaused ) {
		__xnetSyncSetError("stream readable future currently requires paused read mode.");
		return NULL;
	}

	pFuture = xrtNetFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pCtx = (__xnet_stream_future_wait_ctx*)XNET_ALLOC(sizeof(__xnet_stream_future_wait_ctx));
	if ( pCtx == NULL ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	memset(pCtx, 0, sizeof(__xnet_stream_future_wait_ctx));
	pCtx->pFuture = pFuture;
	pCtx->pStream = pStream;
	pCtx->iWaitKind = iWaitKind;
	pCtx->iState = __XNET_SYNC_STREAM_WAIT_POSTED;
	pFuture->pPendingCtx = pCtx;
	pFuture->pfnPendingCancel = __xnetSyncCancelPendingStreamFutureWait;
	pFuture->pfnPendingCleanup = __xnetSyncCleanupStreamFutureWait;
	__xnetFutureAddAsyncHold(pFuture);
	__xnetStreamAddAsyncHold(pStream);

	iPostResult = xrtNetEnginePost(pStream->pEngine, pStream->pWorker->iId, __xnetSyncRegisterStreamFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetStreamReleaseAsyncHold(pStream);
		__xnetFutureReleaseAsyncHold(pFuture);
		__xnetSyncCleanupStreamFutureWait(pFuture);
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	return pFuture;
}


// 网络流 drain Future相关处理
XXAPI xnetfuture* xrtNetStreamDrainFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_DRAIN);
}


// 网络流 writable Future相关处理
XXAPI xnetfuture* xrtNetStreamWritableFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_WRITABLE);
}


// 关闭网络流 Future
XXAPI xnetfuture* xrtNetStreamCloseFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_CLOSE);
}


// 网络流 readable Future相关处理
XXAPI xnetfuture* xrtNetStreamReadableFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_READABLE);
}


// 网络流 Future 扩展相关处理
XXAPI xnetfuture* xrtNetStreamFutureEx(xnetstream* pStream, uint32 iWaitKind)
{
	return __xnetSyncCreateStreamFutureWait(pStream, iWaitKind);
}


// 内部函数：接受同步 create 监听器 Future
static xnetfuture* __xnetSyncCreateListenerFutureAccept(xnetlistener* pListener)
{
	xnetfuture* pFuture = NULL;
	__xnet_listener_future_wait_ctx* pCtx = NULL;
	xnet_result iPostResult;

	if ( pListener == NULL || pListener->pEngine == NULL || pListener->pWorker == NULL ) {
		__xnetSyncSetError("listener is not bound to a running engine worker.");
		return NULL;
	}

	pFuture = xrtNetFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pCtx = (__xnet_listener_future_wait_ctx*)XNET_ALLOC(sizeof(__xnet_listener_future_wait_ctx));
	if ( pCtx == NULL ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	memset(pCtx, 0, sizeof(__xnet_listener_future_wait_ctx));
	pCtx->pFuture = pFuture;
	pCtx->pListener = pListener;
	pCtx->iState = __XNET_SYNC_STREAM_WAIT_POSTED;
	pFuture->pPendingCtx = pCtx;
	pFuture->pfnPendingCancel = __xnetSyncCancelPendingListenerFutureWait;
	pFuture->pfnPendingCleanup = __xnetSyncCleanupListenerFutureWait;
	__xnetFutureAddAsyncHold(pFuture);
	__xnetListenerAddAsyncHold(pListener);

	iPostResult = xrtNetEnginePost(pListener->pEngine, pListener->pWorker->iId, __xnetSyncRegisterListenerFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetListenerReleaseAsyncHold(pListener);
		__xnetFutureReleaseAsyncHold(pFuture);
		__xnetSyncCleanupListenerFutureWait(pFuture);
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	return pFuture;
}


// 接受网络监听器 Future
XXAPI xnetfuture* xrtNetListenerAcceptFuture(xnetlistener* pListener)
{
	return __xnetSyncCreateListenerFutureAccept(pListener);
}


// 内部函数：接收同步 create 数据报 Future
static xnetfuture* __xnetSyncCreateDgramFutureRecv(xdgramsock* pSock)
{
	xnetfuture* pFuture = NULL;
	__xnet_dgram_future_wait_ctx* pCtx = NULL;
	xnet_result iPostResult;

	if ( pSock == NULL || pSock->pEngine == NULL || pSock->pWorker == NULL ) {
		__xnetSyncSetError("datagram socket is not bound to a running engine worker.");
		return NULL;
	}
	if ( pSock->pEvents && pSock->pEvents->OnRecv ) {
		__xnetSyncSetError("datagram recv future cannot be used while OnRecv callback is installed.");
		return NULL;
	}
	if ( !pSock->bRunning || !__xnetDgramSocketIsValid(pSock->hSocket) ) {
		__xnetSyncSetError("datagram socket is not running.");
		return NULL;
	}

	pFuture = xrtNetFutureCreate();
	if ( pFuture == NULL ) {
		return NULL;
	}

	pCtx = (__xnet_dgram_future_wait_ctx*)XNET_ALLOC(sizeof(__xnet_dgram_future_wait_ctx));
	if ( pCtx == NULL ) {
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	memset(pCtx, 0, sizeof(__xnet_dgram_future_wait_ctx));
	pCtx->pFuture = pFuture;
	pCtx->pSock = pSock;
	pCtx->iState = __XNET_SYNC_STREAM_WAIT_POSTED;
	pFuture->pPendingCtx = pCtx;
	pFuture->pfnPendingCancel = __xnetSyncCancelPendingDgramFutureWait;
	pFuture->pfnPendingCleanup = __xnetSyncCleanupDgramFutureWait;
	__xnetFutureAddAsyncHold(pFuture);
	__xnetDgramAddAsyncHold(pSock);

	iPostResult = xrtNetEnginePost(pSock->pEngine, pSock->pWorker->iId, __xnetSyncRegisterDgramFutureWait, pCtx);
	if ( iPostResult != XRT_NET_OK ) {
		__xnetDgramReleaseAsyncHold(pSock);
		__xnetFutureReleaseAsyncHold(pFuture);
		__xnetSyncCleanupDgramFutureWait(pFuture);
		xrtNetFutureDestroy(pFuture);
		return NULL;
	}

	return pFuture;
}


// 接收网络数据报 Future
XXAPI xnetfuture* xrtNetDgramRecvFuture(xdgramsock* pSock)
{
	return __xnetSyncCreateDgramFutureRecv(pSock);
}


// 内部函数：等待同步监听器同步 core 扩展
static xnet_result __xnetSyncWaitListenerSyncCoreEx(xnetlistener* pListener, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	pFuture = __xnetSyncCreateListenerFutureAccept(pListener);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWait(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingListenerFutureWait(pFuture) ) {
			(void)__xnetFutureDestroyCore(pFuture);
			__xnetSyncSetError("listener accept wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	if ( !__xnetFutureDestroyCore(pFuture) ) {
		__xnetSyncSetError("listener accept wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步数据报同步 core 扩展
static xnet_result __xnetSyncWaitDgramSyncCoreEx(xdgramsock* pSock, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	pFuture = __xnetSyncCreateDgramFutureRecv(pSock);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWait(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingDgramFutureWait(pFuture) ) {
			(void)__xnetFutureDestroyCore(pFuture);
			__xnetSyncSetError("datagram wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	if ( !__xnetFutureDestroyCore(pFuture) ) {
		__xnetSyncSetError("datagram wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步数据报同步 core
static xnet_result UNUSED_ATTR __xnetSyncWaitDgramSyncCore(xdgramsock* pSock, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitDgramSyncCoreEx(pSock, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 内部函数：等待同步流同步 core 扩展
static xnet_result __xnetSyncWaitStreamSyncCoreEx(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	pFuture = __xnetSyncCreateStreamFutureWait(pStream, iWaitKind);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWait(pFuture, XNET_WAIT_INFINITE);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWait(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingStreamFutureWait(pFuture) ) {
			(void)__xnetFutureDestroyCore(pFuture);
			__xnetSyncSetError("stream wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	if ( !__xnetFutureDestroyCore(pFuture) ) {
		__xnetSyncSetError("stream wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步流同步 core
static xnet_result UNUSED_ATTR __xnetSyncWaitStreamSyncCore(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitStreamSyncCoreEx(pStream, iWaitKind, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 内部函数：等待同步源同步 core 扩展
static xnet_result __xnetSyncWaitSourceSyncCoreEx(const xnetwaitsrc* pSrc, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	if ( ppValue ) *ppValue = NULL;
	if ( !pSrc ) return XRT_NET_ERROR;
	if ( pSrc->iKind == XNET_WAITSRC_FUTURE ) {
		xnet_result iStatus = XRT_NET_ERROR;
		if ( iWaitMode == 0 ) {
			iStatus = xrtNetFutureWait(pSrc->u.pFuture, XNET_WAIT_INFINITE);
		}
		else if ( iWaitMode == 1 ) {
			iStatus = xrtNetFutureWait(pSrc->u.pFuture, iTimeoutMs);
		}
		else {
			iStatus = xrtNetFutureWaitUntil(pSrc->u.pFuture, iDeadlineMs);
		}
		if ( ppValue ) {
			*ppValue = xrtNetFutureValue(pSrc->u.pFuture);
		}
		return iStatus;
	}
	if ( pSrc->iKind == XNET_WAITSRC_STREAM ) {
		return __xnetSyncWaitStreamSyncCoreEx(
			pSrc->u.tStream.pStream,
			pSrc->u.tStream.iWaitKind,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	if ( pSrc->iKind == XNET_WAITSRC_DGRAM ) {
		return __xnetSyncWaitDgramSyncCoreEx(
			pSrc->u.pDgram,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	if ( pSrc->iKind == XNET_WAITSRC_LISTENER ) {
		return __xnetSyncWaitListenerSyncCoreEx(
			pSrc->u.pListener,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	__xnetSyncSetError("invalid wait source kind.");
	return XRT_NET_ERROR;
}


// 内部函数：等待同步源同步 core
static xnet_result __xnetSyncWaitSourceSyncCore(const xnetwaitsrc* pSrc, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 等待网络流扩展
XXAPI xnet_result xrtNetStreamWaitEx(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 0, 0, 0);
}


// 等待网络流超时扩展
XXAPI xnet_result xrtNetStreamWaitTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 1, 0, iTimeoutMs);
}


// 等待网络流直到指定时刻扩展
XXAPI xnet_result xrtNetStreamWaitUntilEx(xnetstream* pStream, uint32 iWaitKind, int64_t iDeadlineMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 2, iDeadlineMs, 0);
}


// 创建 wait等待源
XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 0, 0, 0);
}


// 创建 wait 超时等待源
XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 1, 0, iTimeoutMs);
}


// 创建 wait 直到指定时刻等待源
XXAPI xnet_result xrtNetWaitSourceWaitUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 2, iDeadlineMs, 0);
}


// 创建 wait 值等待源
XXAPI xnet_result xrtNetWaitSourceWaitValue(const xnetwaitsrc* pSrc, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 0, 0, 0, ppValue);
}


// 创建 wait 值超时等待源
XXAPI xnet_result xrtNetWaitSourceWaitValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 1, 0, iTimeoutMs, ppValue);
}


// 创建 wait 值直到指定时刻等待源
XXAPI xnet_result xrtNetWaitSourceWaitValueUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 2, iDeadlineMs, 0, ppValue);
}

#if defined(XXRTL_CORE)
// 等待 x 源空
XXAPI xwaitsrc xWaitSourceNone(void)
{
	return xrtNetWaitSourceNone();
}


// xWaitSourceFromFuture 相关处理
XXAPI xwaitsrc xWaitSourceFromFuture(xfuture* pFuture)
{
	return xrtNetWaitSourceFuture(pFuture);
}


// xWaitSourceFromStream 相关处理
XXAPI xwaitsrc xWaitSourceFromStream(xnetstream* pStream, uint32 iWaitKind)
{
	return xrtNetWaitSourceStream(pStream, iWaitKind);
}


// xWaitSourceFromDgramRecv 相关处理
XXAPI xwaitsrc xWaitSourceFromDgramRecv(xdgramsock* pSock)
{
	return xrtNetWaitSourceDgramRecv(pSock);
}


// xWaitSourceFromListenerAccept 相关处理
XXAPI xwaitsrc xWaitSourceFromListenerAccept(xnetlistener* pListener)
{
	return xrtNetWaitSourceListenerAccept(pListener);
}


// xWaitSourceWait 相关处理
XXAPI bool xWaitSourceWait(const xwaitsrc* pSrc)
{
	return xrtNetWaitSourceWait(pSrc) == XRT_NET_OK;
}


// xWaitSourceWaitTimeout 相关处理
XXAPI bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs)
{
	if ( iTimeoutMs < 0 ) return xWaitSourceWait(pSrc);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetWaitSourceWaitTimeout(pSrc, (uint32)iTimeoutMs) == XRT_NET_OK;
}


// xWaitSourceWaitUntil 相关处理
XXAPI bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs)
{
	return xrtNetWaitSourceWaitUntil(pSrc, iDeadlineMs) == XRT_NET_OK;
}


// xWaitSourceWaitValue 相关处理
XXAPI ptr xWaitSourceWaitValue(const xwaitsrc* pSrc)
{
	ptr pValue = NULL;
	return xrtNetWaitSourceWaitValue(pSrc, &pValue) == XRT_NET_OK ? pValue : NULL;
}


// 等待 x wait 源值超时
XXAPI ptr xWaitSourceWaitValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs)
{
	ptr pValue = NULL;
	if ( iTimeoutMs < 0 ) return xWaitSourceWaitValue(pSrc);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetWaitSourceWaitValueTimeout(pSrc, (uint32)iTimeoutMs, &pValue) == XRT_NET_OK ? pValue : NULL;
}


// 等待 x wait 源值直到指定时刻
XXAPI ptr xWaitSourceWaitValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs)
{
	ptr pValue = NULL;
	return xrtNetWaitSourceWaitValueUntil(pSrc, iDeadlineMs, &pValue) == XRT_NET_OK ? pValue : NULL;
}
#endif

// 接受网络监听器
XXAPI xnet_result xrtNetListenerAccept(xnetlistener* pListener, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 0, 0, 0, (ptr*)ppStream);
}


// 接受网络监听器超时
XXAPI xnet_result xrtNetListenerAcceptTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppStream);
}


// 接受网络监听器直到指定时刻
XXAPI xnet_result xrtNetListenerAcceptUntil(xnetlistener* pListener, int64_t iDeadlineMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppStream);
}


// 接收网络数据报
XXAPI xnet_result xrtNetDgramRecv(xdgramsock* pSock, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 0, 0, 0, (ptr*)ppPacket);
}


// 接收网络数据报超时
XXAPI xnet_result xrtNetDgramRecvTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppPacket);
}


// 接收网络数据报直到指定时刻
XXAPI xnet_result xrtNetDgramRecvUntil(xdgramsock* pSock, int64_t iDeadlineMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppPacket);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// 内部函数：等待同步流协程 core 扩展
static xnet_result __xnetSyncWaitStreamCoCoreEx(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	str sErr = NULL;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	if ( xrtCoGetCurrent() == NULL ) {
		__xnetSyncSetError("stream coroutine wait requires an active coroutine context.");
		return XRT_NET_ERROR;
	}

	pFuture = __xnetSyncCreateStreamFutureWait(pStream, iWaitKind);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWaitCo(pFuture);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWaitCoTimeout(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitCoUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingStreamFutureWait(pFuture) ) {
			xrtClearError();
			xrtNetFutureDestroy(pFuture);
			__xnetSyncSetError("stream coroutine wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	xrtClearError();
	xrtNetFutureDestroy(pFuture);
	sErr = xrtGetError();
	if ( sErr && sErr[0] != 0 ) {
		__xnetSyncSetError("stream coroutine wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步流协程 core
static xnet_result UNUSED_ATTR __xnetSyncWaitStreamCoCore(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitStreamCoCoreEx(pStream, iWaitKind, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 内部函数：等待同步监听器协程 core 扩展
static xnet_result __xnetSyncWaitListenerCoCoreEx(xnetlistener* pListener, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	str sErr = NULL;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	if ( xrtCoGetCurrent() == NULL ) {
		__xnetSyncSetError("listener coroutine wait requires an active coroutine context.");
		return XRT_NET_ERROR;
	}

	pFuture = __xnetSyncCreateListenerFutureAccept(pListener);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWaitCo(pFuture);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWaitCoTimeout(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitCoUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingListenerFutureWait(pFuture) ) {
			xrtClearError();
			xrtNetFutureDestroy(pFuture);
			__xnetSyncSetError("listener coroutine wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	xrtClearError();
	xrtNetFutureDestroy(pFuture);
	sErr = xrtGetError();
	if ( sErr && sErr[0] != 0 ) {
		__xnetSyncSetError("listener coroutine wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步监听器协程 core
static xnet_result UNUSED_ATTR __xnetSyncWaitListenerCoCore(xnetlistener* pListener, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitListenerCoCoreEx(pListener, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 内部函数：等待同步数据报协程 core 扩展
static xnet_result __xnetSyncWaitDgramCoCoreEx(xdgramsock* pSock, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	xnetfuture* pFuture = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	str sErr = NULL;
	bool bNeedCancel = false;

	if ( ppValue ) *ppValue = NULL;
	if ( xrtCoGetCurrent() == NULL ) {
		__xnetSyncSetError("datagram coroutine wait requires an active coroutine context.");
		return XRT_NET_ERROR;
	}

	pFuture = __xnetSyncCreateDgramFutureRecv(pSock);
	if ( pFuture == NULL ) {
		return XRT_NET_ERROR;
	}

	if ( iWaitMode == 0 ) {
		iStatus = xrtNetFutureWaitCo(pFuture);
	}
	else if ( iWaitMode == 1 ) {
		iStatus = xrtNetFutureWaitCoTimeout(pFuture, iTimeoutMs);
	}
	else {
		iStatus = xrtNetFutureWaitCoUntil(pFuture, iDeadlineMs);
	}

	if ( xrtNetFutureStatus(pFuture) == XRT_NET_AGAIN &&
		(iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_ERROR) ) {
		bNeedCancel = true;
	}

	if ( bNeedCancel ) {
		if ( !__xnetSyncCancelPendingDgramFutureWait(pFuture) ) {
			xrtClearError();
			xrtNetFutureDestroy(pFuture);
			__xnetSyncSetError("datagram coroutine wait could not cancel its pending waiter.");
			return XRT_NET_ERROR;
		}
	}

	if ( ppValue ) {
		*ppValue = xrtNetFutureValue(pFuture);
	}

	xrtClearError();
	xrtNetFutureDestroy(pFuture);
	sErr = xrtGetError();
	if ( sErr && sErr[0] != 0 ) {
		__xnetSyncSetError("datagram coroutine wait could not release its internal future.");
		return XRT_NET_ERROR;
	}
	return iStatus;
}


// 内部函数：等待同步数据报协程 core
static xnet_result UNUSED_ATTR __xnetSyncWaitDgramCoCore(xdgramsock* pSock, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitDgramCoCoreEx(pSock, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}


// 内部函数：等待同步源协程 core 扩展
static xnet_result __xnetSyncWaitSourceCoCoreEx(const xnetwaitsrc* pSrc, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs, ptr* ppValue)
{
	if ( ppValue ) *ppValue = NULL;
	if ( !pSrc ) return XRT_NET_ERROR;
	if ( pSrc->iKind == XNET_WAITSRC_FUTURE ) {
		xnet_result iStatus = XRT_NET_ERROR;
		if ( iWaitMode == 0 ) {
			iStatus = xrtNetFutureWaitCo(pSrc->u.pFuture);
		}
		else if ( iWaitMode == 1 ) {
			iStatus = xrtNetFutureWaitCoTimeout(pSrc->u.pFuture, iTimeoutMs);
		}
		else {
			iStatus = xrtNetFutureWaitCoUntil(pSrc->u.pFuture, iDeadlineMs);
		}
		if ( ppValue ) {
			*ppValue = xrtNetFutureValue(pSrc->u.pFuture);
		}
		return iStatus;
	}
	if ( pSrc->iKind == XNET_WAITSRC_STREAM ) {
		return __xnetSyncWaitStreamCoCoreEx(
			pSrc->u.tStream.pStream,
			pSrc->u.tStream.iWaitKind,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	if ( pSrc->iKind == XNET_WAITSRC_DGRAM ) {
		return __xnetSyncWaitDgramCoCoreEx(
			pSrc->u.pDgram,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	if ( pSrc->iKind == XNET_WAITSRC_LISTENER ) {
		return __xnetSyncWaitListenerCoCoreEx(
			pSrc->u.pListener,
			iWaitMode,
			iDeadlineMs,
			iTimeoutMs,
			ppValue);
	}
	__xnetSyncSetError("invalid wait source kind.");
	return XRT_NET_ERROR;
}


// 内部函数：等待同步源协程 core
static xnet_result __xnetSyncWaitSourceCoCore(const xnetwaitsrc* pSrc, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);


// 等待网络流 drain 协程
XXAPI xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_DRAIN);
}


// 等待网络流 drain 协程超时
XXAPI xnet_result xrtNetStreamWaitDrainCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_DRAIN, iTimeoutMs);
}


// 等待网络流 drain 协程直到指定时刻
XXAPI xnet_result xrtNetStreamWaitDrainCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_DRAIN, iDeadlineMs);
}


// 等待网络流 writable 协程
XXAPI xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_WRITABLE);
}


// 等待网络流 writable 协程超时
XXAPI xnet_result xrtNetStreamWaitWritableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_WRITABLE, iTimeoutMs);
}


// 等待网络流 writable 协程直到指定时刻
XXAPI xnet_result xrtNetStreamWaitWritableCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_WRITABLE, iDeadlineMs);
}


// 关闭网络流 wait 协程
XXAPI xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_CLOSE);
}


// 关闭网络流 wait 协程超时
XXAPI xnet_result xrtNetStreamWaitCloseCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_CLOSE, iTimeoutMs);
}


// 关闭网络流 wait 协程直到指定时刻
XXAPI xnet_result xrtNetStreamWaitCloseCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_CLOSE, iDeadlineMs);
}


// 等待网络流 readable 协程
XXAPI xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_READABLE);
}


// 等待网络流 readable 协程超时
XXAPI xnet_result xrtNetStreamWaitReadableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_READABLE, iTimeoutMs);
}


// 等待网络流 readable 协程直到指定时刻
XXAPI xnet_result xrtNetStreamWaitReadableCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_READABLE, iDeadlineMs);
}


// 等待网络流协程扩展
XXAPI xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 0, 0, 0);
}


// 等待网络流协程超时扩展
XXAPI xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 1, 0, iTimeoutMs);
}


// 等待网络流协程直到指定时刻扩展
XXAPI xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 2, iDeadlineMs, 0);
}


// 创建 wait 协程等待源
XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 0, 0, 0);
}


// 创建 wait 协程超时等待源
XXAPI xnet_result xrtNetWaitSourceWaitCoTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 1, 0, iTimeoutMs);
}


// 创建 wait 协程直到指定时刻等待源
XXAPI xnet_result xrtNetWaitSourceWaitCoUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 2, iDeadlineMs, 0);
}


// 创建 wait 协程值等待源
XXAPI xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 0, 0, 0, ppValue);
}


// 创建 wait 协程值超时等待源
XXAPI xnet_result xrtNetWaitSourceWaitCoValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 1, 0, iTimeoutMs, ppValue);
}


// 创建 wait 协程值直到指定时刻等待源
XXAPI xnet_result xrtNetWaitSourceWaitCoValueUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 2, iDeadlineMs, 0, ppValue);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
// xWaitSourceWaitCo 相关处理
XXAPI bool xWaitSourceWaitCo(const xwaitsrc* pSrc)
{
	return xrtNetWaitSourceWaitCo(pSrc) == XRT_NET_OK;
}


// 等待 x wait 源协程超时
XXAPI bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs)
{
	if ( iTimeoutMs < 0 ) return xWaitSourceWaitCo(pSrc);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetWaitSourceWaitCoTimeout(pSrc, (uint32)iTimeoutMs) == XRT_NET_OK;
}


// 等待 x wait 源协程直到指定时刻
XXAPI bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs)
{
	return xrtNetWaitSourceWaitCoUntil(pSrc, iDeadlineMs) == XRT_NET_OK;
}


// 等待 x wait 源协程值
XXAPI ptr xWaitSourceWaitCoValue(const xwaitsrc* pSrc)
{
	ptr pValue = NULL;
	return xrtNetWaitSourceWaitCoValue(pSrc, &pValue) == XRT_NET_OK ? pValue : NULL;
}


// 等待 x wait 源协程值超时
XXAPI ptr xWaitSourceWaitCoValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs)
{
	ptr pValue = NULL;
	if ( iTimeoutMs < 0 ) return xWaitSourceWaitCoValue(pSrc);
	if ( iTimeoutMs > (int64)XNET_WAIT_INFINITE ) iTimeoutMs = (int64)XNET_WAIT_INFINITE;
	return xrtNetWaitSourceWaitCoValueTimeout(pSrc, (uint32)iTimeoutMs, &pValue) == XRT_NET_OK ? pValue : NULL;
}


// 等待 x wait 源协程值直到指定时刻
XXAPI ptr xWaitSourceWaitCoValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs)
{
	ptr pValue = NULL;
	return xrtNetWaitSourceWaitCoValueUntil(pSrc, iDeadlineMs, &pValue) == XRT_NET_OK ? pValue : NULL;
}
#endif

// 接受网络监听器协程
XXAPI xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 0, 0, 0, (ptr*)ppStream);
}


// 接受网络监听器协程超时
XXAPI xnet_result xrtNetListenerAcceptCoTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppStream);
}


// 接受网络监听器协程直到指定时刻
XXAPI xnet_result xrtNetListenerAcceptCoUntil(xnetlistener* pListener, int64 iDeadlineMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppStream);
}


// 接收网络数据报协程
XXAPI xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 0, 0, 0, (ptr*)ppPacket);
}


// 接收网络数据报协程超时
XXAPI xnet_result xrtNetDgramRecvCoTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppPacket);
}


// 接收网络数据报协程直到指定时刻
XXAPI xnet_result xrtNetDgramRecvCoUntil(xdgramsock* pSock, int64 iDeadlineMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppPacket);
}
#endif


#endif
