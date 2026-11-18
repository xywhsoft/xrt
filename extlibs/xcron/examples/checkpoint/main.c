#include <xcron/checkpoint.h>
#include <stdio.h>
#include <string.h>

static const xcroncheckpointkey Identity={1,1};
static const xcronschedulerconfig Limits={1,1,8,0,65536};
static bool readCheckpoint(const char* Path,xcroncheckpoint* Record)
{
    FILE* File=fopen(Path,"rb");
    if(!File)return false;
    bool OK=fread(Record->Bytes,1,sizeof(Record->Bytes),File)==sizeof(Record->Bytes) &&
            fgetc(File)==EOF && !ferror(File);
    return fclose(File)==0 && OK;
}
static bool writeCheckpoint(const char* Path,const xcroncheckpoint* Record)
{
    FILE* File=fopen(Path,"wb");
    if(!File)return false;
    bool OK=fwrite(Record->Bytes,1,sizeof(Record->Bytes),File)==sizeof(Record->Bytes) && fflush(File)==0;
    return fclose(File)==0 && OK;
}
static bool stop(xcronscheduler* S)
{
    if(!xcronSchedulerClose(S,NULL))return false;
    bool Found;
    do{
        xcronretired Retired;
        if(!xcronSchedulerReap(S,&Retired,&Found,NULL))return false;
    }while(Found);
    return xcronSchedulerDestroy(S,NULL);
}

int main(int Argc,char** Argv)
{
    bool Memory=Argc==1;
    bool Save=Argc==3 && !strcmp(Argv[1],"save");
    bool Inspect=Argc==3 && !strcmp(Argv[1],"inspect");
    bool Interrupted=Argc==3 && !strcmp(Argv[1],"active");
    bool Advance=Argc==4 && !strcmp(Argv[1],"advance");
    if(!Memory && !Save && !Inspect && !Interrupted && !Advance)return 2;

    xcronjobconfig Config={.Begin=0,.End=3000,.RunningLimit=1};
    xcronscheduler* Scheduler=NULL;xcronjobid Job;
    xcronrun Run;xcronpoll Poll;
    xcroncheckpoint Record;
    int Result=3;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&Scheduler,NULL))goto done;
    if(Memory || Save){
        Result=4;
        if(!xcronSchedulerAdd(Scheduler,&Config,&Job,NULL) ||
           !xcronSchedulerPoll(Scheduler,(xcronclock){0,100},&Run,1,&Poll,NULL) || Poll.Count!=1 ||
           !xcronSchedulerComplete(Scheduler,Run.Id,XCRON_RUN_SUCCEEDED,NULL) ||
           !xcronSchedulerSaveCheckpoint(Scheduler,Job,Identity,&Record,NULL))goto done;
        if(Save){Result=writeCheckpoint(Argv[2],&Record)?0:5;goto done;}
        Result=6;
        if(!stop(Scheduler))goto done;
        Scheduler=NULL;
        if(!xcronSchedulerCreate(&Limits,&Scheduler,NULL))goto done;
    }else if(!readCheckpoint(Argv[2],&Record)){Result=7;goto done;}

    Result=8;
    xcronjobinfo Info;
    if(!xcronSchedulerRestoreCheckpoint(Scheduler,&Config,Identity,(xbytesview){Record.Bytes,sizeof(Record.Bytes)},&Job,NULL) ||
       !xcronSchedulerJob(Scheduler,Job,&Info,NULL) || !Info.HasNext ||
       !xcronSchedulerPoll(Scheduler,(xcronclock){Info.Next,1},&Run,1,&Poll,NULL) || Poll.Count!=1)goto done;
    printf("NEXT %lld\n",(long long)Run.ScheduledAt);
    fflush(stdout);
    if(Interrupted){
        /* A test host holds stdin open and terminates the process here. */
        (void)getchar();
    }
    Result=9;
    if(!xcronSchedulerComplete(Scheduler,Run.Id,XCRON_RUN_SUCCEEDED,NULL))goto done;
    Result=0;
    if(Advance && (!xcronSchedulerSaveCheckpoint(Scheduler,Job,Identity,&Record,NULL) ||
       !writeCheckpoint(Argv[3],&Record)))Result=10;
done:
    if(Scheduler && !stop(Scheduler) && !Result)Result=11;
    if(Config.Zone)xcronZoneRelease(Config.Zone);
    return Result;
}
