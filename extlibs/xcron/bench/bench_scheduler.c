#include <xcron.h>
#include <xrt/memory.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static int schedulerCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    uint32 Count=Argc>1?(uint32)strtoul(Argv[1],NULL,10):200000,Jobs=Argc>2?(uint32)strtoul(Argv[2],NULL,10):1;
    if(!Count || Count>1000000 || !Jobs || Jobs>10000)return 1;
    xcronscheduler* S=NULL;xcronzone* Z=NULL;xcronexpression E;xcronrun* Runs=NULL;double* Samples=NULL;int Exit=2;
    xcronschedulerconfig C={Jobs,Jobs,Jobs*3+1,0,64*1024*1024};uint64 Sum=0;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&E,NULL) ||
        !xcronSchedulerCreate(&C,&S,NULL) || !(Runs=xrtMalloc((size_t)Jobs*sizeof(*Runs))) || !(Samples=xrtMalloc((size_t)Count*sizeof(*Samples))))goto Done;
    for(uint32 I=0;I<Jobs;++I){xcronjobconfig J={.Expression=E,.Zone=Z,.Begin=0,.End=INT64_MAX,.RunningLimit=1};xcronjobid Id;if(!xcronSchedulerAdd(S,&J,&Id,NULL))goto Done;}
    double Start=xrtTimer();
    for(uint32 I=0;I<Count;++I){xcronpoll P;double T=xrtTimer();if(!xcronSchedulerPoll(S,(xcronclock){(xtime)I*1000,(uint64)I*1000},Runs,Jobs,&P,NULL) || P.Count!=Jobs)goto Done;
        for(uint32 J=0;J<Jobs;++J){Sum+=(uint64)Runs[J].ScheduledAt;if(!xcronSchedulerComplete(S,Runs[J].Id,XCRON_RUN_SUCCEEDED,NULL))goto Done;}
        Samples[I]=(xrtTimer()-T)*1e6;
    }
    double Elapsed=xrtTimer()-Start;if(!(Elapsed>0))goto Done;qsort(Samples,Count,sizeof(*Samples),schedulerCompare);
    printf("dispatches_per_sec: %.3f\nbatch_p99_us: %.3f\nbatch_size: %u\nchecksum: %" PRIu64 "\n",(double)Count*Jobs/Elapsed,Samples[(Count-1)*99/100],Jobs,Sum);Exit=0;
Done:
    if(S){xcronSchedulerClose(S,NULL);xcronretired R;bool Found;do{Found=false;xcronSchedulerReap(S,&R,&Found,NULL);}while(Found);if(!xcronSchedulerDestroy(S,NULL))Exit=3;}
    xrtFree(Samples);xrtFree(Runs);xcronZoneRelease(Z);xrtClearError();return Exit;
}
