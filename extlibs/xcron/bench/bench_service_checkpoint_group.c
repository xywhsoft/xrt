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

typedef enum groupbenchfilemode {GROUP_BENCH_COMMIT,GROUP_BENCH_INFO,GROUP_BENCH_PERIODIC_INFO} groupbenchfilemode;

static bool groupBenchInfoSamples(xcronservice* S,xcronjobid Job,size_t Iterations,double* Samples,double* Total)
{
    for(size_t I=0;I<Iterations;++I){
        xcronrequestid Ticket;
        xcronreply Reply;
        double Begin=xrtTimer();
        if(!xcronServiceInfoAsync(S,Job,&Ticket,NULL) ||
           xcronServiceCollect(S,Ticket,10000,&Reply,NULL)!=XWAIT_OK || Reply.Code ||
           Reply.Error || Reply.Job!=Job || !Reply.Info.HasNext)return false;
        double Delta=xrtTimer()-Begin;
        *Total+=Delta;
        Samples[I]=Delta*1000000;
    }
    return true;
}

static bool groupBenchFile(const xcronserviceconfig* C,const xcronservicecheckpointgroup* Seed,
    xcronjobid* Ids,xcroncheckpoint* Records,size_t Iterations,double* Samples,double* Total,groupbenchfilemode Mode)
{
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    char Name[96];
    snprintf(Name,sizeof(Name),"xcron-live-bench-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    xroot Root=xrtRootOpen(Base);
    xexecutor* Storage=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
    xcroncheckpointgroup* Store=NULL;
    xcroncheckpointwriter* Writer=NULL;
    xcronservice* S=NULL;
    xcroncheckpointtarget* Targets=malloc((size_t)Seed->Count*sizeof(*Targets));
    if(!Root || !Storage || !Targets || xcronCheckpointGroupOpen(Root,Name,102,Seed->Count,true,&Store)!=XCRON_CHECKPOINT_STORE_OK ||
       !xcronCheckpointWriterCreate(Store,Storage,&Writer) ||
       !xcronServiceCreateCheckpointedFromGroup(C,Seed,&S,Ids,NULL))return false;
    for(uint32 I=0;I<Seed->Count;++I)Targets[I]=(xcroncheckpointtarget){Ids[I],Seed->Jobs[I].Key};
    xcronservicecheckpointcommit Commit={Writer,Targets,Records,0,10000,Seed->Count};
    uint64 Generation=0;
    if(Mode==GROUP_BENCH_COMMIT){
        for(size_t I=0;I<Iterations;++I){
            xcronrequestid Ticket;
            xcronservicecheckpointcommitreply Reply;
            Commit.ExpectedGeneration=I;
            double Begin=xrtTimer();
            if(!xcronServiceCommitCheckpointGroupAsync(S,&Commit,&Ticket,NULL))return false;
            xwaitresult Wait;
            do Wait=xcronServiceCollectCheckpointCommit(S,Ticket,10000,&Reply,NULL);
            while(Wait==XWAIT_TIMEOUT);
            if(Wait!=XWAIT_OK)abort();
            if(Reply.Command.Code || Reply.Command.Error || !Reply.StorageCompleted ||
               Reply.Store!=XCRON_CHECKPOINT_STORE_OK || Reply.Generation!=I+1)return false;
            double Delta=xrtTimer()-Begin;
            *Total+=Delta;
            Samples[I]=Delta*1000000;
        }
        Generation=Iterations;
    }else{
        xcronrequestid Ticket=0;
        const xcronservicecheckpointperiodic Periodic={Commit,4};
        if(Mode==GROUP_BENCH_PERIODIC_INFO && !xcronServiceStartCheckpointPeriodicAsync(S,&Periodic,&Ticket,NULL))return false;
        bool Measured=groupBenchInfoSamples(S,Ids[0],Iterations,Samples,Total);
        if(Ticket){
            if(!xcronServiceStopCheckpointPeriodic(S,Ticket,NULL))abort();
            xcronservicecheckpointperiodicreport Report;
            xwaitresult Wait;
            do Wait=xcronServiceCollectCheckpointPeriodic(S,Ticket,10000,&Report,NULL);
            while(Wait==XWAIT_TIMEOUT);
            if(Wait!=XWAIT_OK)abort();
            Generation=Report.Generation;
            xrtErrorFree(Report.Last.Command.Error);
            if(!Generation || Report.Committed!=Generation || Report.Last.Store==XCRON_CHECKPOINT_STORE_UNCERTAIN)return false;
        }
        if(!Measured)return false;
    }
    free(Targets);
    xcroncheckpointgroupview View;
    return groupBenchStop(S,Seed->Count) && xcronCheckpointWriterDestroy(Writer)==XCRON_CHECKPOINT_STORE_OK &&
           xrtExecutorDestroy(Storage) && xcronCheckpointGroupOpen(Root,Name,102,Seed->Count,false,&Store)==XCRON_CHECKPOINT_STORE_OK &&
           xcronCheckpointGroupRead(Store,&View)==(Generation?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY) &&
           View.Info.Generation==Generation && View.Count==(Generation?Seed->Count:0) &&
           (!Generation || !memcmp(Records,View.Records,(size_t)View.Count*sizeof(*Records))) &&
           xcronCheckpointGroupClose(Store)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,Name) && xrtRootClose(Root);
}

int main(int Argc,char** Argv)
{
    size_t Iterations=Argc>1?(size_t)strtoul(Argv[1],NULL,10):250;
    uint32 Count=Argc>2?(uint32)strtoul(Argv[2],NULL,10):64;
    bool Quiesce=Argc==4 && !strcmp(Argv[3],"quiesce");
    bool Durable=Argc==4 && !strcmp(Argv[3],"commit");
    bool Periodic=Argc==4 && !strcmp(Argv[3],"periodic-info");
    bool Info=Periodic || (Argc==4 && !strcmp(Argv[3],"info"));
    bool Capture=Durable || Info || Quiesce || (Argc==4 && !strcmp(Argv[3],"capture"));
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
        .CommandLimit=Info?2:1,.CompletionLimit=1,.BatchLimit=1,.CommandWorkLimit=Capture?Count:1,
        .ResampleMilliseconds=20,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=4194304,.AdapterBytesLimit=65536
    };
    double Total=0;
    if(Durable || Info){
        groupbenchfilemode Mode=Durable?GROUP_BENCH_COMMIT:Periodic?GROUP_BENCH_PERIODIC_INFO:GROUP_BENCH_INFO;
        if(!groupBenchFile(&C,&Seed,Ids,Records,Iterations,Samples,&Total,Mode))return 4;
    }else if(Capture){
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
    cstr Operation=Info?"info":Durable?"commit":Capture?"capture":"start";
    cstr Amount=Info?"info":Durable?"committed":Capture?"captured":"restored";
    double Items=Info?(double)Iterations:(double)Iterations*Count;
    printf("%ss_per_sec: %.3f\n",Operation,Iterations/Total);
    if(!Info)printf("%s_per_sec: %.3f\n",Amount,Items/Total);
    printf("%s_p50_us: %.3f\n%s_p95_us: %.3f\n%s_p99_us: %.3f\n",Operation,Samples[(Iterations+1)/2-1],
           Operation,Samples[(Iterations*95+99)/100-1],Operation,Samples[(Iterations*99+99)/100-1]);
    free(Samples);
    free(Ids);
    free(Records);
    free(Jobs);
    return 0;
}
