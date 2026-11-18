#include "test.h"
#include <xcron/checkpoint.h>
#include <xrt/crc32.h>
#include <xrt/memory.h>
#include <string.h>

static const xcroncheckpointkey Key={17,23};
static const xcronschedulerconfig Limits={2,2,16,0,65536};

static void closeCheckpointScheduler(xcronscheduler* S,bool Selected)
{
    testRequire(xcronSchedulerClose(S,NULL),"close checkpoint scheduler");
    bool Found;
    do{
        if(Selected){
            xcronselectedretired R;
            testRequire(xcronSchedulerReapSelected(S,&R,&Found,NULL),"reap selected checkpoint job");
        }else{
            xcronretired R;
            testRequire(xcronSchedulerReap(S,&R,&Found,NULL),"reap checkpoint job");
        }
    }while(Found);
    testRequire(xcronSchedulerDestroy(S,NULL),"destroy checkpoint scheduler");
}

static xbytesview checkpointView(const xcroncheckpoint* Record)
{return (xbytesview){Record->Bytes,sizeof(Record->Bytes)};}

static void checkpointActual(void)
{
    xcronjobconfig C={.Begin=-2000,.End=3000,.RunningLimit=1};
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&C.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&C.Expression,NULL),"actual checkpoint input");
    xcronscheduler *Before=NULL,*After=NULL;
    xcronjobid Job=0;
    testRequire(xcronSchedulerCreate(&Limits,&Before,NULL) && xcronSchedulerAdd(Before,&C,&Job,NULL),"original checkpoint job");
    xcronrun Run;xcronpoll P;
    testRequire(xcronSchedulerPoll(Before,(xcronclock){C.Begin,100},&Run,1,&P,NULL) &&
                P.Count==1 && Run.ScheduledAt==-2000,"first occurrence before checkpoint");

    xcroncheckpoint Record;
    memset(&Record,0xa5,sizeof(Record));
    xcroncheckpoint Unchanged=Record;
    xcronschedulerissue Issue;
    testRequire(!xcronSchedulerSaveCheckpoint(Before,Job,Key,&Record,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_BUSY && !memcmp(&Record,&Unchanged,sizeof(Record)),"live run cannot advance the durable cursor");
    testRequire(xcronSchedulerComplete(Before,Run.Id,XCRON_RUN_FAILED,NULL) &&
                xcronSchedulerSaveCheckpoint(Before,Job,Key,&Record,NULL),"save physically completed cursor");
    closeCheckpointScheduler(Before,false);

    testRequire(xcronSchedulerCreate(&Limits,&After,NULL),"fresh checkpoint scheduler");
    xcronjobid Out=UINT64_MAX;
    for(size_t I=0;I<sizeof(Record.Bytes);++I){
        xcroncheckpoint Corrupt=Record;Corrupt.Bytes[I]^=0x01;
        testRequire(!xcronSchedulerRestoreCheckpoint(After,&C,Key,checkpointView(&Corrupt),&Out,&Issue) &&
                    Issue.Code==XCRON_SCHEDULER_CHECKPOINT && Out==UINT64_MAX,"corrupt record changes no job");
    }
    testRequire(!xcronSchedulerRestoreCheckpoint(After,&C,Key,(xbytesview){Record.Bytes,179},&Out,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_CHECKPOINT && Out==UINT64_MAX,"truncated checkpoint");
    testRequire(!xcronSchedulerRestoreCheckpoint(After,&C,(xcroncheckpointkey){Key.Job,Key.Revision+1},
                checkpointView(&Record),&Out,&Issue) && Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH,"wrong job revision");
    xcronjobconfig Different=C;Different.GraceMilliseconds=1;
    testRequire(!xcronSchedulerRestoreCheckpoint(After,&Different,Key,checkpointView(&Record),&Out,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH && Out==UINT64_MAX,"configuration mismatch");

    xcroncheckpoint OffSchedule=Record;
    OffSchedule.Bytes[40]^=1;
    uint32 Crc=xrtCrc32(OffSchedule.Bytes,176);
    for(unsigned I=0;I<4;++I)OffSchedule.Bytes[176+I]=(uint8)(Crc>>(8*I));
    testRequire(!xcronSchedulerRestoreCheckpoint(After,&C,Key,checkpointView(&OffSchedule),&Out,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH && Out==UINT64_MAX,"off-schedule cursor rejected after valid CRC");

    C.Data=&C;
    xrtSetErrorInfo(XERR_STATE,"test.cron.checkpoint",9,"preserve caller error");
    const xerror* Ambient=xrtGetError();
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"checkpoint no-heap scope");
    testRequire(xcronSchedulerRestoreCheckpoint(After,&C,Key,checkpointView(&Record),&Out,NULL) &&
                xcronSchedulerSaveCheckpoint(After,Out,Key,&Unchanged,NULL) &&
                !memcmp(&Record,&Unchanged,sizeof(Record)) && xrtGetError()==Ambient,
                "checkpoint restore and save are canonical without allocation");
    xcronjobid Bad=UINT64_MAX;
    testRequire(!xcronSchedulerRestoreCheckpoint(After,&C,Key,checkpointView(&OffSchedule),&Bad,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH && Bad==UINT64_MAX,
                "typed checkpoint error under diagnostic OOM");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave checkpoint scope");
    xallocationbudgetinfo Usage={.Size=sizeof(Usage)};
    testRequire(xrtAllocationBudgetInfo(Budget,&Usage) && !Usage.LiveCount && !Usage.LiveBytes,"checkpoint retains no budget storage");
    xrtAllocationBudgetFree(Budget);xrtClearError();
    xcronjobinfo Info;xcronschedulerstats Stats;
    testRequire(xcronSchedulerJob(After,Out,&Info,NULL) && Info.Next==-1000 && Info.Failed==1 && Info.Dispatched==1 &&
                xcronSchedulerInspect(After,&Stats,NULL) && !Stats.HasClock && !Stats.Dispatched,
                "restore per-job counters and start a new clock epoch");
    testRequire(xcronSchedulerPoll(After,(xcronclock){-1000,1},&Run,1,&P,NULL) &&
                P.Count==1 && Run.ScheduledAt==-1000 && Run.Data==&C &&
                xcronSchedulerComplete(After,Run.Id,XCRON_RUN_SUCCEEDED,NULL),"resume first uncommitted occurrence");
    testRequire(xcronSchedulerCancel(After,Out,NULL) && xcronSchedulerSaveCheckpoint(After,Out,Key,&Record,NULL),
                "save explicit cancellation");
    closeCheckpointScheduler(After,false);

    testRequire(xcronSchedulerCreate(&Limits,&After,NULL) &&
                xcronSchedulerRestoreCheckpoint(After,&C,Key,checkpointView(&Record),&Out,NULL) &&
                xcronSchedulerJob(After,Out,&Info,NULL) && Info.State==XCRON_JOB_CANCELLED && !Info.HasNext &&
                Info.Succeeded==1 && Info.Failed==1,"restore cancelled tombstone");
    closeCheckpointScheduler(After,false);xcronZoneRelease(C.Zone);
}

static bool pollCheckpointSelected(xcronscheduler* S,xtime Due,xcronselectedrun* Run,xcronselectedpoll* P)
{
    xcronoccurrencebudget Work={32,32,32,32,32};
    for(size_t I=0;I<20;++I){
        if(!xcronSchedulerPollSelected(S,(xcronclock){Due,1},Work,Run,1,P,NULL))return false;
        if(P->Poll.Count || !P->Poll.More)return true;
    }
    return false;
}

static void checkpointSelected(void)
{
    xcronjobconfig C={.RunningLimit=1};
    xdatetime Fold={.Year=2024,.Month=11,.Day=3,.Hour=1,.Minute=30};
    xtime Times[2];size_t Count=0;
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0/2,M11.1.0/2"),XCRON_ZONE_POSIX,&C.Zone,NULL) &&
                xcronZoneResolve(C.Zone,&Fold,Times,2,&Count,NULL)==XCRON_ZONE_FOUND && Count==2 &&
                xcronExpressionCompile(XRT_STR_LITERAL("30 1 * * *"),XCRON_UNIX5,&C.Expression,NULL),"selected fold fixture");
    C.Begin=Times[0];C.End=Times[1];
    xcronscheduler* S=NULL;
    xcronjobid Job=0;
    xcrondeliverypolicy Policy={XCRON_FOLD_LAST,XCRON_MISSING_REJECT};
    testRequire(xcronSchedulerCreateSelected(&Limits,&S,NULL) &&
                xcronSchedulerAddSelected(S,&C,Policy,&Job,NULL),"start selected checkpoint job");
    xcronselectedrun Run;xcronselectedpoll P;xcroncheckpoint Record;xcronschedulerissue Issue;
    testRequire(!xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,&Issue) && Issue.Code==XCRON_SCHEDULER_BUSY,
                "pending query cannot be checkpointed");
    testRequire(pollCheckpointSelected(S,Times[0],&Run,&P) && !P.Poll.Count &&
                xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL),"save later fold before it is due");
    closeCheckpointScheduler(S,true);
    testRequire(xcronSchedulerCreate(&Limits,&S,NULL) &&
                !xcronSchedulerRestoreCheckpoint(S,&C,Key,checkpointView(&Record),&Job,&Issue) &&
                Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH,"reject different scheduler mode");
    closeCheckpointScheduler(S,false);

    testRequire(xcronSchedulerCreateSelected(&Limits,&S,NULL) &&
                xcronSchedulerRestoreCheckpoint(S,&C,Key,checkpointView(&Record),&Job,NULL) &&
                pollCheckpointSelected(S,Times[1],&Run,&P) && P.Poll.Count==1 &&
                Run.Run.ScheduledAt==Times[1] && Run.Candidates==2 && Run.Local.Hour==1 && Run.Local.Minute==30 &&
                xcronSchedulerComplete(S,Run.Run.Id,XCRON_RUN_SUCCEEDED,NULL),"restored fold keeps its global local label");
    testRequire(pollCheckpointSelected(S,Times[1],&Run,&P) && !P.Poll.Count &&
                xcronSchedulerSaveCheckpoint(S,Job,Key,&Record,NULL),"save exhausted selected job");
    closeCheckpointScheduler(S,true);
    xcronselectedjobinfo Info;
    testRequire(xcronSchedulerCreateSelected(&Limits,&S,NULL) &&
                xcronSchedulerRestoreCheckpoint(S,&C,Key,checkpointView(&Record),&Job,NULL) &&
                xcronSchedulerJobSelected(S,Job,&Info,NULL) && Info.State==XCRON_JOB_EXHAUSTED &&
                Info.Policy.Fold==XCRON_FOLD_LAST && Info.Policy.Missing==XCRON_MISSING_REJECT && Info.Succeeded==1,
                "restore exhausted selected tombstone");
    closeCheckpointScheduler(S,true);xcronZoneRelease(C.Zone);
}

