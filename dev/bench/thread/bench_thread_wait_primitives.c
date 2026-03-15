#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    int i;
    int iCount = (argc > 1) ? atoi(argv[1]) : 100000;
    xsem hSem;
    xmutex hMutex;
    xcond hCond;
    double fStart;
    double fEnd;
    double fSec;

    if ( iCount <= 0 ) iCount = 100000;

    hSem = xrtSemCreate(0, (uint32)iCount);
    hMutex = xrtMutexCreate();
    hCond = xrtCondCreate();
    if ( !hSem || !hMutex || !hCond ) {
        fprintf(stderr, "primitive create failed\n");
        return 1;
    }

    fStart = xrtTimer();
    for ( i = 0; i < iCount; ++i ) {
        if ( !xrtSemPost(hSem) ) {
            fprintf(stderr, "sem post failed at %d\n", i);
            return 2;
        }
        if ( xrtSemWaitTimeout(hSem, 1000) != XRT_WAIT_OK ) {
            fprintf(stderr, "sem wait failed at %d\n", i);
            return 3;
        }
    }
    fEnd = xrtTimer();
    fSec = fEnd - fStart;
    if ( fSec <= 0.0 ) fSec = 0.001;
    printf("thread_sem_roundtrip.count=%d\n", iCount);
    printf("thread_sem_roundtrip.elapsed_ms=%.3f\n", fSec * 1000.0);
    printf("thread_sem_roundtrip.ops_per_sec=%.2f\n", (double)iCount / fSec);

    fStart = xrtTimer();
    xrtMutexLock(hMutex);
    for ( i = 0; i < iCount; ++i ) {
        xrtCondSignal(hCond);
    }
    xrtMutexUnlock(hMutex);
    fEnd = xrtTimer();
    fSec = fEnd - fStart;
    if ( fSec <= 0.0 ) fSec = 0.001;
    printf("thread_cond_signal.count=%d\n", iCount);
    printf("thread_cond_signal.elapsed_ms=%.3f\n", fSec * 1000.0);
    printf("thread_cond_signal.ops_per_sec=%.2f\n", (double)iCount / fSec);

    xrtCondDestroy(hCond);
    xrtMutexDestroy(hMutex);
    xrtSemDestroy(hSem);
    return 0;
}
