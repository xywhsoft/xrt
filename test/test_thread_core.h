#ifndef TEST_THREAD_CORE_H
#define TEST_THREAD_CORE_H

#include "../xrt.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	xmutex hLock;
	xcond hCond;
	volatile int bReady;
	volatile int iWakeCount;
} __test_thread_core_cond_ctx;

typedef struct {
	xthread* phThread;
	volatile int iCounter;
} __test_thread_core_stop_ctx;

typedef struct {
	volatile int bAutoAttached;
	volatile int bNestedAttachOk;
	volatile int bTempOk;
	volatile int bErrorOk;
	volatile int bCleanupRan;
	volatile int bRandOk;
	volatile int bLocalDataOk;
} __test_thread_core_runtime_ctx;


// 内部函数：__Test_ThreadCore_Worker
static uint32 __Test_ThreadCore_Worker(ptr pArg)
{
	uint32 iValue = (uint32)(uintptr_t)pArg;
	xrtSleep(25);
	return iValue;
}


// 内部函数：__Test_ThreadCore_CondWorker
static uint32 __Test_ThreadCore_CondWorker(ptr pArg)
{
	__test_thread_core_cond_ctx* pCtx = (__test_thread_core_cond_ctx*)pArg;

	xrtMutexLock(pCtx->hLock);
	while ( !pCtx->bReady ) {
		if ( xrtCondWaitTimeout(pCtx->hCond, pCtx->hLock, 1000) != XRT_WAIT_OK ) {
			xrtMutexUnlock(pCtx->hLock);
			return 10;
		}
	}
	pCtx->iWakeCount++;
	xrtMutexUnlock(pCtx->hLock);
	return 0;
}


// 内部函数：__Test_ThreadCore_StopWorker
static uint32 __Test_ThreadCore_StopWorker(ptr pArg)
{
	__test_thread_core_stop_ctx* pCtx = (__test_thread_core_stop_ctx*)pArg;
	int iIndex;

	if ( !pCtx ) return 2;

	for ( iIndex = 0; iIndex < 50; ++iIndex ) {
		xthread hThread = pCtx->phThread ? *pCtx->phThread : NULL;
		if ( hThread && xrtThreadShouldStop(hThread) ) {
			return 1;
		}
		pCtx->iCounter++;
		xrtSleep(10);
	}

	return 0;
}


// 内部函数：__Test_ThreadCore_Cleanup
static void __Test_ThreadCore_Cleanup(xrtThreadData* pThreadData, ptr pArg)
{
	(void)pThreadData;
	(*(volatile int*)pArg)++;
}


// 内部函数：__Test_ThreadCore_RuntimeWorker
static uint32 __Test_ThreadCore_RuntimeWorker(ptr pArg)
{
	__test_thread_core_runtime_ctx* pCtx = (__test_thread_core_runtime_ctx*)pArg;
	xrtThreadData* pThreadData = xrtThreadGetCurrent();
	char* sTemp = NULL;

	if ( !pCtx ) return 99;

	pCtx->bAutoAttached = xrtThreadIsAttached();
	pCtx->bNestedAttachOk = FALSE;
	pCtx->bTempOk = FALSE;
	pCtx->bErrorOk = FALSE;
	pCtx->bRandOk = FALSE;
	pCtx->bLocalDataOk = FALSE;

	if ( pThreadData ) {
		xrtThreadData* pAttach = xrtThreadAttachCurrent();
		pCtx->bNestedAttachOk = (pAttach == pThreadData);
		xrtThreadDetachCurrent();
	}

	sTemp = (char*)xrtTempMemory(64);
	if ( sTemp ) {
		strcpy(sTemp, "thread-temp-ok");
		pCtx->bTempOk = (strcmp(sTemp, "thread-temp-ok") == 0);
	}

	xrtSetError("thread-error", FALSE);
	pCtx->bErrorOk = (xrtStrComp(xrtGetError(), "thread-error", 0, 0) == 0);

	(void)xrtRand32();
	(void)xrtRand64();
	(void)xrtRandRange(1, 100);
	pCtx->bRandOk = TRUE;
	{
		uint32* pLocal = (uint32*)xrtThreadLocalGetOrCreate("test.thread.value", sizeof(uint32));
		size_t iLocalSize = 0u;
		if ( pLocal ) { *pLocal = 0x12345678u; }
		pCtx->bLocalDataOk = pLocal != NULL &&
			xrtThreadLocalGet("test.thread.value", &iLocalSize) == pLocal &&
			iLocalSize == sizeof(uint32) && *pLocal == 0x12345678u &&
			xrtThreadLocalGetOrCreate("test.thread.value", sizeof(uint16)) == NULL &&
			xrtThreadLocalRemove("test.thread.value") &&
			xrtThreadLocalGet("test.thread.value", NULL) == NULL;
		xrtClearError();
	}
	xrtThreadPushCleanup(__Test_ThreadCore_Cleanup, (ptr)&pCtx->bCleanupRan);
	return 77;
}


