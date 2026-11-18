#include <xcron/service_checkpoint.h>
#include <xcron/checkpoint_store.h>
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct checkpointdemo {
    xatomic64 Wall;
    xevent Started,Release;
} checkpointdemo;

static bool checkpointDemoClock(xcronclock* Clock,ptr Raw)
{
    checkpointdemo* Demo=Raw;
    *Clock=(xcronclock){(xtime)xrtAtomic64Load(&Demo->Wall,XMEMORY_ACQUIRE),0};
    return true;
}

static xcronrunresult checkpointDemoRun(const xcronrun* Run,const xcronexecution* Token,ptr Raw)
{
    (void)Token;
    checkpointdemo* Demo=Raw;
    if(Run->Data!=Demo || !xrtEventSet(&Demo->Started))return XCRON_RUN_FAILED;
    return xrtEventWaitFor(&Demo->Release,10000)==XWAIT_OK?XCRON_RUN_SUCCEEDED:XCRON_RUN_FAILED;
}

static bool checkpointDemoCollect(xcronservice* Service,xcronrequestid Request,xcronreply* Reply,xcroncheckpoint* Record)
{
    if(xcronServiceCollectCheckpoint(Service,Request,10000,Reply,Record,NULL)!=XWAIT_OK)return false;
    xrtErrorFree(Reply->Error);
    return Reply->Code==XCRON_SERVICE_OK;
}

static bool checkpointDemoPause(xcronservice* Service,xcronjobid Job,bool Paused)
{
    xcronrequestid Request;
    xcronreply Reply;
    if(!xcronServiceSetPausedAsync(Service,Job,Paused,&Request,NULL) ||
       xcronServiceCollect(Service,Request,10000,&Reply,NULL)!=XWAIT_OK)return false;
    xrtErrorFree(Reply.Error);
    return Reply.Code==XCRON_SERVICE_OK && Reply.Job==Job;
}

