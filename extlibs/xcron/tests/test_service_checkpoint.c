#include "test.h"
#include <xcron/service_checkpoint.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <string.h>
#include <stdlib.h>

typedef struct servicecheckpointfixture {
    xevent ClockEntered,ClockGate,RunEntered,RunGate;
    xatomic64 Wall;
    bool Selected;
} servicecheckpointfixture;

static const xcroncheckpointkey liveKey={97,3};
static const xcrondeliverypolicy livePolicy={XCRON_FOLD_LAST,XCRON_MISSING_REJECT};

static bool liveClock(xcronclock* Out,ptr Raw)
{
    servicecheckpointfixture* F=Raw;
    if(!xrtEventSet(&F->ClockEntered) || xrtEventWaitFor(&F->ClockGate,10000)!=XWAIT_OK)return false;
    *Out=(xcronclock){(xtime)xrtAtomic64Load(&F->Wall,XMEMORY_ACQUIRE),0};
    return true;
}

static xcronrunresult liveRun(const xcronrun* Run,const xcronexecution* Token,ptr Raw)
{
    servicecheckpointfixture* F=Raw;
    testRequire(Run->Data==F && Run->ScheduledAt==1000,"restored body receives current process data and exact cursor");
    if(F->Selected){
        const xcronexecutionlabel* Label=xcronExecutionSelected(Token);
        testRequire(Label && Label->Candidates==1 && Label->Local.Second==1,"restored selected label");
    }
    testRequire(xrtEventSet(&F->RunEntered) && xrtEventWaitFor(&F->RunGate,10000)==XWAIT_OK,"release body from caller");
    return XCRON_RUN_SUCCEEDED;
}

static xcronjobconfig liveJob(servicecheckpointfixture* F)
{
    xcronjobconfig J={.Begin=1000,.End=2000,.RunningLimit=1,.Data=F};
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&J.Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL),"live checkpoint job");
    return J;
}

static bool liveCreate(bool Selected,bool Checkpointed,const xcronserviceconfig* C,xcronservice** S,xcronserviceissue* Issue)
{
    if(Selected){
        xcronselectedserviceconfig Config={*C,1,0};
        return Checkpointed?xcronServiceCreateSelectedCheckpointed(&Config,S,Issue):xcronServiceCreateSelected(&Config,S,Issue);
    }
    return Checkpointed?xcronServiceCreateCheckpointed(C,S,Issue):xcronServiceCreate(C,S,Issue);
}

static void liveStop(xcronservice* S,bool Selected,xcronjobid Id,ptr Data,uint64 Succeeded)
{
    testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"stop and join checkpoint service");
    bool Found;
    if(Selected){
        xcronselectedretired R;
        testRequire(xcronServiceReapStoppedSelected(S,&R,&Found,NULL) && Found==(Id!=0),"reap selected checkpoint service");
        if(Found)testRequire(R.Id==Id && R.Data==Data && R.Info.Succeeded==Succeeded,"selected retirement and success count");
    }else{
        xcronretired R;
        testRequire(xcronServiceReapStopped(S,&R,&Found,NULL) && Found==(Id!=0),"reap checkpoint service");
        if(Found)testRequire(R.Id==Id && R.Data==Data && R.Info.Succeeded==Succeeded,"original data retirement and success count");
    }
    testRequire(xcronServiceDestroy(S,NULL),"destroy after all ownership returns");
}

static xcronreply liveCollect(xcronservice* S,xcronrequestid Q,xcroncheckpoint* Record)
{
    xcronreply Reply;
    testRequire(xcronServiceCollectCheckpoint(S,Q,10000,&Reply,Record,NULL)==XWAIT_OK && Reply.Request==Q,
                "collect one checkpoint reply");
    return Reply;
}

static xcronjobid liveAdd(xcronservice* S,bool Selected,const xcronjobconfig* J)
{
    xcronrequestid Q;
    if(Selected){
        xcronselectedreply R;
        testRequire(xcronServiceAddSelectedAsync(S,J,livePolicy,&Q,NULL) &&
                    xcronServiceCollectSelected(S,Q,10000,&R,NULL)==XWAIT_OK && R.Job && !R.Code && !R.Error,
                    "add selected job to checkpointed service");
        return R.Job;
    }
    xcronreply R;
    testRequire(xcronServiceAddAsync(S,J,&Q,NULL) &&
                xcronServiceCollect(S,Q,10000,&R,NULL)==XWAIT_OK && R.Job && !R.Code && !R.Error,
                "add actual job to checkpointed service");
    return R.Job;
}

static void liveSetPaused(xcronservice* S,bool Selected,xcronjobid Job,bool Paused)
{
    xcronrequestid Q;
    xcronrequestkind Kind=Paused?XCRON_REQUEST_PAUSE:XCRON_REQUEST_RESUME;
    testRequire(xcronServiceSetPausedAsync(S,Job,Paused,&Q,NULL),"queue pause or resume");
    if(Selected){
        xcronselectedreply R;
        testRequire(xcronServiceCollectSelected(S,Q,10000,&R,NULL)==XWAIT_OK &&
                    !R.Code && !R.Error && R.Job==Job && R.Kind==Kind,"selected pause or resume reply");
    }else{
        xcronreply R;
        testRequire(xcronServiceCollect(S,Q,10000,&R,NULL)==XWAIT_OK &&
                    !R.Code && !R.Error && R.Job==Job && R.Kind==Kind,"actual pause or resume reply");
    }
}

static xcronservicecheckpointreply liveCollectGroup(xcronservice* S,xcronrequestid Q)
{
    xcronservicecheckpointreply Reply;
    testRequire(xcronServiceCollectCheckpointGroup(S,Q,10000,&Reply,NULL)==XWAIT_OK && Reply.Request==Q,
                "collect one group capture reply");
    return Reply;
}

static void liveCapture(xcronservice* S,xcronjobid Job,xcroncheckpoint* Record)
{
    double Until=xrtTimer()+10.0;
    for(;;){
        xcronrequestid Q;
        testRequire(xcronServiceSaveCheckpointAsync(S,Job,liveKey,&Q,NULL),"queue capture");
        xcronreply R=liveCollect(S,Q,Record);
        if(R.Code==XCRON_SERVICE_OK){
            testRequire(R.Kind==XCRON_REQUEST_SAVE_CHECKPOINT && R.Job==Job && !R.Error,"capture has original job");
            return;
        }
        testRequire(R.Code==XCRON_SERVICE_KERNEL && R.Scheduler.Code==XCRON_SCHEDULER_BUSY && xrtTimer()<Until,
                    "initial recurrence eventually becomes quiescent");
        xrtErrorFree(R.Error);
    }
}

static size_t liveArena(xcronservice* S,bool Selected)
{
    if(Selected){
        xcronselectedservicestats Stats;
        testRequire(xcronServiceInspectSelected(S,&Stats,NULL),"inspect selected checkpoint arena");
        return Stats.Service.ArenaBytes;
    }
    xcronservicestats Stats;
    testRequire(xcronServiceInspect(S,&Stats,NULL),"inspect checkpoint arena");
    return Stats.ArenaBytes;
}

