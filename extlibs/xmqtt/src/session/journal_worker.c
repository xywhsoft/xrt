#include <xmqtt/journal_worker.h>
#include "../internal/xmqtt_store_compact_online.h"
#include "../internal/xmqtt_journal_worker.h"
#include "../internal/xmqtt_store_write.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
typedef struct xmqttjwitem {
    struct xmqttjwitem *Next,*OwnedNext,*OwnedPrevious;
    xmqttjournalworker* Owner;size_t Allocation;double Deadline;xpromise* Promise;xcancel* Cancel;
    xmqttcommitresult Result;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    uint64 InboxKey;
#endif
} xmqttjwitem;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
typedef struct xmqttjwcompact {
    xmqttstorecompactconfig Config;
    xmqttstorecompactresult Result;
} xmqttjwcompact;
#define XMQTT_JW_COMPACT_PARAM ,const xmqttstorecompactconfig* Compact
#define XMQTT_JW_COMPACT_ARG(C) ,C
#else
#define XMQTT_JW_COMPACT_PARAM
#define XMQTT_JW_COMPACT_ARG(C)
#endif
typedef struct xmqttjwpump {
    struct xmqttjwpump* Next;xmqttjournalworker* Owner;xfuture* Future;bool Ran,Dropped;
} xmqttjwpump;
struct xmqttjournalworker {
    xmqttjournalworkerconfig Config;xmqttjournalworkerinfo Info;
    xtaskpool* Pool;xmqttjournal* Journal;xmutex Gate;xcond Ready;xatomic32 Refs;
    xmqttjwitem *Head,*Tail,*Owned;xmqttjwpump* Pumps;bool Running;size_t TurnAccepted;
    uintptr_t Low,High;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    xmqttautocompactstate* Auto;
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    xmqttstoreprepare Prepare;xmqttstorefinish Finish;ptr Store;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    xmqttstoresettle Settle;
#endif
#endif
};
typedef struct xmqttjwscope {struct xmqttjwscope* Previous;xmqttjournalworker* Owner;} xmqttjwscope;
static _Thread_local xmqttjwscope* xmqttJWCurrent;
static bool xmqttJWInside(const xmqttjournalworker* W)
{for(xmqttjwscope* S=xmqttJWCurrent;S;S=S->Previous)if(S->Owner==W)return true;return false;}
static bool xmqttJWError(xerrkind K,cstr M){xrtSetErrorInfo(K,"xrt.mqtt.journal.worker",6600+(int32)K,M);return false;}
static bool xmqttJWSpan(const void* P,size_t N){return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttJWSeparate(const void* P,size_t N,const void* Q,size_t M){return !N||!M||!xrtMemRangesOverlap(P,N,Q,M);}
static void xmqttJWRef(xmqttjournalworker* W){xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);}
static void xmqttJWDrop(xmqttjournalworker* W)
{if(xrtAtomic32FetchSub(&W->Refs,1,XMEMORY_ACQ_REL)==1){xrtCondUnit(&W->Ready);xrtMutexUnit(&W->Gate);xrtFree(W);}}
static void xmqttJWCount(uint64* N){if(*N!=UINT64_MAX)++*N;}
static void xmqttJWRange(xmqttjournalworker* W,const void* P,size_t N)
{uintptr_t A=(uintptr_t)P;if(A<W->Low)W->Low=A;if(A+N>W->High)W->High=A+N;}
/* Gate held. Conservative envelope keeps ordinary stack outputs O(1). */
static bool xmqttJWOutput(xmqttjournalworker* W,void* P,size_t N)
{
    if(!xmqttJWSpan(P,N))return false;
    uintptr_t A=(uintptr_t)P;
    if(A>=W->High||A+N<=W->Low)return true;
    if(!xmqttJWSeparate(P,N,W,sizeof(*W)))return false;
    for(xmqttjwitem* J=W->Owned;J;J=J->OwnedNext)if(!xmqttJWSeparate(P,N,J,J->Allocation))return false;
    for(xmqttjwpump* T=W->Pumps;T;T=T->Next)if(!xmqttJWSeparate(P,N,T,sizeof(*T)))return false;
    return true;
}
static void xmqttJWUncharge(xmqttjournalworker* W,size_t N,bool Request)
{
    xrtMutexLock(&W->Gate);--W->Info.Objects;W->Info.Bytes-=N;if(Request)--W->Info.Requests;
    xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);xmqttJWDrop(W);
}
static void xmqttJWResultFree(ptr Value,ptr Data)
{
    (void)Value;xmqttjwitem* J=Data;xmqttjournalworker* W=J->Owner;size_t N=J->Allocation;
    xrtMutexLock(&W->Gate);if(J->OwnedPrevious)J->OwnedPrevious->OwnedNext=J->OwnedNext;else W->Owned=J->OwnedNext;
    if(J->OwnedNext)J->OwnedNext->OwnedPrevious=J->OwnedPrevious;
    xrtMutexUnlock(&W->Gate);
    xrtErrorFree((xerror*)J->Result.Cause);xrtSecureZero(J,N);xrtFree(J);xmqttJWUncharge(W,N,false);
}
/* Native Future becomes terminal only after Data Drop has returned. Reap
 * outside the gate; credits survive actual Core Future/context destruction. */
