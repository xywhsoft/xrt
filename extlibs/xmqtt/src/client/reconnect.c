#include <xmqtt/reconnect.h>
#include "../internal/xmqtt_client_lifecycle.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/time.h>
#include <math.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT)
struct xmqttreconnect {
    xmqttreconnectconfig Config;xmqttreconnectinfo Info;
    xnetengine* Engine;xnetworker* Worker;xtaskpool* Pool;uint64 Affinity;
    xmqttsession* Session;xmqttclientinbox* Inbox;xmqttclient *Client,*Cleaning;
    xmqttclientlease* Leases;xmqttclientlifecycle Lifecycle;
    xmutex Gate,RetirementLock;xcond RetirementReady;xatomic32 Refs,AssistReady;
    xnetpost Post;bool Posted,Driving,Closed,Stop,Failed,NeedsAssist;
    uint64 Timer,Random;double TimerAt,OnlineAt;
    xmqttproperty* Properties;unsigned char* Wire;size_t WireBytes,PropertyCount,Allocation;
};
static void xmqttReconnectWork(xnetworker* Worker,ptr Data);
static void xmqttReconnectTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data);
static bool xmqttReconnectError(xerrkind Kind,const char* Message)
{xrtSetErrorInfo(Kind,"xmqtt.reconnect",(int32)Kind,Message);return false;}
static void xmqttReconnectRef(xmqttreconnect* R)
{xrtAtomic32FetchAdd(&R->Refs,1,XMEMORY_RELAXED);}
static void xmqttReconnectSignal(xmqttreconnect* R)
{xrtMutexLock(&R->RetirementLock);xrtCondBroadcast(&R->RetirementReady);xrtMutexUnlock(&R->RetirementLock);}
static void xmqttReconnectDrop(xmqttreconnect* R)
{
    uint32 N=xrtAtomic32Load(&R->Refs,XMEMORY_ACQUIRE);
    while(N>2){if(xrtAtomic32CompareExchange(&R->Refs,&N,N-1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return;}
    xrtMutexLock(&R->RetirementLock);N=xrtAtomic32FetchSub(&R->Refs,1,XMEMORY_ACQ_REL);
    if(N==2)xrtCondBroadcast(&R->RetirementReady);
    xrtMutexUnlock(&R->RetirementLock);
}
/* Gate held. Engine pin and a new actual reference cover the entire post. */
static bool xmqttReconnectWakeLocked(xmqttreconnect* R)
{
    if(R->Posted)return true;
    xmqttReconnectRef(R);R->Posted=true;
    if(xrtNetPost(R->Worker,&R->Post,xmqttReconnectWork,R))return true;
    R->Posted=false;xmqttReconnectDrop(R);return false;
}
static void xmqttReconnectNotify(xmqttreconnect* R)
{if(R->Config.Notify)R->Config.Notify(R,R->Config.User);}
static bool xmqttReconnectStateLocked(xmqttreconnect* R,xmqttreconnectstate State)
{bool Changed=R->Info.State!=State;R->Info.State=State;return Changed;}
static void xmqttReconnectAssistLocked(xmqttreconnect* R)
{
    xrtAtomic32Store(&R->AssistReady,R->NeedsAssist && !R->Driving && !R->Cleaning && R->Client && !R->Info.Leases,XMEMORY_RELEASE);
}
static void xmqttReconnectFail(xmqttreconnect* R,xmqttclientstatus Status,bool Assist)
{
    xrtMutexLock(&R->Gate);R->Stop=true;
    if(!R->Failed){R->Failed=true;R->Info.LastStatus=Status;}
    if(Assist)R->NeedsAssist=true;
    xmqttReconnectAssistLocked(R);xrtMutexUnlock(&R->Gate);xmqttReconnectSignal(R);
}
static double xmqttReconnectDeadline(double Now,int64 Milliseconds)
{return Milliseconds<0?INFINITY:Now+(double)Milliseconds/1000.0;}
static int64 xmqttReconnectRemaining(double Deadline)
{
    if(isinf(Deadline))return XRT_WAIT_FOREVER;
    double D=ceil((Deadline-xrtTimer())*1000.0);return D<=0?0:D>=(double)INT64_MAX?INT64_MAX:(int64)D;
}
/* One saturating exponential and bounded jitter sample per failed generation. */
static int64 xmqttReconnectDelay(xmqttreconnect* R)
{
    uint64 D=(uint64)R->Config.InitialDelay,Max=(uint64)R->Config.MaxDelay;
    uint32 N=R->Info.ConsecutiveAttempts;
    while(N>1 && D<Max){D=D>Max/2?Max:D*2;--N;}
    uint64 Span=D*R->Config.JitterPermille/1000u;
    uint64 X=R->Random;X^=X<<13;X^=X>>7;X^=X<<17;R->Random=X;
    int64 J=Span?(int64)(X%(2*Span+1))-(int64)Span:0;
    int64 Result=(int64)D+J;return Result>R->Config.MaxDelay?R->Config.MaxDelay:Result;
}
static void xmqttReconnectChanged(ptr Data,xmqttclient* C,bool Closed,xmqttclientstatus Status,uint8 Reason)
{
    xmqttreconnect* R=Data;bool Notify=false;double Now=xrtTimer();
    xrtMutexLock(&R->Gate);
    if(R->Client!=C){xrtMutexUnlock(&R->Gate);return;}
    if(!Closed){
        ++R->Info.Connections;R->OnlineAt=Now;R->Info.RecoveryDeadline=INFINITY;
        if(!R->Stop){R->Info.LastStatus=Status;Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_ONLINE);}
    }else if(!R->Closed){
        R->Closed=true;R->Info.ServerReason=Reason;
        if(!R->Failed)R->Info.LastStatus=Status;
        if(R->Info.State==XMQTT_RECONNECT_ONLINE && Now-R->OnlineAt>=(double)R->Config.StableReset/1000.0)R->Info.ConsecutiveAttempts=0;
        if(!R->Stop){
            if(isinf(R->Info.RecoveryDeadline))R->Info.RecoveryDeadline=xmqttReconnectDeadline(Now,R->Config.RecoveryTimeout);
            if(!(R->Config.RetryStatuses&(UINT32_C(1)<<(unsigned)Status)) ||
               (R->Config.MaxAttempts && R->Info.ConsecutiveAttempts>=R->Config.MaxAttempts)){
                R->Stop=true;R->Failed=Status!=XMQTT_STATUS_OK && Status!=XMQTT_STATUS_CLOSED;
            }else R->Info.NextAttemptAt=xmqttReconnectDeadline(Now,xmqttReconnectDelay(R));
        }
        Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_RETIRING);
    }
    bool Wake=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
    if(!Wake)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    if(Notify)xmqttReconnectNotify(R);
}
/* Worker-only timer ownership. Current-worker admission inserts before return,
 * so successful cancellation completes its callback and reference immediately. */