static void liveRoundtrip(bool Selected)
{
    servicecheckpointfixture F={.Selected=Selected};
    xrtAtomic64Init(&F.Wall,0);
    testRequire(xrtEventInit(&F.ClockEntered,true,false) && xrtEventInit(&F.ClockGate,true,true) &&
                xrtEventInit(&F.RunEntered,true,false) && xrtEventInit(&F.RunGate,true,false),"controlled clock and body gates");
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    const xcronserviceconfig C={
        .Scheduler={2,1,8,0,65536},.Executor=Pool,.Proc=liveRun,.Context=&F,.Clock=liveClock,.ClockContext=&F,
        .CommandLimit=2,.CompletionLimit=1,.BatchLimit=1,.CommandWorkLimit=2,.ResampleMilliseconds=2,
        .RetryIntervalMilliseconds=1,.RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronservice* S=NULL;
    xcronrequestid Q=0;
    testRequire(Pool && liveCreate(Selected,false,&C,&S,NULL),"create ordinary service");
    size_t Ordinary=liveArena(S,Selected);
    testRequire(!xcronServiceSaveCheckpointAsync(S,1,liveKey,&Q,NULL) && !Q,"ordinary service declines checkpoint tickets");
    xrtClearError();
    liveStop(S,Selected,0,NULL,0);

    testRequire(liveCreate(Selected,true,&C,&S,NULL),"create checkpointed live service");
    size_t Arena=liveArena(S,Selected);
    testRequire(Arena>=Ordinary+C.CommandLimit*(sizeof(xcroncheckpoint)+sizeof(xcroncheckpointkey)),"checkpoint capacity charged to arena");
    xcronserviceconfig Tight=C;
    Tight.BytesLimit=Arena+C.Scheduler.BytesLimit+C.AdapterBytesLimit-1;
    xcronservice* Rejected=NULL;
    xcronserviceissue Issue;
    testRequire(!liveCreate(Selected,true,&Tight,&Rejected,&Issue) && !Rejected && Issue.Code==XCRON_SERVICE_LIMIT,
                "total cap includes checkpoint ticket storage");
    xrtClearError();
    xallocationbudget* Budget=NULL;
    testRequire(xrtAllocationBudgetCreate(1,&Budget),"create startup allocation budget");
    Tight=C;
    Tight.InitializationBudget=Budget;
    testRequire(!liveCreate(Selected,true,&Tight,&Rejected,&Issue) && !Rejected && Issue.Code==XCRON_SERVICE_MEMORY,
                "arena OOM publishes no service");
    xrtAllocationBudgetFree(Budget);
    xrtClearError();

    xcronjobconfig J=liveJob(&F);
    xcronjobid Job=liveAdd(S,Selected,&J);
    xcroncheckpoint First;
    liveCapture(S,Job,&First);
    liveStop(S,Selected,Job,&F,0);
    xcronZoneRelease(J.Zone);

    xrtAtomic64Store(&F.Wall,1000,XMEMORY_RELEASE);
    testRequire(xrtEventReset(&F.ClockEntered) && xrtEventReset(&F.ClockGate) &&
                liveCreate(Selected,true,&C,&S,NULL) && xrtEventWaitFor(&F.ClockEntered,10000)==XWAIT_OK,
                "new live owner pauses before reading queued restore");
    J=liveJob(&F);
    xcroncheckpoint Copy=First;
    xcronrequestid B;
    testRequire(xcronServiceRestoreCheckpointAsync(S,&J,(xcroncheckpointkey){97,4},&First,&B,NULL) &&
                xcronServiceRestorePausedCheckpointAsync(S,&J,liveKey,&Copy,&Q,NULL),"queue two bounded restores");
    xcronZoneRelease(J.Zone);
    memset(&J,0,sizeof(J));
    memset(&Copy,0,sizeof(Copy));
    xcronrequestid Extra=100;
    testRequire(!xcronServiceReapAsync(S,&Extra,&Issue) && Extra==100 && Issue.Code==XCRON_SERVICE_LIMIT,
                "pending checkpoints consume ordinary command credits");
    xrtClearError();
    xcronreply Reply={.Job=999};
    testRequire(xcronServiceCollectCheckpoint(S,Q,0,&Reply,&Copy,NULL)==XWAIT_TIMEOUT &&
                Reply.Job==999 && Copy.Bytes[0]==0,"nonblocking collect preserves both outputs");
    if(Selected){
        xcronselectedreply Wrong;
        testRequire(xcronServiceCollectSelected(S,Q,0,&Wrong,&Issue)==XWAIT_ERROR && Issue.Code==XCRON_SERVICE_ARGUMENT,
                    "ordinary selected collector cannot consume checkpoint");
    }else testRequire(xcronServiceCollect(S,Q,0,&Reply,&Issue)==XWAIT_ERROR && Reply.Job==999 && Issue.Code==XCRON_SERVICE_ARGUMENT,
                      "ordinary collector cannot consume checkpoint");
    xrtClearError();
    testRequire(xrtEventSet(&F.ClockGate),"resume owner");
    Reply=liveCollect(S,B,NULL);
    testRequire(!Reply.Job && Reply.Kind==XCRON_REQUEST_RESTORE_CHECKPOINT && Reply.Data==&F &&
                Reply.Code==XCRON_SERVICE_KERNEL && Reply.Scheduler.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH,
                "wrong key returns original data duty with typed status");
    xrtErrorFree(Reply.Error);
    Reply=liveCollect(S,Q,&Copy);
    testRequire(Reply.Job && Reply.Kind==XCRON_REQUEST_RESTORE_PAUSED_CHECKPOINT &&
                !Reply.Code && !Reply.Error && !Reply.Data && Copy.Bytes[0]==0,
                "copied paused restore succeeds and preserves record output");
    Job=Reply.Job;
    testRequire(xcronServiceCollectCheckpoint(S,Q,0,&Reply,NULL,&Issue)==XWAIT_ERROR && Issue.Code==XCRON_SERVICE_STALE,
                "restore ticket is transferred exactly once");
    xrtClearError();

    testRequire(xcronServiceInfoAsync(S,Job,&Q,NULL) &&
                xcronServiceCollectCheckpoint(S,Q,0,&Reply,NULL,&Issue)==XWAIT_ERROR && Issue.Code==XCRON_SERVICE_ARGUMENT,
                "checkpoint collector cannot consume normal ticket");
    xrtClearError();
    if(Selected){
        xcronselectedreply R;
        testRequire(xcronServiceCollectSelected(S,Q,10000,&R,NULL)==XWAIT_OK && !R.Code && !R.Error &&
                    R.Info.HasNext && R.Info.Next==1000 && R.Info.Policy.Fold==livePolicy.Fold &&
                    R.Info.Policy.Missing==livePolicy.Missing,"restored selected next occurrence");
    }else testRequire(xcronServiceCollect(S,Q,10000,&Reply,NULL)==XWAIT_OK && !Reply.Code && !Reply.Error &&
                      Reply.Info.HasNext && Reply.Info.Next==1000,"restored actual next occurrence");

    testRequire(xrtEventWaitFor(&F.RunEntered,30)==XWAIT_TIMEOUT,"overdue restored job cannot start before resume");
    liveCapture(S,Job,&Copy);
    testRequire(!memcmp(&Copy,&First,sizeof(First)),"hold remains outside the checkpoint wire bytes");
    liveSetPaused(S,Selected,Job,false);
    testRequire(xrtEventWaitFor(&F.RunEntered,10000)==XWAIT_OK,"resume publishes the held cursor");
    liveSetPaused(S,Selected,Job,true);
    xrtAtomic64Store(&F.Wall,2000,XMEMORY_RELEASE);
    testRequire(xcronServiceSaveCheckpointAsync(S,Job,liveKey,&Q,NULL),"queue save during active held lease");
    testRequire(xcronServiceCollectCheckpoint(S,Q,0,&Reply,NULL,&Issue)==XWAIT_ERROR && Issue.Code==XCRON_SERVICE_ARGUMENT,
                "save requires the record output");
    xrtClearError();
    Copy=First;
    Reply=liveCollect(S,Q,&Copy);
    testRequire(Reply.Code==XCRON_SERVICE_KERNEL && Reply.Scheduler.Code==XCRON_SCHEDULER_BUSY &&
                !memcmp(&Copy,&First,sizeof(First)),"active lease keeps checkpoint output untouched");
    xrtErrorFree(Reply.Error);
    const xcroncheckpointtarget Busy={Job,liveKey};
    testRequire(xcronServiceSaveCheckpointGroupAsync(S,&Busy,1,&Copy,&Q,NULL),"request group capture with a live member");
    xcronservicecheckpointreply BusyReply=liveCollectGroup(S,Q);
    testRequire(BusyReply.Code==XCRON_SERVICE_KERNEL && BusyReply.Scheduler.Code==XCRON_SCHEDULER_BUSY &&
                BusyReply.Index==0 && !memcmp(&Copy,&First,sizeof(First)),"busy group failure preserves borrowed output");
    xrtErrorFree(BusyReply.Error);
    testRequire(xrtEventSet(&F.RunGate) && xcronServiceWaitReadable(S,10000,NULL)==XWAIT_OK,"wait for physical completion");
    size_t Count;
    xcronservicecompletion Done;
    if(Selected){
        xcronselectedservicecompletion D;
        testRequire(xcronServiceDrainSelected(S,&D,1,&Count,NULL) && Count==1,"selected restored completion");
        Done=D.Completion;
    }else testRequire(xcronServiceDrain(S,&Done,1,&Count,NULL) && Count==1,"actual restored completion");
    testRequire(Done.Result==XCRON_RUN_SUCCEEDED && Done.Job==Job && !Done.Error,"successful physical completion");
    liveCapture(S,Job,&Copy);
    testRequire(memcmp(&Copy,&First,sizeof(First))!=0 &&
                xcronServiceWaitReadable(S,30,NULL)==XWAIT_TIMEOUT,
                "held owner advances checkpoint after physical completion without publishing the due successor");
    liveStop(S,Selected,Job,&F,1);

    testRequire(xrtEventReset(&F.ClockEntered) && xrtEventReset(&F.ClockGate) &&
                liveCreate(Selected,true,&C,&S,NULL) && xrtEventWaitFor(&F.ClockEntered,10000)==XWAIT_OK,"pause before close with accepted restore");
    J=liveJob(&F);
    testRequire(xcronServiceRestoreCheckpointAsync(S,&J,liveKey,&Copy,&Q,NULL),"queue scalar restore before close");
    _Alignas(xcronservicecheckpointreply) xcroncheckpoint Pending=First;
    xcronrequestid G;
    testRequire(xcronServiceSaveCheckpointGroupAsync(S,&Busy,1,&Pending,&G,NULL),"queue borrowed group for close");
    xcronservicecheckpointreply GroupReply={.Index=777};
    testRequire(xcronServiceCollectCheckpointGroup(S,G,0,&GroupReply,NULL)==XWAIT_TIMEOUT && GroupReply.Index==777,
                "a timeout preserves ticket and reply");
    testRequire(xcronServiceCollectCheckpoint(S,G,0,&Reply,&Copy,NULL)==XWAIT_ERROR,"scalar collector rejects group ticket");
    xrtClearError();
    testRequire(xcronServiceCollectCheckpointGroup(S,G,0,(xcronservicecheckpointreply*)&Pending,NULL)==XWAIT_ERROR,
                "group collector cannot overlap the borrowed record output");
    xrtClearError();
    testRequire(xcronServiceClose(S,false,NULL),"close while the grouped borrow is outstanding");
    xcronZoneRelease(J.Zone);
    testRequire(xrtEventSet(&F.ClockGate) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"join before collecting closed restore");
    Reply=liveCollect(S,Q,NULL);
    testRequire(Reply.Code==XCRON_SERVICE_CLOSED && !Reply.Job && Reply.Data==&F,"closed restore returns data without admission");
    xrtErrorFree(Reply.Error);
    GroupReply=liveCollectGroup(S,G);
    testRequire(GroupReply.Code==XCRON_SERVICE_CLOSED && GroupReply.Index==XCRON_CHECKPOINT_NO_INDEX &&
                !memcmp(&Pending,&First,sizeof(First)),"closed capture returns ownership without publishing");
    xrtErrorFree(GroupReply.Error);
    liveStop(S,Selected,0,NULL,0);
    testRequire(xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"host pool remains borrowed");
    xrtEventUnit(&F.RunGate);
    xrtEventUnit(&F.RunEntered);
    xrtEventUnit(&F.ClockGate);
    xrtEventUnit(&F.ClockEntered);
}

enum { LIVE_GROUP_JOBS=3 };
typedef struct livegroupfixture {
    xevent Clocked;
    xcronservice** Service;
    const xcronjobid* Ids;
    unsigned Data[LIVE_GROUP_JOBS];
    bool Selected;
} livegroupfixture;

static bool liveGroupClock(xcronclock* Out,ptr Raw)
{
    livegroupfixture* F=Raw;
    testRequire(*F->Service && F->Ids[0] && F->Ids[1] && F->Ids[2],"clock sees service and entire startup ID array");
    *Out=(xcronclock){3000,1};
    return xrtEventSet(&F->Clocked);
}
static xcronrunresult liveGroupRun(const xcronrun* Run,const xcronexecution* Token,ptr Raw)
{
    livegroupfixture* F=Raw;
    unsigned I=Run->Data==&F->Data[0]?0:1;
    testRequire(Run->Data==&F->Data[I] && Run->Job==F->Ids[I] && Run->ScheduledAt==(xtime)(I+1)*1000,
                "startup job identity, order and process-local data");
    if(F->Selected){
        const xcronexecutionlabel* Label=xcronExecutionSelected(Token);
        testRequire(Label && Label->Candidates==1 && Label->Local.Second==(int32)I+1,"selected startup label");
    }
    return XCRON_RUN_SUCCEEDED;
}

static void liveGroupSeed(livegroupfixture* F,xcronzone* Zone,xcronexpression Exp,
    xcronservicecheckpointjob* Jobs,xcroncheckpoint* Records)
{
    const xcronschedulerconfig Limits={LIVE_GROUP_JOBS,2,16,0,65536};
    xcronscheduler* S=NULL;
    testRequire(F->Selected?xcronSchedulerCreateSelected(&Limits,&S,NULL):xcronSchedulerCreate(&Limits,&S,NULL),
                "create matching startup fixture scheduler");
    xcronjobid Ids[LIVE_GROUP_JOBS];
    for(uint32 I=0;I<LIVE_GROUP_JOBS;++I){
        xtime At=(xtime)(I+1)*1000;
        Jobs[I]=(xcronservicecheckpointjob){{.Zone=Zone,.Expression=Exp,.Begin=At,.End=At,
            .RunningLimit=1,.Data=&F->Data[I]},{300+I,5}};
        testRequire(F->Selected?xcronSchedulerAddSelected(S,&Jobs[I].Config,livePolicy,&Ids[I],NULL):
                    xcronSchedulerAdd(S,&Jobs[I].Config,&Ids[I],NULL),"admit fixture job");
    }
    if(F->Selected){
        for(unsigned Turn=0;;++Turn){
            xcronselectedpoll Poll;
            testRequire(xcronSchedulerPollSelected(S,(xcronclock){0,1},(xcronoccurrencebudget){16,16,16,16,16},
                        NULL,0,&Poll,NULL) && Turn<32,"resolve selected fixture jobs");
            if(!Poll.Searching)break;
        }
    }
    testRequire(xcronSchedulerCancel(S,Ids[2],NULL),"include a terminal group member");
    for(uint32 I=0;I<LIVE_GROUP_JOBS;++I)
        testRequire(xcronSchedulerSaveCheckpoint(S,Ids[I],Jobs[I].Key,&Records[I],NULL),"capture fixture group member");
    testRequire(xcronSchedulerClose(S,NULL),"close fixture scheduler");
    for(uint32 I=0;I<LIVE_GROUP_JOBS;++I){
        bool Found;
        if(F->Selected){
            xcronselectedretired R;
            testRequire(xcronSchedulerReapSelected(S,&R,&Found,NULL) && Found,"reap selected fixture job");
        }else{
            xcronretired R;
            testRequire(xcronSchedulerReap(S,&R,&Found,NULL) && Found,"reap fixture job");
        }
    }
    testRequire(xcronSchedulerDestroy(S,NULL),"destroy group source");
}

static bool liveGroupCreate(livegroupfixture* F,const xcronserviceconfig* C,
    const xcronservicecheckpointgroup* Group,xcronjobid* Out,xcronservicecheckpointissue* Issue)
{
    if(F->Selected){
        xcronselectedserviceconfig Config={*C,1,0};
        return xcronServiceCreateSelectedCheckpointedFromGroup(&Config,Group,F->Service,Out,Issue);
    }
    return xcronServiceCreateCheckpointedFromGroup(C,Group,F->Service,Out,Issue);
}

static void liveGroupTest(bool Selected)
{
    xcronservice* Service=NULL;
    xcronjobid Ids[LIVE_GROUP_JOBS]={0};
    livegroupfixture F={.Service=&Service,.Ids=Ids,.Selected=Selected};
    testRequire(xrtEventInit(&F.Clocked,true,false),"group startup publication observer");
    xcronzone* Zone=NULL;
    xcronexpression Exp={0};
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("UTC0"),XCRON_ZONE_POSIX,&Zone,NULL) &&
                xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&Exp,NULL),"group zone and expression");
    xcronservicecheckpointjob Jobs[LIVE_GROUP_JOBS];
    xcroncheckpoint Records[LIVE_GROUP_JOBS];
    liveGroupSeed(&F,Zone,Exp,Jobs,Records);
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    xallocationbudget* Budget=NULL;
    testRequire(Pool && xrtAllocationBudgetCreate(262144,&Budget),"startup group owner and allocation budget");
    const xcronserviceconfig C={
        .Scheduler={LIVE_GROUP_JOBS,2,16,0,65536},.Executor=Pool,.Proc=liveGroupRun,.Context=&F,
        .Clock=liveGroupClock,.ClockContext=&F,.CommandLimit=2,.CompletionLimit=2,.BatchLimit=2,
        .CommandWorkLimit=2,.ResampleMilliseconds=2,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=262144,.AdapterBytesLimit=65536,.InitializationBudget=Budget
    };
    const xcronservicecheckpointgroup Group={Jobs,Records,LIVE_GROUP_JOBS};
    xcronservicecheckpointissue Issue={.Index=777};
    testRequire(!liveGroupCreate(&F,&C,&Group,(xcronjobid*)Records,&Issue) && Issue.Index==777 &&
                !Service,"input and output aliases preserve issue and service");
    xrtClearError();
    xcronserviceconfig Small=C;
    Small.Scheduler.JobLimit=2;
    testRequire(!liveGroupCreate(&F,&Small,&Group,Ids,&Issue) && Issue.Service.Code==XCRON_SERVICE_LIMIT &&
                Issue.Index==XCRON_SERVICE_CHECKPOINT_NO_INDEX && !Service && !Ids[0],"whole group must fit job capacity");
    xrtClearError();

    Records[1].Bytes[44]^=1;
    testRequire(!liveGroupCreate(&F,&C,&Group,Ids,&Issue) && Issue.Service.Code==XCRON_SERVICE_KERNEL &&
                Issue.Scheduler.Code==XCRON_SCHEDULER_CHECKPOINT && Issue.Index==1 && !Service &&
                !Ids[0] && !Ids[1] && !Ids[2],"late invalid record rolls back earlier startup admission");
    Records[1].Bytes[44]^=1;
    xrtClearError();
    xallocationbudgetinfo Used={.Size=sizeof(Used)};
    testRequire(xrtAllocationBudgetInfo(Budget,&Used) && !Used.LiveCount && !Used.LiveBytes &&
                xrtEventWaitFor(&F.Clocked,0)==XWAIT_TIMEOUT,"rollback releases startup arena and runs no clock");

    Jobs[1].Key=Jobs[0].Key;
    testRequire(!liveGroupCreate(&F,&C,&Group,Ids,&Issue) && Issue.Index==1 &&
                Issue.Scheduler.Code==XCRON_SCHEDULER_CHECKPOINT_MISMATCH && !Service && !Ids[0],
                "duplicate stable job identity aborts the group");
    Jobs[1].Key=(xcroncheckpointkey){301,5};
    xrtClearError();

    xallocationbudget* Refusal=NULL;
    Small=C;
    testRequire(xrtAllocationBudgetCreate(1,&Refusal),"create denial budget");
    Small.InitializationBudget=Refusal;
    testRequire(!liveGroupCreate(&F,&Small,&Group,Ids,&Issue) && Issue.Service.Code==XCRON_SERVICE_MEMORY &&
                Issue.Index==XCRON_SERVICE_CHECKPOINT_NO_INDEX && !Service && !Ids[0],"startup OOM preserves the whole output");
    xrtClearError();
    xrtAllocationBudgetFree(Refusal);

    testRequire(liveGroupCreate(&F,&C,&Group,Ids,&Issue) && !Issue.Service.Code &&
                Issue.Index==XCRON_SERVICE_CHECKPOINT_NO_INDEX &&
                xrtEventWaitFor(&F.Clocked,10000)==XWAIT_OK,"publish complete recovered group");
    xcronservice* Active=Service;
    xcronjobid Unwritten[LIVE_GROUP_JOBS]={1,2,3};
    size_t Arena=liveArena(Service,Selected);
    Small=C;
    Small.BytesLimit=Arena+C.Scheduler.BytesLimit+C.AdapterBytesLimit+sizeof(Ids)-1;
    testRequire(!liveGroupCreate(&F,&Small,&Group,Unwritten,&Issue) && Issue.Service.Code==XCRON_SERVICE_LIMIT &&
                Issue.Index==XCRON_SERVICE_CHECKPOINT_NO_INDEX && Service==Active && Unwritten[2]==3,
                "startup ID scratch counts against total bytes");
    xrtClearError();
    testRequire(xrtAllocationBudgetCreate(Arena,&Refusal),"allow arena but refuse temporary IDs");
    Small=C;
    Small.InitializationBudget=Refusal;
    testRequire(!liveGroupCreate(&F,&Small,&Group,Unwritten,&Issue) && Issue.Service.Code==XCRON_SERVICE_MEMORY &&
                Service==Active && Unwritten[0]==1 && Unwritten[2]==3,"ID scratch allocation failure preserves published outputs");
    xrtClearError();
    testRequire(xrtAllocationBudgetInfo(Refusal,&Used) && Used.PeakBytes==Arena && !Used.LiveBytes,
                "startup scratch OOM returns the preallocated arena");
    xrtAllocationBudgetFree(Refusal);
    const xcroncheckpointtarget Capture[]={{Ids[0],Jobs[0].Key},{Ids[1],Jobs[1].Key},{Ids[2],Jobs[2].Key}};
    xcroncheckpoint Output[LIVE_GROUP_JOBS];
    xcronrequestid Q=0;
    xcronserviceissue Admission;
    testRequire(!xcronServiceSaveCheckpointGroupAsync(Service,Capture,LIVE_GROUP_JOBS,Output,&Q,&Admission) &&
                Admission.Code==XCRON_SERVICE_LIMIT && Q==0,"capture consumes the configured owner work budget");
    xrtClearError();
    testRequire(xcronServiceSaveCheckpointGroupAsync(Service,Capture,2,Output,&Q,NULL),"queue two-job capture");
    xcronservicecheckpointreply Saved=liveCollectGroup(Service,Q);
    testRequire(!Saved.Code && Saved.Index==XCRON_CHECKPOINT_NO_INDEX && !Saved.Error &&
                !memcmp(Output,Records,2*sizeof(*Records)),"group capture after live restore is canonical");
    testRequire(xcronServiceCollectCheckpointGroup(Service,Q,0,&Saved,&Admission)==XWAIT_ERROR &&
                Admission.Code==XCRON_SERVICE_STALE,"group borrow transfers exactly once");
    xrtClearError();
    testRequire(xcronServiceSaveCheckpointGroupAsync(Service,NULL,0,NULL,&Q,NULL),"admit empty capture");
    Saved=liveCollectGroup(Service,Q);
    testRequire(!Saved.Code && !Saved.Error,"collect empty capture");
    xcronZoneRelease(Zone);
    memset(Jobs,0,sizeof(Jobs));
    memset(Records,0,sizeof(Records));
    testRequire(xcronServiceWaitReadable(Service,25,NULL)==XWAIT_TIMEOUT,"both overdue jobs stay paused");
    for(unsigned I=0;I<2;++I){
        liveSetPaused(Service,Selected,Ids[I],false);
        testRequire(xcronServiceWaitReadable(Service,10000,NULL)==XWAIT_OK,"released job finishes");
        size_t Count=0;
        xcronservicecompletion Done;
        if(Selected){
            xcronselectedservicecompletion D;
            testRequire(xcronServiceDrainSelected(Service,&D,1,&Count,NULL) && Count==1,"drain selected startup job");
            Done=D.Completion;
        }else testRequire(xcronServiceDrain(Service,&Done,1,&Count,NULL) && Count==1,"drain startup job");
        testRequire(Done.Job==Ids[I] && Done.Result==XCRON_RUN_SUCCEEDED && !Done.Error,"one job admitted after release");
        testRequire(xcronServiceWaitReadable(Service,25,NULL)==XWAIT_TIMEOUT,"held member remains independent");
    }
    testRequire(xcronServiceClose(Service,false,NULL) && xcronServiceWaitFor(Service,10000,NULL)==XWAIT_OK,"stop recovered owner");
    unsigned Seen=0;
    for(unsigned N=0;N<LIVE_GROUP_JOBS;++N){
        bool Found;
        xcronjobid Id;
        ptr Data;
        xcronjobstate State;
        uint64 Succeeded;
        if(Selected){
            xcronselectedretired R;
            testRequire(xcronServiceReapStoppedSelected(Service,&R,&Found,NULL) && Found,"reap selected restored job");
            Id=R.Id;Data=R.Data;State=R.Info.State;Succeeded=R.Info.Succeeded;
        }else{
            xcronretired R;
            testRequire(xcronServiceReapStopped(Service,&R,&Found,NULL) && Found,"reap restored job");
            Id=R.Id;Data=R.Data;State=R.Info.State;Succeeded=R.Info.Succeeded;
        }
        unsigned I=0;
        while(I<LIVE_GROUP_JOBS && Ids[I]!=Id)++I;
        testRequire(I<LIVE_GROUP_JOBS && !(Seen&(1u<<I)) && Data==&F.Data[I],"return original data exactly once");
        testRequire(I==2?(State==XCRON_JOB_CANCELLED && !Succeeded):(State==XCRON_JOB_EXHAUSTED && Succeeded==1),
                    "preserve terminal checkpoint or physical completion");
        Seen|=1u<<I;
    }
    testRequire(xcronServiceDestroy(Service,NULL) && xrtAllocationBudgetInfo(Budget,&Used) &&
                !Used.LiveCount && !Used.LiveBytes,"successful group releases startup and retained allocations");
    xcronservicecheckpointgroup Empty={0};
    Small=C;
    Small.Clock=NULL;
    Small.ClockContext=NULL;
    Service=NULL;
    testRequire(liveGroupCreate(&F,&Small,&Empty,NULL,&Issue) && Issue.Index==XCRON_SERVICE_CHECKPOINT_NO_INDEX,
                "an empty committed group starts without an ID array");
    liveStop(Service,Selected,0,NULL,0);
    xrtAllocationBudgetFree(Budget);
    testRequire(xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"retire group pool");
    xrtEventUnit(&F.Clocked);
}