static void xmqttJWReap(xmqttjournalworker* W)
{
    xmqttjwpump* List=NULL;xrtMutexLock(&W->Gate);xmqttjwpump** At=&W->Pumps;
    while(*At){xmqttjwpump* T=*At;if(T->Dropped&&T->Future&&xrtFutureDone(T->Future)){*At=T->Next;T->Next=List;List=T;}else At=&T->Next;}
    xrtMutexUnlock(&W->Gate);
    while(List){xmqttjwpump* T=List;List=T->Next;xrtFutureDestroy(T->Future);xrtFree(T);
        xrtMutexLock(&W->Gate);--W->Info.Tasks;W->Info.Bytes-=sizeof(*T);xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);xmqttJWDrop(W);}
}
static xmqttcommitstatus xmqttJWStop(xmqttjournalworker* W,xmqttjwitem* J,xcancel* Native)
{
    if(W->Info.Failed)return XMQTT_COMMIT_INITIALIZATION;
    if(xrtCancelRequested(Native)){W->Info.Failed=W->Info.Closed=true;return XMQTT_COMMIT_CANCELLED;}
    if(W->Info.Closed)return XMQTT_COMMIT_CLOSED;
    if(J&&xrtCancelRequested(J->Cancel))return XMQTT_COMMIT_CANCELLED;
    return J&&xrtTimer()>=J->Deadline?XMQTT_COMMIT_TIMEOUT:XMQTT_COMMIT_OK;
}
static void xmqttJWFinish(xmqttjournalworker* W,xmqttjwitem* J)
{
    xpromise* P=J->Promise;xcancel* C=J->Cancel;J->Promise=NULL;J->Cancel=NULL;xrtCancelDestroy(C);
    xrtMutexLock(&W->Gate);--W->Info.Requests;xmqttJWCount(&W->Info.Completed);xrtCondBroadcast(&W->Ready);
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    bool Closed=W->Info.Closed;
#endif
    xrtMutexUnlock(&W->Gate);
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    /* Run for every accepted item, including a cancelled native pump. The
     * model is installed and no worker gate is held before this callback. */
    if(W->Settle)W->Settle(W->Store,J->InboxKey,&J->Result,Closed);
#endif
    /* Only this owner can complete P. Publication invokes arbitrary watches
     * outside Gate; their retained result still owns the exact object credits. */
    ptr Value=&J->Result;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    if(!J->Result.Data.Data){
        xmqttjwcompact* C=(xmqttjwcompact*)(J+1);
        C->Result.Status=J->Result.Status;
        C->Result.Cause=J->Result.Cause;
        Value=&C->Result;
    }
#endif
    if(!xrtPromiseResolveOwned(P,Value,xmqttJWResultFree,J))abort();
    xrtPromiseDestroy(P); /* may free J: no access afterwards */
}
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
typedef struct xmqttjwstop {
    xmqttjournalworker* Worker;
    xmqttjwitem* Item;
    xcancel* Native;
} xmqttjwstop;

static bool xmqttJWStopCompact(ptr Context)
{
    const xmqttjwstop* S=Context;
    xrtMutexLock(&S->Worker->Gate);
    bool Stop=xmqttJWStop(S->Worker,S->Item,S->Native)!=XMQTT_COMMIT_OK;
    xrtMutexUnlock(&S->Worker->Gate);
    return Stop;
}
static void xmqttJWCompact(xmqttjournalworker* W,xmqttjwitem* J,xcancel* Native,
    const xmqttstorecompactconfig* Config,xmqttstorecompactresult* Result,xmqttjournalinfo After,
    xmqttcompactionrequest* Request)
{
    xerror* Saved=NULL;
    bool LocalFailure=!xrtTrySwapErrorTake(NULL,&Saved);
    xmqttjournalstatus Status=XMQTT_JOURNAL_ERROR;
    if(!LocalFailure){
        xmqttjwstop Stop={W,J,Native};
        Status=xmqttInternalStoreCompactWork(W->Store,&W->Journal,W,Config,xmqttJWStopCompact,&Stop,Result,Request);
        if(!xmqttInternalJournalWorkerInfo(W->Journal,W,&After))abort();
        if(Status>=XMQTT_JOURNAL_EMPTY){
            xerror* Discard=NULL;
            if(!xrtTrySwapErrorTake(Saved,&Discard)){xrtErrorFree(Saved);LocalFailure=true;}
            xrtErrorFree(Discard);
        }else{
            Result->Cause=xrtErrorRef(xrtGetError());
            xrtErrorFree(Saved);
        }
    }
    Result->StorageStatus=Status;
    xrtMutexLock(&W->Gate);
    W->Info.Journal=After;
    if(LocalFailure||After.State==XMQTT_JOURNAL_POISONED)W->Info.Failed=W->Info.Closed=true;
    if(LocalFailure||Status==XMQTT_JOURNAL_ERROR)Result->Status=XMQTT_COMMIT_INITIALIZATION;
    else if(Status==XMQTT_JOURNAL_STATE)Result->Status=XMQTT_COMMIT_INVALID;
    else if(Status<0)Result->Status=XMQTT_COMMIT_STORAGE;
    else if(Status==XMQTT_JOURNAL_LIMIT||Status==XMQTT_JOURNAL_BUSY)Result->Status=XMQTT_COMMIT_LIMIT;
    else Result->Status=xmqttJWStop(W,J,Native);
    xrtMutexUnlock(&W->Gate);
}

