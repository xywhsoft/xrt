/* Matched polling/readiness real UTC fixed arrivals; every expected job/second has a callback claim and
 * physical receipt. No arrival is rescheduled after a late admission. Native
 * process CPU clocks are benchmark instrumentation, never a production adapter. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <xrt/memory_stats.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif
typedef struct arrivalbench arrivalbench;
typedef struct arrivaljob {arrivalbench* Bench;uint32 Index;xcronjobid Id;} arrivaljob;
typedef struct arrivalevent {
    xatomic32 Claimed;bool Received;xcronrun Run;uint64 Ordinal;
    double StartDelay,QueueDelay;
} arrivalevent;
typedef struct arrivalkey {xcronjobid Id;uint32 Index;} arrivalkey;
struct arrivalbench {xtime First;uint32 Jobs,Periods;xatomic32 Calls;arrivaljob* Data;arrivalevent* Events;};
static bool arrivalCPU(double* Milliseconds)
{
#if defined(_WIN32)
    FILETIME Create,Exit,Kernel,User;if(!GetProcessTimes(GetCurrentProcess(),&Create,&Exit,&Kernel,&User))return false;
    uint64 K=((uint64)Kernel.dwHighDateTime<<32)|Kernel.dwLowDateTime,U=((uint64)User.dwHighDateTime<<32)|User.dwLowDateTime;*Milliseconds=(double)(K+U)/10000.0;
#else
    struct timespec T;if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&T))return false;*Milliseconds=(double)T.tv_sec*1000.0+(double)T.tv_nsec/1000000.0;
#endif
    return true;
}
static bool arrivalLabel(xtime When,const xdatetime* L,size_t Candidates)
{xdatetime Expected;xrtTimeSplit(When,&Expected);Expected.Offset=0;Expected.IsDST=-1;return Candidates==1 && !memcmp(L,&Expected,sizeof(Expected));}
static xcronrunresult arrivalProc(const xcronrun* R,const xcronexecution* Token,ptr Raw)
{
    xtime Wall=xrtNow();double Mono=xrtTimer()*1000.0;arrivalbench* B=Raw;arrivaljob* J=R->Data;const xcronexecutionlabel* L=xcronExecutionSelected(Token);
    if(!J || J->Bench!=B || J->Index>=B->Jobs || J!=B->Data+J->Index || R->ScheduledAt<B->First || (R->ScheduledAt-B->First)%1000 || R->Through!=R->ScheduledAt || R->Flags || !L || !arrivalLabel(R->ScheduledAt,&L->Local,L->Candidates) || Wall<R->ScheduledAt || Mono<(double)R->Observed.MonotonicMilliseconds)return XCRON_RUN_FAILED;
    uint64 Period=(uint64)(R->ScheduledAt-B->First)/1000;if(Period>=B->Periods)return XCRON_RUN_FAILED;
    size_t I=(size_t)Period*B->Jobs+J->Index;arrivalevent* E=&B->Events[I];uint32 Empty=0;
    if(!xrtAtomic32CompareExchange(&E->Claimed,&Empty,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return XCRON_RUN_FAILED;
    E->Run=*R;E->Ordinal=I+1;E->StartDelay=(double)(Wall-R->ScheduledAt);E->QueueDelay=Mono-(double)R->Observed.MonotonicMilliseconds;xrtAtomic32FetchAdd(&B->Calls,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
static int arrivalKeyCompare(const void* A,const void* B)
{xcronjobid X=((const arrivalkey*)A)->Id,Y=((const arrivalkey*)B)->Id;return (X>Y)-(X<Y);}
static uint32 arrivalLookup(const arrivalkey* Keys,uint32 Count,xcronjobid Id)
{size_t A=0,B=Count;while(A<B){size_t M=A+(B-A)/2;if(Keys[M].Id<Id)A=M+1;else B=M;}return A<Count && Keys[A].Id==Id?Keys[A].Index:UINT32_MAX;}
static int arrivalCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
static void arrivalPrint(cstr Name,double* Samples,size_t Count)
{
    qsort(Samples,Count,sizeof(*Samples),arrivalCompare);
    printf("%s_p50_ms: %.3f\n%s_p95_ms: %.3f\n%s_p99_ms: %.3f\n%s_p999_ms: %.3f\n%s_max_ms: %.3f\n",Name,Samples[(Count-1)/2],Name,Samples[(Count-1)*95/100],Name,Samples[(Count-1)*99/100],Name,Samples[(Count-1)*999/1000],Name,Samples[Count-1]);
}
int main(int Argc,char** Argv)
{
    uint32 Periods=Argc>1?(uint32)strtoul(Argv[1],NULL,10):4,Jobs=Argc>2?(uint32)strtoul(Argv[2],NULL,10):256,Threads=Argc>3?(uint32)strtoul(Argv[3],NULL,10):4,Width=Argc>4?(uint32)strtoul(Argv[4],NULL,10):64,Pause=Argc>5?(uint32)strtoul(Argv[5],NULL,10):0;
    uint32 Consumer=Argc>6?(uint32)strtoul(Argv[6],NULL,10):1;
    if(Consumer>1 || !Periods || Periods>60 || !Jobs || Jobs>4096 || !Threads || Threads>16 || !Width || Width>64 || Pause>30000)return 1;
    int Exit=2;xexecutor* Pool=NULL;xcronservice* S=NULL;xcronzone* Zone=NULL;xcronrequestid Pending=0;size_t Count=(size_t)Periods*Jobs;arrivalbench B={.Jobs=Jobs,.Periods=Periods};arrivalkey* Keys=NULL;double* Samples=NULL;bool StatsOn=false;uint32 Admitted=0,Retired=0;uint64 Sum=0;double CPUStart=0,CPUEnd=0,PauseObserved=0;uint32 PauseReady=0;
    B.Data=xrtCalloc(Jobs,sizeof(*B.Data));B.Events=xrtCalloc(Count,sizeof(*B.Events));Keys=xrtMalloc((size_t)Jobs*sizeof(*Keys));Samples=xrtMalloc(Count*3*sizeof(*Samples));if(!B.Data || !B.Events || !Keys || !Samples)goto Done;
    for(uint32 I=0;I<Jobs;++I)B.Data[I]=(arrivaljob){.Bench=&B,.Index=I};
    for(size_t I=0;I<Count;++I)xrtAtomic32Init(&B.Events[I].Claimed,0);
    xexecutorconfig PC={Threads,128,0};Pool=xrtExecutorCreate(&PC);if(!Pool)goto Done;
    xcronselectedserviceconfig C={.Service={.Scheduler={Jobs,64,256,0,8*1024*1024},.Executor=Pool,.Proc=arrivalProc,.Context=&B,.CommandLimit=8,.CompletionLimit=Jobs,.BatchLimit=Jobs<64?Jobs:64,.CommandWorkLimit=8,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=2,.RetryAttempts=4,.AdmissionMilliseconds=100,.BytesLimit=16*1024*1024,.AdapterBytesLimit=1024*1024},.SubmissionBatchLimit=Width};
    xcronexpression Expression;if(!xcronServiceCreateSelected(&C,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Zone,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Expression,NULL))goto Done;
    xtime Now=xrtNow();B.First=Now-Now%1000+3000;xtime Last=B.First+(Periods-1)*1000;
    for(uint32 I=0;I<Jobs;++I){xcronjobconfig J={.Expression=Expression,.Zone=Zone,.Begin=B.First,.End=Last,.Misfire=XCRON_MISFIRE_CATCH_UP,.Overlap=XCRON_OVERLAP_WAIT,.RunningLimit=1,.Data=&B.Data[I]};
        if(!xcronServiceAddSelectedAsync(S,&J,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_REJECT},&Pending,NULL))goto Done;
        xcronselectedreply R;if(xcronServiceCollectSelected(S,Pending,10000,&R,NULL)!=XWAIT_OK)goto Done;Pending=0;bool OK=!R.Code && R.Job && !R.Error;B.Data[I].Id=R.Job;xrtErrorFree(R.Error);if(!OK)goto Done;Keys[I]=(arrivalkey){R.Job,I};++Admitted;
    }
    xcronZoneRelease(Zone);Zone=NULL;if(xrtNow()>=B.First){fprintf(stderr,"invalid fixed-arrival sample: admission missed first deadline\n");goto Done;}
    qsort(Keys,Jobs,sizeof(*Keys),arrivalKeyCompare);for(uint32 I=1;I<Jobs;++I)if(Keys[I-1].Id==Keys[I].Id)goto Done;
    xcronselectedservicestats Before,After;if(!xcronServiceInspectSelected(S,&Before,NULL) || Before.Service.Failure.Code || Before.Service.Kernel.Dispatched || xrtAtomic32Load(&B.Calls,XMEMORY_ACQUIRE) || !arrivalCPU(&CPUStart))goto Done;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats MemoryBefore,MemoryAfter;xrtMemStatsEnable(true);StatsOn=true;xrtMemStatsGet(&MemoryBefore);
#endif
    xtime CounterBoundary=xrtNow();if(CounterBoundary>=B.First || xrtAtomic32Load(&B.Calls,XMEMORY_ACQUIRE)){fprintf(stderr,"invalid first-arrival counter window\n");goto Done;}uint64 CounterLead=(uint64)(B.First-CounterBoundary);
    {size_t Returned=0;double Limit=xrtTimer()+Periods+Pause/1000.0+20;bool Paused=Pause==0;
        while(Returned<Count){if(xrtTimer()>Limit)goto Done;
            if(!Paused){xcronselectedservicestats T;if(!xcronServiceInspectSelected(S,&T,NULL))goto Done;if(T.Service.ReadyCompletions<Jobs){xrtSleep(1);continue;}PauseReady=T.Service.ReadyCompletions;double PausedAt=xrtTimer();xrtSleep(Pause);PauseObserved=(xrtTimer()-PausedAt)*1000.0;Paused=true;}
            xcronselectedservicecompletion D[64];size_t N;if(!xcronServiceDrainSelected(S,D,64,&N,NULL))goto Done;xtime Wall=xrtNow();bool Valid=true;
            for(size_t K=0;K<N;++K){xcronservicecompletion* V=&D[K].Completion;uint32 Index=arrivalLookup(Keys,Jobs,V->Job);uint64 Period=V->ScheduledAt>=B.First?(uint64)(V->ScheduledAt-B.First)/1000:UINT64_MAX;bool OK=Index<Jobs && Period<Periods && (V->ScheduledAt-B.First)%1000==0 && V->Through==V->ScheduledAt && V->Result==XCRON_RUN_SUCCEEDED && V->Code==XCRON_COMPLETION_NORMAL && V->Attempts==1 && !V->Error && !V->Flags && Wall>=V->ScheduledAt && arrivalLabel(V->ScheduledAt,&D[K].Local,D[K].Candidates);xrtErrorFree(V->Error);
                if(!OK){Valid=false;continue;}size_t I=(size_t)Period*Jobs+Index;arrivalevent* E=&B.Events[I];xcronrun* R=&E->Run;
                if(E->Received || !xrtAtomic32Load(&E->Claimed,XMEMORY_ACQUIRE) || E->Ordinal!=I+1 || R->Id!=V->Id || R->Job!=V->Job || R->Data!=&B.Data[Index] || R->ScheduledAt!=V->ScheduledAt || R->Through!=V->Through || R->Observed.Wall!=V->Observed.Wall || R->Observed.MonotonicMilliseconds!=V->Observed.MonotonicMilliseconds || R->Flags!=V->Flags){Valid=false;continue;}
                E->Received=true;Samples[I]=E->StartDelay;Samples[Count+I]=E->QueueDelay;Samples[2*Count+I]=(double)(Wall-V->ScheduledAt);Sum+=E->Ordinal;
            }
            Returned+=N;if(!Valid)goto Done;if(!N){if(Consumer){double Left=(Limit-xrtTimer())*1000.0;if(Left<=0)goto Done;xwaitresult W=xcronServiceWaitReadable(S,Left>1000.0?1000:(int64)Left+1,NULL);if(W==XWAIT_ERROR)goto Done;}else xrtSleep(1);}
        }
    }
    if(!arrivalCPU(&CPUEnd) || CPUEnd<CPUStart || xrtAtomic32Load(&B.Calls,XMEMORY_ACQUIRE)!=Count || Sum!=(uint64)Count*(Count+1)/2 || !xcronServiceInspectSelected(S,&After,NULL) || After.Service.Failure.Code || After.Service.Kernel.Faults || After.Service.Kernel.SkippedRanges || After.Service.Kernel.CoalescedRanges || After.Service.Kernel.WallRegressions || After.Service.RejectedSubmissions || After.Service.SubmitAttempts!=Count || After.Service.Published!=Count || After.Service.Collected!=Count || (Width==1 && After.BatchSubmissions) || (Pause && PauseReady!=Jobs))goto Done;
    for(size_t I=0;I<Count;++I)if(!B.Events[I].Received || !xrtAtomic32Load(&B.Events[I].Claimed,XMEMORY_ACQUIRE))goto Done;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&MemoryAfter);xrtMemStatsEnable(false);StatsOn=false;
    printf("memory_stats_enabled: 1\nmalloc_calls: %" PRIu64 "\ncalloc_calls: %" PRIu64 "\nrealloc_calls: %" PRIu64 "\nblock_alloc_calls: %" PRIu64 "\nrequested_bytes: %" PRIu64 "\n",MemoryAfter.MallocCalls-MemoryBefore.MallocCalls,MemoryAfter.CallocCalls-MemoryBefore.CallocCalls,MemoryAfter.ReallocCalls-MemoryBefore.ReallocCalls,MemoryAfter.BlockAllocCalls-MemoryBefore.BlockAllocCalls,(MemoryAfter.MallocBytes-MemoryBefore.MallocBytes)+(MemoryAfter.CallocBytes-MemoryBefore.CallocBytes)+(MemoryAfter.ReallocBytes-MemoryBefore.ReallocBytes));
#else
    puts("memory_stats_enabled: 0\nmalloc_calls: unavailable\ncalloc_calls: unavailable\nrealloc_calls: unavailable\nblock_alloc_calls: unavailable\nrequested_bytes: unavailable");
#endif
    printf("pause_ready_credits: %u\nconsumer_pause_observed_ms: %.3f\n",PauseReady,PauseObserved);
    printf("consumer_wait: %u\ncounter_window_guard: 1\ncounter_lead_ms: %" PRIu64 "\n",Consumer,CounterLead);
    arrivalPrint("start_lateness",Samples,Count);arrivalPrint("owner_to_worker",Samples+Count,Count);arrivalPrint("receipt_lateness",Samples+Count*2,Count);
    printf("periods: %u\narrival_rate_per_sec: %u\nthreads: %u\nsubmission_batch_size: %u\nconsumer_pause_ms: %u\nsamples: %zu\ncallbacks: %u\nchecksum: %" PRIu64 "\nwindow_cpu_ms: %.3f\nservice_bytes: %zu\nfixture_bytes: %zu\nnative_batch_calls: %" PRIu64 "\nbatched_attempts: %" PRIu64 "\nwaiter_backend: %d\n",Periods,Jobs,Threads,Width,Pause,Count,xrtAtomic32Load(&B.Calls,XMEMORY_ACQUIRE),Sum,CPUEnd-CPUStart,Before.Service.Bytes,Count*(sizeof(*B.Events)+3*sizeof(double))+(size_t)Jobs*(sizeof(*B.Data)+sizeof(*Keys)),After.BatchSubmissions,After.BatchedAttempts,(int)Before.Service.Waiter.Backend);Exit=0;
Done:
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(StatsOn)xrtMemStatsEnable(false);
#else
    (void)StatsOn;
#endif
    if(S){if(!xcronServiceClose(S,true,NULL) || xcronServiceWaitFor(S,30000,NULL)!=XWAIT_OK)return 3;
        if(Pending){xcronselectedreply R;if(xcronServiceCollectSelected(S,Pending,10000,&R,NULL)!=XWAIT_OK)return 3;if(!R.Code && R.Job){B.Data[Admitted].Id=R.Job;++Admitted;}xrtErrorFree(R.Error);}
        for(;;){xcronselectedservicecompletion D[64];size_t N;if(!xcronServiceDrainSelected(S,D,64,&N,NULL))return 3;for(size_t I=0;I<N;++I)xrtErrorFree(D[I].Completion.Error);if(!N)break;}
        for(;;){xcronselectedretired R;bool Found;if(!xcronServiceReapStoppedSelected(S,&R,&Found,NULL))return 3;if(!Found)break;bool Known=false;for(uint32 I=0;I<Admitted;++I)if(R.Id==B.Data[I].Id && R.Data==&B.Data[I]){Known=true;if(!Exit && (R.Info.Succeeded!=Periods || R.Info.Dispatched!=Periods || R.Info.Failed || R.Info.Cancelled || R.Info.SkippedRanges || R.Info.CoalescedRanges || R.Info.Policy.Fold!=XCRON_FOLD_ALL || R.Info.Policy.Missing!=XCRON_MISSING_REJECT))return 3;B.Data[I].Id=0;break;}if(!Known)return 3;++Retired;}
        if(Retired!=Admitted || !xcronServiceInspectSelected(S,&After,NULL) || After.Service.ActiveNativeTimers || After.Service.Failure.Code || After.Service.State!=XCRON_SERVICE_STOPPED || !xcronServiceDestroy(S,NULL))return 3;
    }
    if(Pool && (!xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,30000)!=XWAIT_OK || !xrtExecutorDestroy(Pool)))return 3;
    xcronZoneRelease(Zone);xrtFree(Samples);xrtFree(Keys);xrtFree(B.Events);xrtFree(B.Data);if(Exit)fprintf(stderr,"invalid fixed-arrival sample: callback/receipt/lifetime invariant failed\n");return Exit;
}