typedef struct livegroupturn {
    xevent Entered[2],Release[2];
    uint32 Samples;
} livegroupturn;

static bool liveGroupTurnClock(xcronclock* Out,ptr Raw)
{
    livegroupturn* G=Raw;
    uint32 I=G->Samples++;
    if(I<2 && (!xrtEventSet(&G->Entered[I]) || xrtEventWaitFor(&G->Release[I],10000)!=XWAIT_OK))return false;
    *Out=(xcronclock){0,G->Samples};
    return true;
}

static xcronrunresult liveGroupNoRun(const xcronrun* Run,const xcronexecution* Token,ptr Raw)
{
    (void)Run;(void)Token;(void)Raw;
    testRequire(false,"work-budget fixture has no job to dispatch");
    return XCRON_RUN_FAILED;
}

static void liveGroupWorkBudget(void)
{
    livegroupturn G={0};
    for(unsigned I=0;I<2;++I)
        testRequire(xrtEventInit(&G.Entered[I],true,false) && xrtEventInit(&G.Release[I],true,false),"gate owner turns");
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    const xcronserviceconfig C={
        .Scheduler={2,1,8,0,65536},.Executor=Pool,.Proc=liveGroupNoRun,
        .Clock=liveGroupTurnClock,.ClockContext=&G,.CommandLimit=2,.CompletionLimit=1,.BatchLimit=1,
        .CommandWorkLimit=2,.ResampleMilliseconds=2,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronservice* S=NULL;
    testRequire(Pool && xcronServiceCreateCheckpointed(&C,&S,NULL) &&
                xrtEventWaitFor(&G.Entered[0],10000)==XWAIT_OK,"hold initial owner turn");
    const xcroncheckpointtarget Targets[]={{1,{1,1}},{2,{2,1}}};
    xcroncheckpoint Records[2],Unchanged[2];
    memset(Unchanged,0xf2,sizeof(Unchanged));memcpy(Records,Unchanged,sizeof(Records));
    xcronrequestid A=0,B=0;
    testRequire(xcronServiceReapAsync(S,&A,NULL) &&
                xcronServiceSaveCheckpointGroupAsync(S,Targets,2,Records,&B,NULL) &&
                xrtEventSet(&G.Release[0]) && xrtEventWaitFor(&G.Entered[1],10000)==XWAIT_OK,
                "ordinary command and full-budget capture reach the second clock sample");
    xcronreply Reply;
    xcronservicecheckpointreply Pending={.Index=777};
    testRequire(xcronServiceCollect(S,A,0,&Reply,NULL)==XWAIT_OK && !Reply.Code && !Reply.Found && !Reply.Error &&
                xcronServiceCollectCheckpointGroup(S,B,0,&Pending,NULL)==XWAIT_TIMEOUT && Pending.Index==777,
                "full-budget capture waits for another owner turn after a one-unit command");
    xcroncheckpoint Deferred[2];
    memcpy(Deferred,Unchanged,sizeof(Deferred));
    xcronrequestid Expiring=0;
    testRequire(!xcronServiceQuiesceCheckpointGroupAsync(S,Targets,2,Deferred,0,&Expiring,NULL) && !Expiring &&
                xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"group deadline must be positive");
    testRequire(xcronServiceQuiesceCheckpointGroupAsync(S,Targets,2,Deferred,20,&Expiring,NULL),"queue bounded wait behind full work budget");
    xrtSleep(80);
    testRequire(xrtEventSet(&G.Release[1]),"allow remaining commands to progress");
    Pending=liveCollectGroup(S,B);
    testRequire(Pending.Code==XCRON_SERVICE_KERNEL && Pending.Scheduler.Code==XCRON_SCHEDULER_STALE &&
                Pending.Index==0 && !memcmp(Records,Unchanged,sizeof(Records)),"deferred command completes on next turn");
    xrtErrorFree(Pending.Error);
    Pending=liveCollectGroup(S,Expiring);
    testRequire(Pending.Code==XCRON_SERVICE_TIMEOUT && Pending.Index==XCRON_CHECKPOINT_NO_INDEX &&
                xrtErrorKind(Pending.Error)==XERR_TIMEOUT && !memcmp(Deferred,Unchanged,sizeof(Deferred)),
                "queued deadline expires before validating any member");
    xrtErrorFree(Pending.Error);
    liveStop(S,false,0,NULL,0);
    testRequire(xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"join work-budget fixture");
    for(unsigned I=0;I<2;++I){
        xrtEventUnit(&G.Release[I]);
        xrtEventUnit(&G.Entered[I]);
    }
}


typedef struct livequiescence {
    xevent Started,Release;
    xatomic64 Wall;
} livequiescence;

static bool liveQuiescenceClock(xcronclock* Out,ptr Raw)
{
    livequiescence* F=Raw;
    *Out=(xcronclock){(xtime)xrtAtomic64Load(&F->Wall,XMEMORY_ACQUIRE),0};
    return true;
}

static xcronrunresult liveQuiescenceRun(const xcronrun* Run,const xcronexecution* Token,ptr Raw)
{
    (void)Token;
    livequiescence* F=Raw;
    if(Run->Data==F && Run->ScheduledAt==1000)
        testRequire(xrtEventSet(&F->Started) && xrtEventWaitFor(&F->Release,10000)==XWAIT_OK,"release first physical lease");
    return XCRON_RUN_SUCCEEDED;
}

static void liveQuiescenceCancel(xcronservice* S,bool Selected,xcronjobid Job,xcronschedulererror Expected)
{
    xcronrequestid Q=0;
    testRequire(xcronServiceCancelAsync(S,Job,false,&Q,NULL),"queue cancellation");
    xcronserviceerror Code=Expected?XCRON_SERVICE_KERNEL:XCRON_SERVICE_OK;
    if(Selected){
        xcronselectedreply Reply;
        testRequire(xcronServiceCollectSelected(S,Q,10000,&Reply,NULL)==XWAIT_OK && Reply.Code==Code &&
                    Reply.Scheduler.Code==Expected,"selected cancellation result");
        xrtErrorFree(Reply.Error);
    }else{
        xcronreply Reply;
        testRequire(xcronServiceCollect(S,Q,10000,&Reply,NULL)==XWAIT_OK && Reply.Code==Code &&
                    Reply.Scheduler.Code==Expected,"actual cancellation result");
        xrtErrorFree(Reply.Error);
    }
}

static void liveQuiescenceDrain(xcronservice* S,bool Selected,xcronjobid Job)
{
    testRequire(xcronServiceWaitReadable(S,10000,NULL)==XWAIT_OK,"physical completion is readable");
    size_t Count=0;
    xcronservicecompletion Done={0};
    if(Selected){
        xcronselectedservicecompletion D={0};
        testRequire(xcronServiceDrainSelected(S,&D,1,&Count,NULL),"drain selected quiescence fixture");
        Done=D.Completion;
    }else testRequire(xcronServiceDrain(S,&Done,1,&Count,NULL),"drain actual quiescence fixture");
    testRequire(Count==1 && Done.Job==Job && Done.Result==XCRON_RUN_SUCCEEDED && !Done.Error,"one expected lease completion");
}

static void liveReapAll(xcronservice* S,bool Selected,unsigned Count)
{
    for(unsigned I=0;I<Count;++I){
        bool Found=false;
        if(Selected){
            xcronselectedretired R;
            testRequire(xcronServiceReapStoppedSelected(S,&R,&Found,NULL) && Found,"return selected job ownership");
        }else{
            xcronretired R;
            testRequire(xcronServiceReapStopped(S,&R,&Found,NULL) && Found,"return actual job ownership");
        }
    }
}

typedef enum livequiescencecase {LIVE_GROUP_READY,LIVE_GROUP_TIMEOUT,LIVE_GROUP_CLOSE} livequiescencecase;

static void liveGroupQuiescence(bool Selected,livequiescencecase Case)
{
    livequiescence F={0};
    xrtAtomic64Init(&F.Wall,0);
    testRequire(xrtEventInit(&F.Started,true,false) && xrtEventInit(&F.Release,true,false),"gate an active lease");
    xexecutor* Pool=xrtExecutorCreate(&(xexecutorconfig){2,8,0});
    const xcronserviceconfig C={
        .Scheduler={3,2,16,0,65536},.Executor=Pool,.Proc=liveQuiescenceRun,.Context=&F,
        .Clock=liveQuiescenceClock,.ClockContext=&F,
        .CommandLimit=4,.CompletionLimit=2,.BatchLimit=2,.CommandWorkLimit=2,.ResampleMilliseconds=4000,
        .RetryIntervalMilliseconds=1,.RetryAttempts=1,.BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcronservice* S=NULL;
    testRequire(Pool && liveCreate(Selected,true,&C,&S,NULL),"start group quiescence owner");
    xcronjobconfig J=liveJob(NULL);
    J.Data=&F;
    xcronjobid A=liveAdd(S,Selected,&J);
    J.Data=NULL;
    J.Begin=J.End=2000;
    xcronjobid B=liveAdd(S,Selected,&J);
    if(Case!=LIVE_GROUP_READY)liveSetPaused(S,Selected,B,true);
    xcronjobid Other=B;
    if(Case==LIVE_GROUP_READY){
        Other=liveAdd(S,Selected,&J);
        liveQuiescenceCancel(S,Selected,Other,XCRON_SCHEDULER_OK);
    }
    xcronZoneRelease(J.Zone);
    xrtAtomic64Store(&F.Wall,1000,XMEMORY_RELEASE);
    testRequire(xcronServiceWake(S,NULL) && xrtEventWaitFor(&F.Started,10000)==XWAIT_OK,"first group member holds a physical lease");

    const xcroncheckpointtarget Targets[]={{A,{1,1}},{Other,{2,1}}};
    xcroncheckpoint Records[2],Unchanged[2];
    memset(Unchanged,0x64,sizeof(Unchanged));
    memcpy(Records,Unchanged,sizeof(Records));
    uint64 Deadline=Case==LIVE_GROUP_TIMEOUT?500:10000;
    xcronrequestid Q=0,Barrier=0;
    double Begin=xrtTimer();
    testRequire(xcronServiceQuiesceCheckpointGroupAsync(S,Targets,2,Records,Deadline,&Q,NULL) &&
                xcronServiceReapAsync(S,&Barrier,NULL),"queue group wait before a reap barrier");
    if(Selected){
        xcronselectedreply Reply;
        testRequire(xcronServiceCollectSelected(S,Barrier,10000,&Reply,NULL)==XWAIT_OK &&
                    !Reply.Code && !Reply.Found && !Reply.Error,"reap skips reserved selected members");
    }else{
        xcronreply Reply;
        testRequire(xcronServiceCollect(S,Barrier,10000,&Reply,NULL)==XWAIT_OK &&
                    !Reply.Code && !Reply.Found && !Reply.Error,"reap skips reserved actual members");
    }

    if(Case!=LIVE_GROUP_TIMEOUT){
        xcronservicecheckpointreply Pending={.Index=777};
        testRequire(xcronServiceCollectCheckpointGroup(S,Q,0,&Pending,NULL)==XWAIT_TIMEOUT && Pending.Index==777,
                    "group stays pending until the running lease finishes");
    }
    if(Case==LIVE_GROUP_READY){
        liveQuiescenceCancel(S,Selected,A,XCRON_SCHEDULER_BUSY);
        xcroncheckpoint Competing;
        xcronrequestid Conflict=0;
        testRequire(xcronServiceQuiesceCheckpointGroupAsync(S,Targets,1,&Competing,10000,&Conflict,NULL),"queue competing wait");
        xcronservicecheckpointreply Busy=liveCollectGroup(S,Conflict);
        testRequire(Busy.Code==XCRON_SERVICE_BUSY && Busy.Error,"one waiting group per owner");
        xrtErrorFree(Busy.Error);
        testRequire(xcronServiceSaveCheckpointGroupAsync(S,Targets+1,1,&Competing,&Conflict,NULL),"queue overlapping immediate capture");
        Busy=liveCollectGroup(S,Conflict);
        testRequire(Busy.Code==XCRON_SERVICE_KERNEL && Busy.Scheduler.Code==XCRON_SCHEDULER_BUSY && Busy.Index==0,
                    "overlapping capture cannot take a reserved tombstone");
        xrtErrorFree(Busy.Error);

        xrtAtomic64Store(&F.Wall,2000,XMEMORY_RELEASE);
        testRequire(xcronServiceWake(S,NULL),"advance the unrelated job while the group waits");
        liveQuiescenceDrain(S,Selected,B);
        testRequire(xrtEventSet(&F.Release),"allow reserved lease to complete");
    }else if(Case==LIVE_GROUP_CLOSE){
        testRequire(xcronServiceClose(S,false,NULL),"close while the group waits");
    }

    xcronservicecheckpointreply Reply=liveCollectGroup(S,Q);
    if(Case==LIVE_GROUP_READY){
        testRequire(!Reply.Code && !Reply.Error && Reply.Index==XCRON_CHECKPOINT_NO_INDEX,"capture reserved group together");
        for(unsigned I=0;I<2;++I){
            xcroncheckpointkey Key;
            testRequire(xcronCheckpointGetKey(&Records[I],&Key) &&
                        Key.Job==Targets[I].Key.Job && Key.Revision==Targets[I].Key.Revision,"write records in stable key order");
        }
    }else{
        xcronserviceerror Code=Case==LIVE_GROUP_CLOSE?XCRON_SERVICE_CLOSED:XCRON_SERVICE_TIMEOUT;
        testRequire(Reply.Code==Code && Reply.Index==XCRON_CHECKPOINT_NO_INDEX &&
                    xrtErrorKind(Reply.Error)==(Case==LIVE_GROUP_CLOSE?XERR_CLOSED:XERR_TIMEOUT) &&
                    !memcmp(Records,Unchanged,sizeof(Records)),"failed group leaves all output untouched");
        xrtErrorFree(Reply.Error);
        if(Case==LIVE_GROUP_TIMEOUT)
            testRequire(xrtTimer()-Begin<3,"host deadline wakes before the coarse service-clock resample");
        testRequire(xrtEventSet(&F.Release),"complete lease after group rollback");
    }
    liveQuiescenceDrain(S,Selected,A);
    if(Case==LIVE_GROUP_TIMEOUT){
        xrtAtomic64Store(&F.Wall,2000,XMEMORY_RELEASE);
        testRequire(xcronServiceWake(S,NULL),"advance both jobs after rollback");
        liveQuiescenceDrain(S,Selected,A);
    }
    if(Case!=LIVE_GROUP_CLOSE)
        testRequire(xcronServiceWaitReadable(S,25,NULL)==XWAIT_TIMEOUT,
                    "success holds the successor and rollback preserves pre-existing holds");
    testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"join quiescence owner");
    liveReapAll(S,Selected,Case==LIVE_GROUP_READY?3u:2u);
    testRequire(xcronServiceDestroy(S,NULL) && xrtExecutorClose(Pool) &&
                xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"release quiescence fixture");
    xrtEventUnit(&F.Release);
    xrtEventUnit(&F.Started);
}

typedef enum livecommitcase {
    LIVE_COMMIT_OK,LIVE_COMMIT_STALE,LIVE_COMMIT_CANCEL,LIVE_COMMIT_CLOSE,LIVE_COMMIT_UNCERTAIN
} livecommitcase;

typedef struct livecommitgate {
    xevent Entered,Probe,Probed,Release;
    xcronservice* Service;
    xcronrequestid Request;
    bool Periodic;
} livecommitgate;

static void liveCommitGate(ptr Raw)
{
    livecommitgate* F=Raw;
    testRequire(xrtEventSet(&F->Entered) && xrtEventWaitFor(&F->Probe,10000)==XWAIT_OK,"hold storage executor");
    xcronserviceissue Issue;
    if(F->Periodic){
        xcronservicecheckpointperiodicreport Report={.Generation=UINT64_MAX};
        testRequire(xcronServiceCollectCheckpointPeriodic(F->Service,F->Request,10000,&Report,&Issue)==XWAIT_ERROR &&
                    Issue.Code==XCRON_SERVICE_THREAD && Report.Generation==UINT64_MAX,"refuse periodic wait on storage executor");
    }else{
        xcronservicecheckpointcommitreply Reply={.Generation=UINT64_MAX};
        testRequire(xcronServiceCollectCheckpointCommit(F->Service,F->Request,10000,&Reply,&Issue)==XWAIT_ERROR &&
                    Issue.Code==XCRON_SERVICE_THREAD && Reply.Generation==UINT64_MAX,"refuse wait on the same storage executor");
    }
    testRequire(xrtEventSet(&F->Probed) && xrtEventWaitFor(&F->Release,10000)==XWAIT_OK,"wait for storage release");
}

static void liveCommitWaitQueued(xexecutor* Storage)
{
    double Until=xrtTimer()+5.0;
    xexecutorstats Stats;
    do{
        testRequire(xrtExecutorGet(Storage,&Stats),"inspect storage queue");
        if(Stats.Queued)return;
        xrtSleep(1);
    }while(xrtTimer()<Until);
    testRequire(false,"durable request must reach the storage queue");
}

#if !defined(XCRON_SINGLE_HEADER)
static bool liveCommitNoFlush(xfile File)
{
    (void)File;
    xrtSetErrorInfo(XERR_IO,"test.cron.service",1,"injected flush failure");
    return false;
}
#define xrtFlush liveCommitNoFlush
#define xcronCheckpointGroupOpen xcronCheckpointGroupOpenInjected
#define xcronCheckpointGroupRead xcronCheckpointGroupReadInjected
#define xcronCheckpointGroupCommit xcronCheckpointGroupCommitInjected
#define xcronCheckpointGroupClose xcronCheckpointGroupCloseInjected
#include "../src/checkpoint_group/checkpoint_group.c"
#undef xcronCheckpointGroupClose
#undef xcronCheckpointGroupCommit
#undef xcronCheckpointGroupRead
#undef xcronCheckpointGroupOpen
#undef xrtFlush
#endif

static void liveCommitTest(xroot Root,bool Selected,livecommitcase Case)
{
    livequiescence F={0};
    xrtAtomic64Init(&F.Wall,0);
    xexecutor* Jobs=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    xexecutor* Storage=xrtExecutorCreate(&(xexecutorconfig){1,1,0});
    const xcronserviceconfig Config={
        .Scheduler={3,1,16,0,65536},.Executor=Jobs,.Proc=liveQuiescenceRun,.Context=&F,
        .Clock=liveQuiescenceClock,.ClockContext=&F,.CommandLimit=4,.CompletionLimit=3,.BatchLimit=1,
        .CommandWorkLimit=2,.ResampleMilliseconds=6000,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcroncheckpointgroup* Group=NULL;
    xcroncheckpointwriter* Writer=NULL;
    xcronservice* S=NULL;
    const char Path[]="live.xcg";
    testRequire(Jobs && Storage && liveCreate(Selected,true,&Config,&S,NULL) &&
                xcronCheckpointGroupOpen(Root,Path,301,2,true,&Group)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointWriterCreate(Group,Storage,&Writer),"start live durable group fixture");
#if !defined(XCRON_SINGLE_HEADER)
    /* Inject a failed commit while the writer is idle, leaving its physical
     * store uncertain for the subsequent service transaction. */
    if(Case==LIVE_COMMIT_UNCERTAIN)
        testRequire(xcronCheckpointGroupCommitInjected(Group,0,NULL,0)==XCRON_CHECKPOINT_STORE_UNCERTAIN,
                    "prepare an uncertain writer");
#endif
    xcronjobconfig J=liveJob(NULL);
    J.Begin=2000;J.End=3000;
    xcronjobid A=liveAdd(S,Selected,&J),B=liveAdd(S,Selected,&J);
    liveSetPaused(S,Selected,B,true);
    J.End=2000;
    xcronjobid Other=liveAdd(S,Selected,&J);
    xcronZoneRelease(J.Zone);

    livecommitgate Gate={.Service=S};
    testRequire(xrtEventInit(&Gate.Entered,true,false) && xrtEventInit(&Gate.Probe,true,false) &&
                xrtEventInit(&Gate.Probed,true,false) && xrtEventInit(&Gate.Release,true,false) &&
                xrtExecutorSubmit(Storage,liveCommitGate,&Gate,NULL,NULL) &&
                xrtEventWaitFor(&Gate.Entered,10000)==XWAIT_OK,"queue behind an occupied storage worker");
    const xcroncheckpointtarget Targets[]={{A,{11,1}},{B,{12,1}}};
    union {xcroncheckpoint Records[2];xcronservicecheckpointcommitreply Reply;} Scratch;
    const xcronservicecheckpointcommit C={Writer,Targets,Scratch.Records,Case==LIVE_COMMIT_STALE?1:0,10000,2};
    testRequire(xcronServiceCommitCheckpointGroupAsync(S,&C,&Gate.Request,NULL),"admit a live durable group");
    liveCommitWaitQueued(Storage);
    testRequire(xrtEventSet(&Gate.Probe) && xrtEventWaitFor(&Gate.Probed,10000)==XWAIT_OK,"check reentrant storage wait");
    xcronservicecheckpointreply Wrong={.Index=777};
    xcronservicecheckpointcommitreply Reply={.Generation=UINT64_MAX};
    testRequire(xcronServiceCollectCheckpointGroup(S,Gate.Request,0,&Wrong,NULL)==XWAIT_ERROR && Wrong.Index==777 &&
                xcronServiceCollectCheckpointCommit(S,Gate.Request,0,&Scratch.Reply,NULL)==XWAIT_ERROR &&
                xcronServiceCollectCheckpointCommit(S,Gate.Request,20,&Reply,NULL)==XWAIT_TIMEOUT &&
                Reply.Generation==UINT64_MAX,"pending commit preserves result and scratch ownership");
    liveQuiescenceCancel(S,Selected,A,XCRON_SCHEDULER_BUSY);
    xrtAtomic64Store(&F.Wall,2000,XMEMORY_RELEASE);
    testRequire(xcronServiceWake(S,NULL),"make member and unrelated job due while storage waits");
    liveQuiescenceDrain(S,Selected,Other);
    testRequire(xcronServiceWaitReadable(S,25,NULL)==XWAIT_TIMEOUT,"held jobs cannot run while storage waits");
    if(Case==LIVE_COMMIT_CLOSE)
        testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,25,NULL)==XWAIT_TIMEOUT,
                    "close waits for an admitted durable commit");
    if(Case==LIVE_COMMIT_CANCEL)testRequire(xrtExecutorCancel(Storage),"discard the queued commit");
    testRequire(xrtEventSet(&Gate.Release),"release storage queue");

    testRequire(xcronServiceCollectCheckpointCommit(S,Gate.Request,3000,&Reply,NULL)==XWAIT_OK &&
                Reply.Command.Request==Gate.Request && Reply.Command.Index==XCRON_CHECKPOINT_NO_INDEX && Reply.StorageCompleted,
                "writer wakes the owner before the six-second resampling interval");
    xcroncheckpointstorestatus Status=Case==LIVE_COMMIT_STALE?XCRON_CHECKPOINT_STORE_STALE:
        Case==LIVE_COMMIT_CANCEL?XCRON_CHECKPOINT_STORE_CANCELLED:
        Case==LIVE_COMMIT_UNCERTAIN?XCRON_CHECKPOINT_STORE_UNCERTAIN:XCRON_CHECKPOINT_STORE_OK;
    bool Committed=Status==XCRON_CHECKPOINT_STORE_OK;
    testRequire(Reply.Store==Status && Reply.Command.Code==(Committed?XCRON_SERVICE_OK:XCRON_SERVICE_STORAGE) &&
                Reply.Generation==(Committed?1u:0u),"publish exact durable result and generation");
    xrtErrorFree(Reply.Command.Error);

    if(Case!=LIVE_COMMIT_CLOSE){
        if(Case==LIVE_COMMIT_UNCERTAIN){
            testRequire(xcronServiceWaitReadable(S,25,NULL)==XWAIT_TIMEOUT,"uncertain store stops scheduled members");
            liveSetPaused(S,Selected,A,false);
        }
        liveQuiescenceDrain(S,Selected,A);
        testRequire(xcronServiceWaitReadable(S,25,NULL)==XWAIT_TIMEOUT,"preserve previously paused member");
    }
    if(Case==LIVE_COMMIT_CANCEL){
        xcronrequestid Rejected=0;
        testRequire(xcronServiceCommitCheckpointGroupAsync(S,&C,&Rejected,NULL) &&
                    xcronServiceCollectCheckpointCommit(S,Rejected,3000,&Reply,NULL)==XWAIT_OK &&
                    Reply.Command.Code==XCRON_SERVICE_STORAGE && !Reply.StorageCompleted && !Reply.Generation &&
                    xrtErrorKind(Reply.Command.Error)==XERR_CLOSED,"distinguish writer rejection from a cancelled write");
        xrtErrorFree(Reply.Command.Error);
        xrtAtomic64Store(&F.Wall,3000,XMEMORY_RELEASE);
        testRequire(xcronServiceWake(S,NULL),"advance after writer submission rollback");
        liveQuiescenceDrain(S,Selected,A);
    }
    testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"join durable service owner");
    liveReapAll(S,Selected,3);
    testRequire(xcronServiceDestroy(S,NULL) && xcronCheckpointWriterDestroy(Writer)==XCRON_CHECKPOINT_STORE_OK &&
                xrtExecutorClose(Storage) && xrtExecutorWaitFor(Storage,10000)==XWAIT_OK && xrtExecutorDestroy(Storage) &&
                xrtExecutorClose(Jobs) && xrtExecutorWaitFor(Jobs,10000)==XWAIT_OK && xrtExecutorDestroy(Jobs),
                "release jobs, writer and both executors");
    xrtEventUnit(&Gate.Release);xrtEventUnit(&Gate.Probed);xrtEventUnit(&Gate.Probe);xrtEventUnit(&Gate.Entered);
    xcroncheckpointgroupview View;
    testRequire(xcronCheckpointGroupOpen(Root,Path,301,2,false,&Group)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupRead(Group,&View)==(Committed || Case==LIVE_COMMIT_UNCERTAIN?
                    XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY),"reopen the durable group");
    testRequire(View.Info.Generation==(Committed || Case==LIVE_COMMIT_UNCERTAIN?1u:0u) &&
                View.Count==(Committed?2u:0u) && (!Committed || !memcmp(View.Records,Scratch.Records,sizeof(Scratch.Records))),
                "successful generation stores the exact pre-release records");
    testRequire(xcronCheckpointGroupClose(Group)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,Path),"remove durable fixture");
}

