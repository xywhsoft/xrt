#define XRT_MODULE_NET_ENGINE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif
typedef struct timerbench {
    xnetengine* Engine;
    xatomic32 Done;
    size_t Count, Completed;
    double Began, End, *Samples;
} timerbench;
static void require(bool Condition,const char* Message)
{if(!Condition){fprintf(stderr,"[FAIL] timer benchmark: %s\n",Message);exit(1);}}
static double cpuTime(void)
{
#if defined(_WIN32)
    FILETIME Created,Exited,Kernel,User;ULARGE_INTEGER K,U;
    require(GetProcessTimes(GetCurrentProcess(),&Created,&Exited,&Kernel,&User)!=0,"process CPU time");
    K.LowPart=Kernel.dwLowDateTime;K.HighPart=Kernel.dwHighDateTime;U.LowPart=User.dwLowDateTime;U.HighPart=User.dwHighDateTime;
    return (double)(K.QuadPart+U.QuadPart)*.1;
#else
    return (double)clock()*1000000.0/(double)CLOCKS_PER_SEC;
#endif
}
static int compare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static void timerFired(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;(void)Id;timerbench* State=Data;require(Result==XNET_RESULT_OK,"timer outcome");double Now=xrtTimer();
    State->Samples[State->Completed++]=(Now-State->Began)*1000000.0;
    if(State->Completed==State->Count){State->End=Now;xrtAtomic32Store(&State->Done,1,XMEMORY_RELEASE);return;}
    State->Began=xrtTimer();require(xrtNetEngineSchedule(State->Engine,0,1,timerFired,State)!=0,"timer chain");
}
int main(int argc,char**argv)
{
    size_t Count=argc>1?(size_t)strtoull(argv[1],NULL,10):1000;unsigned Mode=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
    if(!Count||Count>1000000||Mode>2)return 2;
    double* Values=xrtMalloc(Count*sizeof(*Values));require(Values!=NULL,"sample storage");
    timerbench State={0};State.Count=Count;State.Samples=Values;xnetport* Port=NULL;
    if(!Mode){xnetportconfig Config;xrtNetPortConfigInit(&Config);Port=xrtNetPortCreate(&Config);require(Port!=NULL,"port");}
    else{xnetengineconfig Config;xrtNetEngineConfigInit(&Config);Config.Workers=1;if(Mode==2)Config.Backend=XNET_PORT_SELECT;
        State.Engine=xrtNetEngineCreate(&Config);require(State.Engine&&xrtNetEngineStart(State.Engine),"engine");}
    double CPU=cpuTime(),Started=xrtTimer();
    if(!Mode){for(size_t Index=0;Index<Count;++Index){xnetportevent Event;size_t Ready=0;double Began=xrtTimer();
        require(xrtNetPortWait(Port,&Event,1,1,&Ready)==XNET_RESULT_TIMEOUT&&!Ready,"port timeout");Values[Index]=(xrtTimer()-Began)*1000000.0;}
        State.Completed=Count;State.End=xrtTimer();}
    else{State.Began=xrtTimer();require(xrtNetEngineSchedule(State.Engine,0,1,timerFired,&State)!=0,"first timer");
        double Deadline=xrtTimer()+120;while(!xrtAtomic32Load(&State.Done,XMEMORY_ACQUIRE)){require(xrtTimer()<Deadline,"watchdog");xrtSleep(10);}}
    CPU=cpuTime()-CPU;double Seconds=State.End-Started;qsort(Values,Count,sizeof(*Values),compare);
    printf("timers_per_sec: %.9f\nlatency_p50_us: %.9f\nlatency_p99_us: %.9f\ncpu_us_per_timer: %.9f\ncompleted: %zu\n",(double)Count/Seconds,
        Values[(Count*50+99)/100-1],Values[(Count*99+99)/100-1],CPU/(double)Count,State.Completed);
    puts(Mode==0?"workload: serial 1ms public port waits, no sockets or application I/O; startup excluded":Mode==1?
        "workload: one-worker AUTO 1ms timer chain; callback-to-callback intervals, no sockets; main observer sleeps 10ms; startup excluded":
        "workload: one-worker SELECT 1ms timer chain control; callback-to-callback intervals, no sockets; main observer sleeps 10ms; startup excluded");
    if(Port)require(xrtNetPortDestroy(Port),"port retirement");
    if(State.Engine)require(xrtNetEngineStop(State.Engine)&&xrtNetEngineDestroy(State.Engine),"engine retirement");
    xrtFree(Values);return 0;
}
