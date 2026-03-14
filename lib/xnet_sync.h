#ifndef XRT_XNET_SYNC_H
#define XRT_XNET_SYNC_H

#include "xnet_stream.h"
#include "xnet_dgram.h"

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

typedef bool (*__xnet_future_pending_cancel_fn)(struct xrt_net_future* pFuture);
typedef void (*__xnet_future_pending_cleanup_fn)(struct xrt_net_future* pFuture);

struct xrt_net_future {
	__xnet_sync_mutex hLock;
	__xnet_sync_cond hCond;
	volatile bool bDone;
	xnet_result iStatus;
	ptr pValue;
	volatile long iAsyncHoldCount;
	ptr pPendingCtx;
	__xnet_future_pending_cancel_fn pfnPendingCancel;
	__xnet_future_pending_cleanup_fn pfnPendingCleanup;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCoEvent;
		volatile long iCoWaitActive;
	#endif
};

typedef xnet_result (*xnet_future_task_fn)(xnetworker* pWorker, ptr pArg, ptr* ppValue);

typedef struct {
	xnetfuture* pFuture;
	xnet_future_task_fn pfnTask;
	ptr pArg;
} __xnet_future_task_ctx;

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

typedef struct {
	__xnet_stream_sync_wait_fn pfnOnReady;
	const char* sRegisterError;
} __xnet_stream_wait_ops;

static const __xnet_stream_wait_ops* __xnetSyncGetStreamWaitOps(uint32 iWaitKind);



/* ============================== Local platform helpers ============================== */

static void __xnetSyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

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

static long __xnetSyncAtomicCompareExchange(volatile long* pValue, long iExchange, long iComparand)
{
	return __xnetAtomicCompareExchange32(pValue, iExchange, iComparand);
}

static void __xnetSyncAtomicStore(volatile long* pValue, long iValue)
{
	(void)__xnetAtomicExchange32(pValue, iValue);
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

static void __xnetSyncSetError(const char* sError)
{
	#if defined(XXRTL_CORE)
		xrtSetError((str)sError, FALSE);
	#else
		(void)sError;
	#endif
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
	pFuture->iAsyncHoldCount = 0;
	return true;
}

static void __xnetFutureAddAsyncHold(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->iAsyncHoldCount++;
	__xnetFutureUnlock(pFuture);
}

static void __xnetFutureReleaseAsyncHold(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	if ( pFuture->iAsyncHoldCount > 0 ) {
		pFuture->iAsyncHoldCount--;
	}
	__xnetFutureUnlock(pFuture);
}

static void __xnetFutureUnit(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->pCoEvent ) {
			xrtCoEventDestroy(pFuture->pCoEvent);
			pFuture->pCoEvent = NULL;
		}
	#endif
	__xnetFuturePrimitiveUnit(pFuture);
}

static void __xnetFutureReset(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	__xnetFutureLock(pFuture);
	pFuture->bDone = false;
	pFuture->iStatus = XRT_NET_AGAIN;
	pFuture->pValue = NULL;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->pCoEvent ) {
			xrtCoEventReset(pFuture->pCoEvent);
		}
	#endif
	__xnetFutureUnlock(pFuture);
}

static bool __xnetFutureResolve(xnetfuture* pFuture, xnet_result iStatus, ptr pValue)
{
	bool bResolved = false;
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		xcoevent pCoEvent = NULL;
	#endif
	if ( !pFuture ) return false;
	__xnetFutureLock(pFuture);
	if ( !pFuture->bDone ) {
		pFuture->bDone = true;
		pFuture->iStatus = iStatus;
		pFuture->pValue = pValue;
		bResolved = true;
		#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
			pCoEvent = pFuture->pCoEvent;
		#endif
		__xnetFutureWakeAll(pFuture);
	}
	__xnetFutureUnlock(pFuture);
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( bResolved && pCoEvent ) {
			xrtCoEventSet(pCoEvent);
		}
	#endif
	return bResolved;
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
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

static xnetwaitsrc xrtNetWaitSourceNone(void)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_NONE;
	return tSrc;
}

static xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_FUTURE;
	tSrc.u.pFuture = pFuture;
	return tSrc;
}

static xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_STREAM;
	tSrc.u.tStream.pStream = pStream;
	tSrc.u.tStream.iWaitKind = iWaitKind;
	return tSrc;
}

static xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_DGRAM;
	tSrc.u.pDgram = pSock;
	return tSrc;
}

static xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener)
{
	xnetwaitsrc tSrc;
	memset(&tSrc, 0, sizeof(tSrc));
	tSrc.iKind = XNET_WAITSRC_LISTENER;
	tSrc.u.pListener = pListener;
	return tSrc;
}

static bool __xnetFutureDestroyCore(xnetfuture* pFuture)
{
	if ( !pFuture ) return true;
	__xnetFutureLock(pFuture);
	if ( pFuture->iAsyncHoldCount != 0 ) {
		__xnetFutureUnlock(pFuture);
		return false;
	}
	#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
		if ( pFuture->iCoWaitActive != 0 ) {
			__xnetFutureUnlock(pFuture);
			return false;
		}
	#endif
	__xnetFutureUnlock(pFuture);
	if ( pFuture->pfnPendingCleanup ) {
		pFuture->pfnPendingCleanup(pFuture);
	}
	__xnetFutureUnit(pFuture);
	XNET_FREE(pFuture);
	return true;
}

static void xrtNetFutureDestroy(xnetfuture* pFuture)
{
	if ( !pFuture ) return;
	if ( !__xnetFutureDestroyCore(pFuture) ) {
		__xnetSyncSetError("cannot destroy future while an async waiter or task still holds it.");
	}
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

static xnet_result xrtNetFutureWaitUntil(xnetfuture* pFuture, int64_t iDeadlineMs)
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
static xnet_result __xnetFutureWaitCoCore(xnetfuture* pFuture, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	xcoevent pEvent = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	bool bWaitOk = false;

	if ( !pFuture ) return XRT_NET_ERROR;

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		iStatus = pFuture->iStatus;
		__xnetFutureUnlock(pFuture);
		return iStatus;
	}
	__xnetFutureUnlock(pFuture);

	pEvent = __xnetFutureEnsureCoEvent(pFuture);
	if ( !pEvent ) {
		return XRT_NET_ERROR;
	}

	__xnetFutureLock(pFuture);
	if ( pFuture->bDone ) {
		iStatus = pFuture->iStatus;
		__xnetFutureUnlock(pFuture);
		return iStatus;
	}
	if ( pFuture->iCoWaitActive != 0 ) {
		__xnetFutureUnlock(pFuture);
		__xnetSyncSetError("multiple coroutine waiters on one future are not supported yet.");
		return XRT_NET_ERROR;
	}
	pFuture->iCoWaitActive = 1;
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
	pFuture->iCoWaitActive = 0;
	iStatus = pFuture->bDone ? pFuture->iStatus : XRT_NET_AGAIN;
	__xnetFutureUnlock(pFuture);

	if ( !bWaitOk && iStatus == XRT_NET_AGAIN ) {
		if ( xrtCoIsCancelRequested() ) {
			__xnetSyncSetError("coroutine future wait was interrupted before resolve.");
			return XRT_NET_ERROR;
		}
		return XRT_NET_TIMEOUT;
	}

	return (iStatus == XRT_NET_AGAIN) ? XRT_NET_ERROR : iStatus;
}

static xnet_result xrtNetFutureWaitCoUntil(xnetfuture* pFuture, int64 iDeadlineMs)
{
	return __xnetFutureWaitCoCore(pFuture, 2, iDeadlineMs, 0);
}

static xnet_result xrtNetFutureWaitCoTimeout(xnetfuture* pFuture, uint32 iTimeoutMs)
{
	if ( iTimeoutMs == XNET_WAIT_INFINITE ) {
		return __xnetFutureWaitCoCore(pFuture, 0, 0, 0);
	}

	return __xnetFutureWaitCoCore(pFuture, 1, 0, iTimeoutMs);
}

