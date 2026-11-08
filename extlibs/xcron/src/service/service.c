#include <xcron/service.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
#include <xcron/service_selected.h>
#endif
#include "../timezone/timezone_internal.h"
#include "../scheduler/scheduler_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE)
#define XCRON_SERVICE_MAGIC UINT32_C(0x58435631)
#define XCRON_SERVICE_NONE UINT32_MAX
typedef enum xcroncommandstate {XCRON_CMD_FREE=0,XCRON_CMD_QUEUED,XCRON_CMD_PROCESSING,XCRON_CMD_READY,XCRON_CMD_ADMITTING} xcroncommandstate;
typedef struct xcroncommand {
    uint64 Generation;
    uint32 Next;
    xcroncommandstate State;
    xcronjobconfig Add;
    xcronjobid Job;
    bool Cancel;
    xcronreply Reply;
} xcroncommand;
typedef struct xcronreservation {
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    /* Common initial Run sequence is readable in both C11 union members.
     * Selected's label occupies the future Done area until physical Finish;
     * Finish snapshots it before writing Done. No extra hot-slot bytes. */
    union {struct {xcronrun Run;xcronservicecompletion Done;};xcronselectedrun Selected;};
#else
    xcronrun Run;
    xcronservicecompletion Done;
#endif
    xerror* Rejection;
    uint64 Deadline,RetryAt;
    uint32 Attempts,Next,HeapPosition;
} xcronreservation;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
typedef struct xcronserviceselectedcommand {
    xcrondeliverypolicy Policy;
    union {xcronselectedjobinfo Info;xcronselectedretired Retired;} Value;
} xcronserviceselectedcommand;
typedef struct xcronserviceselected {
    xcronserviceselectedcommand* Commands;
    xcronexecutionlabel* Labels;
    xcronselectedrun* Runs;
    xcronselectedcompletion* Completed;
    xcronselectedpoll LastPoll;
    uint32 SubmissionBatchLimit;
    uint64 BatchSubmissions,BatchedAttempts,PublishedBatchSubmissions,PublishedBatchedAttempts;
} xcronserviceselected;
#endif
struct xcronservice {
    uint32 Magic;
    size_t ArenaBytes;
    xcronserviceconfig Config;
    xmutex Mutex;
    xcond Changed;
    xevent Wake;
    xeventwaiter Waiter;
    bool WaiterLive;
    xthread* Thread;
    uint64 Owner;
    bool Ready,Activated,Closing,CancelRuns,Stopped;
    xcronserviceissue Startup;
    xerror* StartupError;
    xerror* FatalError;
    xcronscheduler* Kernel;
    xcronexecutor* Bridge;
    xcronservicestats Stats;
    xatomic64 WakeFailures;
    xcroncommand* Commands;
    xcronreservation* Reservations;
    uint32* ActiveMap;
    uint32* RetryHeap;
    uint32* ReservedBatch;
    xcronrun* RunBatch;
    xcroncompletion* DoneBatch;
    uint32 FreeCommand,CommandHead,CommandTail,FreeCompletion,DoneHead,DoneTail;
    uint32 RetryCount;
    uint32 AdmissionHead,AdmissionTail,AdmissionCount;
    uint8 CommandBits,RunBits;
    xcronclock Now;
    bool HasClock;
    uint64 SubmitAttempts,RejectedSubmissions,Retries,AdmissionExpired;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    xcronserviceselected* Selected;
#endif
};
static bool xcronServiceMode(const xcronservice* S,bool Selected)
{
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    return (S->Selected!=NULL)==Selected;
#else
    (void)S;return !Selected;
#endif
}
static bool xcronServiceOutputs(const xcronservice* S,const void* Input,size_t InputSize,void* const* O,const size_t* N,size_t Count)
{
    if(S && (!xrtMemRangeValid(S,sizeof(*S)) || S->Magic!=XCRON_SERVICE_MAGIC || !xrtMemRangeValid(S,S->ArenaBytes)))return false;
    if(!xrtMemRangeValid(Input,InputSize))return false;
    for(size_t I=0;I<Count;++I){
        if(!xrtMemRangeValid(O[I],N[I]) || (S && xrtMemRangesOverlap(S,S->ArenaBytes,O[I],N[I])) || xrtMemRangesOverlap(Input,InputSize,O[I],N[I]))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(O[J],N[J],O[I],N[I]))return false;
    }
    return true;
}
static bool xcronServiceArgs(const xcronservice* S,void* Out,size_t Size,xcronserviceissue* Issue)
{void* O[]={Out,Issue};size_t N[]={Size,Issue?sizeof(*Issue):0};return S && xcronServiceOutputs(S,NULL,0,O,N,2);}
static void xcronServiceOK(xcronserviceissue* I)
{if(I)*I=(xcronserviceissue){XCRON_SERVICE_OK,XERR_NONE};}
static bool xcronServiceFail(xcronserviceerror Code,xerrkind Cause,xcronserviceissue* I)
{
    if(I)*I=(xcronserviceissue){Code,Cause};
    xerrkind Kind=XERR_ARGUMENT;cstr Text="invalid cron service arguments/spans/output aliases";
    switch(Code){
    case XCRON_SERVICE_LIMIT:Kind=XERR_RANGE;Text="cron service bounded capacity or byte limit exceeded";break;
    case XCRON_SERVICE_MEMORY:Kind=XERR_MEMORY;Text="cron service startup allocation failed";break;
    case XCRON_SERVICE_CLOSED:Kind=XERR_CLOSED;Text="cron service is closing";break;
    case XCRON_SERVICE_STALE:Kind=XERR_NOT_FOUND;Text="cron service request ticket is stale";break;
    case XCRON_SERVICE_BUSY:Kind=XERR_STATE;Text="cron service still has owner, jobs, tickets or completion duties";break;
    case XCRON_SERVICE_THREAD:Kind=XERR_STATE;Text="cron service blocking wait would depend on its current owner/executor";break;
    case XCRON_SERVICE_CLOCK:Kind=XERR_STATE;Text="cron service clock failed or monotonic sample regressed";break;
    case XCRON_SERVICE_SYSTEM:Kind=XERR_IO;Text="cron service XRT synchronization or thread operation failed";break;
    case XCRON_SERVICE_KERNEL:Kind=XERR_STATE;Text="cron service kernel operation failed";break;
    case XCRON_SERVICE_EXECUTOR:Kind=XERR_STATE;Text="cron service executor adapter operation failed";break;
    default:break;
    }
    if(Cause!=XERR_NONE){xerror* E=xrtTakeError();xerror* W=xrtErrorWrap(E,Kind,"xrt.cron.service",Code,Text);xrtErrorFree(E);if(W)xrtSetErrorTake(W);else xrtSetErrorKind(XERR_MEMORY);}
    else xrtSetErrorInfo(Kind,"xrt.cron.service",Code,Text);
    return false;
}
static void xcronServiceIncrement(uint64* V)
{if(*V!=UINT64_MAX)++*V;}
static uint64 xcronServiceAddTime(uint64 A,uint64 B)
{return B>UINT64_MAX-A?UINT64_MAX:A+B;}
static uint8 xcronServiceBits(uint32 Limit)
{uint8 B=0;for(uint32 V=Limit-1;V;V>>=1)++B;return B;}
/* Wake must never consume a worker's business error or a caller's ambient error. */
static void xcronServiceSignal(ptr Raw)
{
    xcronservice* S=Raw;xerror* E=xrtTakeError();
    if(!xrtEventSet(&S->Wake)){uint64 Old=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);
        while(Old!=UINT64_MAX && !xrtAtomic64CompareExchange(&S->WakeFailures,&Old,Old+1,XMEMORY_RELAXED,XMEMORY_RELAXED)){} }
    xrtSetErrorTake(E);
}
static void xcronServiceFault(xcronservice* S,xcronserviceerror Code)
{
    xerrkind Cause=xrtErrorKind(xrtGetError());xcronServiceFail(Code,Cause,NULL);xerror* E=xrtTakeError();
    xrtMutexLock(&S->Mutex);
    if(S->Stats.Failure.Code==XCRON_SERVICE_OK){S->Stats.Failure=(xcronserviceissue){Code,Cause};S->FatalError=E;E=NULL;}
    S->Closing=true;S->CancelRuns=true;S->Stats.State=XCRON_SERVICE_STOPPING;
    xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);xrtErrorFree(E);
}
static bool xcronServiceClockRead(xcronservice* S)
{
    xrtClearError();xcronclock C={0};bool OK;
    if(S->Config.Clock)OK=S->Config.Clock(&C,S->Config.ClockContext);
    else {double T=xrtTimer()*1000.0;C.Wall=xrtNow();OK=isfinite(T) && T>=0 && T<18446744073709551616.0 && !xrtGetError();if(OK)C.MonotonicMilliseconds=(uint64)T;}
    if(!OK || (S->HasClock && C.MonotonicMilliseconds<S->Now.MonotonicMilliseconds)){xcronServiceFault(S,XCRON_SERVICE_CLOCK);return false;}
    S->Now=C;S->HasClock=true;xrtClearError();return true;
}
static bool xcronServiceEarlier(const xcronservice* S,uint32 A,uint32 B)
{
    const xcronreservation* X=&S->Reservations[A];const xcronreservation* Y=&S->Reservations[B];
    uint64 XT=X->RetryAt<X->Deadline?X->RetryAt:X->Deadline,YT=Y->RetryAt<Y->Deadline?Y->RetryAt:Y->Deadline;
    return XT<YT || (XT==YT && X->Run.Id<Y->Run.Id);
}
static void xcronServiceHeapUp(xcronservice* S,uint32 P)
{
    uint32 I=S->RetryHeap[P];while(P){uint32 Parent=(P-1)/2;if(!xcronServiceEarlier(S,I,S->RetryHeap[Parent]))break;
        S->RetryHeap[P]=S->RetryHeap[Parent];S->Reservations[S->RetryHeap[P]].HeapPosition=P;P=Parent;}
    S->RetryHeap[P]=I;S->Reservations[I].HeapPosition=P;
}
static void xcronServiceHeapDown(xcronservice* S,uint32 P)
{
    uint32 I=S->RetryHeap[P];while(P<S->RetryCount/2){uint32 C=P*2+1;if(C+1<S->RetryCount && xcronServiceEarlier(S,S->RetryHeap[C+1],S->RetryHeap[C]))++C;
        if(!xcronServiceEarlier(S,S->RetryHeap[C],I))break;
        S->RetryHeap[P]=S->RetryHeap[C];S->Reservations[S->RetryHeap[P]].HeapPosition=P;P=C;}
    S->RetryHeap[P]=I;S->Reservations[I].HeapPosition=P;
}
static void xcronServiceHeapAdd(xcronservice* S,uint32 I)
{uint32 P=S->RetryCount++;S->RetryHeap[P]=I;xcronServiceHeapUp(S,P);}
static void xcronServiceHeapRemove(xcronservice* S,uint32 I)
{
    uint32 P=S->Reservations[I].HeapPosition;--S->RetryCount;S->Reservations[I].HeapPosition=XCRON_SERVICE_NONE;
    if(P==S->RetryCount)return;
    S->RetryHeap[P]=S->RetryHeap[S->RetryCount];S->Reservations[S->RetryHeap[P]].HeapPosition=P;
    if(P && xcronServiceEarlier(S,S->RetryHeap[P],S->RetryHeap[(P-1)/2]))xcronServiceHeapUp(S,P);else xcronServiceHeapDown(S,P);
}
static uint32 xcronServiceRunIndex(const xcronservice* S,xcronrunid Id)
{return (uint32)(Id&((UINT64_C(1)<<S->RunBits)-1));}
static bool xcronServiceFinish(xcronservice* S,uint32 I,xcronrunresult Result,xcronservicecompletioncode Code,xcronexecutorerror Executor,xerror* Error)
{
    xcronreservation* R=&S->Reservations[I];xrtClearError();
    if(!xcronSchedulerComplete(S->Kernel,R->Run.Id,Result,NULL)){xrtErrorFree(Error);xcronServiceFault(S,XCRON_SERVICE_KERNEL);return false;}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(S->Selected)S->Selected->Labels[I]=(xcronexecutionlabel){R->Selected.Local,R->Selected.Candidates};
#endif
    S->ActiveMap[xcronServiceRunIndex(S,R->Run.Id)]=XCRON_SERVICE_NONE;
    R->Done=(xcronservicecompletion){.Id=R->Run.Id,.Job=R->Run.Job,.ScheduledAt=R->Run.ScheduledAt,.Through=R->Run.Through,.Observed=R->Run.Observed,.Flags=R->Run.Flags,.Result=Result,.Code=Code,.Executor=Executor,.Attempts=R->Attempts,.Error=Error};
    /* No borrowed Run.Data escapes in the ready metadata record. */
    R->Run.Data=NULL;R->Next=XCRON_SERVICE_NONE;xrtMutexLock(&S->Mutex);
    if(S->DoneTail==XCRON_SERVICE_NONE)S->DoneHead=I;else S->Reservations[S->DoneTail].Next=I;S->DoneTail=I;
    ++S->Stats.ReadyCompletions;xcronServiceIncrement(&S->Stats.Published);xrtMutexUnlock(&S->Mutex);return true;
}
static void xcronServiceCancelUnsent(xcronservice* S,xcronjobid Job)
{
    /* Cold control operation; heap removal keeps retries ordered after cancellation. */
    for(uint32 Index=0;Index<S->Config.Scheduler.RunLimit;++Index){uint32 I=S->ActiveMap[Index];if(I==XCRON_SERVICE_NONE)continue;xcronreservation* R=&S->Reservations[I];
        if(R->HeapPosition==XCRON_SERVICE_NONE || (Job && R->Run.Job!=Job))continue;
        xcronServiceHeapRemove(S,I);xrtErrorFree(R->Rejection);R->Rejection=NULL;
        xcronServiceFinish(S,I,XCRON_RUN_CANCELLED,XCRON_COMPLETION_CANCELLED,XCRON_EXECUTOR_OK,NULL);
    }
}
static void xcronServiceSubmitResult(xcronservice* S,uint32 I,bool OK,xcronexecutorissue E,xerror* Error)
{
    xcronreservation* R=&S->Reservations[I];uint64 Now=S->Now.MonotonicMilliseconds;xrtErrorFree(R->Rejection);R->Rejection=NULL;
    if(OK){xrtErrorFree(Error);return;}
    xcronServiceIncrement(&S->RejectedSubmissions);
    bool Again=E.Code==XCRON_EXECUTOR_REJECTED && E.Cause==XERR_AGAIN;
    bool Enabled=S->Config.AdmissionMilliseconds!=0;
    if(Again && Enabled && R->Attempts<S->Config.RetryAttempts && Now<R->Deadline){
        R->Rejection=Error;R->RetryAt=xcronServiceAddTime(Now,S->Config.RetryIntervalMilliseconds);xcronServiceHeapAdd(S,I);return;
    }
    xcronservicecompletioncode Code=Again && Enabled?XCRON_COMPLETION_EXPIRED:XCRON_COMPLETION_REJECTED;
    if(Code==XCRON_COMPLETION_EXPIRED)xcronServiceIncrement(&S->AdmissionExpired);
    xcronServiceFinish(S,I,XCRON_RUN_FAILED,Code,E.Code,Error);
}
static void xcronServiceSubmit(xcronservice* S,uint32 I,bool Retry)
{
    xcronreservation* R=&S->Reservations[I];xcronexecutorissue E={0};uint64 Now=S->Now.MonotonicMilliseconds;
    if(Retry && Now>=R->Deadline){xerror* Error=R->Rejection;R->Rejection=NULL;xcronServiceIncrement(&S->AdmissionExpired);
        xcronServiceFinish(S,I,XCRON_RUN_FAILED,XCRON_COMPLETION_EXPIRED,XCRON_EXECUTOR_REJECTED,Error);return;}
    ++R->Attempts;xcronServiceIncrement(&S->SubmitAttempts);if(Retry)xcronServiceIncrement(&S->Retries);
    xrtClearError();bool OK;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(S->Selected)OK=xcronExecutorSubmitSelected(S->Bridge,&R->Selected,&E);
    else
#endif
    OK=xcronExecutorSubmit(S->Bridge,&R->Run,&E);
    xcronServiceSubmitResult(S,I,OK,E,xrtTakeError());
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
static void xcronServiceSubmitReadySelected(xcronservice* S,size_t Count)
{
    for(size_t Begin=0;Begin<Count;){size_t N=Count-Begin;if(N>S->Selected->SubmissionBatchLimit)N=S->Selected->SubmissionBatchLimit;
        if(N==1)xcronServiceSubmit(S,S->ReservedBatch[Begin],false);
        else {
            for(size_t J=0;J<N;++J){++S->Reservations[S->ReservedBatch[Begin+J]].Attempts;xcronServiceIncrement(&S->SubmitAttempts);}
            xcronServiceIncrement(&S->Selected->BatchSubmissions);
            for(size_t J=0;J<N;++J)xcronServiceIncrement(&S->Selected->BatchedAttempts);
            xcronexecutorissue E={0};xrtClearError();bool OK=xcronExecutorSubmitBatchSelected(S->Bridge,S->Selected->Runs+Begin,N,&E);xerror* Error=xrtTakeError();
            for(size_t J=0;J<N;++J)xcronServiceSubmitResult(S,S->ReservedBatch[Begin+J],OK,E,xrtErrorRef(Error));
            xrtErrorFree(Error);
        }
        Begin+=N;
    }
}
#endif
static void xcronServiceReplyReady(xcronservice* S,xcroncommand* C,xcronreply R,bool OK)
{
    if(!OK)R.Error=xrtTakeError();else xrtClearError();
    xrtMutexLock(&S->Mutex);C->Reply=R;C->State=XCRON_CMD_READY;--S->Stats.PendingRequests;++S->Stats.ReadyReplies;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
static void xcronServiceCommandSelected(xcronservice* S,uint32 I)
{
    xcroncommand* C=&S->Commands[I];xcronreply R={.Request=C->Reply.Request,.Kind=C->Reply.Kind};xcronserviceselectedcommand* X=&S->Selected->Commands[I];bool OK=false;
    xrtClearError();
    switch(R.Kind){
    case XCRON_REQUEST_ADD:{
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        if(Closing){R.Code=XCRON_SERVICE_CLOSED;xcronServiceFail(R.Code,XERR_NONE,NULL);}
        else {OK=xcronSchedulerAddSelected(S->Kernel,&C->Add,X->Policy,&R.Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;}
        if(!OK)R.Data=C->Add.Data;
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;break;}
    case XCRON_REQUEST_INFO:R.Job=C->Job;OK=xcronSchedulerJobSelected(S->Kernel,C->Job,&X->Value.Info,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    case XCRON_REQUEST_REAP:OK=xcronSchedulerReapSelected(S->Kernel,&X->Value.Retired,&R.Found,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    default:abort();
    }
    xcronServiceReplyReady(S,C,R,OK);
}
#endif
static void xcronServiceAdmissionAdd(xcronservice* S,uint32 I)
{
    S->Commands[I].Next=XCRON_SERVICE_NONE;
    if(S->AdmissionTail!=XCRON_SERVICE_NONE)S->Commands[S->AdmissionTail].Next=I;else S->AdmissionHead=I;
    S->AdmissionTail=I;++S->AdmissionCount;
}
static void xcronServiceAdmissions(xcronservice* S)
{
    uint32 Count=S->AdmissionCount;if(Count>S->Config.CommandWorkLimit)Count=S->Config.CommandWorkLimit;
    for(uint32 N=0;N<Count;++N){
        uint32 I=S->AdmissionHead;xcroncommand* C=&S->Commands[I];S->AdmissionHead=C->Next;
        if(S->AdmissionHead==XCRON_SERVICE_NONE)S->AdmissionTail=XCRON_SERVICE_NONE;
        --S->AdmissionCount;xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xcronreply R=C->Reply;xrtMutexUnlock(&S->Mutex);
        xrtClearError();xcronadmissionstatus Status=xcronSchedulerFinishAdmission(S->Kernel,C->Job,Closing,&R.Scheduler);
        if(Status==XCRON_ADMISSION_PENDING){xcronServiceAdmissionAdd(S,I);continue;}
        bool OK=Status==XCRON_ADMISSION_ACCEPTED;
        if(OK)R.Job=C->Job;
        else {R.Code=R.Scheduler.Code==XCRON_SCHEDULER_CLOSED?XCRON_SERVICE_CLOSED:XCRON_SERVICE_KERNEL;R.Job=0;R.Data=C->Add.Data;}
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;xcronServiceReplyReady(S,C,R,OK);
    }
}
static void xcronServiceCommand(xcronservice* S,uint32 I)
{
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(S->Selected && S->Commands[I].Reply.Kind!=XCRON_REQUEST_CANCEL){xcronServiceCommandSelected(S,I);return;}
#endif
    xcroncommand* C=&S->Commands[I];xcronreply R={.Request=C->Reply.Request,.Kind=C->Reply.Kind};bool OK=false;
    xrtClearError();
    switch(R.Kind){
    case XCRON_REQUEST_ADD:{
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        if(Closing){R.Code=XCRON_SERVICE_CLOSED;xcronServiceFail(R.Code,XERR_NONE,NULL);}
        else {OK=xcronSchedulerBeginAdmission(S->Kernel,&C->Add,&C->Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;
            else {xrtMutexLock(&S->Mutex);C->Reply=R;C->State=XCRON_CMD_ADMITTING;xrtMutexUnlock(&S->Mutex);xcronServiceAdmissionAdd(S,I);return;}}
        if(!OK)R.Data=C->Add.Data;
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;break;}
    case XCRON_REQUEST_CANCEL:
        R.Job=C->Job;OK=xcronSchedulerCancel(S->Kernel,C->Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;
        if(OK && C->Cancel){uint32 Requested=0;OK=xcronExecutorCancelJob(S->Bridge,C->Job,&Requested,&R.Executor);if(!OK)R.Code=XCRON_SERVICE_EXECUTOR;xcronServiceCancelUnsent(S,C->Job);}break;
    case XCRON_REQUEST_INFO:R.Job=C->Job;OK=xcronSchedulerJob(S->Kernel,C->Job,&R.Info,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    case XCRON_REQUEST_REAP:OK=xcronSchedulerReap(S->Kernel,&R.Retired,&R.Found,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    }
    xcronServiceReplyReady(S,C,R,OK);
}
static void xcronServiceStatsSet(xcronservice* S,const xcronschedulerstats* K,const xcronexecutorstats* E)
{
    xrtMutexLock(&S->Mutex);S->Stats.Kernel=*K;S->Stats.Executor=*E;S->Stats.KernelBytes=K->Bytes;S->Stats.AdapterBytes=E->Bytes;
    S->Stats.Bytes=S->ArenaBytes+K->Bytes+E->Bytes;S->Stats.LastClock=S->Now;S->Stats.Unsent=S->RetryCount;
    S->Stats.SubmitAttempts=S->SubmitAttempts;S->Stats.RejectedSubmissions=S->RejectedSubmissions;S->Stats.Retries=S->Retries;S->Stats.AdmissionExpired=S->AdmissionExpired;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(S->Selected){S->Selected->PublishedBatchSubmissions=S->Selected->BatchSubmissions;S->Selected->PublishedBatchedAttempts=S->Selected->BatchedAttempts;}
#endif
    xrtMutexUnlock(&S->Mutex);
}
static bool xcronServicePublishStats(xcronservice* S,xcronschedulerstats* K,xcronexecutorstats* E)
{
    if(!xcronSchedulerInspect(S->Kernel,K,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);return false;}
    if(!xcronExecutorInspect(S->Bridge,E,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);return false;}
    xcronServiceStatsSet(S,K,E);return true;
}
/* Owner during stop, or an exclusive caller after native join on retry. */
static bool xcronServiceWaiterRelease(xcronservice* S)
{
    if(!S->WaiterLive)return true;
    if(!xrtEventWaiterUnit(&S->Waiter))return false;
    S->WaiterLive=false;
    xrtMutexLock(&S->Mutex);S->Stats.ActiveNativeTimers=0;xrtMutexUnlock(&S->Mutex);
    return true;
}
static void xcronServiceReceive(xcronservice* S,xcronrunid Id,xcronrunresult Result,xcronexecutorerror Code,xerror* Error)
{
    uint32 Index=xcronServiceRunIndex(S,Id);uint32 I=Index<S->Config.Scheduler.RunLimit?S->ActiveMap[Index]:XCRON_SERVICE_NONE;
    if(I==XCRON_SERVICE_NONE || S->Reservations[I].Run.Id!=Id){xrtErrorFree(Error);xrtClearError();xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);return;}
    xcronServiceFinish(S,I,Result,XCRON_COMPLETION_NORMAL,Code,Error);
}
static int32 xcronServiceOwner(ptr Raw)
{
    xcronservice* S=Raw;xallocationscope Scope={0};bool Budget=false,OK=true;xcronserviceerror Code=XCRON_SERVICE_OK;
    S->Owner=xrtThreadCurrentId();
    if(S->Config.InitializationBudget){Budget=xrtAllocationBudgetEnter(S->Config.InitializationBudget,&Scope);if(!Budget){OK=false;Code=XCRON_SERVICE_MEMORY;}}
    if(OK){bool Created;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(S->Selected)Created=xcronSchedulerCreateSelected(&S->Config.Scheduler,&S->Kernel,NULL);
        else
#endif
        Created=xcronSchedulerCreate(&S->Config.Scheduler,&S->Kernel,NULL);
        if(!Created){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_KERNEL;}}
    xcronexecutorconfig B={.Executor=S->Config.Executor,.Proc=S->Config.Proc,.Context=S->Config.Context,.Notify=xcronServiceSignal,.NotifyContext=S,.RunLimit=S->Config.Scheduler.RunLimit,.BytesLimit=S->Config.AdapterBytesLimit};
    if(OK){bool Created;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(S->Selected)Created=xcronExecutorCreateSelected(&B,&S->Bridge,NULL);
        else
#endif
        Created=xcronExecutorCreate(&B,&S->Bridge,NULL);
        if(!Created){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_EXECUTOR;}}
    if(OK){
        xcronschedulerstats K;xcronexecutorstats E;
        if(!xcronSchedulerInspect(S->Kernel,&K,NULL)){OK=false;Code=XCRON_SERVICE_KERNEL;}
        else if(!xcronExecutorInspect(S->Bridge,&E,NULL)){OK=false;Code=XCRON_SERVICE_EXECUTOR;}
        else xcronServiceStatsSet(S,&K,&E);
    }
    /* Last fallible startup resource: Init failure owns no native timer. The
     * owner, not the creator, binds the reusable wait context to its thread. */
    if(OK && !xrtEventWaiterInit(&S->Waiter,&S->Wake,0)){OK=false;Code=XCRON_SERVICE_SYSTEM;}
    if(OK){
        S->WaiterLive=true;
        /* Get has no native/allocation failure with these disjoint live spans. */
        if(!xrtEventWaiterGet(&S->Waiter,&S->Stats.Waiter))abort();
        S->Stats.ActiveNativeTimers=S->Stats.Waiter.NativeTimers;
    }
    xerror* StartupError=OK?NULL:xrtTakeError();
    if(Budget)xrtAllocationBudgetLeave(&Scope);
    if(!OK){if(S->Bridge){xcronExecutorDestroy(S->Bridge,NULL);S->Bridge=NULL;}
        if(S->Kernel){xcronSchedulerDestroy(S->Kernel,NULL);S->Kernel=NULL;}
        xrtMutexLock(&S->Mutex);S->Startup=(xcronserviceissue){Code,xrtErrorKind(StartupError)};S->StartupError=StartupError;S->Ready=true;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);return 0;}
    xrtMutexLock(&S->Mutex);S->Ready=true;xrtCondBroadcast(&S->Changed);
    while(!S->Activated)xrtCondWaitFor(&S->Changed,&S->Mutex,100);
    S->Stats.State=S->Closing?XCRON_SERVICE_STOPPING:XCRON_SERVICE_RUNNING;xrtMutexUnlock(&S->Mutex);
    bool AppliedClose=false,AppliedCancel=false;
    for(;;){
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing,Cancel=S->CancelRuns;xrtMutexUnlock(&S->Mutex);
        if(Closing && !AppliedClose){xcronSchedulerClose(S->Kernel,NULL);xcronExecutorClose(S->Bridge,Cancel,NULL);xcronServiceCancelUnsent(S,0);AppliedClose=true;AppliedCancel=Cancel;}
        else if(Closing && Cancel && !AppliedCancel){xcronExecutorClose(S->Bridge,true,NULL);AppliedCancel=true;}
        for(uint32 N=0;N<S->Config.CommandWorkLimit;++N){
            xrtMutexLock(&S->Mutex);uint32 I=S->CommandHead;
            if(I!=XCRON_SERVICE_NONE){S->CommandHead=S->Commands[I].Next;if(S->CommandHead==XCRON_SERVICE_NONE)S->CommandTail=XCRON_SERVICE_NONE;S->Commands[I].State=XCRON_CMD_PROCESSING;}
            xrtMutexUnlock(&S->Mutex);if(I==XCRON_SERVICE_NONE)break;xcronServiceCommand(S,I);
        }
        xcronServiceAdmissions(S);
        size_t Completed=0;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(S->Selected){
            if(!xcronExecutorDrainSelected(S->Bridge,S->Selected->Completed,S->Config.BatchLimit,&Completed,NULL))xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);
            for(size_t N=0;N<Completed;++N){xcronselectedcompletion* D=&S->Selected->Completed[N];xcronServiceReceive(S,D->Run.Run.Id,D->Result,D->Code,D->Error);}
        }else
#endif
        {
            if(!xcronExecutorDrain(S->Bridge,S->DoneBatch,S->Config.BatchLimit,&Completed,NULL))xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);
            for(size_t N=0;N<Completed;++N){xcroncompletion* D=&S->DoneBatch[N];xcronServiceReceive(S,D->Run.Id,D->Result,D->Code,D->Error);}
        }
        xrtMutexLock(&S->Mutex);Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        bool More=false;uint32 Searching=0;
        if(!Closing && xcronServiceClockRead(S)){
            xrtMutexLock(&S->Mutex);Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
            if(!Closing){
                for(uint32 N=0;N<S->Config.BatchLimit && S->RetryCount;++N){uint32 I=S->RetryHeap[0];xcronreservation* R=&S->Reservations[I];
                    if(S->Now.MonotonicMilliseconds<R->RetryAt && S->Now.MonotonicMilliseconds<R->Deadline)break;
                    xcronServiceHeapRemove(S,I);xcronServiceSubmit(S,I,true);}
                xcronexecutorstats E;xcronExecutorInspect(S->Bridge,&E,NULL);uint32 Capacity=S->Config.Scheduler.RunLimit-E.Active;
                if(Capacity>S->Config.BatchLimit)Capacity=S->Config.BatchLimit;
                xrtMutexLock(&S->Mutex);uint32 Reserved=0;
                while(Reserved<Capacity && S->FreeCompletion!=XCRON_SERVICE_NONE){uint32 I=S->FreeCompletion;S->FreeCompletion=S->Reservations[I].Next;S->ReservedBatch[Reserved++]=I;++S->Stats.ReservedCompletions;}
                xrtMutexUnlock(&S->Mutex);
                uint32 Work=S->Config.Scheduler.WorkLimit;bool Polled;xcronpoll P={0};
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
                if(S->Selected){xcronselectedpoll Q={0};Polled=xcronSchedulerPollSelected(S->Kernel,S->Now,(xcronoccurrencebudget){Work,Work,Work,Work,Work},S->Selected->Runs,Reserved,&Q,NULL);
                    if(Polled){xrtMutexLock(&S->Mutex);S->Selected->LastPoll=Q;xrtMutexUnlock(&S->Mutex);}P=Q.Poll;Searching=Q.Searching;
                }else
#endif
                {xcronpollbudgeted Q={0};Polled=xcronSchedulerPollBudgeted(S->Kernel,S->Now,(xcronzonedbudget){Work,Work,Work},S->RunBatch,Reserved,&Q,NULL);
                    if(Polled){xrtMutexLock(&S->Mutex);S->Stats.LastPoll=Q;xrtMutexUnlock(&S->Mutex);}P=Q.Poll;Searching=Q.Searching;}
                if(!Polled)xcronServiceFault(S,XCRON_SERVICE_KERNEL);
                for(uint32 N=0;N<(uint32)P.Count;++N){uint32 I=S->ReservedBatch[N];xcronreservation* R=&S->Reservations[I];
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
                    if(S->Selected)R->Selected=S->Selected->Runs[N];else
#endif
                    R->Run=S->RunBatch[N];
                    R->Attempts=0;R->Rejection=NULL;R->HeapPosition=XCRON_SERVICE_NONE;R->Deadline=xcronServiceAddTime(S->Now.MonotonicMilliseconds,S->Config.AdmissionMilliseconds);
                    S->ActiveMap[xcronServiceRunIndex(S,R->Run.Id)]=I;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
                    if(!S->Selected)
#endif
                    xcronServiceSubmit(S,I,false);}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
                if(S->Selected)xcronServiceSubmitReadySelected(S,P.Count);
#endif
                xrtMutexLock(&S->Mutex);
                for(uint32 N=(uint32)P.Count;N<Reserved;++N){uint32 I=S->ReservedBatch[N];S->Reservations[I].Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReservedCompletions;}
                xrtMutexUnlock(&S->Mutex);More=Polled && P.More;
            }
        }
        xcronServiceAdmissions(S);
        xcronschedulerstats K;xcronexecutorstats E;if(!xcronServicePublishStats(S,&K,&E))continue;
        xrtMutexLock(&S->Mutex);Closing=S->Closing;bool Commands=S->CommandHead!=XCRON_SERVICE_NONE;
        bool Capacity=S->FreeCompletion!=XCRON_SERVICE_NONE && E.Active<S->Config.Scheduler.RunLimit;
        bool Stop=Closing && AppliedClose && (!S->CancelRuns || AppliedCancel) && !K.Running && !E.Active && !E.Producers && !S->RetryCount && !S->Stats.PendingRequests;
        xrtMutexUnlock(&S->Mutex);
        if(Stop){
            if(!xcronExecutorDestroy(S->Bridge,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);continue;}S->Bridge=NULL;
            /* Publish actual retained allocations. Other executor fields remain
             * the final lifetime counters/configuration, not a live object. */
            xrtMutexLock(&S->Mutex);S->Stats.AdapterBytes=0;S->Stats.Executor.Bytes=0;
            S->Stats.Bytes=S->ArenaBytes+S->Stats.KernelBytes;xrtMutexUnlock(&S->Mutex);
            if(!xcronServiceWaiterRelease(S))xcronServiceFault(S,XCRON_SERVICE_SYSTEM);
            xrtMutexLock(&S->Mutex);S->Stopped=true;S->Stats.State=S->Stats.Failure.Code==XCRON_SERVICE_OK?XCRON_SERVICE_STOPPED:XCRON_SERVICE_FAILED;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);return 0;
        }
        uint64 Delay=S->Config.ResampleMilliseconds;
        if(!Closing && S->HasClock){xcronwake W;
            if(!xcronSchedulerWake(S->Kernel,S->Now,Delay,&W,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);continue;}
            if(Capacity && W.DelayMilliseconds<Delay)Delay=W.DelayMilliseconds;
            if(S->RetryCount){xcronreservation* R=&S->Reservations[S->RetryHeap[0]];uint64 At=R->RetryAt<R->Deadline?R->RetryAt:R->Deadline;
                uint64 Wait=At<=S->Now.MonotonicMilliseconds?0:At-S->Now.MonotonicMilliseconds;if(Wait<Delay)Delay=Wait;}
        }
        if(Commands || S->AdmissionCount || Searching || Completed==S->Config.BatchLimit || (More && Capacity))Delay=0;
        /* The auto-reset event is never reset after the work check. */
        if(xrtEventWaiterWaitFor(&S->Waiter,(int64)Delay)==XWAIT_ERROR)xcronServiceFault(S,XCRON_SERVICE_SYSTEM);
    }
}
static bool xcronServiceArray(size_t Count,size_t Unit,size_t Align,size_t* Total,size_t* Offset)
{
    size_t Padding=(Align-*Total%Align)%Align;if(Padding>SIZE_MAX-*Total)return false;*Total+=Padding;*Offset=*Total;
    if(Count>(SIZE_MAX-*Total)/Unit)return false;
    *Total+=Count*Unit;return true;
}
static bool xcronServiceCreateMode(const xcronserviceconfig* C,xcronservice** Out,xcronserviceissue* Issue,uint32 SelectedBatchLimit)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronServiceOutputs(NULL,C,sizeof(*C),O,N,2) || !C->Executor || !C->Proc || C->Reserved || C->Scheduler.Reserved ||
        !C->Scheduler.JobLimit || C->Scheduler.JobLimit>UINT32_C(1073741824) || !C->Scheduler.RunLimit || C->Scheduler.RunLimit>UINT32_C(1073741824) || !C->Scheduler.WorkLimit ||
        !C->CommandLimit || C->CommandLimit>UINT32_C(1073741824) || !C->CompletionLimit || C->CompletionLimit>UINT32_C(1073741824) ||
        !C->BatchLimit || C->BatchLimit>C->CompletionLimit || C->BatchLimit>C->Scheduler.RunLimit || !C->CommandWorkLimit || !C->ResampleMilliseconds || !C->RetryIntervalMilliseconds || !C->RetryAttempts)
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    size_t Bytes=sizeof(xcronservice),Offsets[10],RunUnit=sizeof(xcronrun),RunAlign=_Alignof(xcronrun),DoneUnit=sizeof(xcroncompletion),DoneAlign=_Alignof(xcroncompletion);
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(SelectedBatchLimit){RunUnit=sizeof(xcronselectedrun);RunAlign=_Alignof(xcronselectedrun);DoneUnit=sizeof(xcronselectedcompletion);DoneAlign=_Alignof(xcronselectedcompletion);}
#else
    (void)SelectedBatchLimit;
#endif
    if(!xcronServiceArray(C->CommandLimit,sizeof(xcroncommand),_Alignof(xcroncommand),&Bytes,&Offsets[0]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(xcronreservation),_Alignof(xcronreservation),&Bytes,&Offsets[1]) ||
        !xcronServiceArray(C->Scheduler.RunLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[2]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[3]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[4]) ||
        !xcronServiceArray(C->BatchLimit,RunUnit,RunAlign,&Bytes,&Offsets[5]) ||
        !xcronServiceArray(C->BatchLimit,DoneUnit,DoneAlign,&Bytes,&Offsets[6]))
        return xcronServiceFail(XCRON_SERVICE_LIMIT,XERR_NONE,Issue);
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(SelectedBatchLimit && (!xcronServiceArray(1,sizeof(xcronserviceselected),_Alignof(xcronserviceselected),&Bytes,&Offsets[7]) ||
        !xcronServiceArray(C->CommandLimit,sizeof(xcronserviceselectedcommand),_Alignof(xcronserviceselectedcommand),&Bytes,&Offsets[8]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(xcronexecutionlabel),_Alignof(xcronexecutionlabel),&Bytes,&Offsets[9])))
        return xcronServiceFail(XCRON_SERVICE_LIMIT,XERR_NONE,Issue);
#endif
    if(Bytes>C->BytesLimit || C->Scheduler.BytesLimit>C->BytesLimit-Bytes || C->AdapterBytesLimit>C->BytesLimit-Bytes-C->Scheduler.BytesLimit)
        return xcronServiceFail(XCRON_SERVICE_LIMIT,XERR_NONE,Issue);
    xallocationscope Scope={0};bool Budget=false;
    if(C->InitializationBudget){Budget=xrtAllocationBudgetEnter(C->InitializationBudget,&Scope);if(!Budget)return xcronServiceFail(XCRON_SERVICE_MEMORY,xrtErrorKind(xrtGetError()),Issue);}
    xcronservice* S=xrtMalloc(Bytes);
    if(!S){if(Budget)xrtAllocationBudgetLeave(&Scope);return xcronServiceFail(XCRON_SERVICE_MEMORY,XERR_MEMORY,Issue);}
    memset(S,0,Bytes);S->Magic=XCRON_SERVICE_MAGIC;S->ArenaBytes=Bytes;S->Config=*C;S->Stats.ArenaBytes=Bytes;S->Stats.Bytes=Bytes;
    S->Commands=(xcroncommand*)((unsigned char*)S+Offsets[0]);S->Reservations=(xcronreservation*)((unsigned char*)S+Offsets[1]);S->ActiveMap=(uint32*)((unsigned char*)S+Offsets[2]);S->RetryHeap=(uint32*)((unsigned char*)S+Offsets[3]);S->ReservedBatch=(uint32*)((unsigned char*)S+Offsets[4]);S->RunBatch=(xcronrun*)((unsigned char*)S+Offsets[5]);S->DoneBatch=(xcroncompletion*)((unsigned char*)S+Offsets[6]);
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(SelectedBatchLimit){S->Selected=(xcronserviceselected*)((unsigned char*)S+Offsets[7]);S->Selected->Commands=(xcronserviceselectedcommand*)((unsigned char*)S+Offsets[8]);S->Selected->Labels=(xcronexecutionlabel*)((unsigned char*)S+Offsets[9]);
        S->Selected->Runs=(xcronselectedrun*)((unsigned char*)S+Offsets[5]);S->Selected->Completed=(xcronselectedcompletion*)((unsigned char*)S+Offsets[6]);S->Selected->SubmissionBatchLimit=SelectedBatchLimit;S->RunBatch=NULL;S->DoneBatch=NULL;}
#endif
    S->CommandHead=S->CommandTail=S->DoneHead=S->DoneTail=S->AdmissionHead=S->AdmissionTail=XCRON_SERVICE_NONE;S->CommandBits=xcronServiceBits(C->CommandLimit);S->RunBits=xcronServiceBits(C->Scheduler.RunLimit);xrtAtomic64Init(&S->WakeFailures,0);
    for(uint32 I=0;I<C->CommandLimit;++I){S->Commands[I].Generation=1;S->Commands[I].Next=I+1<C->CommandLimit?I+1:XCRON_SERVICE_NONE;}
    for(uint32 I=0;I<C->CompletionLimit;++I){S->Reservations[I].Next=I+1<C->CompletionLimit?I+1:XCRON_SERVICE_NONE;S->Reservations[I].HeapPosition=XCRON_SERVICE_NONE;}
    for(uint32 I=0;I<C->Scheduler.RunLimit;++I)S->ActiveMap[I]=XCRON_SERVICE_NONE;
    bool M=xrtMutexInit(&S->Mutex),D=false,W=false;
    if(M)D=xrtCondInit(&S->Changed);
    if(D)W=xrtEventInit(&S->Wake,false,false);
    if(W)S->Thread=xrtThreadCreate(xcronServiceOwner,S,C->StackSize);
    xcronserviceissue Failure={XCRON_SERVICE_SYSTEM,XERR_NONE};
    if(S->Thread){xrtMutexLock(&S->Mutex);while(!S->Ready)xrtCondWaitFor(&S->Changed,&S->Mutex,100);
        if(S->Startup.Code==XCRON_SERVICE_OK){*Out=S;S->Activated=true;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);if(Budget)xrtAllocationBudgetLeave(&Scope);xcronServiceOK(Issue);return true;}
        Failure=S->Startup;xrtMutexUnlock(&S->Mutex);xrtThreadWait(S->Thread);xrtThreadDestroy(S->Thread);xrtSetErrorTake(S->StartupError);
    }else Failure.Cause=xrtErrorKind(xrtGetError());
    xerror* Error=xrtTakeError();if(W)xrtEventUnit(&S->Wake);if(D)xrtCondUnit(&S->Changed);if(M)xrtMutexUnit(&S->Mutex);xrtFree(S);if(Budget)xrtAllocationBudgetLeave(&Scope);xrtSetErrorTake(Error);
    return xcronServiceFail(Failure.Code,Failure.Cause,Issue);
}
XCRON_API bool xcronServiceCreate(const xcronserviceconfig* C,xcronservice** Out,xcronserviceissue* Issue)
{return xcronServiceCreateMode(C,Out,Issue,0);}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API bool xcronServiceCreateSelected(const xcronselectedserviceconfig* C,xcronservice** Out,xcronserviceissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronServiceOutputs(NULL,C,sizeof(*C),O,N,2) || C->Reserved || !C->SubmissionBatchLimit || C->SubmissionBatchLimit>XCRON_EXECUTOR_BATCH_MAX)return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceCreateMode(&C->Service,Out,Issue,C->SubmissionBatchLimit);
}
#endif
static bool xcronServiceEnqueue(xcronservice* S,xcronrequestkind Kind,const xcronjobconfig* Add,const void* Policy,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    xcronserviceerror Code=S->Closing?XCRON_SERVICE_CLOSED:S->FreeCommand==XCRON_SERVICE_NONE?XCRON_SERVICE_LIMIT:XCRON_SERVICE_OK;
    if(Code!=XCRON_SERVICE_OK){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(Code,XERR_NONE,Issue);}
    if(Add && !xcronZoneRetain(Add->Zone)){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_LIMIT,xrtErrorKind(xrtGetError()),Issue);}
    uint32 I=S->FreeCommand;xcroncommand* C=&S->Commands[I];S->FreeCommand=C->Next;
    C->State=XCRON_CMD_QUEUED;C->Next=XCRON_SERVICE_NONE;C->Job=Job;C->Cancel=Cancel;if(Add)C->Add=*Add;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(Policy)S->Selected->Commands[I].Policy=*(const xcrondeliverypolicy*)Policy;
