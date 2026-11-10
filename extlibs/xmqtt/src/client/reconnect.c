#include <xmqtt/reconnect.h>
#include <xmqtt/reconnect_subscriptions.h>
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
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    struct xmqttsubscriptions* Subscriptions;
#endif
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
/* Private optional subscription registry. */
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
#define XMQTT_INTENT_NONE SIZE_MAX
enum {XMQTT_INTENT_WILDCARD=1u,XMQTT_INTENT_SHARED=2u,XMQTT_INTENT_SUBID=4u};
typedef struct xmqttintentorder {uint64 Hash;xstrview Filter;uint32 Index,Requirements;size_t Entry;} xmqttintentorder;
typedef struct xmqttintentgroup {
    size_t Refs,Allocation;uint64 Revision;xmqttpacket Packet;xmqttintentorder* Order;uint32 RemainingBase,Requirements;
} xmqttintentgroup;
typedef struct xmqttintententry {
    xmqttintentgroup* Group;size_t Topic,HashNext,Next,Previous;
    uint64 Hash,Revision,Applied,Attempted,Generation,FlightRevision;
    bool Desired,KnownPresent,Dirty,Flight,FlightDesired;
    uint8 Reason;xmqttclientstatus Status;
} xmqttintententry;
typedef struct xmqttintentwatch {xfuturewatch Watch;xmqttreconnect* Controller;} xmqttintentwatch;
typedef struct xmqttsubscriptions {
    xmqttsubscriptionsconfig Config;xmqttintententry* Entries;size_t* Buckets;
    size_t BucketCount,Free,Head,Tail,Count,Bytes,Reserved,Watchers,Desired,Pending,Rejected;
    uint64 Revision,SettledRevision,Generation,Requests,Recoveries,Admitted,Overloaded,AdmissionRetries;
    bool Ready,Announced;double Deadline;xmqttclientstatus Status;
    xmqttnegotiated Peer;xfuture *ReadyFuture,*Future;size_t MaxOutgoingBytes;
    xmqttintentgroup* FlightGroup;size_t FlightCount,*FlightEntries;xmqttsubscription* FlightTopics;
} xmqttsubscriptions;
static void xmqttSubscriptionsFail(xmqttreconnect* R,xmqttclientstatus Status)
{
    xmqttReconnectFail(R,Status,false);xrtMutexLock(&R->Gate);R->Subscriptions->Status=Status;bool OK=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
    if(!OK)xmqttReconnectFail(R,Status,true);
}
static unsigned xmqttIntentMetrics(xmqttsubscriptions* S,const xmqttintententry* E)
{
    if(!E->Group)return 0;
    return (E->Desired?1u:0u) | ((E->Dirty || E->Flight || E->Attempted!=E->Revision || E->Generation!=S->Generation)?2u:0u) |
        ((E->Attempted==E->Revision && E->Generation==S->Generation && E->Status!=XMQTT_STATUS_OK)?4u:0u);
}
static void xmqttIntentAccount(xmqttsubscriptions* S,unsigned Before,const xmqttintententry* E)
{
    unsigned After=xmqttIntentMetrics(S,E);
    S->Desired=S->Desired-((Before&1u)!=0)+((After&1u)!=0);
    S->Pending=S->Pending-((Before&2u)!=0)+((After&2u)!=0);
    S->Rejected=S->Rejected-((Before&4u)!=0)+((After&4u)!=0);
}
static uint64 xmqttIntentHash(xstrview V)
{uint64 H=UINT64_C(14695981039346656037);for(size_t I=0;I<V.Size;++I){H^=(unsigned char)V.Data[I];H*=UINT64_C(1099511628211);}return H;}
/* Admission validates and owns the entire immutable packet. Cache the only
 * per-filter negotiated capabilities; final Client admission still validates
 * each emitted subset once. No growing-prefix codec scans under the Gate. */