static xnet_result xrtNetFutureWaitCo(xnetfuture* pFuture)
{
	return __xnetFutureWaitCoCore(pFuture, 0, 0, 0);
}
#endif



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

static void __xnetSyncOnStreamDrainFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}

static void __xnetSyncOnStreamCloseFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}

static void __xnetSyncOnStreamReadableFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}

static void __xnetSyncOnStreamWritableFuture(xnetstream* pStream, xnet_result iStatus, ptr pCtx)
{
	__xnet_stream_future_wait_ctx* pWaitCtx = (__xnet_stream_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pStream = pStream;
	}
	__xnetSyncResolveStreamFutureWait(pWaitCtx, iStatus);
}

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

static bool __xnetSyncListenerWaitCanAccept(ptr pCtx)
{
	__xnet_listener_future_wait_ctx* pWaitCtx = (__xnet_listener_future_wait_ctx*)pCtx;
	if ( pWaitCtx == NULL ) return false;
	return __xnetAtomicLoad32(&pWaitCtx->iState) != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED &&
		__xnetAtomicLoad32(&pWaitCtx->iState) != __XNET_SYNC_STREAM_WAIT_FINISHED;
}

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

static void __xnetSyncResolveListenerFutureWait(__xnet_listener_future_wait_ctx* pCtx, xnet_result iStatus, xnetstream* pStream)
{
	long iPrevState;

	if ( pCtx == NULL ) {
		if ( pStream ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		}
		return;
	}

	iPrevState = __xnetAtomicExchange32(&pCtx->iState, __XNET_SYNC_STREAM_WAIT_FINISHED);
	if ( iPrevState == __XNET_SYNC_STREAM_WAIT_FINISHED ) {
		if ( pStream ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
		}
		return;
	}

	if ( iPrevState != __XNET_SYNC_STREAM_WAIT_CANCEL_REQUESTED ) {
		(void)__xnetFutureResolve(pCtx->pFuture, iStatus, pStream);
	} else if ( pStream ) {
		xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
	}

	__xnetListenerReleaseAsyncHold(pCtx->pListener);
	__xnetFutureReleaseAsyncHold(pCtx->pFuture);
	__xnetSyncSignalListenerFutureCancel(pCtx);
}

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

static void __xnetSyncOnListenerAcceptFuture(xnetlistener* pListener, xnet_result iStatus, xnetstream* pStream, ptr pCtx)
{
	__xnet_listener_future_wait_ctx* pWaitCtx = (__xnet_listener_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pListener = pListener;
	}
	__xnetSyncResolveListenerFutureWait(pWaitCtx, iStatus, pStream);
}

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

static void __xnetSyncOnDgramRecvFuture(xdgramsock* pSock, xnet_result iStatus, xnetdgrampkt* pPacket, ptr pCtx)
{
	__xnet_dgram_future_wait_ctx* pWaitCtx = (__xnet_dgram_future_wait_ctx*)pCtx;

	if ( pWaitCtx ) {
		pWaitCtx->pSock = pSock;
	}
	__xnetSyncResolveDgramFutureWait(pWaitCtx, iStatus, pPacket);
}

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

static xnetfuture* xrtNetEnginePostFuture(xnetengine* pEngine, uint32 iAffinityKey, xnet_future_task_fn pfnTask, ptr pArg)
{
	return __xnetSyncCreatePostedFuture(pEngine, iAffinityKey, 0, pfnTask, pArg, false);
}

static xnetfuture* xrtNetEnginePostDelayedFuture(xnetengine* pEngine, uint32 iAffinityKey, uint32 iDelayMs, xnet_future_task_fn pfnTask, ptr pArg)
{
	return __xnetSyncCreatePostedFuture(pEngine, iAffinityKey, iDelayMs, pfnTask, pArg, true);
}

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

static xnetfuture* xrtNetStreamDrainFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_DRAIN);
}

static xnetfuture* xrtNetStreamWritableFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_WRITABLE);
}

static xnetfuture* xrtNetStreamCloseFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_CLOSE);
}

static xnetfuture* xrtNetStreamReadableFuture(xnetstream* pStream)
{
	return __xnetSyncCreateStreamFutureWait(pStream, __XNET_STREAM_WAIT_READABLE);
}

static xnetfuture* xrtNetStreamFutureEx(xnetstream* pStream, uint32 iWaitKind)
{
	return __xnetSyncCreateStreamFutureWait(pStream, iWaitKind);
}

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

static xnetfuture* xrtNetListenerAcceptFuture(xnetlistener* pListener)
{
	return __xnetSyncCreateListenerFutureAccept(pListener);
}

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

static xnetfuture* xrtNetDgramRecvFuture(xdgramsock* pSock)
{
	return __xnetSyncCreateDgramFutureRecv(pSock);
}

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

static xnet_result __xnetSyncWaitDgramSyncCore(xdgramsock* pSock, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitDgramSyncCoreEx(pSock, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

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

static xnet_result __xnetSyncWaitStreamSyncCore(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitStreamSyncCoreEx(pStream, iWaitKind, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

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

static xnet_result __xnetSyncWaitSourceSyncCore(const xnetwaitsrc* pSrc, int iWaitMode, int64_t iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

static xnet_result xrtNetStreamWaitEx(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 0, 0, 0);
}

static xnet_result xrtNetStreamWaitTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 1, 0, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitUntilEx(xnetstream* pStream, uint32 iWaitKind, int64_t iDeadlineMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceSyncCore(&tSrc, 2, iDeadlineMs, 0);
}

static xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 0, 0, 0);
}

static xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 1, 0, iTimeoutMs);
}

static xnet_result xrtNetWaitSourceWaitUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs)
{
	return __xnetSyncWaitSourceSyncCore(pSrc, 2, iDeadlineMs, 0);
}

static xnet_result xrtNetWaitSourceWaitValue(const xnetwaitsrc* pSrc, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 0, 0, 0, ppValue);
}

static xnet_result xrtNetWaitSourceWaitValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 1, 0, iTimeoutMs, ppValue);
}

static xnet_result xrtNetWaitSourceWaitValueUntil(const xnetwaitsrc* pSrc, int64_t iDeadlineMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceSyncCoreEx(pSrc, 2, iDeadlineMs, 0, ppValue);
}

static xnet_result xrtNetListenerAccept(xnetlistener* pListener, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 0, 0, 0, (ptr*)ppStream);
}

static xnet_result xrtNetListenerAcceptTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppStream);
}

static xnet_result xrtNetListenerAcceptUntil(xnetlistener* pListener, int64_t iDeadlineMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppStream);
}

static xnet_result xrtNetDgramRecv(xdgramsock* pSock, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 0, 0, 0, (ptr*)ppPacket);
}

static xnet_result xrtNetDgramRecvTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppPacket);
}

static xnet_result xrtNetDgramRecvUntil(xdgramsock* pSock, int64_t iDeadlineMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceSyncCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppPacket);
}

#if defined(XXRTL_CORE) && !defined(XRT_NO_COROUTINE)
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

static xnet_result __xnetSyncWaitStreamCoCore(xnetstream* pStream, uint32 iWaitKind, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitStreamCoCoreEx(pStream, iWaitKind, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

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

static xnet_result __xnetSyncWaitListenerCoCore(xnetlistener* pListener, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitListenerCoCoreEx(pListener, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

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

static xnet_result __xnetSyncWaitDgramCoCore(xdgramsock* pSock, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitDgramCoCoreEx(pSock, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

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

static xnet_result __xnetSyncWaitSourceCoCore(const xnetwaitsrc* pSrc, int iWaitMode, int64 iDeadlineMs, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, iWaitMode, iDeadlineMs, iTimeoutMs, NULL);
}

static xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind);
static xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);
static xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs);

static xnet_result xrtNetStreamWaitDrainCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_DRAIN);
}

static xnet_result xrtNetStreamWaitDrainCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_DRAIN, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitDrainCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_DRAIN, iDeadlineMs);
}

static xnet_result xrtNetStreamWaitWritableCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_WRITABLE);
}

static xnet_result xrtNetStreamWaitWritableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_WRITABLE, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitWritableCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_WRITABLE, iDeadlineMs);
}

static xnet_result xrtNetStreamWaitCloseCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_CLOSE);
}

static xnet_result xrtNetStreamWaitCloseCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_CLOSE, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitCloseCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_CLOSE, iDeadlineMs);
}

static xnet_result xrtNetStreamWaitReadableCo(xnetstream* pStream)
{
	return xrtNetStreamWaitCoEx(pStream, __XNET_STREAM_WAIT_READABLE);
}

static xnet_result xrtNetStreamWaitReadableCoTimeout(xnetstream* pStream, uint32 iTimeoutMs)
{
	return xrtNetStreamWaitCoTimeoutEx(pStream, __XNET_STREAM_WAIT_READABLE, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitReadableCoUntil(xnetstream* pStream, int64 iDeadlineMs)
{
	return xrtNetStreamWaitCoUntilEx(pStream, __XNET_STREAM_WAIT_READABLE, iDeadlineMs);
}

static xnet_result xrtNetStreamWaitCoEx(xnetstream* pStream, uint32 iWaitKind)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 0, 0, 0);
}

static xnet_result xrtNetStreamWaitCoTimeoutEx(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 1, 0, iTimeoutMs);
}

static xnet_result xrtNetStreamWaitCoUntilEx(xnetstream* pStream, uint32 iWaitKind, int64 iDeadlineMs)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);
	return __xnetSyncWaitSourceCoCore(&tSrc, 2, iDeadlineMs, 0);
}

static xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 0, 0, 0);
}

static xnet_result xrtNetWaitSourceWaitCoTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 1, 0, iTimeoutMs);
}

static xnet_result xrtNetWaitSourceWaitCoUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs)
{
	return __xnetSyncWaitSourceCoCore(pSrc, 2, iDeadlineMs, 0);
}

static xnet_result xrtNetWaitSourceWaitCoValue(const xnetwaitsrc* pSrc, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 0, 0, 0, ppValue);
}

static xnet_result xrtNetWaitSourceWaitCoValueTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 1, 0, iTimeoutMs, ppValue);
}

static xnet_result xrtNetWaitSourceWaitCoValueUntil(const xnetwaitsrc* pSrc, int64 iDeadlineMs, ptr* ppValue)
{
	return __xnetSyncWaitSourceCoCoreEx(pSrc, 2, iDeadlineMs, 0, ppValue);
}

static xnet_result xrtNetListenerAcceptCo(xnetlistener* pListener, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 0, 0, 0, (ptr*)ppStream);
}

static xnet_result xrtNetListenerAcceptCoTimeout(xnetlistener* pListener, uint32 iTimeoutMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppStream);
}

static xnet_result xrtNetListenerAcceptCoUntil(xnetlistener* pListener, int64 iDeadlineMs, xnetstream** ppStream)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceListenerAccept(pListener);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppStream);
}

static xnet_result xrtNetDgramRecvCo(xdgramsock* pSock, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 0, 0, 0, (ptr*)ppPacket);
}

static xnet_result xrtNetDgramRecvCoTimeout(xdgramsock* pSock, uint32 iTimeoutMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 1, 0, iTimeoutMs, (ptr*)ppPacket);
}

static xnet_result xrtNetDgramRecvCoUntil(xdgramsock* pSock, int64 iDeadlineMs, xnetdgrampkt** ppPacket)
{
	xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);
	return __xnetSyncWaitSourceCoCoreEx(&tSrc, 2, iDeadlineMs, 0, (ptr*)ppPacket);
}
#endif


#endif