typedef enum liveperiodiccase {
    LIVE_PERIODIC_REPEAT,LIVE_PERIODIC_PRESSURE,LIVE_PERIODIC_CLOSE,LIVE_PERIODIC_STALE,LIVE_PERIODIC_TIMEOUT
} liveperiodiccase;

static void livePeriodicFiller(ptr Raw)
{
    (void)Raw;
}

static xcronservicecheckpointperiodicreport livePeriodicWait(xcronservice* S,xcronrequestid Q,uint64 Finished,
    xcronserviceerror Code)
{
    double Until=xrtTimer()+10;
    for(;;){
        xcronservicecheckpointperiodicreport Report;
        testRequire(xcronServiceInspectCheckpointPeriodic(S,Q,&Report,NULL),"inspect checkpoint schedule");
        if(Report.Finished>=Finished && Report.Last.Command.Code==Code)return Report;
        xrtErrorFree(Report.Last.Command.Error);
        testRequire(!Report.Stopped && xrtTimer()<Until,"periodic attempt must reach the expected result");
        xrtSleep(2);
    }
}

static void livePeriodicTest(xroot Root,bool Selected,liveperiodiccase Case)
{
    livequiescence F={0};
    xrtAtomic64Init(&F.Wall,0);
    xexecutor* Jobs=xrtExecutorCreate(&(xexecutorconfig){1,4,0});
    xexecutor* Storage=xrtExecutorCreate(&(xexecutorconfig){1,1,0});
    const xcronserviceconfig Config={
        .Scheduler={1,1,16,0,65536},.Executor=Jobs,.Proc=liveQuiescenceRun,.Context=&F,
        .Clock=liveQuiescenceClock,.ClockContext=&F,.CommandLimit=1,.CompletionLimit=1,.BatchLimit=1,
        .CommandWorkLimit=1,.ResampleMilliseconds=6000,.RetryIntervalMilliseconds=1,.RetryAttempts=1,
        .BytesLimit=262144,.AdapterBytesLimit=65536
    };
    xcroncheckpointgroup* Group=NULL;
    xcroncheckpointwriter* Writer=NULL;
    xcronservice* S=NULL;
    const char Path[]="periodic.xcg";
    testRequire(Jobs && Storage && liveCreate(Selected,true,&Config,&S,NULL) &&
                xcronCheckpointGroupOpen(Root,Path,302,1,true,&Group)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointWriterCreate(Group,Storage,&Writer),"create periodic storage fixture");
    const bool Timeout=Case==LIVE_PERIODIC_TIMEOUT;
    xcronjobconfig J=liveJob(NULL);
    if(Timeout)J.Data=&F;
    xcronjobid Job=liveAdd(S,Selected,&J);
    xcronZoneRelease(J.Zone);
    if(Timeout){
        testRequire(xrtEventInit(&F.Started,true,false) && xrtEventInit(&F.Release,true,false),"gate a lease through its checkpoint deadline");
        xrtAtomic64Store(&F.Wall,1000,XMEMORY_RELEASE);
        testRequire(xcronServiceWake(S,NULL) && xrtEventWaitFor(&F.Started,10000)==XWAIT_OK,"occupy the job lease");
    }else liveSetPaused(S,Selected,Job,true);

    livecommitgate Gate={.Service=S,.Periodic=true};
    if(!Timeout)
        testRequire(xrtEventInit(&Gate.Entered,true,false) && xrtEventInit(&Gate.Probe,true,false) &&
                    xrtEventInit(&Gate.Probed,true,false) && xrtEventInit(&Gate.Release,true,false) &&
                    xrtExecutorSubmit(Storage,liveCommitGate,&Gate,NULL,NULL) &&
                    xrtEventWaitFor(&Gate.Entered,10000)==XWAIT_OK,"occupy storage before first periodic tick");
    if(Case==LIVE_PERIODIC_PRESSURE)
        testRequire(xrtExecutorSubmit(Storage,livePeriodicFiller,NULL,NULL,NULL),"fill the bounded storage queue");
    const xcroncheckpointtarget Target={Job,{72,1}};
    union {xcroncheckpoint Record;xcronservicecheckpointperiodicreport Output;} Scratch;
    xcronservicecheckpointperiodic P={{Writer,&Target,&Scratch.Record,Case==LIVE_PERIODIC_STALE?1:0,
        Timeout?40:4000,1},60};
    testRequire(xcronServiceStartCheckpointPeriodicAsync(S,&P,&Gate.Request,NULL),"register a host-monotonic checkpoint cadence");
    xcronrequestid Duplicate=999;
    xcronserviceissue Issue;
    testRequire(!xcronServiceStartCheckpointPeriodicAsync(S,&P,&Duplicate,&Issue) &&
                Issue.Code==XCRON_SERVICE_BUSY && Duplicate==999,"one outstanding schedule ticket");
    testRequire(!xcronServiceInspectCheckpointPeriodic(S,Gate.Request,&Scratch.Output,NULL),"observe without aliasing scratch");

    xcronservicecheckpointperiodicreport Report;
    if(Timeout){
        Report=livePeriodicWait(S,Gate.Request,1,XCRON_SERVICE_TIMEOUT);
        testRequire(!Report.Last.StorageCompleted && !Report.Committed && xrtErrorKind(Report.Last.Command.Error)==XERR_TIMEOUT,
                    "a periodic capture deadline rolls back and leaves the schedule armed");
        xrtErrorFree(Report.Last.Command.Error);
        testRequire(xrtEventSet(&F.Release),"allow a subsequent timed capture");
        liveQuiescenceDrain(S,Selected,Job);
    }else{
        if(Case==LIVE_PERIODIC_PRESSURE){
            Report=livePeriodicWait(S,Gate.Request,1,XCRON_SERVICE_STORAGE);
            testRequire(!Report.Last.StorageCompleted && !Report.Committed &&
                        xrtErrorKind(Report.Last.Command.Error)==XERR_AGAIN,"temporary writer rejection keeps the schedule armed");
            xrtErrorFree(Report.Last.Command.Error);
        }else liveCommitWaitQueued(Storage);
        testRequire(xrtEventSet(&Gate.Probe) && xrtEventWaitFor(&Gate.Probed,10000)==XWAIT_OK,"verify storage reentrant refusal");
        if(Case==LIVE_PERIODIC_REPEAT){
            xrtSleep(205);
            testRequire(xcronServiceInspectCheckpointPeriodic(S,Gate.Request,&Report,NULL) &&
                        Report.Started==1 && !Report.Finished && !Report.Committed,"do not stack missed ticks behind a slow write");
            xrtErrorFree(Report.Last.Command.Error);
        }
        if(Case==LIVE_PERIODIC_CLOSE){
            testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,25,NULL)==XWAIT_TIMEOUT &&
                        xcronServiceCollectCheckpointPeriodic(S,Gate.Request,0,&Report,NULL)==XWAIT_TIMEOUT,
                        "close drains an admitted periodic write");
        }
        testRequire(xrtEventSet(&Gate.Release),"allow storage to progress");
    }

    if(Case!=LIVE_PERIODIC_CLOSE && Case!=LIVE_PERIODIC_STALE){
        Report=livePeriodicWait(S,Gate.Request,Case==LIVE_PERIODIC_REPEAT?3:2,XCRON_SERVICE_OK);
        testRequire(Report.Committed && Report.Last.Generation==Report.Generation &&
                    (Case!=LIVE_PERIODIC_REPEAT || Report.Skipped>=2),"advance generation and coalesce missed ticks");
        xrtErrorFree(Report.Last.Command.Error);
        testRequire(xcronServiceStopCheckpointPeriodic(S,Gate.Request,NULL),"stop with the only ticket still occupied");
    }
    testRequire(xcronServiceCollectCheckpointPeriodic(S,Gate.Request,10000,&Report,NULL)==XWAIT_OK && Report.Stopped &&
                Report.Last.Command.Request==Gate.Request && Report.Started==Report.Finished,"collect the drained periodic schedule");
    uint64 Generation=Report.Generation;
    if(Case==LIVE_PERIODIC_STALE){
        testRequire(Report.Started==1 && !Report.Committed && Report.Last.StorageCompleted &&
                    Report.Last.Store==XCRON_CHECKPOINT_STORE_STALE,"stale storage stops the schedule");
        Generation=0;
    }else{
        testRequire(Report.Committed==Generation && (Case!=LIVE_PERIODIC_CLOSE || Generation==1),
                    "only successful writes advance generation");
    }
    xrtErrorFree(Report.Last.Command.Error);

    if(Case!=LIVE_PERIODIC_CLOSE){
        P.Commit.ExpectedGeneration=Generation;
        P.EveryMilliseconds=UINT64_C(60000);
        testRequire(xcronServiceStartCheckpointPeriodicAsync(S,&P,&Duplicate,NULL) && Duplicate!=Gate.Request &&
                    !xcronServiceStopCheckpointPeriodic(S,Gate.Request,&Issue) && Issue.Code==XCRON_SERVICE_STALE &&
                    xcronServiceStopCheckpointPeriodic(S,Duplicate,NULL) &&
                    xcronServiceCollectCheckpointPeriodic(S,Duplicate,10000,&Report,NULL)==XWAIT_OK &&
                    Report.Stopped && !Report.Started && Report.Generation==Generation,"rearm and stop before the first tick");
        xrtErrorFree(Report.Last.Command.Error);
    }
    testRequire(xcronServiceClose(S,false,NULL) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"join periodic service");
    liveReapAll(S,Selected,1);
    testRequire(xcronServiceDestroy(S,NULL) && xcronCheckpointWriterDestroy(Writer)==XCRON_CHECKPOINT_STORE_OK &&
                xrtExecutorDestroy(Storage) && xrtExecutorDestroy(Jobs),"reclaim schedule and executors");
    if(Timeout){xrtEventUnit(&F.Release);xrtEventUnit(&F.Started);}
    else{
        xrtEventUnit(&Gate.Release);xrtEventUnit(&Gate.Probed);xrtEventUnit(&Gate.Probe);xrtEventUnit(&Gate.Entered);
    }
    xcroncheckpointgroupview View;
    testRequire(xcronCheckpointGroupOpen(Root,Path,302,1,false,&Group)==XCRON_CHECKPOINT_STORE_OK &&
                xcronCheckpointGroupRead(Group,&View)==(Generation?XCRON_CHECKPOINT_STORE_OK:XCRON_CHECKPOINT_STORE_EMPTY) &&
                View.Info.Generation==Generation && View.Count==(Generation?1u:0u),"recover only committed periodic generations");
    testRequire(xcronCheckpointGroupClose(Group)==XCRON_CHECKPOINT_STORE_OK && xrtRootRemove(Root,Path),"remove periodic fixture");
}

