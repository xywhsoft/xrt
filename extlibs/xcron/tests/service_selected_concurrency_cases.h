#include "tzif_fixture.h"
static xcronrunresult ssHoldProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    ssfixture* F=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);testRequire(R->Data==F && L && L->Candidates==F->Candidates && !memcmp(&L->Local,&F->Local,sizeof(F->Local)),"faulted selected job retains original worker snapshot");xrtAtomic32FetchAdd(&F->Started,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&F->Gate,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"selected faulted physical tail release");xrtThreadYield();}
    return xcronExecutionCancelled(T)?XCRON_RUN_CANCELLED:XCRON_RUN_SUCCEEDED;
}
static void testSSFutureFault(void)
{
    tzifblob Blob;int64 Times[]={-1,(int64)INT32_MAX+1};uint8 Indices[]={0,0};tzifkind Types[]={{0,false,"UTC"}};tzifBuild(&Blob,'3',Times,Indices,2,Types,1,"");xcronzone* Z=NULL;testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"selected service global UNKNOWN fixture");xdatetime Local;xrtTimeSplit(XRT_TIME_UNIX_EPOCH,&Local);Local.Offset=0;Local.IsDST=-1;
    ssfixture F=ssFixture(XRT_TIME_UNIX_EPOCH+2000,&Local,1);xrtAtomic32Store(&F.Gate,0,XMEMORY_RELEASE);xexecutor* E=ssPool(1,8);xcronselectedserviceconfig C=ssConfig(E,&F);C.Service.Proc=ssHoldProc;C.Service.Scheduler.WorkLimit=1;ssStart(&F,&C);xcronjobconfig J=ssJob(Z,&F,XRT_TIME_UNIX_EPOCH,XRT_TIME_UNIX_EPOCH+2000);testRequire(xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"),XCRON_SECONDS6,&J.Expression,NULL),"global UNKNOWN recurrence");xcronjobid Job=ssAdd(&F,&J,(xcrondeliverypolicy){0,0});xcronZoneRelease(Z);ssUntil(&F.Started,1,"first selected body runs before future query fault");
    xcronselectedjobinfo Info;double End=xrtTimer()+10;do{Info=ssInfo(F.Service,Job);testRequire(xrtTimer()<End,"future global selected fault observed");}while(Info.State!=XCRON_JOB_FAULTED);testRequire(Info.Running==1 && Info.LastIssue.Code==XCRON_OCCURRENCE_ZONE && Info.LastIssue.Zone==XCRON_ZONE_UNKNOWN && Info.LastIssue.When==J.Begin+1000,"future completeness fault preserves active lease and full typed issue");
    xcronrequestid Q;testRequire(xcronServiceCancelAsync(F.Service,Job,true,&Q,NULL),"cancel faulted job with live selected body");xcronselectedreply Reply=ssReply(F.Service,Q);testRequire(Reply.Code==XCRON_SERVICE_OK && !Reply.Error && xcronServiceReapAsync(F.Service,&Q,NULL),"reap selected fault while body retains Data");Reply=ssReply(F.Service,Q);testRequire(Reply.Code==XCRON_SERVICE_OK && !Reply.Found,"future fault cannot prematurely return Data");
    xrtAtomic32Store(&F.Gate,1,XMEMORY_RELEASE);ssStop(&F);xcronselectedretired R;bool Found;testRequire(xcronServiceReapStoppedSelected(F.Service,&R,&Found,NULL) && Found && R.Id==Job && R.Data==&F && R.Info.Cancelled==1 && R.Info.LastIssue.Code==XCRON_OCCURRENCE_ZONE && R.Info.LastIssue.Zone==XCRON_ZONE_UNKNOWN,"future selected fault survives cancel/physical completion/retirement");xcronselectedservicecompletion D=ssDone(F.Service);testRequire(D.Completion.ScheduledAt==J.Begin && D.Completion.Result==XCRON_RUN_CANCELLED && D.Candidates==1 && !D.Completion.Error && !memcmp(&D.Local,&Local,sizeof(Local)),"earlier physical metadata remains valid beside later global fault");ssClear(&F);ssPoolEnd(E);
}
static void testSSQueuedPoliciesAndStaleUnion(void)
{
    xcronzone* Z=NULL;xtime At[2];xdatetime Local;ssFoldFixture(&Z,At,&Local);ssfixture F=ssFixture(At[1],&Local,2);xexecutor* E=ssPool(2,8);xcronselectedserviceconfig C=ssConfig(E,&F);C.Service.CommandLimit=2;xrtAtomic32Store(&F.HoldClock,1,XMEMORY_RELEASE);ssStart(&F,&C);ssUntil(&F.ClockHeld,1,"hold owner before policy/config snapshots");xcronjobconfig J=ssJob(Z,&F,At[0],At[1]);xcronrequestid Q[2];xcrondeliverypolicy Policy={1,1};
    testRequire(xcronServiceAddSelectedAsync(F.Service,&J,Policy,&Q[0],NULL),"queue copied FIRST policy");Policy.Fold=XCRON_FOLD_LAST;testRequire(xcronServiceAddSelectedAsync(F.Service,&J,Policy,&Q[1],NULL),"queue copied LAST policy");Policy.Fold=XCRON_FOLD_REJECT;memset(&J,0xa5,sizeof(J));xcronZoneRelease(Z);xrtAtomic32Store(&F.HoldClock,0,XMEMORY_RELEASE);xcronjobid Jobs[2];
    for(unsigned N=0;N<2;++N){xcronselectedreply R=ssReply(F.Service,Q[N]);testRequire(R.Code==XCRON_SERVICE_OK && R.Job && !R.Data && !R.Error,"queued selected config/zone/policy admission");Jobs[N]=R.Job;}
    unsigned Seen=0;for(unsigned N=0;N<2;++N){xcronselectedservicecompletion D=ssDone(F.Service);unsigned Index=D.Completion.Job==Jobs[0]?0:1;testRequire(D.Completion.Job==Jobs[Index] && D.Completion.ScheduledAt==At[Index] && !(Seen&(1u<<Index)) && D.Candidates==2 && D.Completion.Result==XCRON_RUN_SUCCEEDED && !D.Completion.Error,"queued policies remain independent after caller mutation");Seen|=1u<<Index;}
    double End=xrtTimer()+10;for(unsigned N=0;N<2;++N){xcronselectedjobinfo Info;do{Info=ssInfo(F.Service,Jobs[N]);testRequire(xrtTimer()<End,"queued selected job reaches exhaustion");}while(Info.State==XCRON_JOB_SEARCHING);testRequire(Info.State==XCRON_JOB_EXHAUSTED && Info.Policy.Fold==(N?XCRON_FOLD_LAST:XCRON_FOLD_FIRST),"full copied selected Info policy");}
    xcronrequestid Ticket;testRequire(xcronServiceReapAsync(F.Service,&Ticket,NULL),"selected live retirement request");xcronselectedreply R=ssReply(F.Service,Ticket);testRequire(R.Code==XCRON_SERVICE_OK && R.Found && R.Retired.Data==&F && R.Retired.Info.Succeeded==1,"selected retirement payload in command union");xcronjobid Reaped=R.Retired.Id;testRequire(xcronServiceInfoAsync(F.Service,Reaped,&Ticket,NULL),"reuse retired command slot for failed Info");R=ssReply(F.Service,Ticket);
    testRequire(R.Code==XCRON_SERVICE_KERNEL && R.Error && !R.Found && !R.Data && !R.Info.Running && !R.Info.Dispatched && !R.Retired.Id && !R.Retired.Data,"failed selected Info cannot expose stale retired union Data");xrtErrorFree(R.Error);ssStop(&F);ssClear(&F);ssPoolEnd(E);
}
typedef struct ssreader {xcronservice* Service;xcronjobid Job;xatomic32 Done;} ssreader;
static int32 ssReadMany(ptr Raw)
{
    ssreader* R=Raw;
    for(unsigned N=0;N<128;++N){xcronrequestid Q;double End=xrtTimer()+10;for(;;){xcronserviceissue I;if(xcronServiceInfoAsync(R->Service,R->Job,&Q,&I))break;testRequire(I.Code==XCRON_SERVICE_LIMIT && xrtTimer()<End,"selected concurrent ticket backpressure");xrtClearError();xrtThreadYield();}
        xcronselectedreply Reply=ssReply(R->Service,Q);testRequire(Reply.Code==XCRON_SERVICE_OK && Reply.Job==R->Job && !Reply.Error && Reply.Info.Policy.Fold==XCRON_FOLD_LAST && Reply.Info.Policy.Missing==XCRON_MISSING_REJECT,"parallel readers receive complete selected policy payloads");}
    xrtAtomic32Store(&R->Done,1,XMEMORY_RELEASE);return 0;
}
typedef struct sscollectrace {xcronservice* Service;xcronrequestid Ticket;xatomic32* Start;bool Won;} sscollectrace;
static int32 ssRaceCollect(ptr Raw)
{
    sscollectrace* R=Raw;ssUntil(R->Start,1,"selected competing collectors released");xcronselectedreply Reply,Before;memset(&Reply,0x5a,sizeof(Reply));Before=Reply;xcronserviceissue I;xwaitresult W=xcronServiceCollectSelected(R->Service,R->Ticket,10000,&Reply,&I);
    if(W==XWAIT_OK){testRequire(Reply.Code==XCRON_SERVICE_OK && !Reply.Error && Reply.Found && Reply.Retired.Data,"winning selected collector owns retirement");R->Won=true;}
    else {testRequire(W==XWAIT_ERROR && I.Code==XCRON_SERVICE_STALE && !memcmp(&Reply,&Before,sizeof(Reply)),"losing selected collector leaves all output untouched");xrtClearError();}
    return 0;
}
static void testSSConcurrency(void)
{
    xcronzone* Z=NULL;xtime At[2];xdatetime Local;ssFoldFixture(&Z,At,&Local);ssfixture F=ssFixture(At[1],&Local,2);xexecutor* E=ssPool(2,8);xcronselectedserviceconfig C=ssConfig(E,&F);ssStart(&F,&C);xcronjobconfig J=ssJob(Z,&F,At[1]+100000,At[1]+100000);xcronjobid Job=ssAdd(&F,&J,(xcrondeliverypolicy){2,1});xcronZoneRelease(Z);ssreader Readers[4];xthread* Threads[4];
    for(unsigned N=0;N<4;++N){Readers[N]=(ssreader){.Service=F.Service,.Job=Job};Threads[N]=xrtThreadCreate(ssReadMany,&Readers[N],0);testRequire(Threads[N]!=NULL,"selected parallel control reader");}
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],10000)==XWAIT_OK && xrtAtomic32Load(&Readers[N].Done,XMEMORY_ACQUIRE),"selected concurrent reader joined");xrtThreadDestroy(Threads[N]);}
    xcronselectedjobinfo Info=ssInfo(F.Service,Job);testRequire(Info.State==XCRON_JOB_EXHAUSTED,"future nonmatching selected job exhausted");xcronrequestid Q;testRequire(xcronServiceReapAsync(F.Service,&Q,NULL),"one selected retirement for competing collectors");xatomic32 Start={0};sscollectrace Race[2]={{.Service=F.Service,.Ticket=Q,.Start=&Start},{.Service=F.Service,.Ticket=Q,.Start=&Start}};xthread* T[2];for(unsigned N=0;N<2;++N){T[N]=xrtThreadCreate(ssRaceCollect,&Race[N],0);testRequire(T[N]!=NULL,"selected competing collector");}xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);
    for(unsigned N=0;N<2;++N){testRequire(xrtThreadWaitFor(T[N],10000)==XWAIT_OK,"selected competing collector joined");xrtThreadDestroy(T[N]);}testRequire((unsigned)Race[0].Won+(unsigned)Race[1].Won==1,"selected retirement Data duty transferred exactly once");ssStop(&F);ssClear(&F);ssPoolEnd(E);
}
static void testSSNativeQueuedCancel(void)
{
    xcronzone* Z=NULL;xtime At[2];xdatetime Local;ssFoldFixture(&Z,At,&Local);ssfixture F=ssFixture(At[1],&Local,2);xexecutor* E=ssPool(1,8);ssblocker Block={0};testRequire(xrtExecutorSubmit(E,ssBlock,&Block,NULL,NULL),"hold host worker before selected native queue");ssUntil(&Block.Started,1,"host blocker started");xcronselectedserviceconfig C=ssConfig(E,&F);ssStart(&F,&C);xcronjobconfig J=ssJob(Z,&F,At[0],At[1]);xcronjobid Job=ssAdd(&F,&J,(xcrondeliverypolicy){0,1});xcronZoneRelease(Z);
    double End=xrtTimer()+10;for(;;){xcronselectedservicestats T=ssStats(F.Service);if(T.Service.Executor.Active==2 && !T.Service.Unsent)break;testRequire(xrtTimer()<End,"selected pair admitted to native queue");xrtThreadYield();}
    xcronrequestid Q;testRequire(xcronServiceCancelAsync(F.Service,Job,true,&Q,NULL),"cancel physically queued selected pair");xcronselectedreply Reply=ssReply(F.Service,Q);testRequire(!Reply.Code && !Reply.Error,"selected queued cancellation reply");testRequire(xcronServiceClose(F.Service,true,NULL) && xcronServiceWaitFor(F.Service,0,NULL)==XWAIT_TIMEOUT,"selected queued duties await physical native return");xrtAtomic32Store(&Block.Gate,1,XMEMORY_RELEASE);testRequire(xcronServiceWaitFor(F.Service,10000,NULL)==XWAIT_OK && !xrtAtomic64Load(&F.Calls,XMEMORY_ACQUIRE),"native cancellation skips selected Proc but returns both leases");
    unsigned Seen=0;for(unsigned N=0;N<2;++N){xcronselectedservicecompletion D=ssDone(F.Service);unsigned K=D.Completion.ScheduledAt==At[0]?0:1;testRequire(!(Seen&(1u<<K)) && D.Completion.ScheduledAt==At[K] && D.Completion.Result==XCRON_RUN_CANCELLED && D.Completion.Code==XCRON_COMPLETION_NORMAL && D.Completion.Attempts==1 && !D.Completion.Error && D.Candidates==2 && !memcmp(&D.Local,&Local,sizeof(Local)),"queued native cancellation preserves complete nominal receipt without Proc");Seen|=1u<<K;}testRequire(Seen==3,"both queued nominal receipts exactly once");ssClear(&F);ssPoolEnd(E);
}