static bool xmqttReconnectSchedule(xmqttreconnect* R,double At)
{
    if(R->Timer && R->TimerAt==At)return true;
    if(R->Timer){uint64 Id=R->Timer;R->Timer=0;R->TimerAt=INFINITY;(void)xrtNetEngineTimerCancelCurrent(R->Engine,Id);}
    if(isinf(At))return true;
    int64 Delay=xmqttReconnectRemaining(At);xmqttReconnectRef(R);
    R->Timer=xrtNetEngineAfter(R->Engine,R->Affinity,Delay,xmqttReconnectTimer,R);R->TimerAt=At;
    if(R->Timer)return true;
    xmqttReconnectDrop(R);xmqttReconnectFail(R,xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_LIMIT,true);return false;
}
static void xmqttReconnectTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xmqttreconnect* R=Data;
    if(R->Timer==Id){R->Timer=0;R->TimerAt=INFINITY;
        if(Result!=XNET_RESULT_OK)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
        xrtMutexLock(&R->Gate);bool Wake=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
        if(!Wake)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    }
    xmqttReconnectDrop(R);
}
static xtaskoutcome xmqttReconnectCleanupTask(xcancel* Cancel,ptr Data,xtaskvalue* Value)
{(void)Cancel;(void)Data;(void)Value;return XTASK_SUCCESS;}
/* Data finalization runs even if a queued task is cancelled before Proc. */
static void xmqttReconnectCleanupDrop(ptr Data,ptr Context)
{
    (void)Context;xmqttreconnect* R=Data;xrtMutexLock(&R->Gate);xmqttclient* C=R->Cleaning;xrtMutexUnlock(&R->Gate);
    bool OK=!xrtNetEngineCurrent(R->Engine) && xmqttClientDestroy(C);
    xrtMutexLock(&R->Gate);R->Cleaning=NULL;
    if(!OK){R->Client=C;R->NeedsAssist=true;R->Stop=R->Failed=true;R->Info.LastStatus=XMQTT_STATUS_INITIALIZATION;}
    bool Wake=xmqttReconnectWakeLocked(R);xmqttReconnectAssistLocked(R);xrtMutexUnlock(&R->Gate);
    if(OK)xmqttReconnectDrop(R); /* old Client's actual controller reference */
    if(!Wake)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    xmqttReconnectSignal(R);xmqttReconnectDrop(R); /* task Data reference */
}
static void xmqttReconnectCleanup(xmqttreconnect* R,xmqttclient* C)
{
    xrtMutexLock(&R->Gate);R->Client=NULL;R->Cleaning=C;xmqttReconnectRef(R);xrtMutexUnlock(&R->Gate);
    xtaskargs Args={0};Args.Destroy=xmqttReconnectCleanupDrop;
    xfuture* F=xrtTaskSubmit(R->Pool,xmqttReconnectCleanupTask,R,&Args);
    if(F){xrtFutureDestroy(F);return;}
    xrtMutexLock(&R->Gate);R->Client=C;R->Cleaning=NULL;xrtMutexUnlock(&R->Gate);
    xmqttReconnectDrop(R);xmqttReconnectFail(R,xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION,true);
}
static void xmqttReconnectAttempt(xmqttreconnect* R)
{
    xmqttcodecbuffers Buffers={R->Properties,R->PropertyCount,NULL,0};xmqttpacket Connect;
    xmqttcodecstatus Decode=R->Config.Client.Version==XMQTT_VERSION_5?
        xmqttPacketRead5((xbytesview){R->Wire,R->WireBytes},XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&Buffers,&Connect,NULL):
        xmqttPacketRead311((xbytesview){R->Wire,R->WireBytes},XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&Buffers,&Connect,NULL);
    xstrview Id;
    if(Decode!=XMQTT_CODEC_DONE || !xmqttSessionClientId(R->Session,&Id)){xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,false);return;}
    if(Id.Size)Connect.Body.Connect.ClientId=Id;
    xrtMutexLock(&R->Gate);
    if(R->Stop){xrtMutexUnlock(&R->Gate);return;}
    if(R->Info.Attempts==UINT64_MAX){xrtMutexUnlock(&R->Gate);xmqttReconnectFail(R,XMQTT_STATUS_LIMIT,false);return;}
    ++R->Info.Attempts;R->Info.Generation=R->Info.Attempts;
    if(R->Info.ConsecutiveAttempts!=UINT32_MAX)++R->Info.ConsecutiveAttempts;
    if(R->Config.CleanStartFirstOnly && R->Info.Attempts>1)Connect.Body.Connect.CleanStart=false;
    bool Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_CONNECTING);R->Closed=false;xrtMutexUnlock(&R->Gate);
    if(Notify)xmqttReconnectNotify(R);
    xrtMutexLock(&R->Gate);bool Stop=R->Stop;xrtMutexUnlock(&R->Gate);if(Stop)return;
    xmqttclient* C=NULL;xmqttclientstatus Status=R->Config.Factory(R->Engine,R->Affinity,&R->Config.Client,R->Session,R->Inbox,&Connect,R->Config.FactoryData,&C);
    if(Status==XMQTT_STATUS_OK && C){
        xrtMutexLock(&R->Gate);R->Client=C;xmqttReconnectRef(R);xrtMutexUnlock(&R->Gate);
        if(!xmqttInternalClientLifecycle(C,R->Worker,R->Session,R->Inbox,&R->Lifecycle))xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    }else{
        if(Status<=XMQTT_STATUS_OK || Status>XMQTT_STATUS_AUTHENTICATION)Status=XMQTT_STATUS_INITIALIZATION;
        /* Reuse the closed-generation policy without a fabricated Client. */
        xmqttReconnectChanged(R,NULL,true,Status,0);
    }
}
static void xmqttReconnectWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xmqttreconnect* R=Data;double At=INFINITY,Now=xrtTimer();
    xrtMutexLock(&R->Gate);R->Posted=false;R->Driving=true;
    if(!R->Stop && Now>=R->Info.RecoveryDeadline){R->Stop=R->Failed=true;R->Info.LastStatus=XMQTT_STATUS_TIMEOUT;}
    xmqttclient* C=R->Client;bool Stop=R->Stop,Closed=R->Closed,Cleaning=R->Cleaning!=NULL,Assist=R->NeedsAssist;size_t Leases=R->Info.Leases;
    if(!Stop)At=R->Info.RecoveryDeadline;
    xrtMutexUnlock(&R->Gate);
    if(C){
        if(Stop && !Closed && !xmqttClientClose(C))xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
        if(Closed && !Leases && !Assist){
            xwaitresult Ready=xmqttClientPollRetired(C);
            if(Ready==XWAIT_OK)xmqttReconnectCleanup(R,C);
            else if(Ready==XWAIT_ERROR)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
            else {double Poll=xmqttReconnectDeadline(Now,R->Config.RetirementPoll);if(Poll<At)At=Poll;}
        }
    }else if(!Cleaning){
        xrtMutexLock(&R->Gate);bool Notify=false;
        if(R->Stop)Notify=xmqttReconnectStateLocked(R,R->Failed?XMQTT_RECONNECT_FAILED:XMQTT_RECONNECT_STOPPED);
        else if(Now<R->Info.NextAttemptAt){Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_WAITING);if(R->Info.NextAttemptAt<At)At=R->Info.NextAttemptAt;}
        bool Attempt=!R->Stop && Now>=R->Info.NextAttemptAt;xrtMutexUnlock(&R->Gate);
        if(Notify)xmqttReconnectNotify(R);
        if(Attempt)xmqttReconnectAttempt(R);
    }
    xrtMutexLock(&R->Gate);Stop=R->Stop;Assist=R->NeedsAssist;bool HasClient=R->Client!=NULL || R->Cleaning!=NULL;
    if(Stop && (!HasClient || Assist || R->Info.Leases || !R->Closed))At=INFINITY;
    xrtMutexUnlock(&R->Gate);
    if(!xmqttReconnectSchedule(R,At)){
        xrtMutexLock(&R->Gate);(void)xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
    }
    xrtMutexLock(&R->Gate);R->Driving=false;xmqttReconnectAssistLocked(R);bool Signal=xrtAtomic32Load(&R->AssistReady,XMEMORY_ACQUIRE)!=0;xrtMutexUnlock(&R->Gate);
    if(Signal)xmqttReconnectSignal(R);
    xmqttReconnectDrop(R);
}
XMQTT_API bool xmqttReconnectConfigInit(xmqttreconnectconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttReconnectError(XERR_ARGUMENT,"invalid reconnect config");
    memset(K,0,sizeof(*K));K->Size=sizeof(*K);xmqttClientConfigInit(&K->Client);
    K->MaxControlBytes=262144;K->MaxLeases=256;K->MaxSessionOutgoing=128;K->MaxSessionIncoming=64;K->MaxSessionBytes=16u*1024u*1024u;K->MaxAssignedIdBytes=1024;
    K->InitialDelay=250;K->MaxDelay=30000;K->StableReset=30000;K->RecoveryTimeout=60000;K->RetirementPoll=1;K->JitterPermille=200;K->MaxAttempts=8;K->CleanStartFirstOnly=true;
    K->RetryStatuses=(UINT32_C(1)<<XMQTT_STATUS_TRANSPORT)|(UINT32_C(1)<<XMQTT_STATUS_TIMEOUT)|(UINT32_C(1)<<XMQTT_STATUS_SERVER_DISCONNECT);return true;
}
XMQTT_API bool xmqttReconnectCreate(xnetengine* Engine,xtaskpool* Pool,uint64 Affinity,const xmqttreconnectconfig* Config,const xmqttpacket* Connect,xmqttreconnect** Out)
{
    double Started=xrtTimer();
    if(!Engine || !Pool || !Config || !Connect || !Out || !xrtMemRangeValid(Config,sizeof(*Config)) || !xrtMemRangeValid(Connect,sizeof(*Connect)) || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttReconnectError(XERR_ARGUMENT,"invalid reconnect factory arguments");
    xmqttreconnectconfig K=*Config;
    if(K.Size!=sizeof(K) || K.Client.Size!=sizeof(K.Client) || (K.Client.Version!=XMQTT_VERSION_5 && K.Client.Version!=XMQTT_VERSION_311) || !K.Factory || !K.MaxLeases || K.InitialDelay<1 || K.MaxDelay<K.InitialDelay || K.MaxDelay>INT32_MAX || K.StableReset<0 || (K.RecoveryTimeout<=0 && K.RecoveryTimeout!=XRT_WAIT_FOREVER) || K.RetirementPoll<1 || K.RetirementPoll>60000 || K.JitterPermille>1000 || K.RetryStatuses>>((unsigned)XMQTT_STATUS_AUTHENTICATION+1) || Connect->Type!=XMQTT_PACKET_CONNECT)
        return xmqttReconnectError(XERR_ARGUMENT,"invalid reconnect policy");
    size_t Bytes=0;bool Valid=K.Client.Version==XMQTT_VERSION_5?xmqttPacketSize5(Connect,XMQTT_CLIENT_TO_SERVER,&K.Client.Codec,&Bytes):K.Client.Version==XMQTT_VERSION_311 && xmqttPacketSize311(Connect,XMQTT_CLIENT_TO_SERVER,&K.Client.Codec,&Bytes);
    if(!Valid)return false;
    size_t Count=Connect->Properties.Count;
    if(Connect->Body.Connect.WillProperties.Count>SIZE_MAX-Count)return xmqttReconnectError(XERR_RANGE,"CONNECT property size overflow");
    Count+=Connect->Body.Connect.WillProperties.Count;
    if(Bytes>K.Client.MaxSetupBytes || Bytes>SIZE_MAX-sizeof(xmqttreconnect) || Count>(SIZE_MAX-sizeof(xmqttreconnect)-Bytes)/sizeof(xmqttproperty) || sizeof(xmqttreconnect)+Bytes+Count*sizeof(xmqttproperty)>K.MaxControlBytes)
        return xmqttReconnectError(XERR_RANGE,"reconnect storage limit");
    if(!xrtNetEnginePin(Engine))return false;
    size_t Allocation=sizeof(xmqttreconnect)+Bytes+Count*sizeof(xmqttproperty);xmqttreconnect* R=xrtCalloc(1,Allocation);
    if(!R){xrtNetEngineUnpin(Engine);return false;}
    R->Config=K;R->Engine=Engine;R->Worker=xrtNetEngineWorker(Engine,Affinity);R->Pool=Pool;R->Affinity=Affinity;R->Allocation=Allocation;
    R->PropertyCount=Count;R->Properties=(xmqttproperty*)(R+1);R->Wire=(unsigned char*)(R->Properties+Count);R->WireBytes=Bytes;
    R->Info.Size=sizeof(R->Info);R->Info.RecoveryDeadline=xmqttReconnectDeadline(Started,K.RecoveryTimeout);R->Info.State=XMQTT_RECONNECT_CONNECTING;R->TimerAt=INFINITY;
    R->Random=K.RandomSeed?K.RandomSeed:xrtNetWorkerOperationId(R->Worker);if(!R->Random)R->Random=UINT64_C(0x9e3779b97f4a7c15);
    R->Lifecycle=(xmqttclientlifecycle){xmqttReconnectChanged,R};xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->AssistReady,0);
    bool Gate=false,Retirement=false,Cond=false;
    if(!(Gate=xrtMutexInit(&R->Gate)) || !(Retirement=xrtMutexInit(&R->RetirementLock)) || !(Cond=xrtCondInit(&R->RetirementReady)) || !xrtNetPostInit(&R->Post))goto Failed;
    if(!(K.Client.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Client.Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Client.Codec,NULL)))goto Failed;
    xmqttsessionconfig S;xmqttSessionConfigInit(&S);S.Version=K.Client.Version;S.ClientId=Connect->Body.Connect.ClientId;S.Codec=K.Client.Codec;S.MaxOutgoing=K.MaxSessionOutgoing;S.MaxIncoming=K.MaxSessionIncoming;S.MaxBytes=K.MaxSessionBytes;
    if(!(S.ClientId.Size?xmqttSessionCreate(&S,&R->Session):xmqttSessionCreateUnassigned(&S,K.MaxAssignedIdBytes,&R->Session)))goto Failed;
    xmqttclientinboxconfig I;xmqttClientInboxConfigInit(&I);I.Version=K.Client.Version;I.MaxObjects=K.Client.MaxObjects;I.MaxObjectBytes=K.Client.MaxObjectBytes;I.MaxMessages=K.Client.MaxMessages;I.MaxMessageBytes=K.Client.MaxMessageBytes;
    if(!xmqttClientInboxCreate(&I,&R->Inbox))goto Failed;
    xrtMutexLock(&R->Gate);bool Posted=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);if(!Posted)goto Failed;
    *Out=R;return true;