static uint32 xmqttIntentRequirements(xstrview Filter)
{
    uint32 Bits=Filter.Size>=7 && !memcmp(Filter.Data,"$share/",7)?XMQTT_INTENT_SHARED:0;
    for(size_t I=0;I<Filter.Size;++I)if(Filter.Data[I]=='+' || Filter.Data[I]=='#'){Bits|=XMQTT_INTENT_WILDCARD;break;}
    return Bits;
}
static bool xmqttIntentFits(xmqttsubscriptions* S,const xmqttintentgroup* G,size_t Topic,uint32* Remaining)
{
    const xmqttsubscription* T=G->Packet.Body.Topics.Data+Topic;
    uint32 Extra=(uint32)T->Filter.Size+2u+(G->Packet.Type==XMQTT_PACKET_SUBSCRIBE);
    if(Extra>XMQTT_VARIABLE_MAX-*Remaining)return false;
    uint32 Next=*Remaining+Extra;size_t Width=Next<128u?1:Next<16384u?2:Next<2097152u?3:4;
    size_t Bytes=1+Width+(size_t)Next;
    if(Bytes>S->Peer.SendPacketBytes || Bytes>S->MaxOutgoingBytes)return false;
    uint32 Bits=G->Requirements|G->Order[Topic].Requirements;
    if(S->Peer.Version==XMQTT_VERSION_5 &&
        (((Bits&XMQTT_INTENT_WILDCARD) && !S->Peer.WildcardAvailable) ||
         ((Bits&XMQTT_INTENT_SHARED) && !S->Peer.SharedAvailable) ||
         ((Bits&XMQTT_INTENT_SUBID) && !S->Peer.SubscriptionIdsAvailable)))return false;
    *Remaining=Next;return true;
}
static bool xmqttIntentEqual(xstrview A,xstrview B)
{return A.Size==B.Size && (!A.Size || !memcmp(A.Data,B.Data,A.Size));}
static size_t xmqttIntentFind(xmqttsubscriptions* S,xstrview Filter,uint64 Hash)
{
    for(size_t I=S->Buckets[Hash&(S->BucketCount-1)];I!=XMQTT_INTENT_NONE;I=S->Entries[I].HashNext){
        xmqttintententry* E=S->Entries+I;
        if(E->Hash==Hash && xmqttIntentEqual(E->Group->Packet.Body.Topics.Data[E->Topic].Filter,Filter))return I;
    }return XMQTT_INTENT_NONE;
}
static void xmqttIntentGroupDrop(xmqttsubscriptions* S,xmqttintentgroup* G)
{if(G && !--G->Refs){size_t N=G->Allocation;S->Bytes-=N;xrtSecureZero(G,N);xrtFree(G);}}
static void xmqttIntentUnqueue(xmqttsubscriptions* S,size_t I)
{
    xmqttintententry* E=S->Entries+I;if(!E->Dirty)return;
    if(E->Previous!=XMQTT_INTENT_NONE)S->Entries[E->Previous].Next=E->Next;else S->Head=E->Next;
    if(E->Next!=XMQTT_INTENT_NONE)S->Entries[E->Next].Previous=E->Previous;else S->Tail=E->Previous;
    E->Dirty=false;E->Next=E->Previous=XMQTT_INTENT_NONE;
}
static void xmqttIntentQueue(xmqttsubscriptions* S,size_t I)
{
    xmqttintententry* E=S->Entries+I;if(E->Dirty)return;
    E->Previous=S->Tail;E->Next=XMQTT_INTENT_NONE;E->Dirty=true;
    if(S->Tail!=XMQTT_INTENT_NONE)S->Entries[S->Tail].Next=I;else S->Head=I;S->Tail=I;
}
static void xmqttIntentErase(xmqttsubscriptions* S,size_t I)
{
    xmqttintententry* E=S->Entries+I;size_t* Link=S->Buckets+(E->Hash&(S->BucketCount-1));
    while(*Link!=I)Link=&S->Entries[*Link].HashNext;
    *Link=E->HashNext;xmqttIntentUnqueue(S,I);xmqttIntentGroupDrop(S,E->Group);
    memset(E,0,sizeof(*E));E->Next=S->Free;S->Free=I;--S->Count;
}
static int xmqttIntentOrderCompare(const xmqttintentorder* X,const xmqttintentorder* Y,bool Index)
{
    if(Index)return X->Index<Y->Index?-1:X->Index>Y->Index;
    if(X->Hash!=Y->Hash)return X->Hash<Y->Hash?-1:1;
    size_t N=X->Filter.Size<Y->Filter.Size?X->Filter.Size:Y->Filter.Size;int C=memcmp(X->Filter.Data,Y->Filter.Data,N);
    return C?C:X->Filter.Size<Y->Filter.Size?-1:X->Filter.Size>Y->Filter.Size;
}
static void xmqttIntentSift(xmqttintentorder* A,size_t Root,size_t Count,bool Index)
{
    while(Root<Count/2){size_t Child=Root*2+1;
        if(Child+1<Count && xmqttIntentOrderCompare(A+Child,A+Child+1,Index)<0)++Child;
        if(xmqttIntentOrderCompare(A+Root,A+Child,Index)>=0)break;
        xmqttintentorder V=A[Root];A[Root]=A[Child];A[Child]=V;Root=Child;
    }
}
/* Fixed-stack in-place heapsort: libc qsort may allocate outside the registry
 * quota on some platforms. Original indices restore caller ordering afterward. */
