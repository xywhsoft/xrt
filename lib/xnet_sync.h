#ifndef XRT_XNET_SYNC_H
#define XRT_XNET_SYNC_H

#include "xnet_engine.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <errno.h>
	#include <pthread.h>
	#include <time.h>
	#include <unistd.h>
#endif


/*
    XNet V2 - Sync Core

    Phase-1 scope in this header:
      - future object and timed wait semantics
      - hidden convenience engine for sync facades

    Still pending:
      - protocol-specific sync facades built on top of futures
*/


/* ============================== Local sync primitives ============================== */

#define XNET_WAIT_INFINITE UINT32_C(0xffffffff)

#if defined(_WIN32) || defined(_WIN64)
	typedef CRITICAL_SECTION __xnet_sync_mutex;
	typedef CONDITION_VARIABLE __xnet_sync_cond;
#else
	typedef pthread_mutex_t __xnet_sync_mutex;
	typedef pthread_cond_t __xnet_sync_cond;
#endif

typedef struct {
	volatile long iLock;
	xnetengine* pEngine;
} __xnet_sync_hidden_state;

struct xrt_net_future {
	__xnet_sync_mutex hLock;
	__xnet_sync_cond hCond;
	volatile bool bDone;
	xnet_result iStatus;
	ptr pValue;
};



/* ============================== Local platform helpers ============================== */

static void __xnetSyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __xnetSyncAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, iExchange, iComparand);
	#else
		return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
	#endif
}

static void __xnetSyncAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static void __xnetSyncSpinLock(volatile long* pLock)
{
	while ( __xnetSyncAtomicCompareExchange(pLock, 1, 0) != 0 ) {
		__xnetSyncSleepMs(1);
	}
}

static void __xnetSyncSpinUnlock(volatile long* pLock)
{
	__xnetSyncAtomicStore(pLock, 0);
}

static bool __xnetFuturePrimitiveInit(xnetfuture* pFuture)
{
	if ( !pFuture ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		InitializeCriticalSection(&pFuture->hLock);
		InitializeConditionVariable(&pFuture->hCond);
		return true;
	#else
		if ( pthread_mutex_init(&pFuture->hLock, NULL) != 0 ) return false;
		if ( pthread_cond_init(&pFuture->hCond, NULL) != 0 ) {
			pthread_mutex_destroy(&pFuture->hLock);
			return false;
		}
		return true;
	#endif
}

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

static void __xnetFutureLock(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		EnterCriticalSection(&pFuture->hLock);
	#else
		pthread_mutex_lock(&pFuture->hLock);
	#endif
}

static void __xnetFutureUnlock(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		LeaveCriticalSection(&pFuture->hLock);
	#else
		pthread_mutex_unlock(&pFuture->hLock);
	#endif
}

static void __xnetFutureWakeAll(xnetfuture* pFuture)
{
	#if defined(_WIN32) || defined(_WIN64)
		WakeAllConditionVariable(&pFuture->hCond);
	#else
		pthread_cond_broadcast(&pFuture->hCond);
	#endif
}

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
			struct timespec tNow;
			struct timespec tAbs;
			uint64 iNs;
			if ( clock_gettime(CLOCK_REALTIME, &tNow) != 0 ) return false;
			iNs = (uint64)tNow.tv_nsec + ((uint64)iTimeoutMs * 1000000ULL);
			tAbs.tv_sec = tNow.tv_sec + (time_t)(iNs / 1000000000ULL);
			tAbs.tv_nsec = (long)(iNs % 1000000000ULL);
			return pthread_cond_timedwait(&pFuture->hCond, &pFuture->hLock, &tAbs) == 0;
		}
	#endif
}

static bool __xnetFutureInit(xnetfuture* pFuture)
{
	if ( !pFuture ) return false;
	memset(pFuture, 0, sizeof(xnetfuture));
	if ( !__xnetFuturePrimitiveInit(pFuture) ) return false;
	pFuture->bDone = false;
	pFuture->iStatus = XRT_NET_AGAIN;
	pFuture->pValue = NULL;
	return true;
}

static void __xnetFutureUnit(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFuturePrimitiveUnit(pFuture);
}