/* Gate held. A cooldown permits a later foreground request to retry a
 * definite transient failure even when there is no room to grow the file. */
static bool xmqttJWAutoDue(const xmqttautocompactstate* A,uint64 End)
{return End>=A->Info.NextEligibleBytes||(A->Info.RetryAfter>0&&xrtTimer()>=A->Info.RetryAfter);}

/* The sole pump owns the actual Journal. Configuration is immutable until
 * this pump is idle, and the application can copy its counters under Gate. */
static bool xmqttJWAutoCompact(xmqttjournalworker* W,xmqttjwitem* Pressure,xcancel* Native,
    xmqttcompactionrequest* Request)
{
    xrtMutexLock(&W->Gate);
    xmqttautocompactstate* A=W->Auto;
    xmqttjournalinfo Before=W->Info.Journal;
    bool Repeated=Request&&A&&A->DeniedEnd==Before.CommittedEnd&&
        Request->Operations>=A->DeniedOperations&&Request->Bytes>=A->DeniedBytes;
    bool Eligible=A&&!Repeated&&xmqttJWAutoDue(A,Before.CommittedEnd)&&
        (Pressure||(!W->Head&&!W->Info.Requests&&Before.CommittedEnd>=A->Config.TriggerBytes))&&
        xmqttJWStop(W,Pressure,Native)==XMQTT_COMMIT_OK;
    if(Eligible){
        A->Info.Running=true;
        W->Info.Busy=true;
        xmqttJWCount(&A->Info.Attempts);
    }
    xrtMutexUnlock(&W->Gate);
    if(!Eligible)return false;

    xmqttstorecompactresult Result={0};
    xmqttJWCompact(W,Pressure,Native,&A->Config.Compact,&Result,Before,Request);
    bool Rotated=Result.Effect==XMQTT_JOURNAL_COMMITTED;
    xrtMutexLock(&W->Gate);
    uint64 End=W->Info.Journal.CommittedEnd,Retry=A->Config.RetryGrowthBytes;
    if(Rotated){
        xmqttJWCount(&A->Info.Rotations);
        A->DeniedEnd=A->Info.NextEligibleBytes=0;
        A->Info.RetryAfter=0;
    }else if(Request&&Request->NoRoom){
        /* Larger requests share this bound at the same cut. Keep smaller
         * requests eligible for their own checkpoint headroom check. */
        A->DeniedEnd=Before.CommittedEnd;
        A->DeniedOperations=Request->Operations;
        A->DeniedBytes=Request->Bytes;
    }else{
        A->Info.NextEligibleBytes=Retry>UINT64_MAX-End?UINT64_MAX:End+Retry;
        bool Transient=!W->Info.Closed&&
            (Result.StorageStatus==XMQTT_JOURNAL_IO||Result.StorageStatus==XMQTT_JOURNAL_BUSY||
             Result.StorageStatus==XMQTT_JOURNAL_ERROR);
        A->Info.RetryAfter=Transient&&A->Config.RetryDelayMs?xrtTimer()+(double)A->Config.RetryDelayMs/1000.0:0;
    }
    A->Info.Status=Result.Status;A->Info.StorageStatus=Result.StorageStatus;A->Info.Effect=Result.Effect;
    A->Info.Generation=Result.Generation;
    A->Info.PreviousBytes=Result.PreviousBytes;A->Info.Bytes=Result.Bytes;
    A->Info.Running=false;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    bool Closed=W->Info.Closed;
#endif
    xrtMutexUnlock(&W->Gate);
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    if(Closed&&W->Settle){
        xmqttcommitresult Failed={.Status=Result.Status,.Effect=Result.Effect};
        W->Settle(W->Store,0,&Failed,Closed);
    }
#endif
    xrtErrorFree((xerror*)Result.Cause);
    return Rotated;
}
#endif