static void xmqttIntentSort(xmqttintentorder* A,size_t Count,bool Index)
{
    for(size_t I=Count/2;I;--I)xmqttIntentSift(A,I-1,Count,Index);
    for(size_t End=Count;End>1;--End){xmqttintentorder V=A[0];A[0]=A[End-1];A[End-1]=V;xmqttIntentSift(A,0,End-1,Index);}
}
static bool xmqttIntentGrow(size_t* N,size_t Count,size_t Width)
{if(Count>(SIZE_MAX-*N)/Width)return false;*N+=Count*Width;return true;}
XMQTT_API bool xmqttSubscriptionsConfigInit(xmqttsubscriptionsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttReconnectError(XERR_ARGUMENT,"invalid subscription config");
    *K=(xmqttsubscriptionsconfig){sizeof(*K),1024,4u*1024u*1024u,32,30000,5,true,NULL,NULL};return true;
}
static bool xmqttSubscriptionsCreate(xmqttreconnect* R,const xmqttsubscriptionsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || !K->MaxFilters || K->MaxFilters>65535 || !K->MaxBatch || K->MaxBatch>K->MaxFilters || K->MaxBatch>R->Config.Client.Codec.MaxTopics || K->OperationTimeout<=0 || K->RetryDelay<1 || K->RetryDelay>60000)
        return xmqttReconnectError(XERR_ARGUMENT,"invalid subscription recovery bounds");
    size_t Buckets=2;while(Buckets<K->MaxFilters*2)Buckets*=2;
    size_t N=sizeof(xmqttsubscriptions);
    if(!xmqttIntentGrow(&N,K->MaxFilters,sizeof(xmqttintententry)) || !xmqttIntentGrow(&N,Buckets,sizeof(size_t)) || !xmqttIntentGrow(&N,K->MaxBatch,sizeof(size_t)+sizeof(xmqttsubscription)) || K->MaxBytes<2*sizeof(xmqttintentwatch) || N>K->MaxBytes-2*sizeof(xmqttintentwatch))
        return xmqttReconnectError(XERR_RANGE,"subscription fixed storage exceeds limit");
    xmqttsubscriptions* S=xrtCalloc(1,N);if(!S)return false;
    S->Config=*K;S->Bytes=N;S->Entries=(xmqttintententry*)(S+1);S->Buckets=(size_t*)(S->Entries+K->MaxFilters);
    S->FlightEntries=S->Buckets+Buckets;S->FlightTopics=(xmqttsubscription*)(S->FlightEntries+K->MaxBatch);
    S->BucketCount=Buckets;S->Head=S->Tail=XMQTT_INTENT_NONE;S->Deadline=INFINITY;
    for(size_t I=0;I<Buckets;++I)S->Buckets[I]=XMQTT_INTENT_NONE;
    for(size_t I=0;I<K->MaxFilters;++I)S->Entries[I].Next=I+1<K->MaxFilters?I+1:XMQTT_INTENT_NONE;
    R->Subscriptions=S;return true;
}
static void xmqttSubscriptionsDestroy(xmqttreconnect* R)
{
    xmqttsubscriptions* S=R->Subscriptions;if(!S)return;
    for(size_t I=0;I<S->Config.MaxFilters;++I)if(S->Entries[I].Group)xmqttIntentGroupDrop(S,S->Entries[I].Group);
    xrtFutureDestroy(S->ReadyFuture);xrtFutureDestroy(S->Future);
    size_t N=sizeof(*S)+S->Config.MaxFilters*sizeof(xmqttintententry)+S->BucketCount*sizeof(size_t)+S->Config.MaxBatch*(sizeof(size_t)+sizeof(xmqttsubscription));
    xrtSecureZero(S,N);xrtFree(S);R->Subscriptions=NULL;
}
static xmqttclientadmission xmqttSubscriptionsSubmit(xmqttreconnect* R,const xmqttpacket* Input,uint64* Revision,xmqttpackettype Type,bool Wake)
{
    if(!R || !Input || !Revision || !xrtMemRangeValid(Input,sizeof(*Input)) || !xrtMemRangeValid(Revision,sizeof(*Revision)) || Input->Type!=Type || Input->Identifier){xmqttReconnectError(XERR_ARGUMENT,"invalid subscription intent");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&R->Gate);xmqttsubscriptions* S=R->Subscriptions;bool Stop=R->Stop;xrtMutexUnlock(&R->Gate);
    if(!S){xmqttReconnectError(XERR_STATE,"controller has no subscription registry");return XMQTT_ADMISSION_ERROR;}
    if(Stop)return XMQTT_ADMISSION_CLOSED;
    xmqttpacket Packet=*Input;Packet.Identifier=1;size_t WireBytes=0;
    bool Valid=R->Config.Client.Version==XMQTT_VERSION_5?xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&WireBytes):xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&WireBytes);
    if(!Valid)return XMQTT_ADMISSION_ERROR;
    size_t Count=Packet.Body.Topics.Count,N=sizeof(xmqttintentgroup);
    if(Count>S->Config.MaxFilters){xrtMutexLock(&R->Gate);if(S->Overloaded!=UINT64_MAX)++S->Overloaded;xrtMutexUnlock(&R->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    if(!xmqttIntentGrow(&N,Packet.Properties.Count,sizeof(xmqttproperty)) || !xmqttIntentGrow(&N,Count,sizeof(xmqttsubscription)+sizeof(xmqttintentorder)) || !xmqttIntentGrow(&N,WireBytes,1)){xmqttReconnectError(XERR_RANGE,"subscription snapshot overflow");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&R->Gate);
    if(R->Stop){xrtMutexUnlock(&R->Gate);return XMQTT_ADMISSION_CLOSED;}
    size_t Used=S->Bytes-S->Watchers*sizeof(xmqttintentwatch),Limit=S->Config.MaxBytes-2*sizeof(xmqttintentwatch);
    if(Used>Limit || S->Reserved>Limit-Used || N>Limit-Used-S->Reserved){if(S->Overloaded!=UINT64_MAX)++S->Overloaded;xrtMutexUnlock(&R->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    S->Reserved+=N;xrtMutexUnlock(&R->Gate);
    xmqttintentgroup* G=xrtCalloc(1,N);bool OK=G!=NULL;
    if(G){
        xmqttproperty* Props=(xmqttproperty*)(G+1);xmqttsubscription* Topics=(xmqttsubscription*)(Props+Packet.Properties.Count);
        G->Order=(xmqttintentorder*)(Topics+Count);unsigned char* Wire=(unsigned char*)(G->Order+Count);
        G->Allocation=N;G->Refs=1;xmqttcodecbuffers B={Props,Packet.Properties.Count,Topics,Count};
        OK=R->Config.Client.Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,WireBytes,&Packet,XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,NULL):xmqttPacketWrite311(Wire,WireBytes,&Packet,XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,NULL);
        if(OK)OK=(R->Config.Client.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Wire,WireBytes},XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&B,&G->Packet,NULL):xmqttPacketRead311((xbytesview){Wire,WireBytes},XMQTT_CLIENT_TO_SERVER,&R->Config.Client.Codec,&B,&G->Packet,NULL))==XMQTT_CODEC_DONE;
        if(OK){
            xmqttwireconfig W;xmqttWireConfigInit(&W);W.Version=R->Config.Client.Version;W.MaxPacketBytes=WireBytes;
            xmqttfixedheader Header;OK=xmqttHeaderRead((xbytesview){Wire,WireBytes},&W,&Header)==XMQTT_WIRE_DONE;
            if(OK){
                G->RemainingBase=Header.RemainingLength;
                for(size_t I=0;I<Count;++I){
                    uint32 Bits=Type==XMQTT_PACKET_SUBSCRIBE && R->Config.Client.Version==XMQTT_VERSION_5?xmqttIntentRequirements(Topics[I].Filter):0;
                    G->Order[I]=(xmqttintentorder){xmqttIntentHash(Topics[I].Filter),Topics[I].Filter,(uint32)I,Bits,0};
                    G->RemainingBase-=(uint32)Topics[I].Filter.Size+2u+(Type==XMQTT_PACKET_SUBSCRIBE);
                }
                if(Type==XMQTT_PACKET_SUBSCRIBE)for(size_t I=0;I<Packet.Properties.Count;++I)
                    if(Props[I].Id==XMQTT_PROPERTY_SUBSCRIPTION_ID)G->Requirements|=XMQTT_INTENT_SUBID;
            }
            if(OK)xmqttIntentSort(G->Order,Count,false);
            for(size_t I=1;OK && I<Count;++I)if(xmqttIntentEqual(G->Order[I-1].Filter,G->Order[I].Filter)){OK=xmqttReconnectError(XERR_ARGUMENT,"duplicate filter in one intent");break;}
            if(OK)xmqttIntentSort(G->Order,Count,true);
        }
    }
    xrtMutexLock(&R->Gate);xmqttclientadmission Admission=XMQTT_ADMISSION_ERROR;
    if(OK){
        size_t New=0;for(size_t I=0;I<Count;++I){G->Order[I].Entry=xmqttIntentFind(S,G->Order[I].Filter,G->Order[I].Hash);if(G->Order[I].Entry==XMQTT_INTENT_NONE)++New;}
        if(R->Stop)Admission=XMQTT_ADMISSION_CLOSED;
        else if(New>S->Config.MaxFilters-S->Count || S->Revision==UINT64_MAX)Admission=XMQTT_ADMISSION_OVERLOADED;
        else {
            S->Bytes+=N;uint64 V=++S->Revision;G->Revision=V;
            for(size_t I=0;I<Count;++I){size_t Index=G->Order[I].Entry;xmqttintententry* E;unsigned Before=Index==XMQTT_INTENT_NONE?0:xmqttIntentMetrics(S,S->Entries+Index);
                if(Index==XMQTT_INTENT_NONE){Index=S->Free;E=S->Entries+Index;S->Free=E->Next;memset(E,0,sizeof(*E));
                    size_t Bucket=G->Order[I].Hash&(S->BucketCount-1);E->HashNext=S->Buckets[Bucket];S->Buckets[Bucket]=Index;E->Hash=G->Order[I].Hash;++S->Count;}
                else {E=S->Entries+Index;xmqttIntentGroupDrop(S,E->Group);}
                E->Group=G;++G->Refs;E->Topic=I;E->Revision=V;E->Desired=Type==XMQTT_PACKET_SUBSCRIBE;E->Status=XMQTT_STATUS_OK;E->Reason=0;xmqttIntentQueue(S,Index);xmqttIntentAccount(S,Before,E);
            }
            xmqttIntentGroupDrop(S,G);G=NULL;S->Announced=false;
            if(S->Ready && isinf(S->Deadline))S->Deadline=xmqttReconnectDeadline(xrtTimer(),S->Config.OperationTimeout);
            *Revision=V;Admission=XMQTT_ADMISSION_ACCEPTED;if(S->Admitted!=UINT64_MAX)++S->Admitted;
        }
    }
    if(Admission==XMQTT_ADMISSION_OVERLOADED && S->Overloaded!=UINT64_MAX)++S->Overloaded;
    if(G){xrtSecureZero(G,N);xrtFree(G);G=NULL;}
    S->Reserved-=N;
    bool Posted=Admission!=XMQTT_ADMISSION_ACCEPTED || !Wake || xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
    if(!Posted)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    return Admission;
}
XMQTT_API xmqttclientadmission xmqttReconnectSubscribeIntent(xmqttreconnect* R,const xmqttpacket* P,uint64* V)
{return xmqttSubscriptionsSubmit(R,P,V,XMQTT_PACKET_SUBSCRIBE,true);}
XMQTT_API xmqttclientadmission xmqttReconnectUnsubscribeIntent(xmqttreconnect* R,const xmqttpacket* P,uint64* V)
{return xmqttSubscriptionsSubmit(R,P,V,XMQTT_PACKET_UNSUBSCRIBE,true);}
static void xmqttSubscriptionsInfoLocked(xmqttreconnect* R,xmqttsubscriptionsinfo* Out)
{
    xmqttsubscriptions* S=R->Subscriptions;xmqttsubscriptionsinfo I={0};I.Size=sizeof(I);I.Filters=S->Count;I.Bytes=S->Bytes;I.Reserved=S->Reserved;I.Watchers=S->Watchers;
    I.Revision=S->Revision;I.SettledRevision=S->SettledRevision;I.Generation=S->Generation;I.Requests=S->Requests;I.Recoveries=S->Recoveries;I.Admitted=S->Admitted;I.Overloaded=S->Overloaded;I.AdmissionRetries=S->AdmissionRetries;I.Connected=S->Ready && !R->Closed && !R->Stop;I.Status=S->Status;
    I.Desired=S->Desired;I.Pending=S->Pending;I.Rejected=S->Rejected;
    I.Synchronized=I.Connected && !I.Pending && !S->Future;*Out=I;
}
XMQTT_API bool xmqttReconnectSubscriptionsInfo(xmqttreconnect* R,xmqttsubscriptionsinfo* Out)
{
    if(!R || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttReconnectError(XERR_ARGUMENT,"invalid subscription info");
    xrtMutexLock(&R->Gate);if(!R->Subscriptions){xrtMutexUnlock(&R->Gate);return xmqttReconnectError(XERR_STATE,"controller has no subscription registry");}
    xmqttSubscriptionsInfoLocked(R,Out);xrtMutexUnlock(&R->Gate);return true;
}
XMQTT_API bool xmqttReconnectSubscriptionInfo(xmqttreconnect* R,xstrview Filter,xmqttsubscriptionintentinfo* Out)
{
    if(!R || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || !Filter.Size || !Filter.Data || !xrtMemRangeValid(Filter.Data,Filter.Size))return xmqttReconnectError(XERR_ARGUMENT,"invalid filter query");
    uint64 Hash=xmqttIntentHash(Filter);xrtMutexLock(&R->Gate);xmqttsubscriptions* S=R->Subscriptions;
    if(!S){xrtMutexUnlock(&R->Gate);return xmqttReconnectError(XERR_STATE,"controller has no subscription registry");}
    size_t N=xmqttIntentFind(S,Filter,Hash);if(N==XMQTT_INTENT_NONE){xrtMutexUnlock(&R->Gate);return false;}
    xmqttintententry* E=S->Entries+N;xmqttsubscriptionintentinfo I={0};I.Size=sizeof(I);I.Revision=E->Revision;I.AppliedRevision=E->Applied;I.Generation=E->Generation;
    I.Desired=E->Desired;I.Pending=E->Dirty || E->Flight || E->Attempted!=E->Revision || E->Generation!=S->Generation;
    I.Confirmed=S->Ready && !R->Closed && !R->Stop && E->Applied==E->Revision && E->Generation==S->Generation;
    I.KnownPresent=E->KnownPresent;I.Options=E->Group->Packet.Body.Topics.Data[E->Topic].Options;I.Reason=E->Reason;I.Status=E->Status;*Out=I;xrtMutexUnlock(&R->Gate);return true;
}
static bool xmqttSubscriptionsBegin(xmqttreconnect* R,xfuture* Ready)
{
    const xmqttreadyresult* Result=xrtFutureValue(Ready);if(!Result || Result->Status!=XMQTT_STATUS_OK)return false;
    xmqttsubscriptions* S=R->Subscriptions;bool Previous=S->Generation!=0;S->ReadyFuture=Ready;S->Peer=Result->Negotiated;S->Ready=true;S->Generation=R->Info.Generation;S->Deadline=INFINITY;S->Announced=false;S->Status=XMQTT_STATUS_OK;
    S->Desired=S->Pending=S->Rejected=0;
    if(Previous && S->Recoveries!=UINT64_MAX)++S->Recoveries;
    for(size_t I=0;I<S->Config.MaxFilters;++I){xmqttintententry* E=S->Entries+I;if(!E->Group)continue;
        if(!S->Peer.SessionPresent){E->KnownPresent=false;E->Applied=E->Attempted=E->Generation=0;if(!E->Desired && !E->Flight){xmqttIntentErase(S,I);continue;}}
        if(E->Applied==E->Revision && E->KnownPresent==E->Desired){E->Attempted=E->Revision;E->Generation=S->Generation;xmqttIntentUnqueue(S,I);}
        else xmqttIntentQueue(S,I);
        xmqttIntentAccount(S,0,E);
    }return true;
}
static void xmqttSubscriptionsFreeWatch(xmqttintentwatch* W)
{
    xmqttreconnect* R=W->Controller;xrtFree(W);xrtMutexLock(&R->Gate);--R->Subscriptions->Watchers;R->Subscriptions->Bytes-=sizeof(*W);xrtMutexUnlock(&R->Gate);
}
static void xmqttSubscriptionsWatchNotify(ptr Data){(void)Data;}
/* Future copies its callback fields before Release. Each registration owns a
 * distinct heap node; it is never reused before that release. The real Root ref
 * includes its wake and retirement broadcast tail. No Client Future is wrapped. */
static void xmqttSubscriptionsWatchRelease(ptr Data)
{
    xmqttintentwatch* W=Data;xmqttreconnect* R=W->Controller;xmqttSubscriptionsFreeWatch(W);
    xrtMutexLock(&R->Gate);bool OK=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);
    if(!OK)xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
    xmqttReconnectDrop(R);
}
static void xmqttSubscriptionsUnflight(xmqttsubscriptions* S,bool Retry)
{
    for(size_t I=0;I<S->FlightCount;++I){size_t N=S->FlightEntries[I];unsigned Before=xmqttIntentMetrics(S,S->Entries+N);S->Entries[N].Flight=false;if(Retry)xmqttIntentQueue(S,N);xmqttIntentAccount(S,Before,S->Entries+N);}
    xmqttIntentGroupDrop(S,S->FlightGroup);S->FlightGroup=NULL;S->FlightCount=0;
}
static xmqttclientstatus xmqttSubscriptionsCollect(xmqttreconnect* R,xfuture** Finished,xmqttintentgroup** Completed,size_t* CompletedCount)
{
    xmqttsubscriptions* S=R->Subscriptions;*Finished=NULL;*Completed=NULL;*CompletedCount=0;
    if(!S->Future || !xrtFutureDone(S->Future))return XMQTT_STATUS_OK;
    xfuture* F=S->Future;const xmqttsubscriptionresult* Result=xrtFutureState(F)==XFUTURE_RESOLVED?xrtFutureValue(F):NULL;
    xmqttclientstatus Failure=XMQTT_STATUS_OK;
    bool Ack=Result && Result->Acknowledged && (Result->Status==XMQTT_STATUS_OK || Result->Status==XMQTT_STATUS_REJECTED);
    bool Remove=S->FlightGroup->Packet.Type==XMQTT_PACKET_UNSUBSCRIBE;
    const xmqttpacket* Packet=Ack && Result->Ack?xmqttMessagePacket(Result->Ack):NULL;
    bool Empty=Remove && R->Config.Client.Version==XMQTT_VERSION_311;
    if(Ack && !Empty && (!Packet || Packet->Body.Reasons.Size!=S->FlightCount))Ack=false;
    for(size_t I=0;I<S->FlightCount;++I){size_t N=S->FlightEntries[I];xmqttintententry* E=S->Entries+N;unsigned Before=xmqttIntentMetrics(S,E);E->Flight=false;
        if(Ack){uint8 Reason=Empty?0:Packet->Body.Reasons.Data[I];bool Accepted=Reason<128;if(Accepted)E->KnownPresent=!Remove;
            if(E->Revision==E->FlightRevision){E->Attempted=E->Revision;E->Generation=S->Generation;E->Reason=Reason;E->Status=Accepted?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED;if(Accepted)E->Applied=E->Revision;
                if(!Accepted && S->Config.FailOnRejected)Failure=XMQTT_STATUS_REJECTED;
                if(Accepted && Remove){xmqttIntentErase(S,N);xmqttIntentAccount(S,Before,E);continue;}
            }
            if(E->Attempted!=E->Revision)xmqttIntentQueue(S,N);
        }else {xmqttIntentQueue(S,N);if(E->Revision==E->FlightRevision){E->Status=Result?Result->Status:XMQTT_STATUS_INITIALIZATION;E->Reason=0;}}
        xmqttIntentAccount(S,Before,E);
    }
    if(!Ack && !R->Closed)Failure=Result && Result->Status!=XMQTT_STATUS_OK?Result->Status:XMQTT_STATUS_INITIALIZATION;
    *Completed=S->FlightGroup;*CompletedCount=S->FlightCount;
    S->FlightGroup=NULL;S->FlightCount=0;S->Future=NULL;*Finished=F;return Failure;
}
static void xmqttSubscriptionsDrive(xmqttreconnect* R,xmqttclient* C,double Now,double* At)
{
    xfuture *Finished=NULL,*Ready=NULL;xmqttclientstatus Failure=XMQTT_STATUS_OK;bool Notify=false;
    xrtMutexLock(&R->Gate);xmqttsubscriptions* S=R->Subscriptions;
    xmqttintentgroup* Completed=NULL;size_t CompletedCount=0;
    Failure=xmqttSubscriptionsCollect(R,&Finished,&Completed,&CompletedCount);
    if(Completed){
        xrtMutexUnlock(&R->Gate);
        if(S->Config.Notify){
            const xmqttsubscriptionresult* Result=xrtFutureState(Finished)==XFUTURE_RESOLVED?xrtFutureValue(Finished):NULL;
            xmqttsubscriptionresult Fallback={0};Fallback.Status=XMQTT_STATUS_INITIALIZATION;
            xmqttpacket Request=Completed->Packet;Request.Wire=(xbytesview){NULL,0};Request.Identifier=Result?Result->Identifier:0;Request.Body.Topics=(xmqttsubscriptionlist){S->FlightTopics,CompletedCount};
            S->Config.Notify(R,Completed->Revision,&Request,Result?Result:&Fallback,S->Config.User);
        }
        xrtMutexLock(&R->Gate);xmqttIntentGroupDrop(S,Completed);Now=xrtTimer();
    }
    if(!S->Ready && S->ReadyFuture){Ready=S->ReadyFuture;S->ReadyFuture=NULL;memset(&S->Peer,0,sizeof(S->Peer));}
    bool Active=!R->Stop && !R->Closed && S->Ready && C;
    if(Active && (Now>=S->Deadline || Now>=R->Info.RecoveryDeadline))Failure=XMQTT_STATUS_TIMEOUT;
    if(Failure!=XMQTT_STATUS_OK){S->Status=Failure;Active=false;}
    bool Send=Active && !S->Future && S->Head!=XMQTT_INTENT_NONE && S->Watchers<2;
    xmqttpacket Packet={0};xmqttintentwatch* Watch=NULL;
    if(Send){
        size_t N=S->Head;xmqttintentgroup* G=S->Entries[N].Group;Packet=G->Packet;Packet.Identifier=1;Packet.Body.Topics.Data=S->FlightTopics;Packet.Body.Topics.Count=0;
        uint32 Remaining=G->RemainingBase;
        while(N!=XMQTT_INTENT_NONE && Packet.Body.Topics.Count<S->Config.MaxBatch && S->Entries[N].Group==G){
            xmqttintententry* E=S->Entries+N;size_t Count=Packet.Body.Topics.Count;S->FlightTopics[Count]=G->Packet.Body.Topics.Data[E->Topic];Packet.Body.Topics.Count=Count+1;
            if(!xmqttIntentFits(S,G,E->Topic,&Remaining)){Packet.Body.Topics.Count=Count;
                if(!Count){unsigned Before=xmqttIntentMetrics(S,E);xmqttIntentUnqueue(S,N);E->Attempted=E->Revision;E->Generation=S->Generation;E->Status=XMQTT_STATUS_RESTRICTED;E->Reason=0;
                    xmqttIntentAccount(S,Before,E);if(S->Config.FailOnRejected)Failure=XMQTT_STATUS_RESTRICTED;
                }break;
            }
            S->FlightEntries[Count]=N;N=E->Next;
        }
        S->FlightCount=Packet.Body.Topics.Count;
        if(S->FlightCount){S->FlightGroup=G;++G->Refs;
            for(size_t I=0;I<S->FlightCount;++I){size_t Index=S->FlightEntries[I];xmqttintententry* E=S->Entries+Index;xmqttIntentUnqueue(S,Index);E->Flight=true;E->FlightRevision=E->Revision;E->FlightDesired=E->Desired;}
            ++S->Watchers;S->Bytes+=sizeof(xmqttintentwatch);
        }else Send=false;
    }
    if(Active && !S->Future && S->Head==XMQTT_INTENT_NONE && !S->FlightCount && Failure==XMQTT_STATUS_OK){
        xmqttsubscriptionsinfo I;xmqttSubscriptionsInfoLocked(R,&I);S->Status=I.Rejected?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK;S->SettledRevision=S->Revision;S->Deadline=INFINITY;
        if(!S->Announced){S->Announced=true;Notify=true;}
        if(R->Info.State!=XMQTT_RECONNECT_ONLINE)R->OnlineAt=Now;
        R->Info.RecoveryDeadline=INFINITY;R->Info.LastStatus=XMQTT_STATUS_OK;*At=INFINITY;
        Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_ONLINE) || Notify;
    }
    if(Active && !isinf(S->Deadline) && S->Deadline<*At)*At=S->Deadline;
    bool Pending=Active && !S->Future && (S->Head!=XMQTT_INTENT_NONE || S->FlightCount);
    xrtMutexUnlock(&R->Gate);xrtFutureDestroy(Finished);xrtFutureDestroy(Ready);
    if(Failure!=XMQTT_STATUS_OK){xmqttSubscriptionsFail(R,Failure);return;}
    if(Notify)xmqttReconnectNotify(R);
    if(Send){
        Watch=xrtMalloc(sizeof(*Watch));
        if(!Watch){xrtMutexLock(&R->Gate);--S->Watchers;S->Bytes-=sizeof(*Watch);xmqttSubscriptionsUnflight(S,true);xrtMutexUnlock(&R->Gate);xmqttSubscriptionsFail(R,XMQTT_STATUS_MEMORY);return;}
        Watch->Controller=R;Packet.Identifier=0;xfuture* F=NULL;
        xmqttclientadmission A=Packet.Type==XMQTT_PACKET_SUBSCRIBE?xmqttClientSubscribeAsync(C,&Packet,S->Config.OperationTimeout,&F):xmqttClientUnsubscribeAsync(C,&Packet,S->Config.OperationTimeout,&F);
        if(A==XMQTT_ADMISSION_ACCEPTED){
            xrtMutexLock(&R->Gate);S->Future=F;if(S->Requests!=UINT64_MAX)++S->Requests;xrtMutexUnlock(&R->Gate);
            xmqttReconnectRef(R);
            bool Init=xrtFutureWatchInit(&Watch->Watch,xmqttSubscriptionsWatchNotify,xmqttSubscriptionsWatchRelease,Watch);
            xfuturewatchresult Added=Init?xrtFutureWatchAdd(F,&Watch->Watch):XFUTURE_WATCH_ERROR;
            if(Added!=XFUTURE_WATCH_PENDING)xmqttSubscriptionsWatchRelease(Watch);
            if(Added==XFUTURE_WATCH_ERROR)xmqttSubscriptionsFail(R,XMQTT_STATUS_INITIALIZATION);
            Pending=false;
        }else {
            xmqttSubscriptionsFreeWatch(Watch);xrtMutexLock(&R->Gate);xmqttSubscriptionsUnflight(S,true);xrtMutexUnlock(&R->Gate);
            if(A==XMQTT_ADMISSION_ERROR)xmqttSubscriptionsFail(R,xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION);
            Pending=A==XMQTT_ADMISSION_OVERLOADED;
            if(Pending){xrtMutexLock(&R->Gate);if(S->AdmissionRetries!=UINT64_MAX)++S->AdmissionRetries;xrtMutexUnlock(&R->Gate);}
        }
    }
    if(Pending){double Poll=xmqttReconnectDeadline(Now,S->Config.RetryDelay);if(Poll<*At)*At=Poll;}
}
#undef XMQTT_INTENT_NONE
#endif

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
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    xfuture* Ready=!Closed && R->Subscriptions?xmqttClientReady(C):NULL;
#endif
    xrtMutexLock(&R->Gate);
    if(R->Client!=C){xrtMutexUnlock(&R->Gate);return;}
    if(!Closed){
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
        if(R->Subscriptions){
            ++R->Info.Connections;
            if(!Ready || !xmqttSubscriptionsBegin(R,Ready)){R->Stop=R->Failed=true;R->Info.LastStatus=XMQTT_STATUS_INITIALIZATION;xrtFutureDestroy(Ready);}
        }else
#endif
        {
        ++R->Info.Connections;R->OnlineAt=Now;R->Info.RecoveryDeadline=INFINITY;
        if(!R->Stop){R->Info.LastStatus=Status;Notify=xmqttReconnectStateLocked(R,XMQTT_RECONNECT_ONLINE);}
        }
    }else if(!R->Closed){
        R->Closed=true;R->Info.ServerReason=Reason;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
        if(R->Subscriptions){R->Subscriptions->Ready=false;R->Subscriptions->Deadline=INFINITY;}
#endif
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
        size_t* Capacity=NULL;
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
        if(R->Subscriptions)Capacity=&R->Subscriptions->MaxOutgoingBytes;
#endif
        if(!xmqttInternalClientLifecycle(C,R->Worker,R->Session,R->Inbox,&R->Lifecycle,Capacity))xmqttReconnectFail(R,XMQTT_STATUS_INITIALIZATION,true);
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
    #if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    if(R->Subscriptions){
        xmqttSubscriptionsDrive(R,C,Now,&At);
        xrtMutexLock(&R->Gate);Stop=R->Stop;Closed=R->Closed;xrtMutexUnlock(&R->Gate);
    }
    #endif
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
static bool xmqttReconnectCreateImpl(xnetengine* Engine,xtaskpool* Pool,uint64 Affinity,const xmqttreconnectconfig* Config,const xmqttpacket* Connect,
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    const xmqttsubscriptionsconfig* Subscriptions,const xmqttpacket* Initial,size_t InitialCount,
#endif
    xmqttreconnect** Out)
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
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    if(Subscriptions){
        if(InitialCount && (!Initial || InitialCount>65535 || !xrtMemRangeValid(Initial,InitialCount*sizeof(*Initial)))){xmqttReconnectError(XERR_ARGUMENT,"invalid initial subscription vector");goto Failed;}
        if(!xmqttSubscriptionsCreate(R,Subscriptions))goto Failed;
        for(size_t J=0;J<InitialCount;++J){uint64 Revision;
            xmqttclientadmission A=xmqttSubscriptionsSubmit(R,Initial+J,&Revision,XMQTT_PACKET_SUBSCRIBE,false);
            if(A!=XMQTT_ADMISSION_ACCEPTED){if(A==XMQTT_ADMISSION_OVERLOADED)xmqttReconnectError(XERR_RANGE,"initial subscription storage limit");goto Failed;}
        }
    }else if(InitialCount){xmqttReconnectError(XERR_ARGUMENT,"initial subscriptions require a registry");goto Failed;}
