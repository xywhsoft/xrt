#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct servicebench {xtime First;uint32 Periods,Jobs;xatomic32 Completed;double* Lateness;double* Queue;} servicebench;
typedef struct servicebenchjob {servicebench* Bench;uint32 Index;} servicebenchjob;
static xcronrunresult serviceBenchRun(const xcronrun* R,const xcronexecution* Token,ptr Raw)
{
    (void)Token;servicebench* B=Raw;servicebenchjob* J=R->Data;xtime Wall=xrtNow();double Mono=xrtTimer()*1000.0;
    if(J->Bench!=B || R->ScheduledAt<B->First || (R->ScheduledAt-B->First)%1000)return XCRON_RUN_FAILED;
    uint64 Period=(uint64)(R->ScheduledAt-B->First)/1000;if(Period>=B->Periods)return XCRON_RUN_FAILED;
    size_t I=(size_t)Period*B->Jobs+J->Index;B->Lateness[I]=(double)(Wall-R->ScheduledAt);B->Queue[I]=Mono>R->Observed.MonotonicMilliseconds?Mono-R->Observed.MonotonicMilliseconds:0;
    xrtAtomic32FetchAdd(&B->Completed,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
static int serviceBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Periods=Argc>1?(uint32)strtoul(Argv[1],NULL,10):8,Jobs=Argc>2?(uint32)strtoul(Argv[2],NULL,10):256;
    if(!Periods || Periods>60 || !Jobs || Jobs>4096)return 1;
    size_t Count=(size_t)Periods*Jobs;double* Samples=xrtMalloc(Count*2*sizeof(double));servicebenchjob* Data=xrtMalloc((size_t)Jobs*sizeof(*Data));if(!Samples || !Data)return 2;
    servicebench B={.Periods=Periods,.Jobs=Jobs,.Lateness=Samples,.Queue=Samples+Count};xexecutorconfig P={4,128,0};xexecutor* Pool=xrtExecutorCreate(&P);if(!Pool)return 3;
    xcronserviceconfig C={.Scheduler={Jobs,64,256,0,4*1024*1024},.Executor=Pool,.Proc=serviceBenchRun,.Context=&B,.CommandLimit=32,.CompletionLimit=256,.BatchLimit=64,.CommandWorkLimit=32,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=2,.RetryAttempts=4,.AdmissionMilliseconds=100,.BytesLimit=8*1024*1024,.AdapterBytesLimit=1024*1024};
    xcronservice* S=NULL;xcronzone* Z=NULL;xcronexpression Expression;if(!xcronServiceCreate(&C,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Expression,NULL))return 4;
    xtime Now=xrtNow();B.First=Now-Now%1000+2000;xtime Last=B.First+(Periods-1)*1000;
    for(uint32 N=0;N<Jobs;++N){Data[N]=(servicebenchjob){&B,N};xcronjobconfig J={.Expression=Expression,.Zone=Z,.Begin=B.First,.End=Last,.RunningLimit=1,.Data=&Data[N]};xcronrequestid Q;xcronreply R;
        if(!xcronServiceAddAsync(S,&J,&Q,NULL) || xcronServiceCollect(S,Q,10000,&R,NULL)!=XWAIT_OK || R.Code || R.Error)return 5;}
    xcronZoneRelease(Z);if(xrtNow()>=B.First)return 6; /* Admission must precede every measured arrival; no favorable retry. */
    size_t Returned=0;double Limit=xrtTimer()+Periods+15;double DoneAt=0;
    while(Returned<Count){xcronservicecompletion D[64];size_t N;if(!xcronServiceDrain(S,D,64,&N,NULL))return 7;
        for(size_t I=0;I<N;++I)if(D[I].Result!=XCRON_RUN_SUCCEEDED || D[I].Error || D[I].Attempts!=1)return 8;
        Returned+=N;if(xrtTimer()>Limit)return 9;if(!N)xrtSleep(1);else DoneAt=xrtTimer();}
    if(xrtAtomic32Load(&B.Completed,XMEMORY_ACQUIRE)!=Count)return 10;
    if(!xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return 11;
    xcronservicestats Stats;if(!xcronServiceInspect(S,&Stats,NULL) || Stats.ActiveNativeTimers)return 14;
    uint32 Retired=0;for(;;){xcronretired R;bool Found;if(!xcronServiceReapStopped(S,&R,&Found,NULL))return 12;if(!Found)break;++Retired;}
    if(Retired!=Jobs || !xcronServiceDestroy(S,NULL) || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xrtExecutorDestroy(Pool))return 13;
    qsort(B.Lateness,Count,sizeof(double),serviceBenchCompare);qsort(B.Queue,Count,sizeof(double),serviceBenchCompare);size_t P99=(Count-1)*99/100;
    printf("arrival_rate_per_sec: %u\nstart_lateness_p99_ms: %.3f\nstart_lateness_max_ms: %.3f\nowner_to_worker_p99_ms: %.3f\nsamples: %zu\ncompletion_monotonic_seconds: %.6f\n",Jobs,B.Lateness[P99],B.Lateness[Count-1],B.Queue[P99],Count,DoneAt);
    printf("waiter_backend: %d\nwaiter_storage_bytes: %zu\nnative_timer_capacity: %u\nnative_timers_after_join: %u\n",(int)Stats.Waiter.Backend,Stats.Waiter.StorageBytes,Stats.Waiter.NativeTimers,Stats.ActiveNativeTimers);
    xrtFree(Data);xrtFree(Samples);return 0;
}