Failed:
    xmqttClientInboxDestroy(R->Inbox);if(R->Session)xmqttSessionDestroy(R->Session);
    if(Cond)xrtCondUnit(&R->RetirementReady);
    if(Retirement)xrtMutexUnit(&R->RetirementLock);
    if(Gate)xrtMutexUnit(&R->Gate);
    xrtSecureZero(R,Allocation);xrtFree(R);xrtNetEngineUnpin(Engine);return false;
}
XMQTT_API bool xmqttReconnectAcquire(xmqttreconnect* R,xmqttclientlease* L)
{
    if(!R || !L || !xrtMemRangeValid(L,sizeof(*L)) || L->Owner)return xmqttReconnectError(XERR_ARGUMENT,"invalid or active Client lease");
    xrtMutexLock(&R->Gate);
    if(R->Stop || R->Info.State!=XMQTT_RECONNECT_ONLINE || !R->Client || R->Info.Leases>=R->Config.MaxLeases){xrtMutexUnlock(&R->Gate);return false;}
    *L=(xmqttclientlease){R->Client,R->Info.Generation,R,NULL,R->Leases};if(R->Leases)R->Leases->Previous=L;R->Leases=L;++R->Info.Leases;xrtMutexUnlock(&R->Gate);return true;
}
XMQTT_API bool xmqttReconnectRelease(xmqttclientlease* L)
{
    if(!L || !xrtMemRangeValid(L,sizeof(*L)) || !L->Owner)return xmqttReconnectError(XERR_ARGUMENT,"inactive Client lease");
    xmqttreconnect* R=L->Owner;xrtMutexLock(&R->Gate);
    if(L->Client!=R->Client || L->Generation!=R->Info.Generation || (L->Previous?L->Previous->Next!=L:R->Leases!=L) || (L->Next && L->Next->Previous!=L)){
        xrtMutexUnlock(&R->Gate);return xmqttReconnectError(XERR_STATE,"Client lease identity mismatch");}
    if(L->Previous)L->Previous->Next=L->Next;else R->Leases=L->Next;
    if(L->Next)L->Next->Previous=L->Previous;
    --R->Info.Leases;memset(L,0,sizeof(*L));bool OK=true;
    bool Needed=!R->Info.Leases && (R->Closed || R->Stop || R->NeedsAssist);
    if(Needed){OK=xmqttReconnectWakeLocked(R);xmqttReconnectAssistLocked(R);}
    xrtMutexUnlock(&R->Gate);
    if(!OK)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    if(Needed)xmqttReconnectSignal(R);
    return OK;
}
XMQTT_API bool xmqttReconnectInfo(xmqttreconnect* R,xmqttreconnectinfo* Out)
{
    if(!R || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttReconnectError(XERR_ARGUMENT,"invalid reconnect info");
    xrtMutexLock(&R->Gate);*Out=R->Info;xrtMutexUnlock(&R->Gate);return true;
}
XMQTT_API bool xmqttReconnectTakeMessage(xmqttreconnect* R,xmqttmessage** Message)
{if(!R)return xmqttReconnectError(XERR_ARGUMENT,"controller is NULL");return xmqttClientInboxTake(R->Inbox,Message);}
XMQTT_API bool xmqttReconnectClose(xmqttreconnect* R)
{
    if(!R)return xmqttReconnectError(XERR_ARGUMENT,"controller is NULL");
    xrtMutexLock(&R->Gate);R->Stop=true;R->Info.StopRequested=true;
    bool Terminal=R->Info.State>=XMQTT_RECONNECT_STOPPED;bool OK=Terminal || xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);return OK;
}
/* Off-worker fallback retains the actual old Client and original timeout.
 * Gate excludes the drive and other helpers before transferring its owner. */
