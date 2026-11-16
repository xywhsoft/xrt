#ifndef XTP_BENCH_CLOCK_H
#define XTP_BENCH_CLOCK_H
#if defined(_WIN32)
#include <windows.h>
static double xtp_bench_cpu(void)
{
    FILETIME created,exited,kernel,user;
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))abort();
    return ((double)kernel.dwHighDateTime*4294967296.0+kernel.dwLowDateTime+
            (double)user.dwHighDateTime*4294967296.0+user.dwLowDateTime)/10000000.0;
}
#else
#include <time.h>
static double xtp_bench_cpu(void){return (double)clock()/CLOCKS_PER_SEC;}
#endif
#endif