#else
    (void)Policy;
#endif
    C->Reply=(xcronreply){.Request=C->Generation<<S->CommandBits|I,.Kind=Kind};
    if(S->CommandTail==XCRON_SERVICE_NONE)S->CommandHead=I;else S->Commands[S->CommandTail].Next=I;S->CommandTail=I;++S->Stats.PendingRequests;
    *Request=C->Reply.Request;xrtMutexUnlock(&S->Mutex);xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceAddAsync(xcronservice* S,const xcronjobconfig* C,xcronrequestid* Request,xcronserviceissue* Issue)
{
    void* O[]={Request,Issue};size_t N[]={sizeof(*Request),Issue?sizeof(*Issue):0};
    if(!S || !C || !Request || !xcronServiceOutputs(S,C,sizeof(*C),O,N,2) || !xcronServiceMode(S,false) || !C->Zone || !xcronZoneOutputs(C->Zone,C,sizeof(*C),O,N,2) ||
        (C->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || C->Begin>C->End || C->Misfire<XCRON_MISFIRE_CATCH_UP || C->Misfire>XCRON_MISFIRE_COALESCE ||
        C->Overlap<XCRON_OVERLAP_WAIT || C->Overlap>XCRON_OVERLAP_SKIP || !C->RunningLimit || C->RunningLimit>S->Config.Scheduler.RunLimit || (C->Overlap!=XCRON_OVERLAP_ALLOW && C->RunningLimit!=1))
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_ADD,C,NULL,0,false,Request,Issue);
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API bool xcronServiceAddSelectedAsync(xcronservice* S,const xcronjobconfig* C,xcrondeliverypolicy Policy,xcronrequestid* Request,xcronserviceissue* Issue)
{
    void* O[]={Request,Issue};size_t N[]={sizeof(*Request),Issue?sizeof(*Issue):0};
    if(!S || !C || !Request || !xcronServiceOutputs(S,C,sizeof(*C),O,N,2) || !xcronServiceMode(S,true) || !C->Zone || !xcronZoneOutputs(C->Zone,C,sizeof(*C),O,N,2) ||
        (C->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || C->Begin>C->End || C->Misfire<XCRON_MISFIRE_CATCH_UP || C->Misfire>XCRON_MISFIRE_COALESCE ||
        C->Overlap<XCRON_OVERLAP_WAIT || C->Overlap>XCRON_OVERLAP_SKIP || !C->RunningLimit || C->RunningLimit>S->Config.Scheduler.RunLimit || (C->Overlap!=XCRON_OVERLAP_ALLOW && C->RunningLimit!=1) ||
        Policy.Fold<XCRON_FOLD_ALL || Policy.Fold>XCRON_FOLD_REJECT || Policy.Missing<XCRON_MISSING_SKIP || Policy.Missing>XCRON_MISSING_REJECT)return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_ADD,C,&Policy,0,false,Request,Issue);
}
#endif
XCRON_API bool xcronServiceCancelAsync(xcronservice* S,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_CANCEL,NULL,NULL,Job,Cancel,Request,Issue);
}
XCRON_API bool xcronServiceInfoAsync(xcronservice* S,xcronjobid Job,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_INFO,NULL,NULL,Job,false,Request,Issue);
}
XCRON_API bool xcronServiceReapAsync(xcronservice* S,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_REAP,NULL,NULL,0,false,Request,Issue);
}
static bool xcronServiceCurrent(const xcronservice* S)
{return xrtThreadCurrentId()==S->Owner || xrtExecutorIsCurrent(S->Config.Executor);}
static xwaitresult xcronServiceCollectValue(xcronservice* S,xcronrequestid Id,int64 Timeout,void* Reply,xcronserviceissue* Issue)
{
    double Start=xrtTimer();if(!xrtMutexLock(&S->Mutex)){xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);return XWAIT_ERROR;}
    for(;;){uint32 I=(uint32)(Id&((UINT64_C(1)<<S->CommandBits)-1));xcroncommand* C=I<S->Config.CommandLimit?&S->Commands[I]:NULL;
        if(!Id || !C || C->State==XCRON_CMD_FREE || C->Generation!=(Id>>S->CommandBits)){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_STALE,XERR_NONE,Issue);return XWAIT_ERROR;}
        if(C->State==XCRON_CMD_READY){
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
            if(S->Selected){xcronreply* R=&C->Reply;xcronselectedreply* D=Reply;*D=(xcronselectedreply){.Request=R->Request,.Kind=R->Kind,.Code=R->Code,.Job=R->Job,.Data=R->Data,.Found=R->Found,.Scheduler=R->Scheduler,.Executor=R->Executor,.Error=R->Error};
                if(R->Code==XCRON_SERVICE_OK && R->Kind==XCRON_REQUEST_INFO)D->Info=S->Selected->Commands[I].Value.Info;
                if(R->Code==XCRON_SERVICE_OK && R->Kind==XCRON_REQUEST_REAP && R->Found)D->Retired=S->Selected->Commands[I].Value.Retired;
            }else
#endif
            *(xcronreply*)Reply=C->Reply;
            C->Reply.Error=NULL;C->State=XCRON_CMD_FREE;--S->Stats.ReadyReplies;
            if(C->Generation<(UINT64_MAX>>S->CommandBits)){++C->Generation;C->Next=S->FreeCommand;S->FreeCommand=I;}
            xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_OK;}
        if(!Timeout){xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_TIMEOUT;}
        if(xcronServiceCurrent(S)){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_THREAD,XERR_NONE,Issue);return XWAIT_ERROR;}
        double Used=(xrtTimer()-Start)*1000.0;
        if(Used>=(double)Timeout){xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return XWAIT_TIMEOUT;}
        double Remaining=(double)Timeout-Used;int64 Wait=Remaining>1000.0?1000:(int64)ceil(Remaining);if(Wait<1)Wait=1;
        xwaitresult R=xrtCondWaitFor(&S->Changed,&S->Mutex,Wait);
        if(R==XWAIT_ERROR){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);return XWAIT_ERROR;}
    }
}
XCRON_API xwaitresult xcronServiceCollect(xcronservice* S,xcronrequestid Id,int64 Timeout,xcronreply* Reply,xcronserviceissue* Issue)
{
    if(!Reply || Timeout<0 || !xcronServiceArgs(S,Reply,sizeof(*Reply),Issue) || !xcronServiceMode(S,false)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    return xcronServiceCollectValue(S,Id,Timeout,Reply,Issue);
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API xwaitresult xcronServiceCollectSelected(xcronservice* S,xcronrequestid Id,int64 Timeout,xcronselectedreply* Reply,xcronserviceissue* Issue)
{
    if(!Reply || Timeout<0 || !xcronServiceArgs(S,Reply,sizeof(*Reply),Issue) || !xcronServiceMode(S,true)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    return xcronServiceCollectValue(S,Id,Timeout,Reply,Issue);
}
#endif
static bool xcronServiceDrainValue(xcronservice* S,void* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue)
{
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    size_t Written=0;while(Written<Capacity && S->DoneHead!=XCRON_SERVICE_NONE){uint32 I=S->DoneHead;xcronreservation* R=&S->Reservations[I];S->DoneHead=R->Next;
        if(S->DoneHead==XCRON_SERVICE_NONE)S->DoneTail=XCRON_SERVICE_NONE;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
        if(S->Selected){xcronselectedservicecompletion* D=(xcronselectedservicecompletion*)Completed+Written;D->Completion=R->Done;D->Local=S->Selected->Labels[I].Local;D->Candidates=S->Selected->Labels[I].Candidates;}else
#endif
        ((xcronservicecompletion*)Completed)[Written]=R->Done;
        ++Written;R->Done.Error=NULL;R->Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReadyCompletions;--S->Stats.ReservedCompletions;xcronServiceIncrement(&S->Stats.Collected);}
    *Count=Written;xrtMutexUnlock(&S->Mutex);if(Written)xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceDrain(xcronservice* S,xcronservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!S || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronServiceOutputs(S,NULL,0,O,N,3) || !xcronServiceMode(S,false))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceDrainValue(S,Completed,Capacity,Count,Issue);
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API bool xcronServiceDrainSelected(xcronservice* S,xcronselectedservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!S || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronServiceOutputs(S,NULL,0,O,N,3) || !xcronServiceMode(S,true))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceDrainValue(S,Completed,Capacity,Count,Issue);
}
#endif
XCRON_API bool xcronServiceClose(xcronservice* S,bool Cancel,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    S->Closing=true;if(Cancel)S->CancelRuns=true;if(!S->Stopped)S->Stats.State=XCRON_SERVICE_STOPPING;xrtMutexUnlock(&S->Mutex);xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API xwaitresult xcronServiceWaitFor(xcronservice* S,int64 Timeout,xcronserviceissue* Issue)
{
    if(Timeout<0 || !xcronServiceArgs(S,NULL,0,Issue)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    if(Timeout && xcronServiceCurrent(S)){xcronServiceFail(XCRON_SERVICE_THREAD,XERR_NONE,Issue);return XWAIT_ERROR;}
    xwaitresult R=xrtThreadWaitFor(S->Thread,Timeout);
    if(R==XWAIT_ERROR)xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);else xcronServiceOK(Issue);return R;
}
static bool xcronServiceReapStoppedValue(xcronservice* S,void* Retired,bool* Found,xcronserviceissue* Issue)
{
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    if(!S->Stopped){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);}
    bool OK;
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
    if(S->Selected)OK=xcronSchedulerReapSelected(S->Kernel,Retired,Found,NULL);else
#endif
    OK=xcronSchedulerReap(S->Kernel,Retired,Found,NULL);
    if(OK){xcronSchedulerInspect(S->Kernel,&S->Stats.Kernel,NULL);}xrtMutexUnlock(&S->Mutex);
    if(!OK)return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceReapStopped(xcronservice* S,xcronretired* Retired,bool* Found,xcronserviceissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronServiceOutputs(S,NULL,0,O,N,3) || !xcronServiceMode(S,false))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceReapStoppedValue(S,Retired,Found,Issue);
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API bool xcronServiceReapStoppedSelected(xcronservice* S,xcronselectedretired* Retired,bool* Found,xcronserviceissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronServiceOutputs(S,NULL,0,O,N,3) || !xcronServiceMode(S,true))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceReapStoppedValue(S,Retired,Found,Issue);
}
#endif
XCRON_API bool xcronServiceInspect(xcronservice* S,xcronservicestats* Stats,xcronserviceissue* Issue)
{
    if(!Stats || !xcronServiceArgs(S,Stats,sizeof(*Stats),Issue) || !xcronServiceMode(S,false))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Stats=S->Stats;Stats->WakeFailures=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
XCRON_API bool xcronServiceInspectSelected(xcronservice* S,xcronselectedservicestats* Stats,xcronserviceissue* Issue)
{
    if(!Stats || !xcronServiceArgs(S,Stats,sizeof(*Stats),Issue) || !xcronServiceMode(S,true))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Stats=(xcronselectedservicestats){.Service=S->Stats,.LastPoll=S->Selected->LastPoll,.BatchSubmissions=S->Selected->PublishedBatchSubmissions,.BatchedAttempts=S->Selected->PublishedBatchedAttempts};Stats->Service.WakeFailures=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);
    xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
#endif
XCRON_API bool xcronServiceFailure(xcronservice* S,xerror** Error,xcronserviceissue* Issue)
{
    if(!Error || !xcronServiceArgs(S,Error,sizeof(*Error),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Error=xrtErrorRef(S->FatalError);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceWake(xcronservice* S,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceDestroy(xcronservice* S,xcronserviceissue* Issue)
{
    if(!xcronServiceArgs(S,NULL,0,Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    xrtMutexLock(&S->Mutex);bool Busy=!S->Stopped || S->Stats.Kernel.Jobs || S->Stats.PendingRequests || S->Stats.ReadyReplies || S->Stats.ReservedCompletions;xrtMutexUnlock(&S->Mutex);
    if(Busy)return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);
    if(xrtThreadWaitFor(S->Thread,0)!=XWAIT_OK)return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);
    if(!xcronServiceWaiterRelease(S))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    if(!xcronSchedulerDestroy(S->Kernel,NULL))return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xrtThreadDestroy(S->Thread);xrtErrorFree(S->FatalError);xrtEventUnit(&S->Wake);xrtCondUnit(&S->Changed);xrtMutexUnit(&S->Mutex);S->Magic=0;xrtFree(S);xcronServiceOK(Issue);return true;
}
#endif