static void checkpointGroupCapture(void)
{
    xcronjobconfig C={.Begin=1000,.End=10000,.RunningLimit=1};
    xcronscheduler* S=NULL;
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&C.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&C.Expression,NULL) &&
                xcronSchedulerCreate(&Limits,&S,NULL),"initialize group capture");
    xcroncheckpointtarget Targets[2]={{.Key={1,1}},{.Key={2,1}}};
    testRequire(xcronSchedulerAdd(S,&C,&Targets[1].Job,NULL),"capture active second member");
    C.Begin=2000;
    testRequire(xcronSchedulerAdd(S,&C,&Targets[0].Job,NULL),"capture idle first member");
    xcronZoneRelease(C.Zone);

    xcronrun A,B;
    xcronpoll Poll;
    testRequire(xcronSchedulerPoll(S,(xcronclock){1000,1},&A,1,&Poll,NULL) && Poll.Count==1 &&
                A.Job==Targets[1].Job,"group fixture has one live lease");
    _Alignas(uint32) xcroncheckpoint Records[2];
    xcroncheckpoint Sentinel[2],Individual[2];
    memset(Sentinel,0x3a,sizeof(Sentinel));memcpy(Records,Sentinel,sizeof(Records));
    xcronschedulerissue Issue;
    uint32 Index=77;
    testRequire(!xcronSchedulerSaveCheckpointGroup(S,Targets,2,Records,&Index,&Issue) &&
                Index==1 && Issue.Code==XCRON_SCHEDULER_BUSY && !memcmp(Records,Sentinel,sizeof(Records)),
                "late busy member preserves all outputs");
    xrtClearError();
    testRequire(xcronSchedulerPoll(S,(xcronclock){2000,2},&B,1,&Poll,NULL) && Poll.Count==1 &&
                B.Job==Targets[0].Job,"failed group does not hold the first member");
    testRequire(xcronSchedulerComplete(S,A.Id,XCRON_RUN_SUCCEEDED,NULL) &&
                xcronSchedulerComplete(S,B.Id,XCRON_RUN_SUCCEEDED,NULL),"return both leases");
    for(unsigned I=0;I<2;++I)
        testRequire(xcronSchedulerSaveCheckpoint(S,Targets[I].Job,Targets[I].Key,&Individual[I],NULL),"canonical scalar capture");

    xcroncheckpointtarget Duplicate[]={Targets[0],Targets[1]};
    Duplicate[1].Job=Duplicate[0].Job;
    testRequire(!xcronSchedulerSaveCheckpointGroup(S,Duplicate,2,Records,&Index,&Issue) &&
                Index==1 && Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH && !memcmp(Records,Sentinel,sizeof(Records)),
                "reject repeated live Job IDs with distinct stable keys");
    xrtClearError();
    Duplicate[1]=Targets[1];
    Duplicate[1].Key=Duplicate[0].Key;
    testRequire(!xcronSchedulerSaveCheckpointGroup(S,Duplicate,2,Records,&Index,&Issue) &&
                Index==1 && Issue.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH,
                "reject repeated stable keys");
    xrtClearError();
    Index=77;
    testRequire(!xcronSchedulerSaveCheckpointGroup(S,Targets,2,Records,(uint32*)Records,&Issue) && Index==77 &&
                !memcmp(Records,Sentinel,sizeof(Records)),"group output aliases preserve records");
    xrtClearError();

    xallocationbudget* Budget=NULL;
    xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"deny allocations in capture");
    testRequire(xcronSchedulerSaveCheckpointGroup(S,Targets,2,Records,&Index,NULL) &&
                Index==XCRON_CHECKPOINT_NO_INDEX && !memcmp(Records,Individual,sizeof(Records)),
                "successful capture writes both canonical records");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave capture scope");
    xrtAllocationBudgetFree(Budget);
    testRequire(xcronSchedulerPoll(S,(xcronclock){5000,3},&A,1,&Poll,NULL) && !Poll.Count && !Poll.More,
                "all captured jobs are held");
    testRequire(xcronSchedulerSaveCheckpointGroup(S,NULL,0,NULL,&Index,NULL) && Index==XCRON_CHECKPOINT_NO_INDEX,
                "empty capture has a defined result");
    testRequire(xcronSchedulerSetPaused(S,Targets[0].Job,false,NULL) &&
                xcronSchedulerPoll(S,(xcronclock){5000,3},&A,1,&Poll,NULL) && Poll.Count==1 && A.Job==Targets[0].Job &&
                xcronSchedulerComplete(S,A.Id,XCRON_RUN_SUCCEEDED,NULL),"resume releases one member");
    closeCheckpointScheduler(S,false);
}

int main(void)
{
    checkpointActual();checkpointSelected();checkpointGroupCapture();
    puts("cron checkpoint tests passed");
    return 0;
}
