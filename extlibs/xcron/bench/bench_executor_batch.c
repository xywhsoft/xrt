/* Matched ready-array scalar/batch adapter fixture: actual worker view reads,
 * immutable payload validation and physical receipt collection. Synthetic
 * positive leases isolate this adapter; no live scheduling/fixed-arrival claim. */
#include <xcron.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
typedef struct selectedexecbench {xcronexecutionlabel Label;xtime When;xatomic64 Calls,Sum;} selectedexecbench;
static uint64 selectedExecDigest(const xcronrun* R,const xcronexecutionlabel* L)
{return R->Id+R->Job+(uint64)R->ScheduledAt+(uint64)R->Through+(uint64)R->Observed.Wall+R->Observed.MonotonicMilliseconds+L->Candidates+(uint64)L->Local.Year+(uint64)(L->Local.Month+L->Local.Day+L->Local.Hour+L->Local.Minute+L->Local.Second+L->Local.YearDay+L->Local.Weekday);}
static bool selectedExecValid(const xcronrun* R,const xcronexecutionlabel* L,const selectedexecbench* C)
{return L && L->Candidates==C->Label.Candidates && !memcmp(&L->Local,&C->Label.Local,sizeof(L->Local)) && R->Job==1+(R->Id-1)%64 && R->Data==C && R->ScheduledAt==C->When && R->Through==C->When && R->Observed.Wall==C->When && R->Observed.MonotonicMilliseconds==R->Id && !R->Flags;}
static xcronrunresult selectedExecProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{selectedexecbench* C=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);if(!selectedExecValid(R,L,C))return XCRON_RUN_FAILED;xrtAtomic64FetchAdd(&C->Sum,selectedExecDigest(R,L),XMEMORY_RELAXED);xrtAtomic64FetchAdd(&C->Calls,1,XMEMORY_RELEASE);return XCRON_RUN_SUCCEEDED;}
static int selectedExecCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Rounds=Argc>1?(uint32)strtoul(Argv[1],NULL,10):1000,Threads=Argc>2?(uint32)strtoul(Argv[2],NULL,10):4;const uint32 Batch=64;uint32 SubmitWidth=Argc>3?(uint32)strtoul(Argv[3],NULL,10):64;if(!Rounds || Rounds>1000000 || !Threads || Threads>16 || !SubmitWidth || SubmitWidth>64 || 64%SubmitWidth)return 1;
    selectedexecbench C={0};xexecutor* E=NULL;xcronexecutor* B=NULL;double* Samples=NULL;uint64 Sum=0;int Exit=2;xtime Local;
    if(!xrtDateTime(2024,11,3,6,30,0,0,&C.When) || !xrtDateTime(2024,11,3,1,30,0,0,&Local))goto Done;
    xrtTimeSplit(Local,&C.Label.Local);C.Label.Local.Offset=0;C.Label.Local.IsDST=-1;C.Label.Candidates=2;
    xexecutorconfig EC={Threads,Batch*2,0};E=xrtExecutorCreate(&EC);xcronexecutorconfig BC={E,selectedExecProc,&C,NULL,NULL,Batch,0,1024*1024};if(!E || !xcronExecutorCreateSelected(&BC,&B,NULL) || !(Samples=xrtMalloc((size_t)Rounds*sizeof(*Samples))))goto Done;
    double Start=xrtTimer();
    for(uint32 Round=0;Round<Rounds;++Round){double T=xrtTimer();
        xcronselectedrun Runs[64];
        for(uint32 J=0;J<Batch;++J){uint64 Id=(uint64)Round*Batch+J+1;Runs[J]=(xcronselectedrun){.Run={.Id=Id,.Job=J+1,.Data=&C,.ScheduledAt=C.When,.Through=C.When,.Observed={C.When,Id}},.Local=C.Label.Local,.Candidates=2};}
        for(uint32 J=0;J<Batch;J+=SubmitWidth){bool OK=SubmitWidth==1?xcronExecutorSubmitSelected(B,&Runs[J],NULL):xcronExecutorSubmitBatchSelected(B,&Runs[J],SubmitWidth,NULL);if(!OK)goto Done;}
        uint32 Received=0;uint64 Seen=0;double Deadline=xrtTimer()+30;
        while(Received<Batch){xcronselectedcompletion D[64];size_t N;if(!xcronExecutorDrainSelected(B,D,Batch-Received,&N,NULL) || xrtTimer()>Deadline)goto Done;
            bool ValidBatch=true;
            for(size_t J=0;J<N;++J){xcronexecutionlabel L={D[J].Run.Local,D[J].Run.Candidates};uint64 Index=D[J].Run.Run.Id-(uint64)Round*Batch-1;bool Valid=Index<64 && !(Seen&(UINT64_C(1)<<Index)) && selectedExecValid(&D[J].Run.Run,&L,&C) && D[J].Result==XCRON_RUN_SUCCEEDED && D[J].Code==XCRON_EXECUTOR_OK && !D[J].Error;xrtErrorFree(D[J].Error);if(!Valid){ValidBatch=false;continue;}Seen|=UINT64_C(1)<<Index;Sum+=selectedExecDigest(&D[J].Run.Run,&L);}
            Received+=(uint32)N;if(!ValidBatch)goto Done;if(!N)xrtThreadYield();
        }if(Seen!=UINT64_MAX)goto Done;Samples[Round]=(xrtTimer()-T)*1e6;
    }
    {double Elapsed=xrtTimer()-Start;if(!(Elapsed>0) || xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE)!=(uint64)Rounds*Batch || xrtAtomic64Load(&C.Sum,XMEMORY_ACQUIRE)!=Sum)goto Done;qsort(Samples,Rounds,sizeof(*Samples),selectedExecCompare);
        printf("executions_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: 64\nthreads: %u\nrounds: %u\nsubmission_batch_size: %u\ncallbacks: %" PRIu64 "\nchecksum: %" PRIu64 "\n",(double)Rounds*Batch/Elapsed,Samples[(Rounds-1)/2],Samples[(Rounds-1)*95/100],Samples[(Rounds-1)*99/100],Samples[(Rounds-1)*999/1000],Threads,Rounds,SubmitWidth,xrtAtomic64Load(&C.Calls,XMEMORY_ACQUIRE),Sum);Exit=0;}
Done:
    if(B)xcronExecutorClose(B,true,NULL);
    if(E && (!xrtExecutorClose(E) || xrtExecutorWaitFor(E,30000)!=XWAIT_OK))return 3;
    if(B){for(;;){xcronselectedcompletion D[64];size_t N=0;if(!xcronExecutorDrainSelected(B,D,64,&N,NULL))return 3;for(size_t J=0;J<N;++J)xrtErrorFree(D[J].Error);if(!N)break;}if(!xcronExecutorDestroy(B,NULL))return 3;}
    if(E && !xrtExecutorDestroy(E))return 3;
    xrtFree(Samples);return Exit;
}