static void xmqttJWCommitOne(xmqttjournalworker* W,xmqttjwitem* J,xcancel* Cancel,
    xmqttjournalinfo Before XMQTT_SW_PRESSURE_PARAM)
{
    if(J->Result.Status==XMQTT_COMMIT_OK){
        xerror* Saved=NULL;bool LocalFailure=!xrtTrySwapErrorTake(NULL,&Saved);
        xmqttjournalstatus S=XMQTT_JOURNAL_ERROR;xmqttjournalinfo After=Before;bool PolicyRefused=false;
        if(!LocalFailure){uint64 Sequence=0;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
            if(W->Prepare){
                J->Result.Status=W->Prepare(W->Store,J->Result.Data XMQTT_SW_INBOX_ARG(J->InboxKey)
                    XMQTT_SW_PRESSURE_ARG(Request));PolicyRefused=J->Result.Status!=XMQTT_COMMIT_OK;
                if(PolicyRefused&&(J->Result.Status==XMQTT_COMMIT_INVALID||J->Result.Status==XMQTT_COMMIT_INITIALIZATION))J->Result.Cause=xrtErrorRef(xrtGetError());
                if(!PolicyRefused){xrtMutexLock(&W->Gate);J->Result.Status=xmqttJWStop(W,J,Cancel);xrtMutexUnlock(&W->Gate);PolicyRefused=J->Result.Status!=XMQTT_COMMIT_OK;}
            }
#endif
            if(PolicyRefused)S=XMQTT_JOURNAL_EMPTY;
            else {J->Result.AppendCalled=true;S=xmqttInternalJournalWorkerAppend(W->Journal,W,J->Result.Data,&Sequence);if(!xmqttInternalJournalWorkerInfo(W->Journal,W,&After))abort();}
            if(After.AttemptSequence>Before.Sequence){J->Result.AttemptSequence=After.AttemptSequence;J->Result.Effect=After.Effect;
                if(After.Effect==XMQTT_JOURNAL_COMMITTED)J->Result.Sequence=After.Sequence;}
            if(S>=0){xerror* Discard=NULL;if(!xrtTrySwapErrorTake(Saved,&Discard)){xrtErrorFree(Saved);LocalFailure=true;}xrtErrorFree(Discard);}
            else {J->Result.Cause=xrtErrorRef(xrtGetError());xrtErrorFree(Saved);}
        }
        J->Result.AppendStatus=S;
        xrtMutexLock(&W->Gate);W->Info.Journal=After;
        if(LocalFailure||S<0||(PolicyRefused&&J->Result.Status==XMQTT_COMMIT_INITIALIZATION)){W->Info.Failed=W->Info.Closed=true;J->Result.Status=LocalFailure?XMQTT_COMMIT_INITIALIZATION:S<0?XMQTT_COMMIT_STORAGE:J->Result.Status;}
        else if(PolicyRefused){} /* typed semantic/normal refusal, no append */
        else if(S==XMQTT_JOURNAL_LIMIT)J->Result.Status=XMQTT_COMMIT_LIMIT;
        else J->Result.Status=xmqttJWStop(W,J,Cancel);
        xrtMutexUnlock(&W->Gate);
    }
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(W->Finish){W->Finish(W->Store,&J->Result);
        if(J->Result.Status==XMQTT_COMMIT_OK){xrtMutexLock(&W->Gate);J->Result.Status=xmqttJWStop(W,J,Cancel);xrtMutexUnlock(&W->Gate);}}
#endif
}

