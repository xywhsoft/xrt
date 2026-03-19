#ifndef TEST_THREAD_CORE_H
#define TEST_THREAD_CORE_H

#include "../xrt.h"

#include <stdio.h>
#include <stdint.h>

typedef struct {
	xmutex hLock;
	xcond hCond;
	volatile int bReady;
	volatile int iWakeCount;
} __test_thread_core_cond_ctx;

static uint32 __Test_ThreadCore_Worker(ptr pArg)
{
	uint32 iValue = (uint32)(uintptr_t)pArg;
	xrtSleep(25);
	return iValue;
}

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

static int __Test_ThreadCore_Check(const char* sName, int bOk)
{
	printf("  %s : %s\n", sName, bOk ? "PASS" : "FAIL");
	return bOk ? 0 : 1;
}

static int Test_ThreadCore(void)
{
	int iFail = 0;
	xthread hThread = NULL;
	xthread hWaiter = NULL;
	xsem hSem = NULL;
	__test_thread_core_cond_ctx tCtx;

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

	return iFail ? 1 : 0;
}

#endif
