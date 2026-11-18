#include <xcron/service_checkpoint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int groupStartupCompare(const void* A,const void* B)
{
    double X=*(const double*)A,Y=*(const double*)B;
    return (X>Y)-(X<Y);
}
static xcronrunresult groupStartupRun(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{
    (void)Run;(void)Token;(void)Context;
    return XCRON_RUN_FAILED;
}

static bool groupBenchStop(xcronservice* S,uint32 Count)
{
    if(!xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return false;
    for(uint32 J=0;J<Count;++J){
        xcronretired R;
        bool Found;
        if(!xcronServiceReapStopped(S,&R,&Found,NULL) || !Found || R.Info.Dispatched)return false;
    }
    return xcronServiceDestroy(S,NULL);
}

static bool groupBenchCapture(const xcronserviceconfig* C,const xcronservicecheckpointgroup* Seed,
    xcronjobid* Ids,xcroncheckpoint* Records,size_t Iterations,double* Samples,double* Total,bool Quiesce)
{
    xcroncheckpointtarget* Targets=malloc((size_t)Seed->Count*sizeof(*Targets));
    xcronservice* S=NULL;
    if(!Targets || !xcronServiceCreateCheckpointedFromGroup(C,Seed,&S,Ids,NULL))return false;
    for(uint32 I=0;I<Seed->Count;++I)Targets[I]=(xcroncheckpointtarget){Ids[I],Seed->Jobs[I].Key};
    for(size_t I=0;I<Iterations;++I){
        xcronrequestid Q;
        xcronservicecheckpointreply R;
        double Begin=xrtTimer();
        bool Queued=Quiesce?xcronServiceQuiesceCheckpointGroupAsync(S,Targets,Seed->Count,Records,10000,&Q,NULL):
            xcronServiceSaveCheckpointGroupAsync(S,Targets,Seed->Count,Records,&Q,NULL);
        if(!Queued || xcronServiceCollectCheckpointGroup(S,Q,10000,&R,NULL)!=XWAIT_OK || R.Code ||
           R.Index!=XCRON_CHECKPOINT_NO_INDEX || R.Error)return false;
        double Delta=xrtTimer()-Begin;
        *Total+=Delta;
        Samples[I]=Delta*1000000;
    }
    free(Targets);
    return groupBenchStop(S,Seed->Count);
}

int main(int Argc,char** Argv)
{
    size_t Iterations=Argc>1?(size_t)strtoul(Argv[1],NULL,10):250;
    uint32 Count=Argc>2?(uint32)strtoul(Argv[2],NULL,10):64;
    bool Quiesce=Argc==4 && !strcmp(Argv[3],"quiesce");
    bool Capture=Quiesce || (Argc==4 && !strcmp(Argv[3],"capture"));
    if((Argc>3 && !Capture) || !Iterations || Iterations>100000 || !Count || Count>1024)return 2;
    xcronservicecheckpointjob* Jobs=malloc((size_t)Count*sizeof(*Jobs));
    xcroncheckpoint* Records=malloc((size_t)Count*sizeof(*Records));
    xcronjobid* Ids=malloc((size_t)Count*sizeof(*Ids));
    double* Samples=malloc(Iterations*sizeof(*Samples));
    if(!Jobs || !Records || !Ids || !Samples)return 3;
    const xcronservicecheckpointgroup Seed={Jobs,Records,Count};
    xcronjobconfig Job={.Begin=1000,.End=60000,.RunningLimit=1};
    xcronscheduler* Pull=NULL;
    xcronjobid Original;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Job.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL) ||
       !xcronSchedulerCreate(&(xcronschedulerconfig){1,1,16,0,65536},&Pull,NULL) ||
       !xcronSchedulerAdd(Pull,&Job,&Original,NULL))return 3;
    for(uint32 I=0;I<Count;++I){
        Jobs[I]=(xcronservicecheckpointjob){Job,{I+1,1}};
        if(!xcronSchedulerSaveCheckpoint(Pull,Original,Jobs[I].Key,&Records[I],NULL))return 3;
    }
    xcronretired Free;
    bool Found;
    if(!xcronSchedulerClose(Pull,NULL) || !xcronSchedulerReap(Pull,&Free,&Found,NULL) ||
       !Found || !xcronSchedulerDestroy(Pull,NULL))return 3;
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
    if(!Pool)return 3;
    const xcronserviceconfig C={
        .Scheduler={Count,1,16,0,2097152},.Executor=Pool,.Proc=groupStartupRun,
        .CommandLimit=1,.CompletionLimit=1,.BatchLimit=1,.CommandWorkLimit=Capture?Count:1,
        .ResampleMilliseconds=20,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=4194304,.AdapterBytesLimit=65536
    };
    double Total=0;
    if(Capture){
        if(!groupBenchCapture(&C,&Seed,Ids,Records,Iterations,Samples,&Total,Quiesce))return 4;
    }else{
        for(size_t I=0;I<Iterations;++I){
            xcronservice* S=NULL;
            double Begin=xrtTimer();
            if(!xcronServiceCreateCheckpointedFromGroup(&C,&Seed,&S,Ids,NULL))return 4;
            double Delta=xrtTimer()-Begin;
            Total+=Delta;
            Samples[I]=Delta*1000000;
            if(!Ids[0] || !Ids[Count-1] || !groupBenchStop(S,Count))return 4;
        }
    }
    if(Total<=0 || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xrtExecutorDestroy(Pool))return 5;
    xcronZoneRelease(Job.Zone);
    qsort(Samples,Iterations,sizeof(*Samples),groupStartupCompare);
    cstr Operation=Capture?"capture":"start",Amount=Capture?"captured":"restored";
    printf("%ss_per_sec: %.3f\n%s_per_sec: %.3f\n%s_p50_us: %.3f\n%s_p95_us: %.3f\n%s_p99_us: %.3f\n",
           Operation,Iterations/Total,Amount,(double)Iterations*Count/Total,Operation,Samples[(Iterations+1)/2-1],
           Operation,Samples[(Iterations*95+99)/100-1],Operation,Samples[(Iterations*99+99)/100-1]);
    free(Samples);
    free(Ids);
    free(Records);
    free(Jobs);
    return 0;
}
