#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static uint32 __bench_worker(ptr pArg)
{
    (void)pArg;
    return 0;
}

int main(int argc, char** argv)
{
    int i;
    int iCount = (argc > 1) ? atoi(argv[1]) : 1000;
    double fStart;
    double fEnd;
    double fSec;
    double fOps;

    if ( iCount <= 0 ) iCount = 1000;

    fStart = xrtTimer();
    for ( i = 0; i < iCount; ++i ) {
        xthread hThread = xrtThreadCreate((ptr)__bench_worker, NULL, 0);
        if ( !hThread ) {
            fprintf(stderr, "thread create failed at %d\n", i);
            return 1;
        }
        xrtThreadWait(hThread);
        xrtThreadDestroy(hThread);
    }
    fEnd = xrtTimer();

    fSec = fEnd - fStart;
    if ( fSec <= 0.0 ) fSec = 0.001;
    fOps = (double)iCount / fSec;

    printf("thread_create_join.count=%d\n", iCount);
    printf("thread_create_join.elapsed_ms=%.3f\n", fSec * 1000.0);
    printf("thread_create_join.ops_per_sec=%.2f\n", fOps);
    return 0;
}
