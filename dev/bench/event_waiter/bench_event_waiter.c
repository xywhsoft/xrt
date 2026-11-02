#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#define XRT_MODULE_EVENT_WAITER
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
static double waiterCPU(void){FILETIME C,E,K,U;if(!GetProcessTimes(GetCurrentProcess(),&C,&E,&K,&U))return -1;return (double)(((uint64)K.dwHighDateTime<<32)|K.dwLowDateTime)*1e-7+(double)(((uint64)U.dwHighDateTime<<32)|U.dwLowDateTime)*1e-7;}
#else
#include <time.h>
static double waiterCPU(void){struct timespec T;if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T))return -1;return (double)T.tv_sec+(double)T.tv_nsec*1e-9;}
#endif
static int waiterCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    unsigned Rounds=Argc>1?(unsigned)strtoul(Argv[1],NULL,10):256,Milliseconds=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;if(!Rounds || Rounds>4096 || !Milliseconds || Milliseconds>10)return 1;
    double* Samples=xrtMalloc((size_t)Rounds*sizeof(double));xevent E;xeventwaiter W;xeventwaiterinfo Info;if(!Samples || !xrtEventInit(&E,false,false) || !xrtEventWaiterInit(&W,&E,0) || !xrtEventWaiterGet(&W,&Info))return 2;
    double P99[2]={0},P50[2]={0},Maximum[2]={0},CPU[2]={0},Wall[2]={0};
    for(unsigned Mode=0;Mode<2;++Mode){double Begin=xrtTimer(),StartCPU=waiterCPU();if(StartCPU<0)return 3;
        for(unsigned N=0;N<Rounds;++N){double Start=xrtTimer();xwaitresult R=Mode?xrtEventWaiterWaitFor(&W,Milliseconds):xrtEventWaitFor(&E,Milliseconds);if(R!=XWAIT_TIMEOUT)return 4;
            double Elapsed=(xrtTimer()-Start)*1000.0;if(Elapsed<Milliseconds)return 5;Samples[N]=Elapsed-Milliseconds;}
        Wall[Mode]=xrtTimer()-Begin;CPU[Mode]=waiterCPU()-StartCPU;if(CPU[Mode]<0)return 6;qsort(Samples,Rounds,sizeof(double),waiterCompare);P50[Mode]=Samples[(Rounds-1)/2];P99[Mode]=Samples[(Rounds-1)*99/100];Maximum[Mode]=Samples[Rounds-1];
    }
    if(!xrtEventWaiterUnit(&W) || !xrtEventUnit(&E))return 7;
    printf("event_overshoot_p50_ms: %.6f\nevent_overshoot_p99_ms: %.6f\nevent_overshoot_max_ms: %.6f\nwaiter_overshoot_p50_ms: %.6f\nwaiter_overshoot_p99_ms: %.6f\nwaiter_overshoot_max_ms: %.6f\nevent_cpu_us_per_wait: %.6f\nwaiter_cpu_us_per_wait: %.6f\nevent_wall_seconds: %.6f\nwaiter_wall_seconds: %.6f\nbackend: %u\nnative_timers: %u\nsamples_per_mode: %u\n",P50[0],P99[0],Maximum[0],P50[1],P99[1],Maximum[1],CPU[0]*1e6/Rounds,CPU[1]*1e6/Rounds,Wall[0],Wall[1],(unsigned)Info.Backend,Info.NativeTimers,Rounds);xrtFree(Samples);return 0;
}