static void __xnetFutureReset(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->bDone = false;
	pFuture->iStatus = XRT_NET_AGAIN;
	pFuture->pValue = NULL;
	__xnetFutureUnlock(pFuture);
}

static bool __xnetFutureResolve(xnetfuture* pFuture, xnet_result iStatus, ptr pValue)
{
	bool bResolved = false;
	if ( !pFuture ) return false;
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		pFuture->bDone = true;
		pFuture->iStatus = iStatus;
		pFuture->pValue = pValue;
		bResolved = true;
		__xnetFutureWakeAll(pFuture);
	}
	__xnetFutureUnlock(pFuture);
	return bResolved;
}

static __xnet_sync_hidden_state* __xnetSyncHiddenState(void)
{
	static __xnet_sync_hidden_state tState;
	return &tState;
}



/* ============================== Public future helpers ============================== */

static xnetfuture* xrtNetFutureCreate(void)
{
	xnetfuture* pFuture = (xnetfuture*)XNET_ALLOC(sizeof(xnetfuture));
	if ( !pFuture ) return NULL;
	if ( !__xnetFutureInit(pFuture) ) {
		XNET_FREE(pFuture);
		return NULL;
	}
	return pFuture;
}

static void xrtNetFutureDestroy(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureUnit(pFuture);
	XNET_FREE(pFuture);
}

static xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs)
{
	xnet_result iStatus;
	if ( !pFuture ) return XRT_NET_ERROR;

	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		if ( iTimeoutMs == 0 ) {
			__xnetFutureUnlock(pFuture);
			return XRT_NET_TIMEOUT;
		}
		if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
			while ( !pFuture->bDone ) {
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
					return XRT_NET_ERROR;
				}
				iDeadlineMs = ((uint64)tNow.tv_sec * 1000ULL) + ((uint64)tNow.tv_nsec / 1000000ULL) + (uint64)iTimeoutMs;
			#endif

			while ( !pFuture->bDone ) {
				uint32 iWaitMs;
				uint64 iNowMs;
				bool bSignaled;
				#if defined(_WIN32) || defined(_WIN64)
					iNowMs = GetTickCount64();
				#else
					struct timespec tNowLoop;
					if ( clock_gettime(CLOCK_MONOTONIC, &tNowLoop) != 0 ) {
						__xnetFutureUnlock(pFuture);
						return XRT_NET_ERROR;
					}
					iNowMs = ((uint64)tNowLoop.tv_sec * 1000ULL) + ((uint64)tNowLoop.tv_nsec / 1000000ULL);
				#endif
				if ( iNowMs >= iDeadlineMs ) {
					__xnetFutureUnlock(pFuture);
					return XRT_NET_TIMEOUT;
				}
				iWaitMs = (uint32)(iDeadlineMs - iNowMs);
				bSignaled = __xnetFutureWaitOnce(pFuture, iWaitMs);
				if ( !bSignaled && !pFuture->bDone ) {
					__xnetFutureUnlock(pFuture);
					return XRT_NET_TIMEOUT;
				}
			}
		}
	}

	iStatus = pFuture->iStatus;
	__xnetFutureUnlock(pFuture);
	return iStatus;
}

static xnet_result xrtNetFutureStatus(xnetfuture* pFuture)
{
	xnet_result iStatus;
	if ( !pFuture ) return XRT_NET_ERROR;
	__xnetFutureLock(pFuture);
	iStatus = pFuture->bDone ? pFuture->iStatus : XRT_NET_AGAIN;
	__xnetFutureUnlock(pFuture);
	return iStatus;
}

static ptr xrtNetFutureValue(xnetfuture* pFuture)
{
	ptr pValue;
	if ( !pFuture ) return NULL;
	__xnetFutureLock(pFuture);
	pValue = pFuture->bDone ? pFuture->pValue : NULL;
	__xnetFutureUnlock(pFuture);
	return pValue;
}



/* ============================== Hidden convenience engine helpers ============================== */

static xnetengine* xrtNetSyncGetHiddenEngine(void)
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

static void xrtNetSyncShutdownHiddenEngine(void)
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

static xnetengine* __xnetSyncResolveEngine(xnetengine* pEngine)
{
	return pEngine ? pEngine : xrtNetSyncGetHiddenEngine();
}


#endif
