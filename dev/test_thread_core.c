#include "xrt.h"

#include <stdio.h>
#include <stdint.h>

typedef struct {
    xmutex hLock;
    xcond hCond;
    volatile int bReady;
    volatile int iWakeCount;
} cond_ctx;

static uint32 __thread_core_worker(ptr pArg)
{
    uint32 iValue = (uint32)(uintptr_t)pArg;
    xrtSleep(25);
    return iValue;
}

static uint32 __thread_cond_worker(ptr pArg)
{
    cond_ctx* pCtx = (cond_ctx*)pArg;
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

static int __check(const char* sName, int bOk)
{
    printf("  %s : %s\n", sName, bOk ? "PASS" : "FAIL");
    return bOk ? 0 : 1;
}

int main(void)
{
    int iFail = 0;
    xthread hThread = NULL;
    xthread hWaiter = NULL;
    xsem hSem = NULL;
    cond_ctx tCtx;

    printf("\n\n------------------------------------\n\n Thread Core Test:\n\n");

    hThread = xrtThreadCreate((ptr)__thread_core_worker, (ptr)(uintptr_t)7u, 0);
    iFail += __check("thread create", hThread != NULL);
    if ( !hThread ) return 1;

    iFail += __check("thread state running", xrtThreadGetState(hThread) == XRT_THREAD_RUNNING);
    iFail += __check("thread wait timeout immediate", xrtThreadWaitTimeout(hThread, 0) == XRT_WAIT_TIMEOUT);
    iFail += __check("thread wait timeout completes", xrtThreadWaitTimeout(hThread, 2000) == XRT_WAIT_OK);
    iFail += __check("thread exit code", xrtThreadGetExitCode(hThread) == 7u);
    iFail += __check("thread state stopped", xrtThreadGetState(hThread) == XRT_THREAD_STOPPED);
    xrtThreadDestroy(hThread);

    hSem = xrtSemCreate(0, 2);
    iFail += __check("sem create", hSem != NULL);
    if ( !hSem ) return 2;
    iFail += __check("sem trywait empty", xrtSemTryWait(hSem) == FALSE);
    iFail += __check("sem timeout empty", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_TIMEOUT);
    iFail += __check("sem post multiple", xrtSemPostMultiple(hSem, 2) == TRUE);
    iFail += __check("sem trywait #1", xrtSemTryWait(hSem) == TRUE);
    iFail += __check("sem waittimeout #2", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_OK);
    iFail += __check("sem timeout drained", xrtSemWaitTimeout(hSem, 0) == XRT_WAIT_TIMEOUT);
    xrtSemDestroy(hSem);

    memset(&tCtx, 0, sizeof(tCtx));
    tCtx.hLock = xrtMutexCreate();
    tCtx.hCond = xrtCondCreate();
    iFail += __check("cond mutex create", tCtx.hLock != NULL);
    iFail += __check("cond create", tCtx.hCond != NULL);
    if ( !tCtx.hLock || !tCtx.hCond ) return 3;

    xrtMutexLock(tCtx.hLock);
    iFail += __check("cond timeout", xrtCondWaitTimeout(tCtx.hCond, tCtx.hLock, 0) == XRT_WAIT_TIMEOUT);
    xrtMutexUnlock(tCtx.hLock);

    hWaiter = xrtThreadCreate((ptr)__thread_cond_worker, &tCtx, 0);
    iFail += __check("cond waiter create", hWaiter != NULL);
    if ( !hWaiter ) return 4;

    xrtSleep(20);
    xrtMutexLock(tCtx.hLock);
    tCtx.bReady = 1;
    xrtCondSignal(tCtx.hCond);
    xrtMutexUnlock(tCtx.hLock);

    iFail += __check("cond waiter join", xrtThreadWaitTimeout(hWaiter, 2000) == XRT_WAIT_OK);
    iFail += __check("cond waiter exit code", xrtThreadGetExitCode(hWaiter) == 0u);
    iFail += __check("cond wake count", tCtx.iWakeCount == 1);

    xrtThreadDestroy(hWaiter);
    xrtCondDestroy(tCtx.hCond);
    xrtMutexDestroy(tCtx.hLock);

    return iFail ? 1 : 0;
}
