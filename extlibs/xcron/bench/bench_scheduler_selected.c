/* End-to-end owner pull, complete selected-label resolution and physical
 * completion. Caller-owned batches include nominal metadata and cold Add. */
#include <xcron.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
static int selectedBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Count=Argc>1?(uint32)strtoul(Argv[1],NULL,10):200000,Jobs=Argc>2?(uint32)strtoul(Argv[2],NULL,10):1;
    if(!Count || Count>1000000 || !Jobs || Jobs>10000)return 1;
    xcronzone* Z=NULL;xcronscheduler* S=NULL;xcronselectedrun* Runs=NULL;double* Samples=NULL;xcronexpression E;xcronschedulerconfig C={Jobs,Jobs,Jobs*8+1,0,64*1024*1024};uint64 Sum=0,Segments=0,Months=0,Years=0,Checks=0,Offsets=0,Actions=0;size_t Active=0,Completed=0;int Exit=2;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&E,NULL) || !xcronSchedulerCreateSelected(&C,&S,NULL) || !(Runs=xrtMalloc((size_t)Jobs*sizeof(*Runs))) || !(Samples=xrtMalloc((size_t)Count*sizeof(*Samples))))goto Done;
    double Start=xrtTimer();
    for(uint32 I=0;I<Jobs;++I){xcronjobconfig J={.Expression=E,.Zone=Z,.Begin=0,.End=INT64_MAX,.RunningLimit=1};xcronjobid Id;if(!xcronSchedulerAddSelected(S,&J,(xcrondeliverypolicy){0,0},&Id,NULL))goto Done;}
    for(uint32 I=0;I<Count;++I){size_t Done=0;Active=Completed=0;double T=xrtTimer();unsigned Turns=0;
        do{xcronselectedpoll P;if(!xcronSchedulerPollSelected(S,(xcronclock){(xtime)I*1000,(uint64)I*1000},(xcronoccurrencebudget){C.WorkLimit,C.WorkLimit,C.WorkLimit,C.WorkLimit,C.WorkLimit},Runs+Done,Jobs-Done,&P,NULL))goto Done;
            Done+=P.Poll.Count;Active=Done;Segments+=P.Used.Segments;Months+=P.Used.Months;Years+=P.Used.RuleYears;Checks+=P.Used.Checks;Offsets+=P.Used.Offsets;Actions+=P.Poll.Work;
            if(++Turns>Jobs*16)goto Done;
        }while(Done<Jobs);
        for(uint32 J=0;J<Jobs;++J){xcronselectedrun* R=&Runs[J];xdatetime* D=&R->Local;int Day=(int)(I/86400)+1;
            if(R->Run.ScheduledAt!=(xtime)I*1000 || R->Run.Through!=R->Run.ScheduledAt || R->Run.Flags || R->Run.Observed.Wall!=R->Run.ScheduledAt || R->Run.Observed.MonotonicMilliseconds!=(uint64)I*1000 || R->Candidates!=1 || D->Year!=1 || D->Month!=1 || D->Day!=Day || D->YearDay!=Day || D->Weekday!=Day%7 || D->Hour!=(int)(I/3600%24) || D->Minute!=(int)(I/60%60) || D->Second!=(int)(I%60) || D->Millisecond || D->Offset || D->IsDST!=-1 || !xcronSchedulerComplete(S,R->Run.Id,XCRON_RUN_SUCCEEDED,NULL))goto Done;
            ++Completed;Sum+=(uint64)R->Run.ScheduledAt+(uint64)R->Run.Through+(uint64)R->Run.Observed.Wall+R->Run.Observed.MonotonicMilliseconds+R->Candidates+(uint64)D->Year+(uint64)(D->Month+D->Day+D->YearDay+D->Weekday+D->Hour+D->Minute+D->Second);
        }
        Samples[I]=(xrtTimer()-T)*1e6;
    }
    {double Elapsed=xrtTimer()-Start;xcronschedulerstats Stats;if(!(Elapsed>0) || !xcronSchedulerInspect(S,&Stats,NULL) || Stats.Dispatched!=(uint64)Count*Jobs || Stats.Completed!=(uint64)Count*Jobs || Stats.Faults || Stats.Running)goto Done;qsort(Samples,Count,sizeof(*Samples),selectedBenchCompare);
        printf("dispatches_per_sec: %.3f\nbatch_p50_us: %.3f\nbatch_p95_us: %.3f\nbatch_p99_us: %.3f\nbatch_p999_us: %.3f\nbatch_size: %u\niterations: %u\nchecksum: %" PRIu64 "\nsegments: %" PRIu64 "\nmonths: %" PRIu64 "\nrule_years: %" PRIu64 "\nchecks: %" PRIu64 "\noffsets: %" PRIu64 "\nactions: %" PRIu64 "\n",(double)Count*Jobs/Elapsed,Samples[(Count-1)/2],Samples[(Count-1)*95/100],Samples[(Count-1)*99/100],Samples[(Count-1)*999/1000],Jobs,Count,Sum,Segments,Months,Years,Checks,Offsets,Actions);Exit=0;}
Done:
    if(S){xcronSchedulerClose(S,NULL);for(size_t N=Completed;N<Active;++N)xcronSchedulerComplete(S,Runs[N].Run.Id,XCRON_RUN_CANCELLED,NULL);xcronselectedretired R;bool Found;do{Found=false;xcronSchedulerReapSelected(S,&R,&Found,NULL);}while(Found);if(!xcronSchedulerDestroy(S,NULL))Exit=3;}
    xrtFree(Samples);xrtFree(Runs);xcronZoneRelease(Z);return Exit;
}