// 内部函数：__Test_ThreadCore_Check
static int __Test_ThreadCore_Check(const char* sName, int bOk)
{
	printf("  %s : %s\n", sName, bOk ? "PASS" : "FAIL");
	return bOk ? 0 : 1;
}


// 线程核心测试
static int Test_ThreadCore(void)
{
	int iFail = 0;
	xthread hThread = NULL;
	xthread hWaiter = NULL;
	xthread arrWaiters[3] = { 0 };
	xsem hSem = NULL;
	__test_thread_core_cond_ctx tCtx;
	__test_thread_core_stop_ctx tStopCtx;
	__test_thread_core_runtime_ctx tRuntimeCtx;
	xthread hRuntimeThread = NULL;
	xthread hDestroyGuard = NULL;
	xmutex hTryMutex = NULL;
	xmutex hBroadcastLock = NULL;
	xcond hBroadcastCond = NULL;
	xthread hBroadcastA = NULL;
	xthread hBroadcastB = NULL;
	xthread hBroadcastC = NULL;
	xrtThreadData* pThreadData1 = NULL;
	xrtThreadData* pThreadData2 = NULL;

	printf("\n\n------------------------------------\n\n Thread Core Test:\n\n");

	hThread = xrtThreadCreate((ptr)__Test_ThreadCore_Worker, (ptr)(uintptr_t)7u, 0);
	iFail += __Test_ThreadCore_Check("thread create", hThread != NULL);
	if ( !hThread ) return 1;

	iFail += __Test_ThreadCore_Check("thread state running", xrtThreadGetState(hThread) == XRT_THREAD_RUNNING);
	iFail += __Test_ThreadCore_Check("thread wait timeout immediate", xrtThreadWaitTimeout(hThread, 0) == XRT_WAIT_TIMEOUT);
	iFail += __Test_ThreadCore_Check("thread wait timeout completes", xrtThreadWaitTimeout(hThread, 2000) == XRT_WAIT_OK);
	iFail += __Test_ThreadCore_Check("thread exit code", xrtThreadGetExitCode(hThread) == 7u);
	iFail += __Test_ThreadCore_Check("thread state stopped", xrtThreadGetState(hThread) == XRT_THREAD_STOPPED);
	xrtThreadDestroy(hThread);

	hSem = xrtSemCreate(0, 2);
	iFail += __Test_ThreadCore_Check("sem create", hSem != NULL);
	if ( !hSem ) return 2;
	iFail += __Test_ThreadCore_Check("sem trywait empty", xrtSemTryWait(hSem) == FALSE);
	iFail += __Test_ThreadCore_Check("sem timeout empty", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_TIMEOUT);
	iFail += __Test_ThreadCore_Check("sem post multiple", xrtSemPostMultiple(hSem, 2) == TRUE);
	iFail += __Test_ThreadCore_Check("sem trywait #1", xrtSemTryWait(hSem) == TRUE);
	iFail += __Test_ThreadCore_Check("sem waittimeout #2", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_OK);
	iFail += __Test_ThreadCore_Check("sem timeout drained", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_TIMEOUT);
	xrtSemDestroy(hSem);
	hSem = xrtSemCreate(1, 2);
	iFail += __Test_ThreadCore_Check("sem overflow create", hSem != NULL);
	if ( !hSem ) return 2;
	iFail += __Test_ThreadCore_Check("sem post multiple overflow false", xrtSemPostMultiple(hSem, 2) == FALSE);
	iFail += __Test_ThreadCore_Check("sem overflow keeps original #1", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_OK);
	iFail += __Test_ThreadCore_Check("sem overflow no partial post", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_TIMEOUT);
	xrtSemDestroy(hSem);

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.hLock = xrtMutexCreate();
	tCtx.hCond = xrtCondCreate();
	iFail += __Test_ThreadCore_Check("cond mutex create", tCtx.hLock != NULL);
	iFail += __Test_ThreadCore_Check("cond create", tCtx.hCond != NULL);
	if ( !tCtx.hLock || !tCtx.hCond ) return 3;

	xrtMutexLock(tCtx.hLock);
	iFail += __Test_ThreadCore_Check("cond timeout", xrtCondWaitTimeout(tCtx.hCond, tCtx.hLock, 0) == XRT_WAIT_TIMEOUT);
	xrtMutexUnlock(tCtx.hLock);

	hWaiter = xrtThreadCreate((ptr)__Test_ThreadCore_CondWorker, &tCtx, 0);
	iFail += __Test_ThreadCore_Check("cond waiter create", hWaiter != NULL);
	if ( !hWaiter ) return 4;

	xrtSleep(20);
	xrtMutexLock(tCtx.hLock);
	tCtx.bReady = 1;
	xrtCondSignal(tCtx.hCond);
	xrtMutexUnlock(tCtx.hLock);

	iFail += __Test_ThreadCore_Check("cond waiter join", xrtThreadWaitTimeout(hWaiter, 2000) == XRT_WAIT_OK);
	iFail += __Test_ThreadCore_Check("cond waiter exit code", xrtThreadGetExitCode(hWaiter) == 0u);
	iFail += __Test_ThreadCore_Check("cond wake count", tCtx.iWakeCount == 1);

	xrtThreadDestroy(hWaiter);
	xrtCondDestroy(tCtx.hCond);
	xrtMutexDestroy(tCtx.hLock);

	memset(&tStopCtx, 0, sizeof(tStopCtx));
	tStopCtx.phThread = &hThread;
	hThread = xrtThreadCreate(__Test_ThreadCore_StopWorker, &tStopCtx, 0);
	iFail += __Test_ThreadCore_Check("thread stop create", hThread != NULL);
	if ( hThread ) {
		xrtSleep(40);
		xrtThreadStop(hThread);
		iFail += __Test_ThreadCore_Check("thread stop wait", xrtThreadWaitTimeout(hThread, 2000) == XRT_WAIT_OK);
		iFail += __Test_ThreadCore_Check("thread stop exit code", xrtThreadGetExitCode(hThread) == 1u);
		iFail += __Test_ThreadCore_Check("thread stop observed work", tStopCtx.iCounter > 0);
		xrtThreadDestroy(hThread);
		hThread = NULL;
	}

	hTryMutex = xrtMutexCreate();
	iFail += __Test_ThreadCore_Check("mutex create", hTryMutex != NULL);
	if ( hTryMutex ) {
		xrtMutexLock(hTryMutex);
		iFail += __Test_ThreadCore_Check("mutex trylock busy", xrtMutexTryLock(hTryMutex) == FALSE);
		xrtMutexUnlock(hTryMutex);
		xrtMutexDestroy(hTryMutex);
	}

	memset(&tCtx, 0, sizeof(tCtx));
	hBroadcastLock = xrtMutexCreate();
	hBroadcastCond = xrtCondCreate();
	tCtx.hLock = hBroadcastLock;
	tCtx.hCond = hBroadcastCond;
	iFail += __Test_ThreadCore_Check("cond broadcast mutex create", hBroadcastLock != NULL);
	iFail += __Test_ThreadCore_Check("cond broadcast create", hBroadcastCond != NULL);
	if ( hBroadcastLock && hBroadcastCond ) {
		arrWaiters[0] = xrtThreadCreate((ptr)__Test_ThreadCore_CondWorker, &tCtx, 0);
		arrWaiters[1] = xrtThreadCreate((ptr)__Test_ThreadCore_CondWorker, &tCtx, 0);
		arrWaiters[2] = xrtThreadCreate((ptr)__Test_ThreadCore_CondWorker, &tCtx, 0);
		hBroadcastA = arrWaiters[0];
		hBroadcastB = arrWaiters[1];
		hBroadcastC = arrWaiters[2];
		iFail += __Test_ThreadCore_Check("cond broadcast waiter #1", hBroadcastA != NULL);
		iFail += __Test_ThreadCore_Check("cond broadcast waiter #2", hBroadcastB != NULL);
		iFail += __Test_ThreadCore_Check("cond broadcast waiter #3", hBroadcastC != NULL);
		xrtSleep(20);
		xrtMutexLock(hBroadcastLock);
		tCtx.bReady = 1;
		xrtCondBroadcast(hBroadcastCond);
		xrtMutexUnlock(hBroadcastLock);
		if ( hBroadcastA ) iFail += __Test_ThreadCore_Check("cond broadcast join #1", xrtThreadWaitTimeout(hBroadcastA, 2000) == XRT_WAIT_OK);
		if ( hBroadcastB ) iFail += __Test_ThreadCore_Check("cond broadcast join #2", xrtThreadWaitTimeout(hBroadcastB, 2000) == XRT_WAIT_OK);
		if ( hBroadcastC ) iFail += __Test_ThreadCore_Check("cond broadcast join #3", xrtThreadWaitTimeout(hBroadcastC, 2000) == XRT_WAIT_OK);
		iFail += __Test_ThreadCore_Check("cond broadcast wake count", tCtx.iWakeCount == 3);
	}
	if ( hBroadcastA ) xrtThreadDestroy(hBroadcastA);
	if ( hBroadcastB ) xrtThreadDestroy(hBroadcastB);
	if ( hBroadcastC ) xrtThreadDestroy(hBroadcastC);
	if ( hBroadcastCond ) xrtCondDestroy(hBroadcastCond);
	if ( hBroadcastLock ) xrtMutexDestroy(hBroadcastLock);

	memset(&tRuntimeCtx, 0, sizeof(tRuntimeCtx));
	xrtSetError("main-error", FALSE);
	hRuntimeThread = xrtThreadCreate(__Test_ThreadCore_RuntimeWorker, &tRuntimeCtx, 0);
	iFail += __Test_ThreadCore_Check("runtime worker create", hRuntimeThread != NULL);
	if ( hRuntimeThread ) {
		iFail += __Test_ThreadCore_Check("runtime worker join", xrtThreadWaitTimeout(hRuntimeThread, 2000) == XRT_WAIT_OK);
		iFail += __Test_ThreadCore_Check("runtime auto attach", tRuntimeCtx.bAutoAttached);
		iFail += __Test_ThreadCore_Check("runtime nested attach", tRuntimeCtx.bNestedAttachOk);
		iFail += __Test_ThreadCore_Check("runtime temp memory", tRuntimeCtx.bTempOk);
		iFail += __Test_ThreadCore_Check("runtime error isolated", tRuntimeCtx.bErrorOk);
		iFail += __Test_ThreadCore_Check("runtime cleanup", tRuntimeCtx.bCleanupRan == 1);
		iFail += __Test_ThreadCore_Check("runtime random", tRuntimeCtx.bRandOk);
		iFail += __Test_ThreadCore_Check("runtime named local data", tRuntimeCtx.bLocalDataOk);
		iFail += __Test_ThreadCore_Check("runtime exit code", xrtThreadGetExitCode(hRuntimeThread) == 77u);
		iFail += __Test_ThreadCore_Check("runtime main error preserved", xrtStrComp(xrtGetError(), "main-error", 0, 0) == 0);
		xrtThreadDestroy(hRuntimeThread);
	}
	xrtClearError();

	pThreadData1 = xrtThreadGetCurrent();
	pThreadData2 = xrtThreadAttachCurrent();
	iFail += __Test_ThreadCore_Check("thread attach current", pThreadData1 != NULL && pThreadData1 == pThreadData2);
	xrtThreadDetachCurrent();
	iFail += __Test_ThreadCore_Check("thread detach keeps attached", xrtThreadIsAttached());

	memset(&tStopCtx, 0, sizeof(tStopCtx));
	tStopCtx.phThread = &hDestroyGuard;
	hDestroyGuard = xrtThreadCreate(__Test_ThreadCore_StopWorker, &tStopCtx, 0);
	iFail += __Test_ThreadCore_Check("thread destroy guard create", hDestroyGuard != NULL);
	if ( hDestroyGuard ) {
		xrtSleep(20);
		xrtClearError();
		xrtThreadDestroy(hDestroyGuard);
		iFail += __Test_ThreadCore_Check("thread destroy guard rejects running", xrtStrComp(xrtGetError(), "thread is still running.", 0, 0) == 0);
		xrtThreadStop(hDestroyGuard);
		(void)xrtThreadWaitTimeout(hDestroyGuard, 2000);
		xrtThreadDestroy(hDestroyGuard);
		xrtClearError();
	}

	return iFail ? 1 : 0;
}

#endif