static int checkpointDemoPhase(xroot Root,cstr Mode)
{
    bool Create=!strcmp(Mode,"init"),Interrupt=!strcmp(Mode,"interrupt"),Advance=!strcmp(Mode,"advance");
    if(!Create && !Interrupt && !Advance && strcmp(Mode,"observe"))return 2;

    const xcroncheckpointkey Key={199,1};
    xcroncheckpointstore* Store=NULL;
    if(xcronCheckpointStoreOpen(Root,"job.xcs",Key,Create,&Store)!=XCRON_CHECKPOINT_STORE_OK)return 3;
    checkpointdemo Demo={0};
    xrtAtomic64Init(&Demo.Wall,Create?0:4000);
    if(!xrtEventInit(&Demo.Started,true,false) || !xrtEventInit(&Demo.Release,true,!Interrupt))return 4;
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    xcronserviceconfig Limits={
        .Scheduler={2,1,16,0,65536},.Executor=Pool,.Proc=checkpointDemoRun,.Context=&Demo,
        .Clock=checkpointDemoClock,.ClockContext=&Demo,.CommandLimit=4,.CompletionLimit=1,
        .BatchLimit=1,.CommandWorkLimit=4,.ResampleMilliseconds=10,.RetryIntervalMilliseconds=1,
        .RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronjobconfig Job={.Begin=1000,.End=3000,.RunningLimit=1,.Data=&Demo};
    xcronservice* Service=NULL;
    if(!Pool || !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Job.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Job.Expression,NULL) ||
       !xcronServiceCreateCheckpointed(&Limits,&Service,NULL))return 4;
    xcroncheckpoint Record;
    xcroncheckpointstoreinfo Stored;
    xcronrequestid Request;
    xcronreply Reply;
    if(Create){
        if(xcronCheckpointStoreRead(Store,NULL,&Stored)!=XCRON_CHECKPOINT_STORE_EMPTY ||
           !xcronServiceAddAsync(Service,&Job,&Request,NULL) ||
           xcronServiceCollect(Service,Request,10000,&Reply,NULL)!=XWAIT_OK || Reply.Code)return 5;
    }else{
        if(xcronCheckpointStoreRead(Store,&Record,&Stored)!=XCRON_CHECKPOINT_STORE_OK ||
           !xcronServiceRestorePausedCheckpointAsync(Service,&Job,Key,&Record,&Request,NULL) ||
           !checkpointDemoCollect(Service,Request,&Reply,NULL))return 5;
    }
    xcronZoneRelease(Job.Zone);
    xcronjobid Id=Reply.Job;
    if(!Id || !xcronServiceInfoAsync(Service,Id,&Request,NULL) ||
       xcronServiceCollect(Service,Request,10000,&Reply,NULL)!=XWAIT_OK ||
       Reply.Code || !Reply.Info.HasNext)return 6;
    printf("GEN %llu NEXT %lld SUCCEEDED %llu\n",(unsigned long long)Stored.Generation,
           (long long)Reply.Info.Next,(unsigned long long)Reply.Info.Succeeded);
    fflush(stdout);
    if(!Create && xrtEventWaitFor(&Demo.Started,30)!=XWAIT_TIMEOUT)return 7;
    if(Interrupt || Advance){
        /* Set the example clock to one occurrence, then resume explicitly. */
        xrtAtomic64Store(&Demo.Wall,(uint64)Reply.Info.Next,XMEMORY_RELEASE);
        if(!checkpointDemoPause(Service,Id,false))return 7;
        if(xrtEventWaitFor(&Demo.Started,10000)!=XWAIT_OK)return 7;
        if(!checkpointDemoPause(Service,Id,true))return 7;
        if(Interrupt){
            puts("LEASE ACTIVE");fflush(stdout);
            (void)getchar();
            if(!xrtEventSet(&Demo.Release))return 7;
        }
        xcronservicecompletion Done;
        size_t Count=0;
        if(xcronServiceWaitReadable(Service,10000,NULL)!=XWAIT_OK ||
           !xcronServiceDrain(Service,&Done,1,&Count,NULL) ||
           Count!=1 || Done.Job!=Id || Done.ScheduledAt!=Reply.Info.Next ||
           Done.Code!=XCRON_COMPLETION_NORMAL || Done.Result!=XCRON_RUN_SUCCEEDED)return 7;
        xrtErrorFree(Done.Error);
    }
    if(Create || Advance){
        if(Create && !checkpointDemoPause(Service,Id,true))return 8;
        if(!xcronServiceSaveCheckpointAsync(Service,Id,Key,&Request,NULL) ||
           !checkpointDemoCollect(Service,Request,&Reply,&Record) ||
           xcronCheckpointStoreCommit(Store,Stored.Generation,&Record)!=XCRON_CHECKPOINT_STORE_OK)return 8;
        printf("COMMITTED %llu\n",(unsigned long long)(Stored.Generation+1));
    }
    xcronretired Retired;
    bool Found;
    if(!xcronServiceClose(Service,false,NULL) || xcronServiceWaitFor(Service,10000,NULL)!=XWAIT_OK ||
       !xcronServiceReapStopped(Service,&Retired,&Found,NULL) || !Found || Retired.Id!=Id ||
       Retired.Data!=&Demo || !xcronServiceDestroy(Service,NULL) ||
       !xrtExecutorClose(Pool) || xrtExecutorWaitFor(Pool,10000)!=XWAIT_OK || !xrtExecutorDestroy(Pool) ||
       xcronCheckpointStoreClose(Store)!=XCRON_CHECKPOINT_STORE_OK)return 9;
    xrtEventUnit(&Demo.Release);
    xrtEventUnit(&Demo.Started);
    return 0;
}

int main(int Argc,char** Argv)
{
    if(Argc==3){
        xroot Root=xrtRootOpen(Argv[2]);
        if(!Root)return 2;
        int Result=checkpointDemoPhase(Root,Argv[1]);
        return xrtRootClose(Root)?Result:9;
    }
    if(Argc!=1)return 2;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#endif
    if(!Base || !*Base)Base=".";
    xroot Root=xrtRootOpen(Base);
    char Path[96],File[128];
    snprintf(Path,sizeof(Path),"xcron-service-checkpoint-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    snprintf(File,sizeof(File),"%s/job.xcs",Path);
    if(!Root || !xrtRootDirCreate(Root,Path,0700))return 3;
    xroot Owned=xrtRootOpenIn(Root,Path);
    if(!Owned)return 3;
    int Result=checkpointDemoPhase(Owned,"init");
    if(!Result)Result=checkpointDemoPhase(Owned,"advance");
    if(!Result)Result=checkpointDemoPhase(Owned,"observe");
    if(!xrtRootClose(Owned) || !xrtRootRemove(Root,File) || !xrtRootRemove(Root,Path) || !xrtRootClose(Root))return 9;
    return Result;
}