#endif
    xrtMutexLock(&R->Gate);bool Posted=xmqttReconnectWakeLocked(R);xrtMutexUnlock(&R->Gate);if(!Posted)goto Failed;
    *Out=R;return true;
Failed:
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    xmqttSubscriptionsDestroy(R);
#endif
    xmqttClientInboxDestroy(R->Inbox);if(R->Session)xmqttSessionDestroy(R->Session);
    if(Cond)xrtCondUnit(&R->RetirementReady);
    if(Retirement)xrtMutexUnit(&R->RetirementLock);
    if(Gate)xrtMutexUnit(&R->Gate);
    xrtSecureZero(R,Allocation);xrtFree(R);xrtNetEngineUnpin(Engine);return false;
}
XMQTT_API bool xmqttReconnectCreate(xnetengine* E,xtaskpool* P,uint64 A,const xmqttreconnectconfig* K,const xmqttpacket* C,xmqttreconnect** Out)
{
    return xmqttReconnectCreateImpl(E,P,A,K,C,
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
        NULL,NULL,0,
#endif
        Out);
}
#if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
XMQTT_API bool xmqttReconnectCreateSubscribed(xnetengine* E,xtaskpool* P,uint64 A,const xmqttreconnectconfig* K,const xmqttsubscriptionsconfig* S,const xmqttpacket* C,const xmqttpacket* Initial,size_t Count,xmqttreconnect** Out)
{
    if(!S)return xmqttReconnectError(XERR_ARGUMENT,"subscription config is NULL");
    return xmqttReconnectCreateImpl(E,P,A,K,C,S,Initial,Count,Out);
}
#endif
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
    #if defined(XMQTT_FEATURE_XMQTT_RECONNECT_SUBSCRIPTIONS)
    xmqttSubscriptionsDestroy(R);
    #endif
    xnetengine* Engine=R->Engine;size_t Allocation=R->Allocation;
    xrtCondUnit(&R->RetirementReady);xrtMutexUnit(&R->RetirementLock);xrtMutexUnit(&R->Gate);xrtSecureZero(R,Allocation);xrtFree(R);return xrtNetEngineUnpin(Engine);
}
#endif
