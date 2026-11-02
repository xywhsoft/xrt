#include <xcron/service.h>
#include "../timezone/timezone_internal.h"
#include <xrt/atomic.h>
#include <xrt/sync.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <math.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE)
#define XCRON_SERVICE_MAGIC UINT32_C(0x58435631)
#define XCRON_SERVICE_NONE UINT32_MAX
typedef enum xcroncommandstate {XCRON_CMD_FREE=0,XCRON_CMD_QUEUED,XCRON_CMD_PROCESSING,XCRON_CMD_READY} xcroncommandstate;
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
    xcronrun Run;
    xcronservicecompletion Done;
    xerror* Rejection;
    uint64 Deadline,RetryAt;
    uint32 Attempts,Next,HeapPosition;
} xcronreservation;
struct xcronservice {
    uint32 Magic;
    size_t ArenaBytes;
    xcronserviceconfig Config;
    xmutex Mutex;
    xcond Changed;
    xevent Wake;
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
    uint8 CommandBits,RunBits;
    xcronclock Now;
    bool HasClock;
    uint64 SubmitAttempts,RejectedSubmissions,Retries,AdmissionExpired;
};
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
static void xcronServiceSubmit(xcronservice* S,uint32 I,bool Retry)
{
    xcronreservation* R=&S->Reservations[I];xcronexecutorissue E;uint64 Now=S->Now.MonotonicMilliseconds;
    if(Retry && Now>=R->Deadline){xerror* Error=R->Rejection;R->Rejection=NULL;xcronServiceIncrement(&S->AdmissionExpired);
        xcronServiceFinish(S,I,XCRON_RUN_FAILED,XCRON_COMPLETION_EXPIRED,XCRON_EXECUTOR_REJECTED,Error);return;}
    ++R->Attempts;xcronServiceIncrement(&S->SubmitAttempts);if(Retry)xcronServiceIncrement(&S->Retries);
    xrtClearError();bool OK=xcronExecutorSubmit(S->Bridge,&R->Run,&E);xerror* Error=xrtTakeError();xrtErrorFree(R->Rejection);R->Rejection=NULL;
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
static void xcronServiceCommand(xcronservice* S,uint32 I)
{
    xcroncommand* C=&S->Commands[I];xcronreply R={.Request=C->Reply.Request,.Kind=C->Reply.Kind};bool OK=false;
    xrtClearError();
    switch(R.Kind){
    case XCRON_REQUEST_ADD:{
        xrtMutexLock(&S->Mutex);bool Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        if(Closing){R.Code=XCRON_SERVICE_CLOSED;xcronServiceFail(R.Code,XERR_NONE,NULL);}
        else {OK=xcronSchedulerAdd(S->Kernel,&C->Add,&R.Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;}
        if(!OK)R.Data=C->Add.Data;
        xcronZoneRelease(C->Add.Zone);C->Add.Zone=NULL;break;}
    case XCRON_REQUEST_CANCEL:
        R.Job=C->Job;OK=xcronSchedulerCancel(S->Kernel,C->Job,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;
        if(OK && C->Cancel){uint32 Requested=0;OK=xcronExecutorCancelJob(S->Bridge,C->Job,&Requested,&R.Executor);if(!OK)R.Code=XCRON_SERVICE_EXECUTOR;xcronServiceCancelUnsent(S,C->Job);}break;
    case XCRON_REQUEST_INFO:R.Job=C->Job;OK=xcronSchedulerJob(S->Kernel,C->Job,&R.Info,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    case XCRON_REQUEST_REAP:OK=xcronSchedulerReap(S->Kernel,&R.Retired,&R.Found,&R.Scheduler);if(!OK)R.Code=XCRON_SERVICE_KERNEL;break;
    }
    if(!OK)R.Error=xrtTakeError();else xrtClearError();
    xrtMutexLock(&S->Mutex);C->Reply=R;C->State=XCRON_CMD_READY;--S->Stats.PendingRequests;++S->Stats.ReadyReplies;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);
}
static bool xcronServicePublishStats(xcronservice* S,xcronschedulerstats* K,xcronexecutorstats* E)
{
    if(!xcronSchedulerInspect(S->Kernel,K,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);return false;}
    if(!xcronExecutorInspect(S->Bridge,E,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);return false;}
    xrtMutexLock(&S->Mutex);S->Stats.Kernel=*K;S->Stats.Executor=*E;S->Stats.KernelBytes=K->Bytes;S->Stats.AdapterBytes=E->Bytes;
    S->Stats.Bytes=S->ArenaBytes+K->Bytes+E->Bytes;S->Stats.LastClock=S->Now;S->Stats.Unsent=S->RetryCount;
    S->Stats.SubmitAttempts=S->SubmitAttempts;S->Stats.RejectedSubmissions=S->RejectedSubmissions;S->Stats.Retries=S->Retries;S->Stats.AdmissionExpired=S->AdmissionExpired;
    xrtMutexUnlock(&S->Mutex);return true;
}
static int32 xcronServiceOwner(ptr Raw)
{
    xcronservice* S=Raw;xallocationscope Scope={0};bool Budget=false,OK=true;xcronserviceerror Code=XCRON_SERVICE_OK;
    S->Owner=xrtThreadCurrentId();
    if(S->Config.InitializationBudget){Budget=xrtAllocationBudgetEnter(S->Config.InitializationBudget,&Scope);if(!Budget){OK=false;Code=XCRON_SERVICE_MEMORY;}}
    if(OK && !xcronSchedulerCreate(&S->Config.Scheduler,&S->Kernel,NULL)){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_KERNEL;}
    xcronexecutorconfig B={.Executor=S->Config.Executor,.Proc=S->Config.Proc,.Context=S->Config.Context,.Notify=xcronServiceSignal,.NotifyContext=S,.RunLimit=S->Config.Scheduler.RunLimit,.BytesLimit=S->Config.AdapterBytesLimit};
    if(OK && !xcronExecutorCreate(&B,&S->Bridge,NULL)){OK=false;Code=xrtErrorKind(xrtGetError())==XERR_MEMORY?XCRON_SERVICE_MEMORY:XCRON_SERVICE_EXECUTOR;}
    xerror* StartupError=OK?NULL:xrtTakeError();
    if(Budget)xrtAllocationBudgetLeave(&Scope);
    if(!OK){if(S->Kernel){xcronSchedulerDestroy(S->Kernel,NULL);S->Kernel=NULL;}
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
        size_t Completed=0;
        if(!xcronExecutorDrain(S->Bridge,S->DoneBatch,S->Config.BatchLimit,&Completed,NULL))xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);
        for(size_t N=0;N<Completed;++N){xcroncompletion* D=&S->DoneBatch[N];uint32 Index=xcronServiceRunIndex(S,D->Run.Id);
            uint32 I=Index<S->Config.Scheduler.RunLimit?S->ActiveMap[Index]:XCRON_SERVICE_NONE;
            if(I==XCRON_SERVICE_NONE || S->Reservations[I].Run.Id!=D->Run.Id){xrtErrorFree(D->Error);xrtClearError();xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);continue;}
            xcronServiceFinish(S,I,D->Result,XCRON_COMPLETION_NORMAL,D->Code,D->Error);
        }
        xrtMutexLock(&S->Mutex);Closing=S->Closing;xrtMutexUnlock(&S->Mutex);
        bool More=false;
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
                xcronpoll P={0};bool Polled=false;
                if(Reserved){Polled=xcronSchedulerPoll(S->Kernel,S->Now,S->RunBatch,Reserved,&P,NULL);if(!Polled)xcronServiceFault(S,XCRON_SERVICE_KERNEL);}
                for(uint32 N=0;N<(uint32)P.Count;++N){uint32 I=S->ReservedBatch[N];xcronreservation* R=&S->Reservations[I];
                    R->Run=S->RunBatch[N];R->Attempts=0;R->Rejection=NULL;R->HeapPosition=XCRON_SERVICE_NONE;R->Deadline=xcronServiceAddTime(S->Now.MonotonicMilliseconds,S->Config.AdmissionMilliseconds);
                    S->ActiveMap[xcronServiceRunIndex(S,R->Run.Id)]=I;xcronServiceSubmit(S,I,false);}
                xrtMutexLock(&S->Mutex);
                for(uint32 N=(uint32)P.Count;N<Reserved;++N){uint32 I=S->ReservedBatch[N];S->Reservations[I].Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReservedCompletions;}
                xrtMutexUnlock(&S->Mutex);More=Polled && P.More;
            }
        }
        xcronschedulerstats K;xcronexecutorstats E;if(!xcronServicePublishStats(S,&K,&E))continue;
        xrtMutexLock(&S->Mutex);Closing=S->Closing;bool Commands=S->CommandHead!=XCRON_SERVICE_NONE;
        bool Capacity=S->FreeCompletion!=XCRON_SERVICE_NONE && E.Active<S->Config.Scheduler.RunLimit;
        bool Stop=Closing && AppliedClose && (!S->CancelRuns || AppliedCancel) && !K.Running && !E.Active && !E.Producers && !S->RetryCount && !S->Stats.PendingRequests;
        xrtMutexUnlock(&S->Mutex);
        if(Stop){
            if(!xcronExecutorDestroy(S->Bridge,NULL)){xcronServiceFault(S,XCRON_SERVICE_EXECUTOR);continue;}S->Bridge=NULL;
            xrtMutexLock(&S->Mutex);S->Stopped=true;S->Stats.State=S->Stats.Failure.Code==XCRON_SERVICE_OK?XCRON_SERVICE_STOPPED:XCRON_SERVICE_FAILED;xrtCondBroadcast(&S->Changed);xrtMutexUnlock(&S->Mutex);return 0;
        }
        uint64 Delay=S->Config.ResampleMilliseconds;
        if(!Closing && S->HasClock){xcronwake W;
            if(!xcronSchedulerWake(S->Kernel,S->Now,Delay,&W,NULL)){xcronServiceFault(S,XCRON_SERVICE_KERNEL);continue;}
            if(Capacity && W.DelayMilliseconds<Delay)Delay=W.DelayMilliseconds;
            if(S->RetryCount){xcronreservation* R=&S->Reservations[S->RetryHeap[0]];uint64 At=R->RetryAt<R->Deadline?R->RetryAt:R->Deadline;
                uint64 Wait=At<=S->Now.MonotonicMilliseconds?0:At-S->Now.MonotonicMilliseconds;if(Wait<Delay)Delay=Wait;}
        }
        if(Commands || Completed==S->Config.BatchLimit || (More && Capacity))Delay=0;
        /* The auto-reset event is never reset after the work check. */
        if(xrtEventWaitFor(&S->Wake,(int64)Delay)==XWAIT_ERROR)xcronServiceFault(S,XCRON_SERVICE_SYSTEM);
    }
}
static bool xcronServiceArray(size_t Count,size_t Unit,size_t Align,size_t* Total,size_t* Offset)
{
    size_t Padding=(Align-*Total%Align)%Align;if(Padding>SIZE_MAX-*Total)return false;*Total+=Padding;*Offset=*Total;
    if(Count>(SIZE_MAX-*Total)/Unit)return false;
    *Total+=Count*Unit;return true;
}
XCRON_API bool xcronServiceCreate(const xcronserviceconfig* C,xcronservice** Out,xcronserviceissue* Issue)
{
    void* O[]={Out,Issue};size_t N[]={sizeof(*Out),Issue?sizeof(*Issue):0};
    if(!C || !Out || !xcronServiceOutputs(NULL,C,sizeof(*C),O,N,2) || !C->Executor || !C->Proc || C->Reserved || C->Scheduler.Reserved ||
        !C->Scheduler.JobLimit || C->Scheduler.JobLimit>UINT32_C(1073741824) || !C->Scheduler.RunLimit || C->Scheduler.RunLimit>UINT32_C(1073741824) || !C->Scheduler.WorkLimit ||
        !C->CommandLimit || C->CommandLimit>UINT32_C(1073741824) || !C->CompletionLimit || C->CompletionLimit>UINT32_C(1073741824) ||
        !C->BatchLimit || C->BatchLimit>C->CompletionLimit || C->BatchLimit>C->Scheduler.RunLimit || !C->CommandWorkLimit || !C->ResampleMilliseconds || !C->RetryIntervalMilliseconds || !C->RetryAttempts)
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    size_t Bytes=sizeof(xcronservice),Offsets[7];
    if(!xcronServiceArray(C->CommandLimit,sizeof(xcroncommand),_Alignof(xcroncommand),&Bytes,&Offsets[0]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(xcronreservation),_Alignof(xcronreservation),&Bytes,&Offsets[1]) ||
        !xcronServiceArray(C->Scheduler.RunLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[2]) ||
        !xcronServiceArray(C->CompletionLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[3]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(uint32),_Alignof(uint32),&Bytes,&Offsets[4]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(xcronrun),_Alignof(xcronrun),&Bytes,&Offsets[5]) ||
        !xcronServiceArray(C->BatchLimit,sizeof(xcroncompletion),_Alignof(xcroncompletion),&Bytes,&Offsets[6]) ||
        Bytes>C->BytesLimit || C->Scheduler.BytesLimit>C->BytesLimit-Bytes || C->AdapterBytesLimit>C->BytesLimit-Bytes-C->Scheduler.BytesLimit)
        return xcronServiceFail(XCRON_SERVICE_LIMIT,XERR_NONE,Issue);
    xallocationscope Scope={0};bool Budget=false;
    if(C->InitializationBudget){Budget=xrtAllocationBudgetEnter(C->InitializationBudget,&Scope);if(!Budget)return xcronServiceFail(XCRON_SERVICE_MEMORY,xrtErrorKind(xrtGetError()),Issue);}
    xcronservice* S=xrtMalloc(Bytes);
    if(!S){if(Budget)xrtAllocationBudgetLeave(&Scope);return xcronServiceFail(XCRON_SERVICE_MEMORY,XERR_MEMORY,Issue);}
    memset(S,0,Bytes);S->Magic=XCRON_SERVICE_MAGIC;S->ArenaBytes=Bytes;S->Config=*C;S->Stats.ArenaBytes=Bytes;S->Stats.Bytes=Bytes;
    S->Commands=(xcroncommand*)((unsigned char*)S+Offsets[0]);S->Reservations=(xcronreservation*)((unsigned char*)S+Offsets[1]);S->ActiveMap=(uint32*)((unsigned char*)S+Offsets[2]);S->RetryHeap=(uint32*)((unsigned char*)S+Offsets[3]);S->ReservedBatch=(uint32*)((unsigned char*)S+Offsets[4]);S->RunBatch=(xcronrun*)((unsigned char*)S+Offsets[5]);S->DoneBatch=(xcroncompletion*)((unsigned char*)S+Offsets[6]);
    S->CommandHead=S->CommandTail=S->DoneHead=S->DoneTail=XCRON_SERVICE_NONE;S->CommandBits=xcronServiceBits(C->CommandLimit);S->RunBits=xcronServiceBits(C->Scheduler.RunLimit);xrtAtomic64Init(&S->WakeFailures,0);
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
static bool xcronServiceEnqueue(xcronservice* S,xcronrequestkind Kind,const xcronjobconfig* Add,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    xcronserviceerror Code=S->Closing?XCRON_SERVICE_CLOSED:S->FreeCommand==XCRON_SERVICE_NONE?XCRON_SERVICE_LIMIT:XCRON_SERVICE_OK;
    if(Code!=XCRON_SERVICE_OK){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(Code,XERR_NONE,Issue);}
    if(Add && !xcronZoneRetain(Add->Zone)){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_LIMIT,xrtErrorKind(xrtGetError()),Issue);}
    uint32 I=S->FreeCommand;xcroncommand* C=&S->Commands[I];S->FreeCommand=C->Next;
    C->State=XCRON_CMD_QUEUED;C->Next=XCRON_SERVICE_NONE;C->Job=Job;C->Cancel=Cancel;if(Add)C->Add=*Add;
    C->Reply=(xcronreply){.Request=C->Generation<<S->CommandBits|I,.Kind=Kind};
    if(S->CommandTail==XCRON_SERVICE_NONE)S->CommandHead=I;else S->Commands[S->CommandTail].Next=I;S->CommandTail=I;++S->Stats.PendingRequests;
    *Request=C->Reply.Request;xrtMutexUnlock(&S->Mutex);xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceAddAsync(xcronservice* S,const xcronjobconfig* C,xcronrequestid* Request,xcronserviceissue* Issue)
{
    void* O[]={Request,Issue};size_t N[]={sizeof(*Request),Issue?sizeof(*Issue):0};
    if(!S || !C || !Request || !xcronServiceOutputs(S,C,sizeof(*C),O,N,2) || !C->Zone || !xcronZoneOutputs(C->Zone,C,sizeof(*C),O,N,2) ||
        (C->SearchFlags&~XCRON_SEARCH_EXCLUSIVE) || C->Begin>C->End || C->Misfire<XCRON_MISFIRE_CATCH_UP || C->Misfire>XCRON_MISFIRE_COALESCE ||
        C->Overlap<XCRON_OVERLAP_WAIT || C->Overlap>XCRON_OVERLAP_SKIP || !C->RunningLimit || C->RunningLimit>S->Config.Scheduler.RunLimit || (C->Overlap!=XCRON_OVERLAP_ALLOW && C->RunningLimit!=1))
        return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_ADD,C,0,false,Request,Issue);
}
XCRON_API bool xcronServiceCancelAsync(xcronservice* S,xcronjobid Job,bool Cancel,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_CANCEL,NULL,Job,Cancel,Request,Issue);
}
XCRON_API bool xcronServiceInfoAsync(xcronservice* S,xcronjobid Job,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Job || !Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_INFO,NULL,Job,false,Request,Issue);
}
XCRON_API bool xcronServiceReapAsync(xcronservice* S,xcronrequestid* Request,xcronserviceissue* Issue)
{
    if(!Request || !xcronServiceArgs(S,Request,sizeof(*Request),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    return xcronServiceEnqueue(S,XCRON_REQUEST_REAP,NULL,0,false,Request,Issue);
}
static bool xcronServiceCurrent(const xcronservice* S)
{return xrtThreadCurrentId()==S->Owner || xrtExecutorIsCurrent(S->Config.Executor);}
XCRON_API xwaitresult xcronServiceCollect(xcronservice* S,xcronrequestid Id,int64 Timeout,xcronreply* Reply,xcronserviceissue* Issue)
{
    if(!Reply || Timeout<0 || !xcronServiceArgs(S,Reply,sizeof(*Reply),Issue)){xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);return XWAIT_ERROR;}
    double Start=xrtTimer();if(!xrtMutexLock(&S->Mutex)){xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);return XWAIT_ERROR;}
    for(;;){uint32 I=(uint32)(Id&((UINT64_C(1)<<S->CommandBits)-1));xcroncommand* C=I<S->Config.CommandLimit?&S->Commands[I]:NULL;
        if(!Id || !C || C->State==XCRON_CMD_FREE || C->Generation!=(Id>>S->CommandBits)){xrtMutexUnlock(&S->Mutex);xcronServiceFail(XCRON_SERVICE_STALE,XERR_NONE,Issue);return XWAIT_ERROR;}
        if(C->State==XCRON_CMD_READY){*Reply=C->Reply;C->Reply.Error=NULL;C->State=XCRON_CMD_FREE;--S->Stats.ReadyReplies;
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
XCRON_API bool xcronServiceDrain(xcronservice* S,xcronservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue)
{
    void* O[]={Completed,Count,Issue};size_t N[]={Capacity<=SIZE_MAX/sizeof(*Completed)?Capacity*sizeof(*Completed):SIZE_MAX,sizeof(*Count),Issue?sizeof(*Issue):0};
    if(!S || !Completed || !Count || !Capacity || Capacity>SIZE_MAX/sizeof(*Completed) || !xcronServiceOutputs(S,NULL,0,O,N,3))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    size_t Written=0;while(Written<Capacity && S->DoneHead!=XCRON_SERVICE_NONE){uint32 I=S->DoneHead;xcronreservation* R=&S->Reservations[I];S->DoneHead=R->Next;
        if(S->DoneHead==XCRON_SERVICE_NONE)S->DoneTail=XCRON_SERVICE_NONE;
        Completed[Written++]=R->Done;R->Done.Error=NULL;R->Next=S->FreeCompletion;S->FreeCompletion=I;--S->Stats.ReadyCompletions;--S->Stats.ReservedCompletions;xcronServiceIncrement(&S->Stats.Collected);}
    *Count=Written;xrtMutexUnlock(&S->Mutex);if(Written)xcronServiceSignal(S);xcronServiceOK(Issue);return true;
}
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
XCRON_API bool xcronServiceReapStopped(xcronservice* S,xcronretired* Retired,bool* Found,xcronserviceissue* Issue)
{
    void* O[]={Retired,Found,Issue};size_t N[]={sizeof(*Retired),sizeof(*Found),Issue?sizeof(*Issue):0};
    if(!S || !Retired || !Found || !xcronServiceOutputs(S,NULL,0,O,N,3))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    if(!S->Stopped){xrtMutexUnlock(&S->Mutex);return xcronServiceFail(XCRON_SERVICE_BUSY,XERR_NONE,Issue);}
    bool OK=xcronSchedulerReap(S->Kernel,Retired,Found,NULL);if(OK)xcronSchedulerInspect(S->Kernel,&S->Stats.Kernel,NULL);xrtMutexUnlock(&S->Mutex);
    if(!OK)return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xcronServiceOK(Issue);return true;
}
XCRON_API bool xcronServiceInspect(xcronservice* S,xcronservicestats* Stats,xcronserviceissue* Issue)
{
    if(!Stats || !xcronServiceArgs(S,Stats,sizeof(*Stats),Issue))return xcronServiceFail(XCRON_SERVICE_ARGUMENT,XERR_NONE,NULL);
    if(!xrtMutexLock(&S->Mutex))return xcronServiceFail(XCRON_SERVICE_SYSTEM,xrtErrorKind(xrtGetError()),Issue);
    *Stats=S->Stats;Stats->WakeFailures=xrtAtomic64Load(&S->WakeFailures,XMEMORY_RELAXED);xrtMutexUnlock(&S->Mutex);xcronServiceOK(Issue);return true;
}
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
    if(!xcronSchedulerDestroy(S->Kernel,NULL))return xcronServiceFail(XCRON_SERVICE_KERNEL,xrtErrorKind(xrtGetError()),Issue);
    xrtThreadDestroy(S->Thread);xrtErrorFree(S->FatalError);xrtEventUnit(&S->Wake);xrtCondUnit(&S->Changed);xrtMutexUnit(&S->Mutex);S->Magic=0;xrtFree(S);xcronServiceOK(Issue);return true;
}
#endif
