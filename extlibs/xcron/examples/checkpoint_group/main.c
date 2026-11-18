#include <xcron/checkpoint_group.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int groupExample(xroot Root,cstr Action)
{
    const uint32 Capacity=32,Count=2;
    bool Create=!strcmp(Action,"init"),Advance=!strcmp(Action,"advance");
    if(!Create && !Advance && strcmp(Action,"show"))return 2;
    xcroncheckpointgroup* Store=NULL;
    xcroncheckpointstorestatus Status=xcronCheckpointGroupOpen(Root,"group.xcg",940,Capacity,Create,&Store);
    if(Status!=XCRON_CHECKPOINT_STORE_OK){printf("OPEN %d\n",Status);return 3;}

    const xcronschedulerconfig Limits={2,2,16,0,65536};
    xcronjobconfig Config={.Begin=1000,.End=60000,.RunningLimit=1};
    xcronscheduler* Scheduler=NULL;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&Scheduler,NULL))return 4;
    xcronjobid Jobs[2];
    xcroncheckpoint Records[2];
    xcroncheckpointgroupview View;
    Status=xcronCheckpointGroupRead(Store,&View);
    if(Create?Status!=XCRON_CHECKPOINT_STORE_EMPTY:Status!=XCRON_CHECKPOINT_STORE_OK || View.Count!=Count)
        return 5;
    for(uint32 I=0;I<Count;++I){
        xcroncheckpointkey Key={I+1,1};
        if(Create){
            if(!xcronSchedulerAdd(Scheduler,&Config,&Jobs[I],NULL))return 6;
        }else{
            const xcroncheckpoint* Record=&View.Records[I];
            xcroncheckpointkey Saved;
            if(!xcronCheckpointGetKey(Record,&Saved) || Saved.Job!=Key.Job || Saved.Revision!=Key.Revision ||
               !xcronSchedulerRestorePausedCheckpoint(Scheduler,&Config,Key,
                   (xbytesview){Record->Bytes,sizeof(Record->Bytes)},&Jobs[I],NULL))return 6;
        }
    }
    xcronZoneRelease(Config.Zone);
    if(!Create){
        xcronjobinfo Info;
        if(!xcronSchedulerJob(Scheduler,Jobs[0],&Info,NULL) || !Info.HasNext)return 7;
        xcronrun Runs[2];
        xcronpoll Poll;
        xcronclock Now={Info.Next,1};
        if(!xcronSchedulerPoll(Scheduler,Now,Runs,Count,&Poll,NULL) || Poll.Count || Poll.More)return 7;
        if(Advance){
            for(uint32 I=0;I<Count;++I)
                if(!xcronSchedulerSetPaused(Scheduler,Jobs[I],false,NULL))return 7;
            if(!xcronSchedulerPoll(Scheduler,Now,Runs,Count,&Poll,NULL) || Poll.Count!=Count)return 7;
            for(size_t I=0;I<Poll.Count;++I)
                if(!xcronSchedulerComplete(Scheduler,Runs[I].Id,XCRON_RUN_SUCCEEDED,NULL))return 7;
        }
    }
    if(Create || Advance){
        for(uint32 I=0;I<Count;++I)
            if(!xcronSchedulerSaveCheckpoint(Scheduler,Jobs[I],(xcroncheckpointkey){I+1,1},&Records[I],NULL))return 8;
        if(xcronCheckpointGroupCommit(Store,View.Info.Generation,Records,Count)!=XCRON_CHECKPOINT_STORE_OK ||
           xcronCheckpointGroupRead(Store,&View)!=XCRON_CHECKPOINT_STORE_OK)return 8;
    }
    xcronjobinfo Info[2];
    for(uint32 I=0;I<Count;++I)
        if(!xcronSchedulerJob(Scheduler,Jobs[I],&Info[I],NULL) || !Info[I].HasNext)return 9;
    printf("GEN %llu COUNT %u NEXT %lld %lld SLOTS %u\n",(unsigned long long)View.Info.Generation,View.Count,
           (long long)Info[0].Next,(long long)Info[1].Next,View.Info.ValidSlots);
    if(!xcronSchedulerClose(Scheduler,NULL))return 9;
    for(uint32 I=0;I<Count;++I){
        xcronretired Retired;
        bool Found;
        if(!xcronSchedulerReap(Scheduler,&Retired,&Found,NULL) || !Found)return 9;
    }
    if(!xcronSchedulerDestroy(Scheduler,NULL) || xcronCheckpointGroupClose(Store)!=XCRON_CHECKPOINT_STORE_OK)return 9;
    return 0;
}

int main(int Argc,char** Argv)
{
    if(Argc==3){
        xroot Root=xrtRootOpen(Argv[2]);
        if(!Root)return 2;
        int Code=groupExample(Root,Argv[1]);
        return xrtRootClose(Root)?Code:9;
    }
    if(Argc!=1)return 2;
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#endif
    if(!Base || !*Base)Base=".";
    char Name[96],File[128];
    snprintf(Name,sizeof(Name),"xcron-group-example-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    snprintf(File,sizeof(File),"%s/group.xcg",Name);
    xroot Parent=xrtRootOpen(Base);
    if(!Parent || !xrtRootDirCreate(Parent,Name,0700))return 3;
    xroot Root=xrtRootOpenIn(Parent,Name);
    if(!Root)return 3;
    int Code=groupExample(Root,"init");
    if(!Code)Code=groupExample(Root,"advance");
    if(!Code)Code=groupExample(Root,"show");
    if(!xrtRootClose(Root) || !xrtRootRemove(Parent,File) || !xrtRootRemove(Parent,Name) || !xrtRootClose(Parent))return 9;
    return Code;
}
