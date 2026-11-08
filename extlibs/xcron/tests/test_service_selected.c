#include "test.h"
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
typedef struct ssfixture {
    xcronservice* Service;xatomic64 Wall,Mono,Calls;
    xatomic32 HoldClock,ClockEntered,ClockHeld,Gate,Started,FailClock,Probes,ProbeDone;
    xcronrequestid ProbeRequest;
    xdatetime Local;size_t Candidates;int Mode;
} ssfixture;
static void ssUntil(const xatomic32* V,uint32 N,cstr Text)
{double End=xrtTimer()+10;while(xrtAtomic32Load(V,XMEMORY_ACQUIRE)<N){testRequire(xrtTimer()<End,Text);xrtThreadYield();}}
static bool ssClock(xcronclock* C,ptr Raw)
{
    ssfixture* F=Raw;xrtAtomic32Store(&F->ClockEntered,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(xrtAtomic32Load(&F->HoldClock,XMEMORY_ACQUIRE)){xrtAtomic32Store(&F->ClockHeld,1,XMEMORY_RELEASE);testRequire(xrtTimer()<End,"selected owner clock gate");xrtThreadYield();}
    xrtAtomic32Store(&F->ClockHeld,0,XMEMORY_RELEASE);
    if(xrtAtomic32Load(&F->FailClock,XMEMORY_ACQUIRE)){xrtSetErrorInfo(XERR_IO,"selected.clock",37,"clock failed");return false;}
    *C=(xcronclock){(xtime)xrtAtomic64Load(&F->Wall,XMEMORY_ACQUIRE),xrtAtomic64Load(&F->Mono,XMEMORY_ACQUIRE)};
    if(xrtAtomic32Exchange(&F->Probes,0,XMEMORY_ACQ_REL)){xcronserviceissue I;xcronselectedreply R;
        testRequire(xcronServiceReapAsync(F->Service,&F->ProbeRequest,NULL),"selected owner can admit an async command");
        testRequire(xcronServiceCollectSelected(F->Service,F->ProbeRequest,10,&R,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"selected owner blocking collect refused");xrtClearError();
        testRequire(xcronServiceCollectSelected(F->Service,F->ProbeRequest,0,&R,NULL)==XWAIT_TIMEOUT,"selected owner nonblocking collect allowed");
        testRequire(xcronServiceWaitFor(F->Service,10,&I)==XWAIT_ERROR && I.Code==XCRON_SERVICE_THREAD,"selected owner blocking join refused");xrtClearError();xrtAtomic32Store(&F->ProbeDone,1,XMEMORY_RELEASE);
    }
    return true;
}
static xcronrunresult ssProc(const xcronrun* R,const xcronexecution* T,ptr Raw)
{
    ssfixture* F=Raw;const xcronexecutionlabel* L=xcronExecutionSelected(T);testRequire(R->Data==F && L && L->Candidates==F->Candidates && !memcmp(&L->Local,&F->Local,sizeof(F->Local)),"selected service delivers immutable nominal snapshot and original Data");
    xrtAtomic64FetchAdd(&F->Calls,1,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&F->Started,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&F->Gate,XMEMORY_ACQUIRE)){if(xcronExecutionCancelled(T))return XCRON_RUN_CANCELLED;testRequire(xrtTimer()<End,"selected body gate");xrtThreadYield();}
    testRequire(!memcmp(&L->Local,&F->Local,sizeof(F->Local)),"selected view lives through business body");
    if(F->Mode==1){xrtSetErrorInfo(XERR_IO,"selected.service.body",29,"worker failure");return XCRON_RUN_FAILED;}
    return F->Mode==2?(xcronrunresult)99:XCRON_RUN_SUCCEEDED;
}
static xexecutor* ssPool(uint32 Threads,size_t Queue)
{xexecutorconfig C={Threads,Queue,0};xexecutor* E=xrtExecutorCreate(&C);testRequire(E!=NULL,"selected service borrowed pool");return E;}
static void ssPoolEnd(xexecutor* E)
{testRequire(xrtExecutorClose(E) && xrtExecutorWaitFor(E,10000)==XWAIT_OK && xrtExecutorDestroy(E),"host physically joins selected service pool");}
static xcronselectedserviceconfig ssConfig(xexecutor* E,ssfixture* F)
{return (xcronselectedserviceconfig){.Service={.Scheduler={8,4,32,0,65536},.Executor=E,.Proc=ssProc,.Context=F,.Clock=ssClock,.ClockContext=F,.CommandLimit=8,.CompletionLimit=4,.BatchLimit=4,.CommandWorkLimit=8,.ResampleMilliseconds=5,.RetryIntervalMilliseconds=10,.RetryAttempts=3,.AdmissionMilliseconds=100,.BytesLimit=262144,.AdapterBytesLimit=65536},.SubmissionBatchLimit=64};}
static ssfixture ssFixture(xtime Wall,const xdatetime* Local,size_t Candidates)
{ssfixture F={.Local=*Local,.Candidates=Candidates};xrtAtomic64Init(&F.Wall,(uint64)Wall);xrtAtomic64Init(&F.Mono,100);xrtAtomic32Init(&F.Gate,1);return F;}
static void ssStart(ssfixture* F,const xcronselectedserviceconfig* C)
{testRequire(xcronServiceCreateSelected(C,&F->Service,NULL),"selected service factory");ssUntil(&F->ClockEntered,1,"selected owner started");}
static xcronselectedreply ssReply(xcronservice* S,xcronrequestid Id)
{xcronselectedreply R;testRequire(xcronServiceCollectSelected(S,Id,10000,&R,NULL)==XWAIT_OK,"collect complete selected reply");return R;}
static xcronjobid ssAdd(ssfixture* F,const xcronjobconfig* J,xcrondeliverypolicy P)
{xcronrequestid Id;testRequire(xcronServiceAddSelectedAsync(F->Service,J,P,&Id,NULL),"queue selected policy copy");xcronselectedreply R=ssReply(F->Service,Id);testRequire(R.Code==XCRON_SERVICE_OK && R.Job && !R.Data && !R.Error,"selected cheap admission succeeds independently of recurrence result");return R.Job;}
static xcronselectedservicestats ssStats(xcronservice* S)
{xcronselectedservicestats T;testRequire(xcronServiceInspectSelected(S,&T,NULL),"complete selected service stats");return T;}
static xcronselectedservicecompletion ssDone(xcronservice* S)
{double End=xrtTimer()+10;for(;;){xcronselectedservicecompletion D;size_t N;testRequire(xcronServiceDrainSelected(S,&D,1,&N,NULL),"selected service metadata drain");if(N)return D;testRequire(xrtTimer()<End,"selected service physical completion deadline");xrtThreadYield();}}
static xcronselectedjobinfo ssInfo(xcronservice* S,xcronjobid Job)
{xcronrequestid Q;testRequire(xcronServiceInfoAsync(S,Job,&Q,NULL),"shared selected Info request");xcronselectedreply R=ssReply(S,Q);testRequire(R.Code==XCRON_SERVICE_OK && R.Job==Job && !R.Error,"full selected job status reply");return R.Info;}
static void ssStop(ssfixture* F)
{testRequire(xcronServiceClose(F->Service,true,NULL) && xcronServiceWaitFor(F->Service,10000,NULL)==XWAIT_OK,"selected owner close and physical join");}
static void ssClear(ssfixture* F)
{
    for(;;){xcronselectedservicecompletion D;size_t N;testRequire(xcronServiceDrainSelected(F->Service,&D,1,&N,NULL),"drain remaining selected metadata");if(!N)break;xrtErrorFree(D.Completion.Error);}
    for(;;){xcronselectedretired R;bool Found;testRequire(xcronServiceReapStoppedSelected(F->Service,&R,&Found,NULL),"retire selected stopped jobs");if(!Found)break;testRequire(R.Data==F,"return original selected business data");}
    testRequire(xcronServiceDestroy(F->Service,NULL),"selected service all duties consumed");F->Service=NULL;
}
static void ssFoldFixture(xcronzone** Z,xtime At[2],xdatetime* Local)
{
    testRequire(xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,Z,NULL) && xrtDateTime(2024,11,3,5,30,0,0,&At[0]),"explicit repeated-label fixture");At[1]=At[0]+XRT_TIME_HOUR;xtime Nominal;testRequire(xrtDateTime(2024,11,3,1,30,0,0,&Nominal),"nominal fixture");xrtTimeSplit(Nominal,Local);Local->Offset=0;Local->IsDST=-1;
}
static xcronjobconfig ssJob(xcronzone* Z,ssfixture* F,xtime Begin,xtime End)
{xcronjobconfig J={.Zone=Z,.Begin=Begin,.End=End,.Overlap=XCRON_OVERLAP_ALLOW,.RunningLimit=4,.Data=F};testRequire(xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&J.Expression,NULL),"selected job expression");return J;}
static void testSSPolicies(void)
{
    for(unsigned Fold=0;Fold<4;++Fold)for(unsigned Missing=0;Missing<2;++Missing){xcronzone* Z=NULL;xtime At[2];xdatetime Local;ssFoldFixture(&Z,At,&Local);ssfixture F=ssFixture(At[1],&Local,2);xexecutor* E=ssPool(2,8);xcronselectedserviceconfig C=ssConfig(E,&F);ssStart(&F,&C);xcronjobconfig J=ssJob(Z,&F,At[0],At[1]);xcronjobid Job=ssAdd(&F,&J,(xcrondeliverypolicy){(xcronfoldpolicy)Fold,(xcronmissingpolicy)Missing});xcronZoneRelease(Z);memset(&J,0xa5,sizeof(J));
        if(Fold==XCRON_FOLD_REJECT){double End=xrtTimer()+10;xcronselectedjobinfo Info;do{Info=ssInfo(F.Service,Job);testRequire(xrtTimer()<End,"initial selected ambiguity is a job fault");}while(Info.State!=XCRON_JOB_FAULTED);testRequire(Info.LastIssue.Code==XCRON_OCCURRENCE_AMBIGUOUS && Info.LastIssue.Candidates==2 && Info.Policy.Fold==XCRON_FOLD_REJECT && !Info.Dispatched && !xrtAtomic64Load(&F.Calls,XMEMORY_RELAXED),"initial global ambiguity survives selected async admission");}
        else {unsigned Count=Fold==XCRON_FOLD_ALL?2:1;unsigned Seen=0;for(unsigned N=0;N<Count;++N){xcronselectedservicecompletion D=ssDone(F.Service);unsigned Index=D.Completion.ScheduledAt==At[0]?0:1;testRequire(D.Completion.Job==Job && D.Completion.ScheduledAt==At[Index] && !(Seen&(1u<<Index)) && D.Completion.Result==XCRON_RUN_SUCCEEDED && D.Completion.Code==XCRON_COMPLETION_NORMAL && D.Completion.Attempts==1 && !D.Completion.Error && D.Candidates==2 && !memcmp(&D.Local,&Local,sizeof(Local)),"every selected physical receipt preserves its nominal label after union phase change");Seen|=1u<<Index;}
            testRequire(Seen==(Fold==XCRON_FOLD_ALL?3u:Fold==XCRON_FOLD_FIRST?1u:2u),"complete first/last/all policy results");}
        ssStop(&F);xcronselectedservicestats T=ssStats(F.Service);testRequire(T.Service.State==XCRON_SERVICE_STOPPED && T.Service.LastPoll.Used.Segments==0 && T.LastPoll.Used.Segments<=32 && T.LastPoll.Used.Months<=32 && T.LastPoll.Used.RuleYears<=32 && T.LastPoll.Used.Checks<=32 && T.LastPoll.Used.Offsets<=32 && !T.Service.ActiveNativeTimers,"selected five-budget poll and native timer shutdown");
        xcronselectedretired R;bool Found;testRequire(xcronServiceReapStoppedSelected(F.Service,&R,&Found,NULL) && Found && R.Id==Job && R.Data==&F && R.Info.Policy.Fold==(xcronfoldpolicy)Fold,"full selected retirement and original Data");ssClear(&F);ssPoolEnd(E);
    }
}
#include "service_selected_cases.h"
#include "service_selected_control_cases.h"
#include "service_selected_concurrency_cases.h"
#include "service_selected_budget_cases.h"
int main(void)
{testSSPolicies();testSSMissingAndWindows();testSSRetryMetadata();testSSPhysicalAndCredits();testSSCommandsAndModes();testSSSelfWaitAndClock();testSSBudgetsAndWarm();testSSFutureFault();testSSQueuedPoliciesAndStaleUnion();testSSConcurrency();testSSNativeQueuedCancel();testSSBudgetedTurns();testSSFactoryArguments();puts("[PASS] selected live service: full fold/missing/global-UNKNOWN, nominal retries/cancel/Data tails, tickets/self-waits/clock faults,512 concurrent queries, five-budget fairness, OOM and2048 zero-heap cycles");return 0;}
