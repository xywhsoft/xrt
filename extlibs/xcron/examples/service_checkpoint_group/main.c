#include <xcron/service_checkpoint.h>
#include <xcron/checkpoint_writer.h>
#include <xrt/atomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DEMO_JOBS=2 };

static bool groupDemoClock(xcronclock* Out,ptr Raw)
{
    *Out=(xcronclock){(xtime)xrtAtomic64Load((xatomic64*)Raw,XMEMORY_ACQUIRE),0};
    return true;
}

static xcronrunresult groupDemoRun(const xcronrun* Run,const xcronexecution* Token,ptr Context)
{
    (void)Run;(void)Token;(void)Context;
    return XCRON_RUN_SUCCEEDED;
}

static bool groupDemoInitial(xcroncheckpointgroup* Store,const xcronservicecheckpointjob* Jobs)
{
    xcronscheduler* S=NULL;
    if(!xcronSchedulerCreate(&(xcronschedulerconfig){DEMO_JOBS,1,16,0,65536},&S,NULL))return false;
    xcroncheckpoint Records[DEMO_JOBS];
    for(unsigned I=0;I<DEMO_JOBS;++I){
        xcronjobid Id;
        if(!xcronSchedulerAdd(S,&Jobs[I].Config,&Id,NULL) ||
           !xcronSchedulerSaveCheckpoint(S,Id,Jobs[I].Key,&Records[I],NULL))return false;
    }
    if(xcronCheckpointGroupCommit(Store,0,Records,DEMO_JOBS)!=XCRON_CHECKPOINT_STORE_OK ||
       !xcronSchedulerClose(S,NULL))return false;
    for(unsigned I=0;I<DEMO_JOBS;++I){
        xcronretired R;
        bool Found;
        if(!xcronSchedulerReap(S,&R,&Found,NULL) || !Found)return false;
    }
    return xcronSchedulerDestroy(S,NULL);
}

static bool groupDemoPaused(xcronservice* S,xcronjobid Id,bool Paused)
{
    xcronrequestid Q;
    xcronreply R;
    if(!xcronServiceSetPausedAsync(S,Id,Paused,&Q,NULL) ||
       xcronServiceCollect(S,Q,10000,&R,NULL)!=XWAIT_OK)return false;
    xrtErrorFree(R.Error);
    return !R.Code && R.Job==Id;
}

static bool groupDemoInfo(xcronservice* S,xcronjobid Id,xcronjobinfo* Info)
{
    xcronrequestid Q;
    xcronreply R;
    if(!xcronServiceInfoAsync(S,Id,&Q,NULL) || xcronServiceCollect(S,Q,10000,&R,NULL)!=XWAIT_OK)return false;
    xrtErrorFree(R.Error);
    *Info=R.Info;
    return !R.Code && R.Info.HasNext;
}

static bool groupDemoAdvance(xcronservice* S,xcronjobid Id)
{
    if(!groupDemoPaused(S,Id,false) || xcronServiceWaitReadable(S,10000,NULL)!=XWAIT_OK)return false;
    xcronservicecompletion Done;
    size_t Count;
    if(!xcronServiceDrain(S,&Done,1,&Count,NULL) || Count!=1)return false;
    xrtErrorFree(Done.Error);
    return Done.Job==Id && Done.Result==XCRON_RUN_SUCCEEDED;
}

static bool groupDemoCommit(xcroncheckpointgroup* Store,xcronservice* S,const xcroncheckpointtarget* Targets,
    uint64* Generation)
{
    xexecutor* Storage=xrtExecutorCreate(&(xexecutorconfig){1,2,0});
    xcroncheckpointwriter* Writer=NULL;
    bool Opened=Storage && xcronCheckpointWriterCreate(Store,Storage,&Writer);
    xcroncheckpoint Scratch[DEMO_JOBS];
    const xcronservicecheckpointcommit Commit={Writer,Targets,Scratch,*Generation,10000,DEMO_JOBS};
    xcronrequestid Ticket=0;
    bool Submitted=Opened && xcronServiceCommitCheckpointGroupAsync(S,&Commit,&Ticket,NULL);
    xcronservicecheckpointcommitreply Reply={0};
    if(Submitted){
        xwaitresult Collected;
        do Collected=xcronServiceCollectCheckpointCommit(S,Ticket,10000,&Reply,NULL);
        while(Collected==XWAIT_TIMEOUT);
        if(Collected!=XWAIT_OK)abort(); /* The owner still borrows both arrays. */
    }
    bool Committed=Submitted && Reply.Command.Code==XCRON_SERVICE_OK &&
        Reply.StorageCompleted && Reply.Store==XCRON_CHECKPOINT_STORE_OK;
    if(Committed)*Generation=Reply.Generation;
    xrtErrorFree(Reply.Command.Error);
    xcroncheckpointstorestatus Closed=Opened?xcronCheckpointWriterDestroy(Writer):xcronCheckpointGroupClose(Store);
    bool Released=Storage && xrtExecutorDestroy(Storage);
    return Committed && Closed==XCRON_CHECKPOINT_STORE_OK && Released;
}

