#include "test.h"
#include <xcron/service_checkpoint.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <string.h>

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
    testRequire(xrtEventSet(&G.Release[1]),"allow remaining command to progress");
    Pending=liveCollectGroup(S,B);
    testRequire(Pending.Code==XCRON_SERVICE_KERNEL && Pending.Scheduler.Code==XCRON_SCHEDULER_STALE &&
                Pending.Index==0 && !memcmp(Records,Unchanged,sizeof(Records)),"deferred command completes on next turn");
    xrtErrorFree(Pending.Error);
    liveStop(S,false,0,NULL,0);
    testRequire(xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"join work-budget fixture");
    for(unsigned I=0;I<2;++I){
        xrtEventUnit(&G.Release[I]);
        xrtEventUnit(&G.Entered[I]);
    }
}

int main(void)
{
    liveGroupWorkBudget();
    liveGroupTest(false);
    liveGroupTest(true);
    liveRoundtrip(false);
    liveRoundtrip(true);
    puts("live cron service checkpoint tests passed");
    return 0;
}
