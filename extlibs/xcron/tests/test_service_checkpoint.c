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
    testRequire(xcronServiceRestoreCheckpointAsync(S,&J,liveKey,&Copy,&Q,NULL) && xcronServiceClose(S,false,NULL),"close with queued restore");
    xcronZoneRelease(J.Zone);
    testRequire(xrtEventSet(&F.ClockGate) && xcronServiceWaitFor(S,10000,NULL)==XWAIT_OK,"join before collecting closed restore");
    Reply=liveCollect(S,Q,NULL);
    testRequire(Reply.Code==XCRON_SERVICE_CLOSED && !Reply.Job && Reply.Data==&F,"closed restore returns data without admission");
    xrtErrorFree(Reply.Error);
    liveStop(S,Selected,0,NULL,0);
    testRequire(xrtExecutorClose(Pool) && xrtExecutorWaitFor(Pool,10000)==XWAIT_OK && xrtExecutorDestroy(Pool),"host pool remains borrowed");
    xrtEventUnit(&F.RunGate);
    xrtEventUnit(&F.RunEntered);
    xrtEventUnit(&F.ClockGate);
    xrtEventUnit(&F.ClockEntered);
}

int main(void)
{
    liveRoundtrip(false);
    liveRoundtrip(true);
    puts("live cron service checkpoint tests passed");
    return 0;
}
