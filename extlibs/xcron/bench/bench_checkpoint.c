#include <xcron/checkpoint.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>

int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):100000;
    if(Argc>2 || !Count || Count>10000000)return 2;
    const xcronschedulerconfig Limits={2,1,8,0,65536};
    const xcroncheckpointkey Key={1,1};
    xcronjobconfig Config={.Begin=0,.End=60000,.RunningLimit=1};
    xcronscheduler* S=NULL;xcronjobid Job;
    xcroncheckpoint Record;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Config.Zone,NULL) ||
       !xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Config.Expression,NULL) ||
       !xcronSchedulerCreate(&Limits,&S,NULL) || !xcronSchedulerAdd(S,&Config,&Job,NULL))return 3;
    double Start=xrtTimer();
    for(size_t I=0;I<Count;++I){
        if(!xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL))return 4;
    }
    double Save=xrtTimer()-Start;
    xbytesview Wire={Record.Bytes,sizeof(Record.Bytes)};
    Start=xrtTimer();
    for(size_t I=0;I<Count;++I){
        xcronjobid Copy;xcronretired Retired;bool Found;
        if(!xcronSchedulerRestoreCheckpoint(S,&Config,Key,Wire,&Copy,NULL) ||
           !xcronSchedulerCancel(S,Copy,NULL) || !xcronSchedulerReap(S,&Retired,&Found,NULL) ||
           !Found || Retired.Id!=Copy)return 5;
    }
    double Restore=xrtTimer()-Start;
    xcronretired Retired;bool Found;
    if(!xcronSchedulerClose(S,NULL) || !xcronSchedulerReap(S,&Retired,&Found,NULL) ||
       !Found || !xcronSchedulerDestroy(S,NULL))return 6;
    xcronZoneRelease(Config.Zone);
    if(Save<=0 || Restore<=0)return 7;
    printf("save_records_per_sec: %.3f\nrestore_reap_records_per_sec: %.3f\n",Count/Save,Count/Restore);
    return 0;
}