static void liveCommitTests(void)
{
    cstr Base=getenv("TMPDIR");
#if defined(_WIN32)
    if(!Base || !*Base)Base=getenv("TEMP");
#else
    if(!Base || !*Base)Base="/tmp";
#endif
    if(!Base || !*Base)Base=".";
    char Name[96];
    snprintf(Name,sizeof(Name),"xcron-service-commit-%lld-%lld",(long long)xrtNow(),(long long)(xrtTimer()*1000000));
    xroot Parent=xrtRootOpen(Base);
    testRequire(Parent && xrtRootDirCreate(Parent,Name,0700),"create live commit test directory");
    xroot Root=xrtRootOpenIn(Parent,Name);
    testRequire(Root!=NULL,"open live commit test directory");
    liveCommitTest(Root,false,LIVE_COMMIT_OK);
    liveCommitTest(Root,true,LIVE_COMMIT_OK);
    liveCommitTest(Root,false,LIVE_COMMIT_STALE);
    liveCommitTest(Root,true,LIVE_COMMIT_CANCEL);
    liveCommitTest(Root,true,LIVE_COMMIT_CLOSE);
    livePeriodicTest(Root,false,LIVE_PERIODIC_REPEAT);
    livePeriodicTest(Root,true,LIVE_PERIODIC_PRESSURE);
    livePeriodicTest(Root,false,LIVE_PERIODIC_CLOSE);
    livePeriodicTest(Root,true,LIVE_PERIODIC_STALE);
    livePeriodicTest(Root,false,LIVE_PERIODIC_TIMEOUT);
#if !defined(XCRON_SINGLE_HEADER)
    liveCommitTest(Root,false,LIVE_COMMIT_UNCERTAIN);
#endif
    testRequire(xrtRootClose(Root) && xrtRootRemove(Parent,Name) && xrtRootClose(Parent),"remove live commit test directory");
}

int main(void)
{
    liveCommitTests();
    for(unsigned Selected=0;Selected<2;++Selected){
        liveGroupQuiescence(Selected!=0,LIVE_GROUP_READY);
        liveGroupQuiescence(Selected!=0,LIVE_GROUP_TIMEOUT);
        liveGroupQuiescence(Selected!=0,LIVE_GROUP_CLOSE);
    }
    liveGroupWorkBudget();
    liveGroupTest(false);
    liveGroupTest(true);
    liveRoundtrip(false);
    liveRoundtrip(true);
    puts("live cron service checkpoint tests passed");
    return 0;
}
