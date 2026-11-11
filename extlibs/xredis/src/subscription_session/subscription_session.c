#include <xredis/subscription_session.h>
#include "../internal/subscription_replay.h"
#include <xrt/memory.h>
#include <math.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
typedef struct xredissessionwatch {
    xfuturewatch Watch;xfuture* Future;xredissubscriptionsession* Session;bool Active;
} xredissessionwatch;
struct xredissubscriptionsession {
    xatomic32 References;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetpost Work;
    xredissubscriptions* Store;xredissubscriptionsessionconfig Config;xredissubscriptionsessioninfo Info;
    xredisclient *Client,*Published;xredissubscriptionreplay* Replay;xredissubscriptionsnapshot* Applied;
    xredissessionwatch Ready,Closed,Control;xpromise* Promise;xfuture* Future;
    xredissubscriptionsessionclosed* Result;
    xredisrequeststatus StopStatus;size_t APIs;bool Queued,Closing,Finished,Notify,Draining,ReadyOK;
    uint32 Consecutive;uint64 Timer,Random;double Due,TimerDue,Entered,WakeDue;
};
static void xredisSessionWork(xnetworker*,ptr);
static bool xredisSessionReport(xerrkind Kind,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.redis",Kind==XERR_STATE?XREDIS_CLIENT_STATE:XREDIS_CLIENT_ARGUMENT,Text);return false;}
static void xredisSessionRef(xredissubscriptionsession* S){xrtAtomic32FetchAdd(&S->References,1,XMEMORY_RELAXED);}
static void xredisSessionIncrement(uint64* N){if(*N!=UINT64_MAX)++*N;}
static double xredisSessionDeadline(double Now,int64 Timeout)
{return nextafter(Now+(double)Timeout*.001,INFINITY);}
XREDIS_API bool xredisSubscriptionSessionRetain(xredissubscriptionsession* S)
{
    if(!S)return xredisSessionReport(XERR_ARGUMENT,"session is NULL");
    uint32 N=xrtAtomic32Load(&S->References,XMEMORY_RELAXED);
    for(;;){if(N>=UINT32_MAX-32)return xredisSessionReport(XERR_RANGE,"session reference overflow");if(xrtAtomic32CompareExchange(&S->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisSubscriptionSessionRelease(xredissubscriptionsession* S)
{
    if(!S || xrtAtomic32FetchSub(&S->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtPromiseDestroy(S->Promise);xrtFutureDestroy(S->Future);xrtFree(S->Result);xrtMutexUnit(&S->Gate);xrtFree(S);
}
/* Gate protects worker borrowing and post admission; no allocation. */
static void xredisSessionWakeLocked(xredissubscriptionsession* S)
{
    if(S->Finished || S->Queued)return;
    S->Queued=true;xredisSessionRef(S);
    if(!xrtNetPost(S->Worker,&S->Work,xredisSessionWork,S)){S->Queued=false;xredisSubscriptionSessionRelease(S);}
}
static void xredisSessionWake(xredissubscriptionsession* S)
{xrtMutexLock(&S->Gate);xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);}
/* Release follows Core's final access to Watch storage. Only this callback
 * publishes reuse; logical Future completion alone is not mechanical retirement. */
static void xredisSessionWatchNotify(ptr Data){(void)Data;}
static void xredisSessionWatchDrop(ptr Data)
{
    xredissessionwatch* W=Data;xredissubscriptionsession* S=W->Session;
    xrtMutexLock(&S->Gate);W->Active=false;xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);
    xredisSubscriptionSessionRelease(S); /* No further Watch/Engine access. */
}
static bool xredisSessionWatchActive(xredissubscriptionsession* S,xredissessionwatch* W)
{xrtMutexLock(&S->Gate);bool Active=W->Active;xrtMutexUnlock(&S->Gate);return Active;}
static bool xredisSessionWatchStop(xredissubscriptionsession* S,xredissessionwatch* W)
{
    if(!W->Future)return true;
    if(xredisSessionWatchActive(S,W))(void)xrtFutureWatchDetach(W->Future,&W->Watch);
    if(xredisSessionWatchActive(S,W))return false;
    xrtFutureDestroy(W->Future);W->Future=NULL;return true;
}
/* Takes the supplied observer reference even if registration fails. */
static bool xredisSessionWatchStart(xredissubscriptionsession* S,xredissessionwatch* W,xfuture* F)
{
    W->Future=F;W->Session=S;if(!F || !xrtFutureWatchInit(&W->Watch,xredisSessionWatchNotify,xredisSessionWatchDrop,W))return false;
    xredisSessionRef(S);xrtMutexLock(&S->Gate);W->Active=true;xrtMutexUnlock(&S->Gate);
    xfuturewatchresult A=xrtFutureWatchAdd(F,&W->Watch);
    if(A!=XFUTURE_WATCH_PENDING){xrtMutexLock(&S->Gate);W->Active=false;xrtMutexUnlock(&S->Gate);xredisSubscriptionSessionRelease(S);}
    return A!=XFUTURE_WATCH_ERROR;
}
static void xredisSessionState(xredissubscriptionsession* S,xredissubscriptionsessionstate State)
{xrtMutexLock(&S->Gate);if(!S->Closing && S->Info.State!=State){S->Info.State=State;S->Notify=true;}xrtMutexUnlock(&S->Gate);}
static bool xredisSessionClosing(xredissubscriptionsession* S)
{xrtMutexLock(&S->Gate);bool Closing=S->Closing;xrtMutexUnlock(&S->Gate);return Closing;}
static void xredisSessionNotify(xredissubscriptionsession* S)
{
    xrtMutexLock(&S->Gate);bool Needed=S->Notify;S->Notify=false;
    xredissubscriptionsessionnotify Notify=Needed?S->Config.Notify:NULL;ptr Data=S->Config.Data;xredissubscriptionsessioninfo Info=S->Info;xrtMutexUnlock(&S->Gate);
    if(Notify)Notify(S,&Info,Data);
}
static void xredisSessionTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xredissubscriptionsession* S=Data;if(S->Timer==Id)S->Timer=0;
    if(Result!=XNET_RESULT_OK && Result!=XNET_RESULT_CANCELLED)(void)xredisSubscriptionSessionClose(S);
    xredisSessionWake(S);xredisSubscriptionSessionRelease(S);
}
static bool xredisSessionArm(xredissubscriptionsession* S,double Due)
{
    if(S->Timer && S->TimerDue==Due)return true;
    if(S->Timer)(void)xrtNetEngineTimerCancelCurrent(S->Engine,S->Timer);
    if(S->Timer)return false;
    if(Due==INFINITY)return true;
    S->TimerDue=Due;xredisSessionRef(S);
    S->Timer=__xrtNetEngineSchedule(S->Engine,xrtNetWorkerIndex(S->Worker),Due,xredisSessionTimer,S);
    if(S->Timer)return true;
    xredisSubscriptionSessionRelease(S);return false;
}
static void xredisSessionFailure(xredissubscriptionsession* S,xredisrequeststatus Status,bool Fatal)
{
    xrtMutexLock(&S->Gate);
    if(!S->Closing){xredisSessionIncrement(&S->Info.Failures);S->Info.LastFailure=Status;S->Notify=true;S->Published=NULL;
        if(S->Consecutive!=UINT32_MAX)++S->Consecutive;
        if(Fatal || (S->Config.MaxFailures && S->Info.Failures && S->Consecutive>=S->Config.MaxFailures)){S->Closing=true;S->StopStatus=Status;S->Info.State=XREDIS_SESSION_CLOSING;}else S->Info.State=XREDIS_SESSION_DRAINING;}
    xrtMutexUnlock(&S->Gate);
    S->Draining=true;xredisSessionState(S,XREDIS_SESSION_DRAINING);if(S->Client)(void)xredisClientClose(S->Client);xredisSessionWake(S);
}
static void xredisSessionBackoff(xredissubscriptionsession* S)
{
    uint64 Cap=(uint64)S->Config.RetryMin<<(S->Consecutive<31?S->Consecutive:31);if(Cap>(uint64)S->Config.RetryMax)Cap=(uint64)S->Config.RetryMax;
    uint64 R=S->Random;R^=R<<13;R^=R>>7;R^=R<<17;S->Random=R;
    int64 Delay=S->Config.RetryMin+(int64)(R%(Cap-(uint64)S->Config.RetryMin+1));S->Due=xredisSessionDeadline(xrtTimer(),Delay);
    xrtMutexLock(&S->Gate);S->Info.NextAttempt=S->Due;xrtMutexUnlock(&S->Gate);xredisSessionState(S,XREDIS_SESSION_BACKOFF);
}
static xredisrequeststatus xredisSessionReadyStatus(xredisreadystatus Status)
{
    switch(Status){case XREDIS_READY_TIMEOUT:return XREDIS_REQUEST_TIMEOUT;case XREDIS_READY_DISCONNECTED:return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_READY_CLOSED:return XREDIS_REQUEST_CLOSED;case XREDIS_READY_PROTOCOL:return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_READY_LIMIT:return XREDIS_REQUEST_LIMIT;case XREDIS_READY_MEMORY:return XREDIS_REQUEST_MEMORY;default:return XREDIS_REQUEST_INITIALIZATION;}
}
static void xredisSessionResultDrop(ptr Value,ptr Data){(void)Data;xrtFree(Value);}
static void xredisSessionFinish(xredissubscriptionsession* S)
{
    xrtMutexLock(&S->Gate);
    if(S->Queued || S->APIs || S->Client || S->Timer || S->Ready.Active || S->Closed.Active || S->Control.Active){xrtMutexUnlock(&S->Gate);return;}
    S->Finished=true;S->Info.State=XREDIS_SESSION_CLOSED;S->Info.NextAttempt=0;S->Notify=true;
    xnetengine* Engine=S->Engine;xredissubscriptions* Store=S->Store;S->Engine=NULL;S->Worker=NULL;S->Store=NULL;
    xredissubscriptionsessiondrop Drop=S->Config.Drop;ptr Data=S->Config.Data;xrtMutexUnlock(&S->Gate);
    __xredisSubscriptionsUnclaim(Store,S);xredisSubscriptionsRelease(Store);xredisSessionNotify(S);
    S->Config.Notify=NULL;S->Config.Connect=NULL;S->Config.Drop=NULL;S->Config.Data=NULL;if(Drop)Drop(Data);
    (void)xrtNetEngineUnpin(Engine);xpromise* P=S->Promise;S->Promise=NULL;xredissubscriptionsessionclosed* R=S->Result;S->Result=NULL;
    *R=(xredissubscriptionsessionclosed){S->StopStatus,S->Info.Attempts,S->Info.Generations};
    if(!xrtPromiseResolveOwned(P,R,xredisSessionResultDrop,NULL))xrtFree(R);
    xrtPromiseDestroy(P);xredisSubscriptionSessionRelease(S); /* Runtime. */
}
static bool xredisSessionDrain(xredissubscriptionsession* S)
{
    bool Ready=xredisSessionWatchStop(S,&S->Ready),Control=xredisSessionWatchStop(S,&S->Control);
    if(S->Replay){xredisSubscriptionReplayDestroy(S->Replay);S->Replay=NULL;}
    if(S->Client){
        (void)xredisClientClose(S->Client);
        if(!S->Closed.Future){if(!xredisSessionWatchStart(S,&S->Closed,xredisClientClosed(S->Client)))return false;}
        if(!xrtFutureDone(S->Closed.Future) || !Ready || !Control)return false;
        if(!xredisSessionWatchStop(S,&S->Closed))return false;
        xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);
        if(I.Requests || I.InitBytes){S->WakeDue=xredisSessionDeadline(xrtTimer(),1);return false;}
        __xredisSubscriberReplayRelease(S->Client,S);xredisClientRelease(S->Client);S->Client=NULL;
    }
    if(!Ready || !Control)return false;
    xredisSubscriptionSnapshotRelease(S->Applied);S->Applied=NULL;S->Draining=false;
    if(!xredisSessionClosing(S))xredisSessionBackoff(S);
    return true;
}
static void xredisSessionStart(xredissubscriptionsession* S)
{
    S->ReadyOK=false;S->Entered=xrtTimer();S->Due=xredisSessionDeadline(S->Entered,S->Config.AttemptTimeout);
    xrtMutexLock(&S->Gate);xredisSessionIncrement(&S->Info.Attempts);uint64 Attempt=S->Info.Attempts;S->Info.NextAttempt=0;xrtMutexUnlock(&S->Gate);
    xredisSessionState(S,XREDIS_SESSION_CONNECTING);xredisclient* C=NULL;
    bool OK=S->Config.Connect(S->Engine,Attempt,S->Config.AttemptTimeout,S->Config.Data,&C);
    if(!OK || !C){xredisSessionFailure(S,XREDIS_REQUEST_INITIALIZATION,false);return;}
    S->Client=C;
    if(!__xredisSubscriptionClientProfile(C,S->Engine)){xredisSessionFailure(S,XREDIS_REQUEST_INITIALIZATION,true);return;}
    bool A=xredisSessionWatchStart(S,&S->Closed,xredisClientClosed(C));bool B=xredisSessionWatchStart(S,&S->Ready,xredisClientReady(C));
    if(!A || !B)xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);
    else if(xrtFutureDone(S->Ready.Future))xredisSessionWake(S);
}
static void xredisSessionRestore(xredissubscriptionsession* S)
{
    if((S->Replay || !S->Applied) && xrtTimer()>=S->Due){xredisSessionFailure(S,XREDIS_REQUEST_TIMEOUT,false);return;}
    if(!S->Replay){
        if(!S->Applied){
            if(xrtTimer()>=S->Due){xredisSessionFailure(S,XREDIS_REQUEST_TIMEOUT,false);return;}
            if(!S->ReadyOK){
            if(!S->Ready.Future || !xrtFutureDone(S->Ready.Future))return;
            const xredisreadyresult* Ready=xrtFutureValue(S->Ready.Future);
            if(!Ready || Ready->Status!=XREDIS_READY_OK || Ready->Protocol!=S->Config.Protocol){xredisrequeststatus Why=Ready?xredisSessionReadyStatus(Ready->Status):XREDIS_REQUEST_INITIALIZATION;
                xredisSessionFailure(S,Why,Why!=XREDIS_REQUEST_TIMEOUT && Why!=XREDIS_REQUEST_DISCONNECTED && Why!=XREDIS_REQUEST_CLOSED);return;}
            if(!xredisSessionWatchStop(S,&S->Ready))return;
            S->ReadyOK=true;}
            xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);
            if(I.Requests || I.InitBytes){S->WakeDue=xredisSessionDeadline(xrtTimer(),1);return;}
            if(__xredisSubscriberReplayStart(S->Client,&S->Config.Replay.Registry,S)!=XREDIS_ADMISSION_ACCEPTED){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
            xredisSessionState(S,XREDIS_SESSION_RESTORING);
        }else{S->Entered=xrtTimer();S->Due=xredisSessionDeadline(S->Entered,S->Config.AttemptTimeout);xredisSessionState(S,XREDIS_SESSION_UPDATING);}
        xredissubscriptionreplayconfig K=S->Config.Replay;K.Timeout=S->Config.AttemptTimeout;
        if(__xredisSubscriptionTransitionCreate(S->Store,S->Client,S->Applied,&K,S->Entered,S,&S->Replay)!=XREDIS_ADMISSION_ACCEPTED){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
    }
    if(S->Control.Future){if(!xrtFutureDone(S->Control.Future))return;if(!xredisSessionWatchStop(S,&S->Control))return;}
    xredissubscriptionstep Step=xredisSubscriptionReplayStep(S->Replay);
    if(Step==XREDIS_SUBSCRIPTIONS_WAIT){xfuture* F=xredisSubscriptionReplayPending(S->Replay);if(F){if(!xredisSessionWatchStart(S,&S->Control,F))xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);else if(xrtFutureDone(F))xredisSessionWake(S);}else xredisSessionWake(S);return;}
    if(Step==XREDIS_SUBSCRIPTIONS_MORE){xredisSessionWake(S);return;}
    xredissubscriptionreplayresult R={0};R.Size=sizeof(R);
    if(Step==XREDIS_SUBSCRIPTIONS_STEP_ERROR || !xredisSubscriptionReplayResult(S->Replay,&R)){xredisSessionFailure(S,XREDIS_REQUEST_PROTOCOL,true);return;}
    if(R.Status!=XREDIS_SUBSCRIPTIONS_OK){xredisSessionFailure(S,R.NativeStatus,R.NativeStatus!=XREDIS_REQUEST_TIMEOUT && R.NativeStatus!=XREDIS_REQUEST_DISCONNECTED && R.NativeStatus!=XREDIS_REQUEST_CLOSED);return;}
    xredissubscriptionsnapshot* Applied=__xredisSubscriptionTransitionSnapshot(S->Replay);if(!Applied){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);return;}
    bool Fresh=S->Applied==NULL;xredisSubscriptionSnapshotRelease(S->Applied);S->Applied=Applied;xredisSubscriptionReplayDestroy(S->Replay);S->Replay=NULL;S->Consecutive=0;
    xrtMutexLock(&S->Gate);if(Fresh)xredisSessionIncrement(&S->Info.Generations);xredisSessionIncrement(&S->Info.Restored);S->Info.AppliedRevision=R.Revision;S->Published=S->Closing?NULL:S->Client;
    if(!S->Closing){S->Info.State=S->Info.DesiredRevision==R.Revision?XREDIS_SESSION_ONLINE:XREDIS_SESSION_UPDATING;S->Notify=true;}xrtMutexUnlock(&S->Gate);xredisSessionWake(S);
}
static void xredisSessionWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xredissubscriptionsession* S=Data;S->WakeDue=INFINITY;xrtMutexLock(&S->Gate);S->Queued=false;bool Closing=S->Closing;xrtMutexUnlock(&S->Gate);
    if(Closing || S->Draining){
        if(S->Timer)(void)xrtNetEngineTimerCancelCurrent(S->Engine,S->Timer);
        S->Draining=true;(void)xredisSessionDrain(S);
        if(xredisSessionClosing(S) && !S->Client && !S->Draining)xredisSessionFinish(S);
    }else if(!S->Client){if(xrtTimer()>=S->Due)xredisSessionStart(S);}
    else if(xrtFutureDone(S->Closed.Future)){xredisclientinfo I={0};I.Size=sizeof(I);(void)xredisClientInfo(S->Client,&I);xredisSessionFailure(S,I.CloseStatus==XREDIS_REQUEST_OK?XREDIS_REQUEST_DISCONNECTED:I.CloseStatus,false);}
    else{
        xredissubscriptionsinfo I={0};I.Size=sizeof(I);(void)xredisSubscriptionsInfo(S->Store,&I);
        xrtMutexLock(&S->Gate);if(S->Info.DesiredRevision<I.Revision)S->Info.DesiredRevision=I.Revision;bool Needed=!S->Applied || S->Replay || S->Info.AppliedRevision!=S->Info.DesiredRevision;if(!Needed && !S->Closing && S->Info.State!=XREDIS_SESSION_ONLINE){S->Info.State=XREDIS_SESSION_ONLINE;S->Notify=true;}xrtMutexUnlock(&S->Gate);
        if(Needed)xredisSessionRestore(S);
    }
    if(!S->Finished){
        double Due=INFINITY;
        if(S->Draining || xredisSessionClosing(S)){
            xrtMutexLock(&S->Gate);bool Watching=S->Ready.Active || S->Closed.Active || S->Control.Active;xrtMutexUnlock(&S->Gate);
            if(S->Draining && !Watching && (!S->Client || (S->Closed.Future && xrtFutureDone(S->Closed.Future) && S->WakeDue==INFINITY)))xredisSessionWake(S);
        }else Due=(!S->Client || S->Replay || !S->Applied)?S->Due:INFINITY;
        if(S->WakeDue<Due)Due=S->WakeDue;
        if(!xredisSessionArm(S,Due)){xredisSessionFailure(S,XREDIS_REQUEST_MEMORY,true);xredisSessionWake(S);}
        xredisSessionNotify(S);
    }
    xredisSubscriptionSessionRelease(S); /* Post. */
}
XREDIS_API bool xredisSubscriptionSessionConfigInit(xredissubscriptionsessionconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xredisSessionReport(XERR_ARGUMENT,"invalid session config");
    *K=(xredissubscriptionsessionconfig){0};K->Size=sizeof(*K);K->Protocol=XREDIS_RESP3;K->AttemptTimeout=5000;K->RetryMin=100;K->RetryMax=10000;return xredisSubscriptionReplayConfigInit(&K->Replay);
}
XREDIS_API xredisadmission xredisSubscriptionSessionCreate(xnetengine* Engine,uint64 Affinity,xredissubscriptions* Store,const xredissubscriptionsessionconfig* Config,xredissubscriptionsession** Output)
{
    if(!Engine || !Store || !Config || !xrtMemRangeValid(Config,sizeof(*Config)) || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(Config,sizeof(*Config),Output,sizeof(*Output))){xredisSessionReport(XERR_ARGUMENT,"invalid session arguments");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsessionconfig K=*Config;
    if(K.Size!=sizeof(K) || !K.Connect || (K.Protocol!=XREDIS_RESP2 && K.Protocol!=XREDIS_RESP3) || K.AttemptTimeout<=0 || K.AttemptTimeout>60000 || K.RetryMin<=0 || K.RetryMax<K.RetryMin || K.RetryMax>60000 || K.Replay.Size!=sizeof(K.Replay) || !K.Replay.MaxBatchNames || K.Replay.MaxBatchNames>XREDIS_PUBSUB_NAMES_MAX || !K.Replay.MaxBatchBytes || K.Replay.Registry.Size!=sizeof(K.Replay.Registry) || !K.Replay.Registry.MaxSubscriptions || K.Replay.Registry.MaxSubscriptions>100000 || !K.Replay.Registry.MaxNameBytes){xredisSessionReport(XERR_ARGUMENT,"invalid session bounds/profile");return XREDIS_ADMISSION_ERROR;}
    xredissubscriptionsinfo Info={0};Info.Size=sizeof(Info);(void)xredisSubscriptionsInfo(Store,&Info);
    if(!__xredisSubscriptionsFits(Store,&K.Replay.Registry) || !__xredisSubscriptionRegistryBounds(&K.Replay.Registry)){xredisSessionReport(XERR_RANGE,"session registry cannot fit intent");return XREDIS_ADMISSION_ERROR;}
    if(!xrtNetEnginePin(Engine))return XREDIS_ADMISSION_ERROR;
    xredissubscriptionsession* S=xrtCalloc(1,sizeof(*S));if(!S || !xrtMutexInit(&S->Gate)){xrtFree(S);(void)xrtNetEngineUnpin(Engine);return XREDIS_ADMISSION_ERROR;}
    xrtAtomic32Init(&S->References,1);S->Engine=Engine;S->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));S->Config=K;
    S->Info.Size=sizeof(S->Info);S->Info.State=XREDIS_SESSION_CONNECTING;S->Info.DesiredRevision=Info.Revision;S->Random=((uint64)(xrtTimer()*1000000.0)^(uint64)(uintptr_t)S)|1u;S->Notify=true;
    S->Result=xrtMalloc(sizeof(*S->Result));
    if(!S->Result || !xrtNetPostInit(&S->Work) || !(S->Promise=xrtPromiseCreate(&S->Future,NULL)) || !xredisSubscriptionsRetain(Store)){(void)xrtNetEngineUnpin(Engine);xredisSubscriptionSessionRelease(S);return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=__xredisSubscriptionsClaim(Store,S,Output);if(A!=XREDIS_ADMISSION_ACCEPTED){xredisSubscriptionsRelease(Store);(void)xrtNetEngineUnpin(Engine);xredisSubscriptionSessionRelease(S);return A;}
    (void)xredisSubscriptionsInfo(Store,&Info);S->Info.DesiredRevision=Info.Revision;
    S->Store=Store;xredisSessionRef(S); /* Runtime. */
    xrtMutexLock(&S->Gate);*Output=S;xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisSubscriptionSessionApply(xredissubscriptionsession* S,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision)
{
    if(!S || !Revision || xrtMemRangesOverlap(S,S?sizeof(*S):0,Revision,sizeof(*Revision))){xredisSessionReport(XERR_ARGUMENT,"invalid session edit");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&S->Gate);if(S->Closing){xrtMutexUnlock(&S->Gate);return XREDIS_ADMISSION_CLOSED;}++S->APIs;xredissubscriptions* Store=S->Store;xrtMutexUnlock(&S->Gate);
    xredisadmission A=__xredisSubscriptionsApply(Store,Command,Names,Count,Revision,S);
    xrtMutexLock(&S->Gate);--S->APIs;if(A==XREDIS_ADMISSION_ACCEPTED && S->Info.DesiredRevision<*Revision){S->Info.DesiredRevision=*Revision;S->Notify=true;if(!S->Closing && S->Info.State==XREDIS_SESSION_ONLINE)S->Info.State=XREDIS_SESSION_UPDATING;}xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return A;
}
XREDIS_API bool xredisSubscriptionSessionInfo(xredissubscriptionsession* S,xredissubscriptionsessioninfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Info,sizeof(*Info)))return xredisSessionReport(XERR_ARGUMENT,"invalid session info");
    xrtMutexLock(&S->Gate);*Info=S->Info;xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API xredisadmission xredisSubscriptionSessionCurrent(xredissubscriptionsession* S,xredissubscriptionsessioncurrent* Output)
{
    if(!S || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xrtMemRangesOverlap(S,S?sizeof(*S):0,Output,sizeof(*Output))){xredisSessionReport(XERR_ARGUMENT,"invalid current output");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&S->Gate);xredisadmission A=S->Closing?XREDIS_ADMISSION_CLOSED:S->Published && S->Info.State==XREDIS_SESSION_ONLINE && S->Info.AppliedRevision==S->Info.DesiredRevision?XREDIS_ADMISSION_ACCEPTED:XREDIS_ADMISSION_OVERLOADED;
    if(A==XREDIS_ADMISSION_ACCEPTED){if(xredisClientRetain(S->Published))*Output=(xredissubscriptionsessioncurrent){S->Published,S->Info.Generations,S->Info.AppliedRevision};else A=XREDIS_ADMISSION_ERROR;}xrtMutexUnlock(&S->Gate);return A;
}
XREDIS_API bool xredisSubscriptionSessionClose(xredissubscriptionsession* S)
{
    if(!S)return xredisSessionReport(XERR_ARGUMENT,"session is NULL");
    xrtMutexLock(&S->Gate);if(!S->Closing){S->Closing=true;S->Info.State=XREDIS_SESSION_CLOSING;S->Notify=true;S->Published=NULL;}xredisSessionWakeLocked(S);xrtMutexUnlock(&S->Gate);return true;
}
XREDIS_API xfuture* xredisSubscriptionSessionClosed(xredissubscriptionsession* S)
{if(!S){xredisSessionReport(XERR_ARGUMENT,"session is NULL");return NULL;}return xrtFutureRef(S->Future);}
#endif