static xtaskoutcome xmqttJWPump(xcancel* Cancel,ptr Data,xtaskvalue* Value)
{
    (void)Value;xmqttjwpump* T=Data;xmqttjournalworker* W=T->Owner;xmqttjwscope Scope={xmqttJWCurrent,W};xmqttJWCurrent=&Scope;
    xrtMutexLock(&W->Gate);while(W->Info.Scheduling)xrtCondWait(&W->Ready,&W->Gate);T->Ran=true;xrtMutexUnlock(&W->Gate);
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    bool IdleChecked=false;
#endif
    for(;;){
        xrtMutexLock(&W->Gate);xmqttjwitem* J=W->Head;
        if(!J){
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
            if(!IdleChecked&&W->Auto&&!W->Info.Requests&&!W->Info.Closed&&
               W->Info.Journal.CommittedEnd>=W->Auto->Config.TriggerBytes&&
               xmqttJWAutoDue(W->Auto,W->Info.Journal.CommittedEnd)){
                IdleChecked=true;
                xrtMutexUnlock(&W->Gate);
                (void)xmqttJWAutoCompact(W,NULL,Cancel,NULL);
                continue;
            }
#endif
            W->Running=false;W->Info.Busy=false;xrtMutexUnlock(&W->Gate);break;
        }
        W->Head=J->Next;if(!W->Head)W->Tail=NULL;--W->Info.Queued;W->Info.Busy=true;
        J->Result.Status=xmqttJWStop(W,J,Cancel);xmqttjournalinfo Before=W->Info.Journal;xrtMutexUnlock(&W->Gate);
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
        if(!J->Result.Data.Data){
            if(J->Result.Status==XMQTT_COMMIT_OK){
                xmqttjwcompact* C=(xmqttjwcompact*)(J+1);
                xmqttJWCompact(W,J,Cancel,&C->Config,&C->Result,Before,NULL);
                J->Result.Status=C->Result.Status;
                J->Result.Effect=C->Result.Effect;
                J->Result.Cause=C->Result.Cause;
                if(C->Result.Effect==XMQTT_JOURNAL_COMMITTED){
                    xrtMutexLock(&W->Gate);
                    if(W->Auto){
                        W->Auto->DeniedEnd=W->Auto->Info.NextEligibleBytes=0;
                        W->Auto->Info.RetryAfter=0;
                    }
                    xrtMutexUnlock(&W->Gate);
                    IdleChecked=true;
                }
            }
            xmqttJWFinish(W,J);
            continue;
        }
        xmqttcompactionrequest Request={0};
#endif
        xmqttJWCommitOne(W,J,Cancel,Before XMQTT_SW_PRESSURE_ARG(W->Auto?&Request:NULL));
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
        /* Both limits precede the write. The typed plan is already disposed,
         * and the original request, deadline and ACK reservation stay live. */
        if(W->Auto&&J->Result.Status==XMQTT_COMMIT_LIMIT&&
           (Request.ReplayBlocked||(J->Result.AppendStatus==XMQTT_JOURNAL_LIMIT&&
            xmqttInternalJournalCompactFilePressure(W->Journal,J->Result.Data.Size)))){
            IdleChecked=true;
            bool Rotated=xmqttJWAutoCompact(W,J,Cancel,&Request);
            xrtMutexLock(&W->Gate);
            if(W->Info.Failed)J->Result.Status=XMQTT_COMMIT_STORAGE;
            else if(Rotated)J->Result.Status=xmqttJWStop(W,J,Cancel);
            Before=W->Info.Journal;
            xrtMutexUnlock(&W->Gate);
            if(Rotated)xmqttJWCommitOne(W,J,Cancel,Before XMQTT_SW_PRESSURE_ARG(NULL));
        }
#endif
        xmqttJWFinish(W,J);
    }
    xmqttJWCurrent=Scope.Previous;return XTASK_SUCCESS;
}
static void xmqttJWPumpDrop(ptr Data,ptr Context)
{
    (void)Context;xmqttjwpump* T=Data;xmqttjournalworker* W=T->Owner;xmqttjwscope Scope={xmqttJWCurrent,W};xmqttJWCurrent=&Scope;
    xrtMutexLock(&W->Gate);while(W->Info.Scheduling)xrtCondWait(&W->Ready,&W->Gate);
    if(!T->Ran){W->Info.Failed=W->Info.Closed=true;
        for(;;){xmqttjwitem* J=W->Head;if(!J)break;W->Head=J->Next;--W->Info.Queued;J->Result.Status=XMQTT_COMMIT_INITIALIZATION;
            xrtMutexUnlock(&W->Gate);xmqttJWFinish(W,J);xrtMutexLock(&W->Gate);}
        W->Tail=NULL;W->Running=false;W->Info.Busy=false;}
    T->Dropped=true;xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);xmqttJWCurrent=Scope.Previous;
}
XMQTT_API bool xmqttJournalWorkerConfigInit(xmqttjournalworkerconfig* C)
{
    if(!xmqttJWSpan(C,sizeof(*C)))return xmqttJWError(XERR_ARGUMENT,"invalid journal worker config output");
    *C=(xmqttjournalworkerconfig){sizeof(*C),128,256,4194304,2,1048576,32};return true;
}
XMQTT_API bool xmqttJournalWorkerCreate(xtaskpool* Pool,xmqttjournal* Journal,const xmqttjournalworkerconfig* C,xmqttjournalworker** Out)
{
    if(!Pool||!Journal||!xmqttJWSpan(C,sizeof(*C))||!xmqttJWSpan(Out,sizeof(*Out))||!xmqttJWSeparate(Out,sizeof(*Out),C,sizeof(*C))||C->Size!=sizeof(*C)||
       C->MaxObjects>UINT32_MAX/2u||C->MaxTasks>UINT32_MAX/2u||C->MaxBytes<sizeof(xmqttjournalworker))return xmqttJWError(XERR_ARGUMENT,"invalid journal worker inputs/limits");
    xtaskpoolstats Stats;if(!xrtTaskPoolGet(Pool,&Stats))return false;if(Stats.Closed)return xmqttJWError(XERR_STATE,"host storage pool is closed");
    xmqttjournalworker* W=xrtCalloc(1,sizeof(*W));if(!W)return false;bool Gate=xrtMutexInit(&W->Gate),Ready=false;
    if(Gate)Ready=xrtCondInit(&W->Ready);
    if(!Ready){if(Gate)xrtMutexUnit(&W->Gate);xrtFree(W);return false;}
    W->Config=*C;W->Pool=Pool;W->Journal=Journal;W->Info.Size=sizeof(W->Info);W->Info.BaseBytes=W->Info.Bytes=sizeof(*W);W->Low=(uintptr_t)W;W->High=W->Low+sizeof(*W);xrtAtomic32Init(&W->Refs,1);
    if(!xmqttInternalJournalWorkerClaim(Journal,W,Out,sizeof(*Out),&W->Info.Journal)){xmqttJWDrop(W);return false;}
    *Out=W;return true;
}
static xmqttjournaladmission xmqttJWAppendAt(xmqttjournalworker* W,xbytesview Data,double Deadline,xfuture** Out
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    ,uint64 InboxKey
#endif
    XMQTT_JW_COMPACT_PARAM
)
{
    if(!W||isnan(Deadline)||!xmqttJWSpan(Data.Data,Data.Size)||!xmqttJWSpan(Out,sizeof(*Out))||!xmqttJWSeparate(Out,sizeof(*Out),Data.Data,Data.Size)){
        xmqttJWError(XERR_ARGUMENT,"invalid append admission span/deadline");return XMQTT_JWORK_ERROR;}
    xmqttjwscope Scope={xmqttJWCurrent,W};xmqttJWCurrent=&Scope;xmqttJWReap(W);xmqttjournaladmission A=XMQTT_JWORK_ERROR;
    size_t N=0;xmqttjwitem* J=NULL;xmqttjwpump* T=NULL;xfuture* F=NULL;bool Reserved=false,PumpReserved=false,Linked=false;
    xrtMutexLock(&W->Gate);
    if(!xmqttJWOutput(W,Out,sizeof(*Out))||!xmqttJWSeparate(Data.Data,Data.Size,W,sizeof(*W))){xrtMutexUnlock(&W->Gate);xmqttJWError(XERR_ARGUMENT,"append output overlaps owned state/result");goto End;}
    if(W->Info.Closed){A=XMQTT_JWORK_CLOSED;goto Refuse;}
    if(Data.Size>W->Config.MaxTransactionBytes||Data.Size>SIZE_MAX-sizeof(*J)||W->Info.Requests>=W->Config.MaxRequests||W->Info.Objects>=W->Config.MaxObjects||W->Info.Scheduling||!W->Config.WorkTransactions||(W->Running&&W->TurnAccepted>=W->Config.WorkTransactions))goto Overload;
    N=sizeof(*J)+Data.Size;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    if(Compact)N=sizeof(*J)+sizeof(xmqttjwcompact);
#endif
    if(N>W->Config.MaxBytes-W->Info.Bytes)goto Overload;
    ++W->Info.Requests;++W->Info.Objects;W->Info.Bytes+=N;xmqttJWRef(W);Reserved=true;xrtMutexUnlock(&W->Gate);
    J=xrtCalloc(1,N);if(!J)goto End;J->Owner=W;J->Allocation=N;J->Deadline=Deadline;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    if(Compact)((xmqttjwcompact*)(J+1))->Config=*Compact;
    else
#endif
    {
        /* Even an empty opaque append has a non-NULL owned data marker. */
        J->Result.Data=(xbytesview){(const unsigned char*)(J+1),Data.Size};
        if(Data.Size)memcpy(J+1,Data.Data,Data.Size);
    }
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    J->InboxKey=InboxKey;
#endif
    J->Promise=xrtPromiseCreate(&F,NULL);if(!J->Promise)goto End;J->Cancel=xrtPromiseCancelToken(J->Promise);if(!J->Cancel)goto End;
    xrtMutexLock(&W->Gate);
    if(W->Info.Closed){A=XMQTT_JWORK_CLOSED;goto Refuse;}
    if(W->Info.Scheduling||(W->Running&&W->TurnAccepted>=W->Config.WorkTransactions))goto Overload;
    if(!W->Running){
        if(W->Info.Tasks>=W->Config.MaxTasks||sizeof(*T)>W->Config.MaxBytes-W->Info.Bytes)goto Overload;
        ++W->Info.Tasks;W->Info.Bytes+=sizeof(*T);xmqttJWRef(W);PumpReserved=true;W->TurnAccepted=0;W->Info.Scheduling=W->Running=true;xrtMutexUnlock(&W->Gate);
        T=xrtCalloc(1,sizeof(*T));if(!T)goto End;T->Owner=W;xrtMutexLock(&W->Gate);
        if(W->Info.Closed){A=XMQTT_JWORK_CLOSED;goto Refuse;}
    }
    J->OwnedNext=W->Owned;if(W->Owned)W->Owned->OwnedPrevious=J;W->Owned=J;xmqttJWRange(W,J,N);Linked=true;
    if(W->Tail)W->Tail->Next=J;else W->Head=J;W->Tail=J;++W->Info.Queued;
    if(!PumpReserved){++W->TurnAccepted;xmqttJWCount(&W->Info.Accepted);xrtMutexUnlock(&W->Gate);A=XMQTT_JWORK_ACCEPTED;goto End;}
    T->Next=W->Pumps;W->Pumps=T;xmqttJWRange(W,T,sizeof(*T));xrtMutexUnlock(&W->Gate);
    xtaskargs Args={0};Args.Destroy=xmqttJWPumpDrop;xfuture* Native=xrtTaskSubmit(W->Pool,xmqttJWPump,T,&Args);
    xrtMutexLock(&W->Gate);
    if(Native){T->Future=Native;++W->TurnAccepted;W->Info.Scheduling=false;xmqttJWCount(&W->Info.Accepted);xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);A=XMQTT_JWORK_ACCEPTED;goto End;}
    /* Other submissions refuse while Scheduling; no accepted sibling can be
     * stranded by native rejection. Proc/Drop cannot run on rejection. */
    W->Head=W->Tail=NULL;--W->Info.Queued;W->Pumps=T->Next;W->Owned=J->OwnedNext;if(W->Owned)W->Owned->OwnedPrevious=NULL;Linked=false;xmqttJWCount(&W->Info.Rejected);
    xrtMutexUnlock(&W->Gate);goto End;
