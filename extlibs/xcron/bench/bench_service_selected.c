/* Full selected service component, controlled64-due bursts. Measures owner,
 * recurrence, actual workers, nominal validation and receipt collection.
 * Fake arrivals and two shared callback atomics are part of this fixture. */
#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
typedef struct selectedservicebench {xtime First;uint64 Total;xatomic64 Wall,Mono,Calls,Sum;} selectedservicebench;
static bool selectedServiceClock(xcronclock* Clock,ptr Raw)
{selectedservicebench* C=Raw;*Clock=(xcronclock){(xtime)xrtAtomic64Load(&C->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&C->Mono,XMEMORY_ACQUIRE)};return true;}
static bool selectedServiceLabel(xtime When,const xdatetime* Local,size_t Candidates)
{xdatetime Expected;xrtTimeSplit(When,&Expected);Expected.Offset=0;Expected.IsDST=-1;return Candidates==1 && !memcmp(Local,&Expected,sizeof(Expected));}
static xcronrunresult selectedServiceRun(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    selectedservicebench* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);uint64 N=(uint64)(R->ScheduledAt-C->First)/1000+1;
    if(R->Data!=C || !L || R->ScheduledAt<C->First || (R->ScheduledAt-C->First)%1000 || N>C->Total || R->Through!=R->ScheduledAt || R->Flags || !selectedServiceLabel(R->ScheduledAt,&L->Local,L->Candidates))return XCRON_RUN_FAILED;
    xrtAtomic64FetchAdd(&C->Sum,N,XMEMORY_RELAXED);xrtAtomic64FetchAdd(&C->Calls,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;
}
static int selectedServiceCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Rounds=Argc>1?(uint32)strtoul(Argv[1],NULL,10):1000,Threads=Argc>2?(uint32)strtoul(Argv[2],NULL,10):4,Width=Argc>3?(uint32)strtoul(Argv[3],NULL,10):64;
    if(!Rounds || Rounds>1000000 || !Threads || Threads>16 || !Width || Width>64)return 1;
    int Exit=2;xexecutor* E=NULL;xcronservice* S=NULL;xcronzone* Z=NULL;double* Samples=NULL;xcronrequestid Pending=0;xcronjobid Job=0;selectedservicebench C={.First=XRT_TIME_UNIX_EPOCH,.Total=(uint64)Rounds*64};xrtAtomic64Init(&C.Wall,(uint64)(C.First-1000));xrtAtomic64Init(&C.Mono,100);
    xexecutorconfig PC={Threads,128,0};E=xrtExecutorCreate(&PC);Samples=xrtMalloc((size_t)Rounds*sizeof(*Samples));if(!E || !Samples)goto Done;
    xcronselectedserviceconfig SC={.Service={.Scheduler={1,64,256,0,1024*1024},.Executor=E,.Proc=selectedServiceRun,.Context=&C,.Clock=selectedServiceClock,.ClockContext=&C,.CommandLimit=8,.CompletionLimit=64,.BatchLimit=64,.CommandWorkLimit=8,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=5,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=4*1024*1024,.AdapterBytesLimit=1024*1024},.SubmissionBatchLimit=Width};
    xcronjobconfig J={.Begin=C.First,.End=C.First+(xtime)(C.Total-1)*1000,.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=64,.Data=&C};
    if(!xcronServiceCreateSelected(&SC,&S,NULL) || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL))goto Done;
    J.Zone=Z;if(!xcronServiceAddSelectedAsync(S,&J,(xcrondeliverypolicy){XCRON_FOLD_ALL,XCRON_MISSING_REJECT},&Pending,NULL))goto Done;
    xcronZoneRelease(Z);Z=NULL;xcronselectedreply R;if(xcronServiceCollectSelected(S,Pending,10000,&R,NULL)!=XWAIT_OK)goto Done;
    Pending=0;Job=R.Job;bool OK=!R.Code && Job && !R.Error;xrtErrorFree(R.Error);if(!OK)goto Done;
    {double Start=xrtTimer();uint64 Sum=0;
        for(uint32 Round=0;Round<Rounds;++Round){double T=xrtTimer();xtime Wall=C.First+((xtime)Round*64+63)*1000;xrtAtomic64Store(&C.Mono,100+((uint64)Round+1)*64000,XMEMORY_RELEASE);xrtAtomic64Store(&C.Wall,(uint64)Wall,XMEMORY_RELEASE);if(!xcronServiceWake(S,NULL))goto Done;
            uint32 Received=0;uint64 Seen=0;double Deadline=xrtTimer()+30;
            while(Received<64){xcronselectedservicecompletion D[64];size_t N;if(!xcronServiceDrainSelected(S,D,64-Received,&N,NULL) || xrtTimer()>Deadline)goto Done;bool Valid=true;
                for(size_t I=0;I<N;++I){xcronservicecompletion* V=&D[I].Completion;uint64 Ordinal=(uint64)(V->ScheduledAt-C.First)/1000,Index=Ordinal-(uint64)Round*64;bool Good=Index<64 && !(Seen&(UINT64_C(1)<<Index)) && V->Job==Job && V->Id && V->ScheduledAt>=C.First && (V->ScheduledAt-C.First)%1000==0 && V->Through==V->ScheduledAt && V->Observed.Wall==Wall && !V->Flags && V->Result==XCRON_RUN_SUCCEEDED && V->Code==XCRON_COMPLETION_NORMAL && V->Attempts==1 && !V->Error && selectedServiceLabel(V->ScheduledAt,&D[I].Local,D[I].Candidates);xrtErrorFree(V->Error);if(!Good){Valid=false;continue;}Seen|=UINT64_C(1)<<Index;Sum+=Ordinal+1;}
                Received+=(uint32)N;if(!Valid)goto Done;if(!N)xrtThreadYield();
            }
            if(Seen!=UINT64_MAX)goto Done;
            Samples[Round]=(xrtTimer()-T)*1e6;
        }
        double Elapsed=xrtTimer()-Start;if(!(Elapsed>0) || xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE)!=C.Total || xrtAtomic64Load(&C.Sum,XMEMORY_ACQUIRE)!=Sum || Sum!=C.Total*(C.Total+1)/2)goto Done;
        qsort(Samples,Rounds,sizeof(*Samples),selectedServiceCompare);xcronselectedservicestats Stats;if(!xcronServiceInspectSelected(S,&Stats,NULL) || Stats.Service.RejectedSubmissions || Stats.Service.SubmitAttempts!=C.Total || Stats.Service.Published!=C.Total || (Width==1 && Stats.BatchSubmissions))goto Done;
        printf("executions_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 64\nthreads: %u\nrounds: %u\nsubmission_batch_size: %u\ncallbacks: %" PRIu64 "\nchecksum: %" PRIu64 "\nnative_batch_calls: %" PRIu64 "\nbatched_attempts: %" PRIu64 "\n",(double)C.Total/Elapsed,Samples[(Rounds-1)/2],Samples[(Rounds-1)*95/100],Samples[(Rounds-1)*99/100],Samples[(Rounds-1)*999/1000],Threads,Rounds,Width,C.Total,Sum,Stats.BatchSubmissions,Stats.BatchedAttempts);Exit=0;
    }
Done:
    if(S){if(!xcronServiceClose(S,true,NULL) || xcronServiceWaitFor(S,30000,NULL)!=XWAIT_OK)return 3;if(Pending){xcronselectedreply V;if(xcronServiceCollectSelected(S,Pending,10000,&V,NULL)!=XWAIT_OK)return 3;xrtErrorFree(V.Error);}
        for(;;){xcronselectedservicecompletion D[64];size_t N;if(!xcronServiceDrainSelected(S,D,64,&N,NULL))return 3;for(size_t I=0;I<N;++I)xrtErrorFree(D[I].Completion.Error);if(!N)break;}
        for(;;){xcronselectedretired V;bool Found;if(!xcronServiceReapStoppedSelected(S,&V,&Found,NULL))return 3;if(!Found)break;if(V.Data!=&C)return 3;}if(!xcronServiceDestroy(S,NULL))return 3;
    }
    if(E && (!xrtExecutorClose(E) || xrtExecutorWaitFor(E,30000)!=XWAIT_OK || !xrtExecutorDestroy(E)))return 3;
    xcronZoneRelease(Z);xrtFree(Samples);return Exit;
}
