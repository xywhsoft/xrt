#include <xcron/checkpoint_store.h>
#include <xrt/memory.h>
#include <xrt/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const xcroncheckpointkey Key={1,1};
static const char* Name="job.xcs";

static bool stop(xcronscheduler* S)
{
    if(!xcronSchedulerClose(S,NULL))return false;
    bool Found;
    do{
        xcronretired R;
        if(!xcronSchedulerReap(S,&R,&Found,NULL))return false;
    }while(Found);
    return xcronSchedulerDestroy(S,NULL);
}

static bool useStore(xroot Root,bool Create,bool Inspect,bool Interrupt)
{
    xcroncheckpointstore* Store=NULL;
    xcroncheckpointstorestatus Status=xcronCheckpointStoreOpen(Root,Name,Key,Create,&Store);
    if(Status!=XCRON_CHECKPOINT_STORE_OK){
        printf("OPEN %d\n",(int)Status);
        return false;
    }

    bool OK=false;
    xcroncheckpoint Record;xcroncheckpointstoreinfo Info;
    Status=xcronCheckpointStoreRead(Store,&Record,&Info);
    const xcronschedulerconfig Limits={1,1,8,0,65536};
    xcronjobconfig Config={.End=60000,.RunningLimit=1};
    xcronscheduler* S=NULL;xcronjobid Job;
    if((Status!=XCRON_CHECKPOINT_STORE_OK && Status!=XCRON_CHECKPOINT_STORE_EMPTY) ||
       !xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&S,NULL))goto done;
    if(Status==XCRON_CHECKPOINT_STORE_EMPTY){
        if(!xcronSchedulerAdd(S,&Config,&Job,NULL))goto done;
    }else if(!xcronSchedulerRestoreCheckpoint(S,&Config,Key,(xbytesview){Record.Bytes,sizeof(Record.Bytes)},&Job,NULL))goto done;

    xcronjobinfo Current;
    if(!xcronSchedulerJob(S,Job,&Current,NULL) || !Current.HasNext)goto done;
    if(Inspect){
        printf("GEN %llu NEXT %lld SLOTS %u\n",(unsigned long long)Info.Generation,(long long)Current.Next,Info.ValidSlots);
        OK=true;goto done;
    }
    xcronrun Run;xcronpoll Poll;
    if(!xcronSchedulerPoll(S,(xcronclock){Current.Next,1},&Run,1,&Poll,NULL) || Poll.Count!=1)goto done;
    if(Interrupt){
        printf("ACTIVE %llu %lld\n",(unsigned long long)Info.Generation,(long long)Run.ScheduledAt);
        fflush(stdout);
        (void)getchar();
        OK=xcronSchedulerComplete(S,Run.Id,XCRON_RUN_CANCELLED,NULL);
        goto done;
    }
    if(!xcronSchedulerComplete(S,Run.Id,XCRON_RUN_SUCCEEDED,NULL) ||
       !xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL) ||
       xcronCheckpointStoreCommit(Store,Info.Generation,&Record)!=XCRON_CHECKPOINT_STORE_OK)goto done;
    OK=xcronSchedulerJob(S,Job,&Current,NULL);
    if(OK)printf("GEN %llu NEXT %lld\n",(unsigned long long)(Info.Generation+1),(long long)Current.Next);

done:
    if(S)OK=stop(S)&&OK;
    if(Config.Zone)xcronZoneRelease(Config.Zone);
    return xcronCheckpointStoreClose(Store)==XCRON_CHECKPOINT_STORE_OK && OK;
}

int main(int Argc,char** Argv)
{
    if(Argc==3){
        bool Create=!strcmp(Argv[1],"init"),Step=!strcmp(Argv[1],"step");
        bool Inspect=!strcmp(Argv[1],"show"),Interrupt=!strcmp(Argv[1],"active");
        if(!Create && !Step && !Inspect && !Interrupt)return 2;
        xroot Root=xrtRootOpen(Argv[2]);
        if(!Root)return 3;
        bool OK=useStore(Root,Create,Inspect,Interrupt);
        return xrtRootClose(Root) && OK?0:4;
    }
    if(Argc!=1)return 2;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    xroot Parent=xrtRootOpen(Base);
    char Fresh[96];
    snprintf(Fresh,sizeof(Fresh),"xcron-store-demo-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    if(!Parent)return 5;
    bool Created=xrtRootDirCreate(Parent,Fresh,0700);
    str Path=Created?xrtPathJoin(Base,Fresh):NULL;
    xroot Root=Path?xrtRootOpen(Path):NULL;
    xrtFree(Path);
    bool OK=false;
    if(Root){
        OK=useStore(Root,true,false,false) && useStore(Root,false,true,false);
        if(OK)OK=xrtRootRemove(Root,Name);
        OK=xrtRootClose(Root)&&OK;
    }
    if(Created)OK=xrtRootRemove(Parent,Fresh)&&OK;
    return xrtRootClose(Parent)&&OK?0:6;
}
