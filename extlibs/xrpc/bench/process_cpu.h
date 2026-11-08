#ifndef XRPC_BENCH_PROCESS_CPU_H
#define XRPC_BENCH_PROCESS_CPU_H
/* Benchmark-only native process accounting, including ALL process threads.
 * Core has no process-CPU API. Keep this out of production dependencies. */
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
static inline bool rpcBenchProcessCpu(double* Microseconds)
{
    FILETIME Created,Exited,Kernel,User;
    if(!GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User))return false;
    ULARGE_INTEGER K,U;K.LowPart=Kernel.dwLowDateTime;K.HighPart=Kernel.dwHighDateTime;
    U.LowPart=User.dwLowDateTime;U.HighPart=User.dwHighDateTime;
    *Microseconds=((double)K.QuadPart+(double)U.QuadPart)*.1;return true;
}
#else
#include <sys/resource.h>
static inline bool rpcBenchProcessCpu(double* Microseconds)
{
    struct rusage R;if(getrusage(RUSAGE_SELF,&R))return false;
    *Microseconds=((double)R.ru_utime.tv_sec+(double)R.ru_stime.tv_sec)*1000000.
        +(double)R.ru_utime.tv_usec+(double)R.ru_stime.tv_usec;return true;
}
#endif
#endif