Overload:A=XMQTT_JWORK_OVERLOADED;
Refuse:xmqttJWCount(&W->Info.Rejected);xrtMutexUnlock(&W->Gate);
End:
    if(A==XMQTT_JWORK_ACCEPTED){*Out=F;xmqttJWCurrent=Scope.Previous;return A;}
    if(Linked)abort();
    if(PumpReserved){xrtFree(T);xrtMutexLock(&W->Gate);--W->Info.Tasks;W->Info.Bytes-=sizeof(*T);W->Info.Scheduling=W->Running=false;xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);xmqttJWDrop(W);}
    if(J){xrtCancelDestroy(J->Cancel);xrtPromiseDestroy(J->Promise);xrtSecureZero(J,N);xrtFree(J);}xrtFutureDestroy(F);
    if(Reserved)xmqttJWUncharge(W,N,true);
    xmqttJWCurrent=Scope.Previous;return A;
}
XMQTT_API xmqttjournaladmission xmqttJournalWorkerAppendAsync(xmqttjournalworker* W,xbytesview Data,int64 Timeout,xfuture** Out)
{
    double Deadline=Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0;
    if(Timeout<XRT_WAIT_FOREVER){xmqttJWError(XERR_ARGUMENT,"invalid append timeout");return XMQTT_JWORK_ERROR;}
    return xmqttJWAppendAt(W,Data,Deadline,Out
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
        ,0
#endif
        XMQTT_JW_COMPACT_ARG(NULL)
    );
}
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalJournalWorkerAppendAt(xmqttjournalworker* W,xbytesview Data,double Deadline,xfuture** Out XMQTT_SW_INBOX_PARAM)
{return xmqttJWAppendAt(W,Data,Deadline,Out XMQTT_SW_INBOX_ARG(InboxKey) XMQTT_JW_COMPACT_ARG(NULL));}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalJournalCompactSubmit(xmqttjournalworker* W,
    const xmqttstorecompactconfig* Config,double Deadline,xfuture** Out)
{return xmqttJWAppendAt(W,(xbytesview){0},Deadline,Out XMQTT_SW_INBOX_ARG(0) XMQTT_JW_COMPACT_ARG(Config));}
XMQTT_INTERNAL bool xmqttInternalJournalAutoCompactSet(xmqttjournalworker* W,
    xmqttautocompactstate* State,const xmqttstoreautocompactconfig* Config)
{
    xrtMutexLock(&W->Gate);
    bool OK=!W->Running&&!W->Info.Requests&&!W->Info.Scheduling&&!W->Info.Closed&&
        (!Config||xmqttInternalJournalCompactAvailable(W->Journal,W));
    if(OK){
        if(Config){
            *State=(xmqttautocompactstate){.Config=*Config,.Info={.Size=sizeof(State->Info),.Enabled=true}};
            W->Auto=State;
        }else W->Auto=NULL;
    }
    xrtMutexUnlock(&W->Gate);
    return OK?true:xmqttJWError(XERR_STATE,"auto compaction requires an idle open slot Writer");
}
XMQTT_INTERNAL bool xmqttInternalJournalAutoCompactInfo(xmqttjournalworker* W,xmqttstoreautocompactinfo* Info)
{
    xrtMutexLock(&W->Gate);
    bool OK=xmqttJWOutput(W,Info,sizeof(*Info));
    if(OK)*Info=W->Auto?W->Auto->Info:(xmqttstoreautocompactinfo){.Size=sizeof(*Info)};
    xrtMutexUnlock(&W->Gate);
    return OK?true:xmqttJWError(XERR_ARGUMENT,"invalid auto compaction output");
}
#endif
XMQTT_API bool xmqttJournalWorkerInfo(xmqttjournalworker* W,xmqttjournalworkerinfo* I)
{
    if(!W)return xmqttJWError(XERR_ARGUMENT,"NULL journal worker");
    xmqttjwscope Scope={xmqttJWCurrent,W};xmqttJWCurrent=&Scope;xmqttJWReap(W);xrtMutexLock(&W->Gate);
    bool OK=xmqttJWOutput(W,I,sizeof(*I));if(OK)*I=W->Info;xrtMutexUnlock(&W->Gate);
    if(!OK)xmqttJWError(XERR_ARGUMENT,"invalid/overlapping worker info output");
    xmqttJWCurrent=Scope.Previous;return OK;
}
XMQTT_API bool xmqttJournalWorkerClose(xmqttjournalworker* W)
{if(!W)return xmqttJWError(XERR_ARGUMENT,"NULL journal worker");xrtMutexLock(&W->Gate);W->Info.Closed=true;xrtCondBroadcast(&W->Ready);xrtMutexUnlock(&W->Gate);return true;}
XMQTT_API xwaitresult xmqttJournalWorkerWaitFor(xmqttjournalworker* W,int64 Timeout)
{
    if(!W||Timeout<XRT_WAIT_FOREVER){xmqttJWError(XERR_ARGUMENT,"invalid worker wait");return XWAIT_ERROR;}
    if(xmqttJWInside(W)||xrtTaskPoolIsCurrentWorkerV1(W->Pool)){xmqttJWError(XERR_STATE,"cannot wait from own pool/callback/admission");return XWAIT_ERROR;}
    double Deadline=Timeout<0?INFINITY:xrtTimer()+(double)Timeout/1000.0;
    for(;;){xmqttJWReap(W);xrtMutexLock(&W->Gate);
        if(!W->Info.Closed){xrtMutexUnlock(&W->Gate);xmqttJWError(XERR_STATE,"Close required before physical wait");return XWAIT_ERROR;}
        if(!W->Info.Requests&&!W->Info.Tasks&&!W->Info.Scheduling){xrtMutexUnlock(&W->Gate);return XWAIT_OK;}
        double Left=ceil((Deadline-xrtTimer())*1000.0);if(Left<=0){xrtMutexUnlock(&W->Gate);return XWAIT_TIMEOUT;}
        /* Sleep during native IO/reservations. Drop signals before the Core
         * Future terminal edge: only that final tail needs bounded1ms polling. */
        bool Tail=false;for(xmqttjwpump* T=W->Pumps;T;T=T->Next)if(T->Dropped){Tail=true;break;}
        int64 Delay=isinf(Left)?XRT_WAIT_FOREVER:Left>=(double)INT64_MAX?INT64_MAX:(int64)Left;if(Tail&&(Delay<0||Delay>1))Delay=1;
        xwaitresult R=xrtCondWaitFor(&W->Ready,&W->Gate,Delay);xrtMutexUnlock(&W->Gate);if(R==XWAIT_ERROR)return R;
    }
}
XMQTT_API bool xmqttJournalWorkerDestroy(xmqttjournalworker* W)
{
    if(!W)return true;
    if(xmqttJWInside(W)||xrtTaskPoolIsCurrentWorkerV1(W->Pool))return xmqttJWError(XERR_STATE,"cannot destroy from own pool/callback/admission");
    if(!xmqttJournalWorkerClose(W)||xmqttJournalWorkerWaitFor(W,XRT_WAIT_FOREVER)!=XWAIT_OK)return false;
    if(!xmqttInternalJournalWorkerDestroy(W->Journal,W))return false;
    W->Journal=NULL;W->Pool=NULL;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    W->Auto=NULL;
#endif
    xmqttJWDrop(W);return true;
}

#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
XMQTT_INTERNAL bool xmqttInternalJournalWorkerBind(xmqttjournalworker* W,xmqttstoreprepare Prepare,xmqttstorefinish Finish,ptr Context XMQTT_SW_SETTLE_PARAM)
{
    if(!W||!Prepare||!Finish||!Context)return xmqttJWError(XERR_ARGUMENT,"invalid private semantic owner hooks");
    xrtMutexLock(&W->Gate);bool OK=!W->Prepare&&!W->Info.Accepted&&!W->Info.Closed&&!W->Info.Requests&&!W->Info.Tasks;
    if(OK){
        W->Prepare=Prepare;W->Finish=Finish;W->Store=Context;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
        W->Settle=Settle;
#endif
    }xrtMutexUnlock(&W->Gate);return OK;
}
#endif
#endif