static int groupDemo(xroot Root,bool Create,bool Advance)
{
    xcroncheckpointgroup* Store=NULL;
    if(xcronCheckpointGroupOpen(Root,"group.xcg",940,32,Create,&Store)!=XCRON_CHECKPOINT_STORE_OK)return 3;
    xcronjobconfig Job={.Begin=1000,.End=60000,.RunningLimit=1};
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Job.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL))return 4;
    const xcronservicecheckpointjob Jobs[DEMO_JOBS]={{Job,{1,1}},{Job,{2,1}}};
    xcroncheckpointgroupview View;
    if((Create && !groupDemoInitial(Store,Jobs)) || xcronCheckpointGroupRead(Store,&View)!=XCRON_CHECKPOINT_STORE_OK ||
       View.Count!=DEMO_JOBS)return 5;

    xatomic64 Clock;
    xrtAtomic64Init(&Clock,60000);
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    const xcronserviceconfig Config={
        .Scheduler={DEMO_JOBS,1,16,0,65536},.Executor=Pool,.Proc=groupDemoRun,.Clock=groupDemoClock,.ClockContext=&Clock,
        .CommandLimit=2,.CompletionLimit=1,.BatchLimit=1,.CommandWorkLimit=2,.ResampleMilliseconds=2,
        .RetryIntervalMilliseconds=1,.RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronservice* S=NULL;
    xcronjobid Ids[DEMO_JOBS];
    const xcronservicecheckpointgroup Seed={Jobs,View.Records,View.Count};
    if(!Pool || !xcronServiceCreateCheckpointedFromGroup(&Config,&Seed,&S,Ids,NULL))return 6;
    xcronZoneRelease(Job.Zone);
    if(xcronServiceWaitReadable(S,25,NULL)!=XWAIT_TIMEOUT)return 6;
    uint64 Generation=View.Info.Generation;
    if(Advance){
        xcroncheckpointtarget Targets[DEMO_JOBS];
        for(unsigned I=0;I<DEMO_JOBS;++I){
            xcronjobinfo Info;
            if(!groupDemoInfo(S,Ids[I],&Info))return 7;
            xrtAtomic64Store(&Clock,(uint64)Info.Next,XMEMORY_RELEASE);
            if(!groupDemoAdvance(S,Ids[I]))return 7;
            Targets[I]=(xcroncheckpointtarget){Ids[I],Jobs[I].Key};
        }
        if(!groupDemoCommit(Store,S,Targets,&Generation))return 8;
    }else if(xcronCheckpointGroupClose(Store)!=XCRON_CHECKPOINT_STORE_OK)return 8;
    xcronjobinfo A,B;
    if(!groupDemoInfo(S,Ids[0],&A) || !groupDemoInfo(S,Ids[1],&B))return 9;
    printf("GEN %llu NEXT %lld %lld SUCCEEDED %llu %llu\n",(unsigned long long)Generation,
           (long long)A.Next,(long long)B.Next,(unsigned long long)A.Succeeded,(unsigned long long)B.Succeeded);
    if(!xcronServiceClose(S,false,NULL) || xcronServiceWaitFor(S,10000,NULL)!=XWAIT_OK)return 9;
    for(unsigned I=0;I<DEMO_JOBS;++I){
        xcronretired R;
        bool Found;
        if(!xcronServiceReapStopped(S,&R,&Found,NULL) || !Found)return 9;
    }
    if(!xcronServiceDestroy(S,NULL) || !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK ||
       !xrtExecutorDestroy(Pool))return 9;
    return 0;
}

int main(int Argc,char** Argv)
{
    if(Argc==3 && (!strcmp(Argv[1],"show") || !strcmp(Argv[1],"advance"))){
        xroot Root=xrtRootOpen(Argv[2]);
        if(!Root)return 2;
        int Code=groupDemo(Root,false,!strcmp(Argv[1],"advance"));
        return xrtRootClose(Root)?Code:9;
    }
    if(Argc!=1)return 2;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#endif
    if(!Base || !*Base)Base=".";
    char Name[96],File[128];
    snprintf(Name,sizeof(Name),"xcron-live-group-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    snprintf(File,sizeof(File),"%s/group.xcg",Name);
    xroot Parent=xrtRootOpen(Base);
    if(!Parent || !xrtRootDirCreate(Parent,Name,0700))return 3;
    xroot Root=xrtRootOpenIn(Parent,Name);
    if(!Root)return 3;
    int Code=groupDemo(Root,true,true);
    if(!Code)Code=groupDemo(Root,false,false);
    if(!xrtRootClose(Root) || !xrtRootRemove(Parent,File) || !xrtRootRemove(Parent,Name) || !xrtRootClose(Parent))return 9;
    return Code;
}