static void xmqttReconnectAssist(xmqttreconnect* R,double Deadline)
{
    xrtMutexLock(&R->Gate);
    if(!R->NeedsAssist || R->Driving || R->Cleaning || !R->Client || R->Info.Leases){xrtMutexUnlock(&R->Gate);return;}
    xmqttclient* C=R->Client;R->Client=NULL;R->Cleaning=C;xrtAtomic32Store(&R->AssistReady,0,XMEMORY_RELEASE);xrtMutexUnlock(&R->Gate);
    bool OK=xmqttClientClose(C) && xmqttClientWaitFor(C,xmqttReconnectRemaining(Deadline))==XWAIT_OK && xmqttClientDestroy(C);
    xrtMutexLock(&R->Gate);R->Cleaning=NULL;
    if(!OK)R->Client=C;
    else R->NeedsAssist=false;
    bool Wake=xmqttReconnectWakeLocked(R);xmqttReconnectAssistLocked(R);xrtMutexUnlock(&R->Gate);
    if(OK)xmqttReconnectDrop(R);
    if(!Wake)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    xmqttReconnectSignal(R);
}
XMQTT_API xwaitresult xmqttReconnectWaitFor(xmqttreconnect* R,int64 Timeout)
{
    if(!R || Timeout<XRT_WAIT_FOREVER){xmqttReconnectError(XERR_ARGUMENT,"invalid reconnect wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(R->Engine) || xrtTaskPoolIsCurrentWorkerV1(R->Pool)){xmqttReconnectError(XERR_STATE,"cannot wait from an Engine/cleanup worker");return XWAIT_ERROR;}
    double Deadline=xmqttReconnectDeadline(xrtTimer(),Timeout);
    for(;;){
        if(xrtAtomic32Load(&R->AssistReady,XMEMORY_ACQUIRE))xmqttReconnectAssist(R,Deadline);
        if(!xrtMutexLock(&R->RetirementLock))return XWAIT_ERROR;
        bool Retired=xrtAtomic32Load(&R->Refs,XMEMORY_ACQUIRE)==1;
        if(Retired){xrtMutexUnlock(&R->RetirementLock);xrtMutexLock(&R->Gate);bool Terminal=R->Info.State>=XMQTT_RECONNECT_STOPPED;xrtMutexUnlock(&R->Gate);if(Terminal)return XWAIT_OK;}
        else if(xrtAtomic32Load(&R->AssistReady,XMEMORY_ACQUIRE)){xrtMutexUnlock(&R->RetirementLock);if(xrtTimer()<Deadline)continue;}
        else {
            int64 Remaining=xmqttReconnectRemaining(Deadline);if(!Remaining){xrtMutexUnlock(&R->RetirementLock);return XWAIT_TIMEOUT;}
            xwaitresult Result=xrtCondWaitFor(&R->RetirementReady,&R->RetirementLock,Remaining);xrtMutexUnlock(&R->RetirementLock);if(Result==XWAIT_ERROR)return Result;
        }
        if(xrtTimer()>=Deadline)return XWAIT_TIMEOUT;
    }
}
XMQTT_API bool xmqttReconnectDestroy(xmqttreconnect* R)
{
    if(!R)return true;
    if(xrtNetEngineCurrent(R->Engine) || xrtTaskPoolIsCurrentWorkerV1(R->Pool))return xmqttReconnectError(XERR_STATE,"cannot destroy from an Engine/cleanup worker");
    if(!xmqttReconnectClose(R) || xmqttReconnectWaitFor(R,XRT_WAIT_FOREVER)!=XWAIT_OK)return false;
    if(R->Session && !xmqttSessionDestroy(R->Session))return false;
    R->Session=NULL;
    if(!xmqttClientInboxDestroy(R->Inbox))return false;
    R->Inbox=NULL;
    xnetengine* Engine=R->Engine;size_t Allocation=R->Allocation;
    xrtCondUnit(&R->RetirementReady);xrtMutexUnit(&R->RetirementLock);xrtMutexUnit(&R->Gate);xrtSecureZero(R,Allocation);xrtFree(R);return xrtNetEngineUnpin(Engine);
}
#endif
