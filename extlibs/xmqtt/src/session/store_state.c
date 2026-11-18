#include <xmqtt/store_state.h>
#include "../internal/xmqtt_store_compact_online.h"
#include <xmqtt/store_compact.h>
#include "../internal/xmqtt_store_packet.h"
#include <xrt/memory.h>
#include "../internal/xmqtt_journal_replay.h"
#include "../internal/xmqtt_internal.h"
#include "../internal/xmqtt_store_session_source.h"
#include "../internal/xmqtt_store_write.h"
#include "../internal/xmqtt_client_store.h"
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
typedef struct xmqttstoreentry {
    struct xmqttstoreentry *Next,*Previous,*KeyNext,*IdNext;
    xmqttstoreview View;size_t Allocation;unsigned Receive;bool Out;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    struct xmqttstoreentry* ReadyNext;
    uint64 Lease;
#endif
} xmqttstoreentry;
struct xmqttstorestate {
    xmqttstorestateconfig Config;xmqttstorestateinfo Info;
    xmqttstoreentry *Head,*Tail,**Keys,**Ids,**Views[3];size_t Buckets;
    uint32 Expiry;uint64 ClosedWall;bool Opened,Live;
    uintptr_t Lowest,Highest;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    bool ProjectionDirty;
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    const void* WriteOwner;bool WriteEstablished;xmqttstoreplan *Plan,*Retiring;uint32 JournalChain;uint64 JournalEnd;xstrview ExpectedId;
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    xmqttstoreentry *InboxReadyHead,*InboxReadyTail;
    bool InboxEnabled;
#endif
};
static bool xmqttSSpan(const void* P,size_t N){return (P||!N)&&xrtMemRangeValid(P,N);}
static bool xmqttSSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N||!M||!xrtMemRangesOverlap(A,N,B,M);}
static xmqttstorestatus xmqttSError(xmqttstorestatus S,cstr Message)
{xrtSetErrorInfo(S==XMQTT_STORE_CORRUPT?XERR_PROTOCOL:S==XMQTT_STORE_STATE_ERROR?XERR_STATE:XERR_ARGUMENT,"xrt.mqtt.store",6400-(int32)S,Message);return S;}
XMQTT_API bool xmqttStoreStateConfigInit(xmqttstorestateconfig* C)
{
    if(!xmqttSSpan(C,sizeof(*C))){xmqttSError(XMQTT_STORE_ERROR,"invalid recovery config output");return false;}
    *C=(xmqttstorestateconfig){0};C->Size=sizeof(*C);C->MaxOutgoing=128;C->MaxIncoming=64;C->MaxInbox=1024;C->MaxMemoryBytes=16u*1024u*1024u;
    C->MaxTransactions=1000000;C->MaxOperations=8000000;C->MaxReplayBytes=UINT64_C(1073741824);return xmqttStoreDeltaConfigInit(&C->Delta);
}
static size_t xmqttSKeyBucket(const xmqttstorestate* S,uint64 Key)
{Key^=Key>>33;Key*=UINT64_C(0xff51afd7ed558ccd);Key^=Key>>33;return (size_t)Key&(S->Buckets-1u);}
static size_t xmqttSIdBucket(const xmqttstorestate* S,uint16 Id,bool Out)
{return ((size_t)Id*2654435761u+(Out?1u:0u))&(S->Buckets-1u);}
static xmqttstoreentry* xmqttSFindBase(const xmqttstorestate* S,uint64 Key)
{if(S->Buckets)for(xmqttstoreentry* E=S->Keys[xmqttSKeyBucket(S,Key)];E;E=E->KeyNext)if(E->View.Key==Key)return E;return NULL;}
static const xmqttstoreentry* xmqttSIdFindBase(const xmqttstorestate* S,uint16 Id,bool Out)
{
    if(S->Buckets)
        for(const xmqttstoreentry* E=S->Ids[xmqttSIdBucket(S,Id,Out)];E;E=E->IdNext)
            if(E->View.Identifier==Id&&E->Out==Out)return E;
    return NULL;
}
static bool xmqttSIdUsedBase(const xmqttstorestate* S,uint16 Id,bool Out)
{return xmqttSIdFindBase(S,Id,Out)!=NULL;}
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
typedef struct xmqttstorechange {
    xmqttstoreentry Value;struct xmqttstorechange *KeyNext,*IdNext;
    xmqttstoreentry *Base,*Fresh;bool Alive;
} xmqttstorechange;
struct xmqttstoreplan {
    xmqttstorestate Shadow;size_t Allocation,PhysicalBytes,Count,Capacity,Buckets,Work,MaxWork;bool Exhausted;
    xmqttstorechange *Nodes,**Keys,**Ids;xmqttstoreentry* Retired;size_t Refund;bool Published;
};
static bool xmqttSPlanSize(size_t Cap,size_t* BucketCount,size_t* Bytes)
{
    size_t B=Cap?8:0;if(Cap>SIZE_MAX/2u)return false;
    while(B<Cap*2u){if(B>SIZE_MAX/2u)return false;B*=2u;}
    if(Cap>(SIZE_MAX-sizeof(xmqttstoreplan))/sizeof(xmqttstorechange))return false;
    size_t N=sizeof(xmqttstoreplan)+Cap*sizeof(xmqttstorechange);if(B>(SIZE_MAX-N)/(2u*sizeof(xmqttstorechange*)))return false;
    *BucketCount=B;*Bytes=N+B*2u*sizeof(xmqttstorechange*);return true;
}
static size_t xmqttSPlanKey(const xmqttstoreplan* P,uint64 Key)
{Key^=Key>>33;Key*=UINT64_C(0xff51afd7ed558ccd);Key^=Key>>33;return (size_t)Key&(P->Buckets-1u);}
static size_t xmqttSPlanId(const xmqttstoreplan* P,uint16 Id,bool Out)
{return ((size_t)Id*2654435761u+(Out?1u:0u))&(P->Buckets-1u);}
static xmqttstorechange* xmqttSPlanNode(const xmqttstoreplan* P,uint64 Key)
{if(P->Buckets)for(xmqttstorechange* N=P->Keys[xmqttSPlanKey(P,Key)];N;N=N->KeyNext)if(N->Value.View.Key==Key)return N;return NULL;}
static void xmqttSPlanLinkId(xmqttstoreplan* P,xmqttstorechange* N)
{if(N->Value.Out||N->Value.Receive){size_t B=xmqttSPlanId(P,N->Value.View.Identifier,N->Value.Out);N->IdNext=P->Ids[B];P->Ids[B]=N;}}
static void xmqttSPlanUnlinkId(xmqttstoreplan* P,xmqttstoreentry* E)
{xmqttstorechange* N=(xmqttstorechange*)E;xmqttstorechange** At=&P->Ids[xmqttSPlanId(P,E->View.Identifier,E->Out)];while(*At!=N)At=&(*At)->IdNext;*At=N->IdNext;N->IdNext=NULL;}
static xmqttstorechange* xmqttSPlanTouch(xmqttstoreplan* P,xmqttstoreentry* E,bool Fresh)
{
    if(P->Count==P->Capacity)abort(); /* batch-derived bound is established before allocation */
    xmqttstorechange* N=P->Nodes+P->Count++;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    /* Only the publisher under the Writer gate touches live lease metadata.
     * Staging outside that gate copies only the immutable model prefix. */
    memcpy(&N->Value,E,offsetof(xmqttstoreentry,ReadyNext));
#else
    N->Value=*E;
#endif
    N->Base=Fresh?NULL:E;N->Fresh=Fresh?E:NULL;N->Alive=true;
    size_t B=xmqttSPlanKey(P,E->View.Key);N->KeyNext=P->Keys[B];P->Keys[B]=N;xmqttSPlanLinkId(P,N);return N;
}
static xmqttstoreentry* xmqttSPlanFind(xmqttstorestate* S,uint64 Key)
{
    xmqttstoreplan* P=S->Plan;xmqttstorechange* N=xmqttSPlanNode(P,Key);
    if(!N){xmqttstoreentry* E=xmqttSFindBase(S,Key);if(!E)return NULL;N=xmqttSPlanTouch(P,E,false);}
    return N->Alive?&N->Value:NULL;
}
static bool xmqttSPlanIdUsed(const xmqttstorestate* S,uint16 Id,bool Out)
{
    const xmqttstoreplan* P=S->Plan;
    if(P->Buckets)for(xmqttstorechange* N=P->Ids[xmqttSPlanId(P,Id,Out)];N;N=N->IdNext)if(N->Alive&&N->Value.View.Identifier==Id&&N->Value.Out==Out)return true;
    if(S->Buckets)for(xmqttstoreentry* E=S->Ids[xmqttSIdBucket(S,Id,Out)];E;E=E->IdNext)if(E->View.Identifier==Id&&E->Out==Out&&!xmqttSPlanNode(P,E->View.Key))return true;
    return false;
}
#endif
static xmqttstoreentry* xmqttSFind(xmqttstorestate* S,uint64 Key)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan)return xmqttSPlanFind(S,Key);
#endif
    return xmqttSFindBase(S,Key);
}
static bool xmqttSIdUsed(const xmqttstorestate* S,uint16 Id,bool Out)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan)return xmqttSPlanIdUsed(S,Id,Out);
#endif
    return xmqttSIdUsedBase(S,Id,Out);
}
static size_t xmqttSAvailable(const xmqttstorestate* S)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan)return S->Config.MaxMemoryBytes-S->Plan->PhysicalBytes;
#endif
    return S->Config.MaxMemoryBytes-S->Info.MemoryBytes;
}
static void xmqttSUnlinkId(xmqttstorestate* S,xmqttstoreentry* E)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan)xmqttSPlanUnlinkId(S->Plan,E);else
#endif
    {xmqttstoreentry** P=&S->Ids[xmqttSIdBucket(S,E->View.Identifier,E->Out)];while(*P!=E)P=&(*P)->IdNext;*P=E->IdNext;E->IdNext=NULL;}
    if(E->Out)--S->Info.Outgoing;else {--S->Info.Incoming;E->Receive=0;}
}
static void xmqttSDelete(xmqttstorestate* S,xmqttstoreentry* E)
{
    if(E->Out||E->Receive)xmqttSUnlinkId(S,E);
    if(E->View.InboxPending)--S->Info.Inbox;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan){((xmqttstorechange*)E)->Alive=false;S->Info.MemoryBytes-=E->Allocation;return;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    if(S->InboxEnabled&&E->View.InboxPending&&!(E->Lease&XMQTT_INBOX_ACKING))abort();
#endif
    xmqttstoreentry** P=&S->Keys[xmqttSKeyBucket(S,E->View.Key)];while(*P!=E)P=&(*P)->KeyNext;*P=E->KeyNext;
    if(E->Previous)E->Previous->Next=E->Next;else S->Head=E->Next;
    if(E->Next)E->Next->Previous=E->Previous;else S->Tail=E->Previous;
    S->Info.MemoryBytes-=E->Allocation;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Retiring){E->Next=S->Retiring->Retired;S->Retiring->Retired=E;return;}
#endif
    xrtFree(E);
}
static void xmqttSClear(xmqttstorestate* S)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    S->ProjectionDirty=false;
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan){
        for(xmqttstoreentry* E=S->Head;E;E=E->Next){if(S->Plan->Work==S->Plan->MaxWork){S->Plan->Exhausted=true;return;}++S->Plan->Work;if(!xmqttSPlanNode(S->Plan,E->View.Key))xmqttSPlanTouch(S->Plan,E,false);}
        for(size_t I=0;I<S->Plan->Count;++I){if(S->Plan->Work==S->Plan->MaxWork){S->Plan->Exhausted=true;return;}++S->Plan->Work;xmqttstorechange* N=S->Plan->Nodes+I;if(!N->Alive)continue;xmqttstoreentry* E=&N->Value;
            if(E->Out)xmqttSDelete(S,E);else {if(E->Receive)xmqttSUnlinkId(S,E);if(!E->View.InboxPending)xmqttSDelete(S,E);}}
        S->Info.HasSession=false;return;
    }
#endif
    for(xmqttstoreentry* E=S->Head,*Next;E;E=Next){Next=E->Next;if(E->Out)xmqttSDelete(S,E);
        else {if(E->Receive)xmqttSUnlinkId(S,E);if(!E->View.InboxPending)xmqttSDelete(S,E);}}
    S->Info.HasSession=false;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan)S->WriteEstablished=false;
#endif
}
static void xmqttSPeak(xmqttstorestate* S)
{if(S->Info.MemoryBytes>S->Info.PeakMemoryBytes)S->Info.PeakMemoryBytes=S->Info.MemoryBytes;}
static bool xmqttSExpired(const xmqttstorestate* S,uint64 Wall)
{return S->Info.HasSession&&!S->Live&&S->Expiry!=UINT32_MAX&&Wall-S->ClosedWall>=(uint64)S->Expiry*1000u;}
static xmqttstorestatus xmqttSAdd(xmqttstorestate* S,xmqttstoreview View,bool Out,unsigned Receive)
{
    if(View.Key<=S->Info.LastKey)
        return xmqttSError(XMQTT_STORE_CORRUPT,"reused or reversed persistent flow key");
    if((Out&&S->Info.Outgoing==S->Config.MaxOutgoing)||
       (View.InboxPending&&S->Info.Inbox==S->Config.MaxInbox)||
       (Receive&&S->Info.Incoming==S->Config.MaxIncoming))return XMQTT_STORE_LIMIT;
    if((Out||Receive)&&xmqttSIdUsed(S,View.Identifier,Out))
        return xmqttSError(XMQTT_STORE_CORRUPT,"duplicate active MQTT identifier");
    if(View.Wire.Size>SIZE_MAX-sizeof(xmqttstoreentry)||
       sizeof(xmqttstoreentry)+View.Wire.Size>xmqttSAvailable(S))return XMQTT_STORE_LIMIT;
    size_t N=sizeof(xmqttstoreentry)+View.Wire.Size;
    xmqttstoreentry* E=xrtMalloc(N);
    if(!E)return XMQTT_STORE_ERROR;
    memset(E,0,sizeof(*E));
    E->Allocation=N;E->Out=Out;E->Receive=Receive;E->View=View;
    E->View.Wire.Data=(const unsigned char*)(E+1);
    memcpy(E+1,View.Wire.Data,View.Wire.Size);
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->Plan){S->Plan->PhysicalBytes+=N;xmqttSPlanTouch(S->Plan,E,true);
    }else
#endif
    {
        size_t B=xmqttSKeyBucket(S,View.Key);
        E->KeyNext=S->Keys[B];S->Keys[B]=E;
        if(Out||Receive){
            B=xmqttSIdBucket(S,View.Identifier,Out);
            E->IdNext=S->Ids[B];S->Ids[B]=E;
        }
        E->Previous=S->Tail;
        if(S->Tail)S->Tail->Next=E;else S->Head=E;
        S->Tail=E;
    }
    if(Out)++S->Info.Outgoing;
    if(Receive)++S->Info.Incoming;
    if(View.InboxPending)++S->Info.Inbox;
    S->Info.LastKey=View.Key;S->Info.MemoryBytes+=N;xmqttSPeak(S);
    return XMQTT_STORE_DONE;
}
static xmqttstorestatus xmqttSInsert(xmqttstorestate* S,const xmqttstoredelta* D)
{
    bool Out=D->Type==XMQTT_STORE_OUT_PUT;
    uint8 Qos=(D->Data.Data[0]>>1)&3u;
    xmqttstoreview View={.Epoch=D->Epoch,.Key=D->Key,.Identifier=D->Identifier,.Qos=Qos,
        .Attempted=(D->Flags&1u)!=0,.Sent=(D->Flags&2u)!=0,.InboxPending=!Out,.Wire=D->Data};
    return xmqttSAdd(S,View,Out,!Out&&Qos==2?2u:0u);
}
static xmqttstorestatus xmqttSApply(xmqttstorestate* S,const xmqttstoredelta* D)
{
    if(D->Type==XMQTT_STORE_BIND){
        if(S->Info.ClientId.Size)return xmqttSError(XMQTT_STORE_CORRUPT,"identity must be bound exactly once");
        if(S->Config.ClientId.Size&&(S->Config.ClientId.Size!=D->Data.Size||memcmp(S->Config.ClientId.Data,D->Data.Data,D->Data.Size)))return xmqttSError(XMQTT_STORE_STATE_ERROR,"bound ClientId differs");
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
        /* ExpectedId was already copied and charged by empty-prefix recovery.
         * Borrow it during preparation; transfer its existing ownership only
         * after COMMITTED. Abort must never free the original expectation. */
        if(S->Plan&&S->ExpectedId.Size){S->Info.ClientId=S->ExpectedId;S->Info.Epoch=D->Epoch;S->Info.LastWallMilliseconds=D->WallMilliseconds;return XMQTT_STORE_DONE;}
#endif
        if(D->Data.Size>xmqttSAvailable(S))return XMQTT_STORE_LIMIT;
        char* Id=xrtMalloc(D->Data.Size);if(!Id)return XMQTT_STORE_ERROR;memcpy(Id,D->Data.Data,D->Data.Size);
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
        if(S->Plan)S->Plan->PhysicalBytes+=D->Data.Size;
#endif
        S->Info.ClientId=(xstrview){Id,D->Data.Size};S->Info.Epoch=D->Epoch;S->Info.LastWallMilliseconds=D->WallMilliseconds;
        S->Info.MemoryBytes+=D->Data.Size;xmqttSPeak(S);return XMQTT_STORE_DONE;
    }
    if(!S->Info.ClientId.Size)return xmqttSError(XMQTT_STORE_CORRUPT,"operation precedes durable identity binding");
    bool Clock=D->Type==XMQTT_STORE_OPEN||D->Type==XMQTT_STORE_END||D->Type==XMQTT_STORE_CLEAR;
    if(Clock&&D->WallMilliseconds<S->Info.LastWallMilliseconds)return xmqttSError(XMQTT_STORE_CORRUPT,"recorded wall clock went backwards");
    if(D->Type==XMQTT_STORE_OPEN){
        bool Present=(D->Flags&2u)!=0,Clean=(D->Flags&1u)!=0;
        bool Available=S->Info.HasSession&&S->Expiry!=0&&!xmqttSExpired(S,D->WallMilliseconds);
        if(Present?(!Available||D->Epoch!=S->Info.Epoch):
           (S->Opened?(S->Info.Epoch==UINT64_MAX||D->Epoch!=S->Info.Epoch+1u):D->Epoch!=S->Info.Epoch))
            return xmqttSError(XMQTT_STORE_CORRUPT,"invalid broker-session epoch/presence transition");
        if(S->Config.Delta.Version==XMQTT_VERSION_311&&Available&&!Clean&&!Present)return xmqttSError(XMQTT_STORE_CORRUPT,"311 lost broker state requires explicit reset");
        if(!Present)xmqttSClear(S);
        S->Info.Epoch=D->Epoch;S->Info.HasSession=true;S->Opened=S->Live=true;S->Expiry=D->Expiry;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
        if(S->Plan)S->WriteEstablished=true;
#endif
    }else if(D->Type==XMQTT_STORE_INBOX_ACK){
        xmqttstoreentry* E=xmqttSFind(S,D->Key);
        if(!E||E->Out||!E->View.InboxPending||E->View.Epoch!=D->Epoch)return xmqttSError(XMQTT_STORE_CORRUPT,"stale or repeated application delivery acknowledgement");
        E->View.InboxPending=false;--S->Info.Inbox;if(!E->Receive)xmqttSDelete(S,E);
    }else {
        if(D->Epoch!=S->Info.Epoch)return xmqttSError(XMQTT_STORE_CORRUPT,"stale protocol epoch");
        if(D->Type==XMQTT_STORE_END){
            /* A graceful END asserts a validated complete DISCONNECT. Only
             * the live packet owner knows the original CONNECT expiry (the
             * OPEN record holds the effective CONNACK value). */
            if(!S->Live||(!(D->Flags&1u)&&D->Expiry!=S->Expiry)||
               (S->Config.Delta.Version==XMQTT_VERSION_311&&D->Expiry!=S->Expiry))return xmqttSError(XMQTT_STORE_CORRUPT,"invalid connection-end expiry transition");
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
            if(S->Plan)S->WriteEstablished=false;
#endif
            S->Live=false;S->Expiry=D->Expiry;S->ClosedWall=D->WallMilliseconds;if(!S->Expiry)xmqttSClear(S);
        }else if(D->Type==XMQTT_STORE_CLEAR){
            if(S->Live||(D->Reason==1&&!xmqttSExpired(S,D->WallMilliseconds)))return xmqttSError(XMQTT_STORE_CORRUPT,"invalid live or premature expiry reset");
            xmqttSClear(S);
        }else {
            if(!S->Live)return xmqttSError(XMQTT_STORE_CORRUPT,"protocol transition while offline");
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
            if(S->Plan&&!S->WriteEstablished)return xmqttSError(XMQTT_STORE_STATE_ERROR,"fresh broker OPEN is required before live flow transitions");
#endif
            if(D->Type==XMQTT_STORE_OUT_PUT||D->Type==XMQTT_STORE_IN_ACCEPT)return xmqttSInsert(S,D);
            xmqttstoreentry* E=xmqttSFind(S,D->Key);
            if(!E||E->View.Epoch!=D->Epoch||E->View.Identifier!=D->Identifier)return xmqttSError(XMQTT_STORE_CORRUPT,"stale flow key/identifier");
            if(D->Type==XMQTT_STORE_OUT_ATTEMPT){
                if(!E->Out||E->View.Pubrel)return xmqttSError(XMQTT_STORE_CORRUPT,"attempt outside outgoing PUBLISH stage");
                E->View.Attempted=true;
            }else if(D->Type==XMQTT_STORE_OUT_PUBREL){
                if(!E->Out||E->View.Qos!=2||!E->View.Attempted)return xmqttSError(XMQTT_STORE_CORRUPT,"PUBREC without an attempted QoS2 flow");
                if(!E->View.Pubrel)E->View.PubrecReason=D->Reason;
                E->View.Pubrel=E->View.Sent=true;
            }else if(D->Type==XMQTT_STORE_OUT_COMPLETE){
                if(!E->Out||!E->View.Attempted||(E->View.Pubrel?D->Flags!=XMQTT_PACKET_PUBCOMP:
                    (E->View.Qos==1?D->Flags!=XMQTT_PACKET_PUBACK:D->Flags!=XMQTT_PACKET_PUBREC)))return xmqttSError(XMQTT_STORE_CORRUPT,"ACK type disagrees with outgoing QoS/stage");
                xmqttSDelete(S,E);
            }else if(D->Type==XMQTT_STORE_IN_PUBREL){
                if(E->Out||!E->Receive)return xmqttSError(XMQTT_STORE_CORRUPT,"PUBREL without active receive identity");
                E->Receive=3;E->View.Pubrel=true;
            }else if(D->Type==XMQTT_STORE_IN_COMPLETE){
                if(E->Out||E->Receive!=3)return xmqttSError(XMQTT_STORE_CORRUPT,"PUBCOMP completion before received PUBREL");
                xmqttSUnlinkId(S,E);if(!E->View.InboxPending)xmqttSDelete(S,E);
            }
        }
    }
    if(Clock)S->Info.LastWallMilliseconds=D->WallMilliseconds;
    return XMQTT_STORE_DONE;
}
XMQTT_API void xmqttStoreStateDestroy(xmqttstorestate* S)
{
    if(!S)return;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->WriteOwner){xmqttSError(XMQTT_STORE_STATE_ERROR,"State is exclusively claimed by its writer");return;}
    xrtFree((ptr)S->ExpectedId.Data);
#endif
    for(xmqttstoreentry* E=S->Head,*Next;E;E=Next){Next=E->Next;xrtFree(E);}
    xrtFree((ptr)S->Info.ClientId.Data);xrtFree(S);
}
static bool xmqttSOutput(const xmqttstorestate* S,void* P,size_t N)
{
    if(!S||!xmqttSSpan(P,N))return false;
    uintptr_t A=(uintptr_t)P;
    if(A>=S->Highest||A+N<=S->Lowest)return true;
    if(!xmqttSSeparate(P,N,S,S->Info.BaseBytes)||!xmqttSSeparate(P,N,S->Info.ClientId.Data,S->Info.ClientId.Size))return false;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(!xmqttSSeparate(P,N,S->ExpectedId.Data,S->ExpectedId.Size))return false;
#endif
    for(xmqttstoreentry* E=S->Head;E;E=E->Next)if(!xmqttSSeparate(P,N,E,E->Allocation))return false;
    return true;
}
XMQTT_API bool xmqttStoreStateInfo(const xmqttstorestate* S,xmqttstorestateinfo* I)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S&&S->WriteOwner){xmqttSError(XMQTT_STORE_STATE_ERROR,"State is exclusively claimed by its writer");return false;}
#endif
if(!xmqttSOutput(S,I,sizeof(*I))){xmqttSError(XMQTT_STORE_ERROR,"invalid recovery info output");return false;}*I=S->Info;return true;}
XMQTT_API xmqttstorestatus xmqttStoreStateView(const xmqttstorestate* S,xmqttstoreviewkind K,size_t Index,xmqttstoreview* V)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S&&S->WriteOwner)return xmqttSError(XMQTT_STORE_STATE_ERROR,"State is exclusively claimed by its writer");
#endif
    if(!xmqttSOutput(S,V,sizeof(*V))||K<XMQTT_STORE_OUTGOING||K>XMQTT_STORE_INBOX)return xmqttSError(XMQTT_STORE_ERROR,"invalid recovery view output/kind");
    size_t N=K==XMQTT_STORE_OUTGOING?S->Info.Outgoing:K==XMQTT_STORE_INCOMING?S->Info.Incoming:S->Info.Inbox;
    if(Index>=N)return XMQTT_STORE_EMPTY;
    *V=S->Views[K][Index]->View;return XMQTT_STORE_DONE;
}
static bool xmqttSConfig(const xmqttstorestateconfig* C,xmqttstorestateconfig* K)
{
    if(!xmqttSSpan(C,sizeof(*C)))return false;
    *K=*C;
    if(K->Size!=sizeof(*K)||!K->Generation||K->WallMilliseconds>INT64_MAX||!K->Binding.Size||K->Binding.Size>1024||!xmqttSSpan(K->Binding.Data,K->Binding.Size)||
       K->ClientId.Size>65535||!xmqttSSpan(K->ClientId.Data,K->ClientId.Size)||K->MaxOutgoing>65535||K->MaxIncoming>65535||
       K->Delta.Size!=sizeof(K->Delta)||(K->Delta.Version!=XMQTT_VERSION_5&&K->Delta.Version!=XMQTT_VERSION_311)||
       (K->Delta.Version==XMQTT_VERSION_311&&!K->ClientId.Size)||K->Delta.MaxOperations>UINT32_MAX||
       K->Delta.MaxOperations>SIZE_MAX/sizeof(xmqttstoredelta)||K->Delta.Codec.MaxProperties>SIZE_MAX/sizeof(xmqttproperty))return false;
    if(!xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&K->Delta.Codec,&K->Delta.Codec))return false;
    return !K->ClientId.Size||xmqttUtf8StringCheck(K->ClientId,K->Delta.Codec.Utf8Flags,NULL);
}
#if defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreSessionSource(const xmqttstorestate* S,
    xbytesview Binding,uint64 Generation,void* Output,size_t Bytes,uint64* RecoveryWall)
{
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S&&S->WriteOwner)return xmqttSError(XMQTT_STORE_STATE_ERROR,"State is exclusively claimed by its writer");
#endif
    if(!xmqttSOutput(S,Output,Bytes))return xmqttSError(XMQTT_STORE_ERROR,"import output overlaps recovered state");
    if(S->Config.Generation!=Generation||S->Config.Binding.Size!=Binding.Size||memcmp(S->Config.Binding.Data,Binding.Data,Binding.Size))
        return xmqttSError(XMQTT_STORE_STATE_ERROR,"import broker/principal/slot or storage generation differs");
    *RecoveryWall=S->Config.WallMilliseconds;return XMQTT_STORE_DONE;
}
#endif
enum {
    XMQTT_CK_HEADER=64, XMQTT_CK_CHUNK=16, XMQTT_CK_ENTRY=24,
    XMQTT_CK_SESSION=1, XMQTT_CK_OPENED=2, XMQTT_CK_LIVE=4,
    XMQTT_CK_OUT=1, XMQTT_CK_ATTEMPT=2, XMQTT_CK_SENT=4,
    XMQTT_CK_PUBREL=8, XMQTT_CK_INBOX=16, XMQTT_CK_RECEIVE=32
};
typedef struct xmqttstorecheckpoint {
    uint64 Remaining,LastKey;
} xmqttstorecheckpoint;

static uint64 xmqttSCGet(const unsigned char* P,unsigned Bytes)
{
    uint64 Value=0;
    for(unsigned I=0;I<Bytes;++I)Value=(Value<<8)|P[I];
    return Value;
}
static xmqttstorestatus xmqttSCBegin(xmqttstorestate* S,xbytesview Data,xmqttstorecheckpoint* Cut)
{
    const unsigned char* B=Data.Data;
    if(Data.Size<XMQTT_CK_HEADER||B[8]!=1||B[9]!=S->Info.Version||B[11]||xmqttSCGet(B+60,4))
        return xmqttSError(XMQTT_STORE_CORRUPT,"invalid store checkpoint descriptor");
    size_t IdBytes=(size_t)xmqttSCGet(B+12,4);
    uint64 Epoch=xmqttSCGet(B+16,8),LastKey=xmqttSCGet(B+24,8),Wall=xmqttSCGet(B+32,8);
    uint64 Closed=xmqttSCGet(B+40,8),Count=xmqttSCGet(B+48,8);
    uint32 Expiry=(uint32)xmqttSCGet(B+56,4);
    bool HasSession=(B[10]&XMQTT_CK_SESSION)!=0,Opened=(B[10]&XMQTT_CK_OPENED)!=0,Live=(B[10]&XMQTT_CK_LIVE)!=0;
    if((B[10]&~7u)||!Epoch||Wall>INT64_MAX||Closed>Wall||Count>LastKey||
       (!Opened&&(HasSession||Live||LastKey||Closed||Expiry))||(Live&&!HasSession)||
       (HasSession&&!Live&&!Expiry)||
       (S->Info.Version==XMQTT_VERSION_311&&Expiry&&Expiry!=UINT32_MAX)||
       !IdBytes||IdBytes>65535||IdBytes!=Data.Size-XMQTT_CK_HEADER||
       !xmqttUtf8StringCheck((xstrview){(const char*)B+XMQTT_CK_HEADER,IdBytes},S->Config.Delta.Codec.Utf8Flags,NULL))
        return xmqttSError(XMQTT_STORE_CORRUPT,"invalid store checkpoint identity, clock or counts");
    if(!S->Config.Delta.MaxOperations||Count>=S->Config.MaxOperations||
       Count>S->Config.MaxOutgoing+S->Config.MaxIncoming+S->Config.MaxInbox)
        return XMQTT_STORE_LIMIT;
    xmqttstoredelta Bind={.Type=XMQTT_STORE_BIND,.Epoch=Epoch,.WallMilliseconds=Wall,
        .Data={B+XMQTT_CK_HEADER,IdBytes}};
    xmqttstorestatus Status=xmqttSApply(S,&Bind);
    if(Status!=XMQTT_STORE_DONE)return Status;
    S->Info.HasSession=HasSession;S->Opened=Opened;S->Live=Live;
    S->Expiry=Expiry;S->ClosedWall=Closed;
    *Cut=(xmqttstorecheckpoint){Count,LastKey};
    if(!Count)S->Info.LastKey=LastKey;
    return XMQTT_STORE_DONE;
}
static xmqttstorestatus xmqttSCEntry(xmqttstorestate* S,const unsigned char* H,xbytesview Wire,
    uint64 LastKey,xmqttproperty* Properties,size_t Capacity)
{
    uint8 Flags=H[22],Reason=H[23];
    bool Out=(Flags&XMQTT_CK_OUT)!=0,Receive=(Flags&XMQTT_CK_RECEIVE)!=0;
    xmqttstoreview View={.Key=xmqttSCGet(H,8),.Epoch=xmqttSCGet(H+8,8),.Identifier=(uint16)xmqttSCGet(H+20,2),
        .Qos=(Wire.Data[0]>>1)&3u,.Wire=Wire,.PubrecReason=Reason,
        .Attempted=(Flags&XMQTT_CK_ATTEMPT)!=0,.Sent=(Flags&XMQTT_CK_SENT)!=0,
        .Pubrel=(Flags&XMQTT_CK_PUBREL)!=0,.InboxPending=(Flags&XMQTT_CK_INBOX)!=0};
    if((Flags&~63u)||!View.Key||View.Key>LastKey||!View.Epoch||View.Epoch>S->Info.Epoch||
       ((Out||Receive)&&(!S->Info.HasSession||View.Epoch!=S->Info.Epoch))||
       ((Receive||View.Pubrel)&&View.Qos!=2)||
       (Out?(Receive||View.InboxPending||(View.Sent&&!View.Attempted)||(View.Pubrel&&!View.Sent)||
             (Reason&&(!View.Pubrel||Reason!=0x10||S->Info.Version!=XMQTT_VERSION_5))):
            ((!Receive&&!View.InboxPending)||View.Attempted||View.Sent||Reason)))
        return xmqttSError(XMQTT_STORE_CORRUPT,"invalid store checkpoint flow stage or flags");
    xmqttstoredelta Packet={.Type=Out?XMQTT_STORE_OUT_PUT:XMQTT_STORE_IN_ACCEPT,
        .Identifier=View.Identifier,.Data=Wire};
    xmqttstoredeltastatus Decode=xmqttInternalStorePacket(&S->Config.Delta,&Packet,Properties,Capacity);
    if(Decode!=XMQTT_STORE_DELTA_DONE)return Decode==XMQTT_STORE_DELTA_LIMIT?XMQTT_STORE_LIMIT:
        Decode==XMQTT_STORE_DELTA_ERROR?XMQTT_STORE_ERROR:XMQTT_STORE_CORRUPT;
    return xmqttSAdd(S,View,Out,Receive?(View.Pubrel?3u:2u):0u);
}
static xmqttstorestatus xmqttSCRows(xmqttstorestate* S,xbytesview Data,xmqttstorecheckpoint* Cut,
    xmqttproperty* Properties,size_t Capacity,uint64* Operations)
{
    if(Data.Size<XMQTT_CK_CHUNK||memcmp(Data.Data,"XMQCROW",8)||xmqttSCGet(Data.Data+12,4))
        return xmqttSError(XMQTT_STORE_CORRUPT,"missing or invalid store checkpoint row block");
    size_t Count=(size_t)xmqttSCGet(Data.Data+8,4);
    if(!Count||Count>Cut->Remaining||Count>(Data.Size-XMQTT_CK_CHUNK)/XMQTT_CK_ENTRY)
        return xmqttSError(XMQTT_STORE_CORRUPT,"invalid store checkpoint row count");
    if(Count>S->Config.Delta.MaxOperations)return XMQTT_STORE_LIMIT;
    size_t At=XMQTT_CK_CHUNK;
    for(size_t I=0;I<Count;++I){
        if(Data.Size-At<XMQTT_CK_ENTRY)
            return xmqttSError(XMQTT_STORE_CORRUPT,"truncated store checkpoint entry");
        const unsigned char* H=Data.Data+At;
        size_t Bytes=(size_t)xmqttSCGet(H+16,4);
        At+=XMQTT_CK_ENTRY;
        if(!Bytes||Bytes>Data.Size-At)
            return xmqttSError(XMQTT_STORE_CORRUPT,"invalid checkpoint packet length");
        xmqttstorestatus Status=xmqttSCEntry(S,H,(xbytesview){Data.Data+At,Bytes},Cut->LastKey,Properties,Capacity);
        if(Status!=XMQTT_STORE_DONE)return Status;
        At+=Bytes;
    }
    if(At!=Data.Size)return xmqttSError(XMQTT_STORE_CORRUPT,"trailing bytes in store checkpoint chunk");
    Cut->Remaining-=Count;
    if(!Cut->Remaining)S->Info.LastKey=Cut->LastKey;
    *Operations=Count;
    return XMQTT_STORE_DONE;
}

static xmqttstorestatus xmqttSRecover(xmqttjournal* J,const xmqttstorestateconfig* Config,
    xmqttstorestate** Out,bool ProjectExpiry)
{
    xmqttstorestateconfig K;
    if(!xmqttSConfig(Config,&K)||!xmqttSSpan(Out,sizeof(*Out))||!xmqttSSeparate(Out,sizeof(*Out),Config,sizeof(*Config))||
       !xmqttSSeparate(Out,sizeof(*Out),K.Binding.Data,K.Binding.Size)||!xmqttSSeparate(Out,sizeof(*Out),K.ClientId.Data,K.ClientId.Size))return xmqttSError(XMQTT_STORE_ERROR,"invalid recovery inputs/config or overlap");
    uint64 Transactions=0;if(!xmqttJournalReplayStart(J,K.Delta.Version,K.Binding,K.Generation,Out,sizeof(*Out),&Transactions))return XMQTT_STORE_STATE_ERROR;
    if(Transactions>K.MaxTransactions)return XMQTT_STORE_LIMIT;
    if(K.MaxInbox>SIZE_MAX-K.MaxOutgoing-K.MaxIncoming)return XMQTT_STORE_LIMIT;
    size_t Slots=K.MaxOutgoing+K.MaxIncoming+K.MaxInbox,Buckets=Slots?8u:0;
    if(Slots>SIZE_MAX/2u)return XMQTT_STORE_LIMIT;
    while(Buckets<Slots*2u){if(Buckets>SIZE_MAX/2u)return XMQTT_STORE_LIMIT;Buckets*=2u;}
    if(Buckets>(SIZE_MAX-Slots)/2u||(Buckets*2u+Slots)>(SIZE_MAX-sizeof(xmqttstorestate)-K.Binding.Size)/sizeof(xmqttstoreentry*))return XMQTT_STORE_LIMIT;
    size_t Base=sizeof(xmqttstorestate)+(Buckets*2u+Slots)*sizeof(xmqttstoreentry*)+K.Binding.Size;
    size_t Ops=Transactions?K.Delta.MaxOperations*sizeof(xmqttstoredelta):0,Props=Transactions?K.Delta.Codec.MaxProperties*sizeof(xmqttproperty):0;
    if(Base>K.MaxMemoryBytes||Ops>K.MaxMemoryBytes-Base||Props>K.MaxMemoryBytes-Base-Ops)return XMQTT_STORE_LIMIT;
    xerror* Saved=NULL;if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_STORE_ERROR;
    xmqttstorestatus Result=XMQTT_STORE_ERROR;xmqttstorestate* S=xrtCalloc(1,Base);xmqttstoredelta* D=NULL;xmqttproperty* P=NULL;
    if(!S)goto Finish;
    S->Config=K;S->Info.Version=K.Delta.Version;S->Info.BaseBytes=S->Info.MemoryBytes=Base;S->Buckets=Buckets;
    S->Keys=(xmqttstoreentry**)(S+1);S->Ids=S->Keys+Buckets;S->Views[0]=S->Ids+Buckets;S->Views[1]=S->Views[0]+K.MaxOutgoing;S->Views[2]=S->Views[1]+K.MaxIncoming;
    unsigned char* Binding=(unsigned char*)(S->Views[2]+K.MaxInbox);memcpy(Binding,K.Binding.Data,K.Binding.Size);S->Config.Binding=(xbytesview){Binding,K.Binding.Size};
    if(Ops){D=xrtMalloc(Ops);if(!D)goto Finish;S->Info.MemoryBytes+=Ops;}if(Props){P=xrtMalloc(Props);if(!P)goto Finish;S->Info.MemoryBytes+=Props;}xmqttSPeak(S);
    xmqttstorecheckpoint Cut={0};
    for(;;){
        xmqttjournalrecord R;xmqttjournalstatus JS=xmqttJournalNext(J,&R);
        if(JS==XMQTT_JOURNAL_EMPTY)break;
        if(JS!=XMQTT_JOURNAL_DONE){Result=JS==XMQTT_JOURNAL_LIMIT?XMQTT_STORE_LIMIT:JS==XMQTT_JOURNAL_CORRUPT?XMQTT_STORE_CORRUPT:JS==XMQTT_JOURNAL_IO||JS==XMQTT_JOURNAL_UNCERTAIN?XMQTT_STORE_IO:XMQTT_STORE_ERROR;goto Finish;}
        if(R.Sequence!=S->Info.Transactions+1u||R.Sequence>Transactions){Result=xmqttSError(XMQTT_STORE_CORRUPT,"invalid recovery transaction sequence");goto Finish;}
        if(R.Data.Size>K.MaxReplayBytes-S->Info.ReplayBytes){Result=XMQTT_STORE_LIMIT;goto Finish;}
        uint64 Operations;
        if(Cut.Remaining){
            if(R.Data.Size>K.Delta.MaxBytes){Result=XMQTT_STORE_LIMIT;goto Finish;}
            Result=xmqttSCRows(S,R.Data,&Cut,P,K.Delta.Codec.MaxProperties,&Operations);
            if(Result!=XMQTT_STORE_DONE)goto Finish;
        }else if(R.Sequence==1&&R.Data.Size>=8&&!memcmp(R.Data.Data,"XMQCKPT",8)){
            if(R.Data.Size>K.Delta.MaxBytes){Result=XMQTT_STORE_LIMIT;goto Finish;}
            Result=xmqttSCBegin(S,R.Data,&Cut);
            if(Result!=XMQTT_STORE_DONE)goto Finish;
            Operations=1;
        }else{
            xmqttstoredeltadocument Doc;
            xmqttstoredeltastatus DS=xmqttStoreDeltaRead(&K.Delta,R.Data,D,K.Delta.MaxOperations,P,K.Delta.Codec.MaxProperties,&Doc);
            if(DS!=XMQTT_STORE_DELTA_DONE){Result=DS==XMQTT_STORE_DELTA_LIMIT?XMQTT_STORE_LIMIT:DS==XMQTT_STORE_DELTA_ERROR?XMQTT_STORE_ERROR:XMQTT_STORE_CORRUPT;goto Finish;}
            if(Doc.Count>K.MaxOperations-S->Info.Operations){Result=XMQTT_STORE_LIMIT;goto Finish;}
            for(size_t I=0;I<Doc.Count;++I){Result=xmqttSApply(S,D+I);if(Result!=XMQTT_STORE_DONE)goto Finish;}
            Operations=Doc.Count;
        }
        ++S->Info.Transactions;S->Info.Operations+=Operations;S->Info.ReplayBytes+=R.Data.Size;
    }
    if(Cut.Remaining||S->Info.Transactions!=Transactions){Result=xmqttSError(XMQTT_STORE_CORRUPT,"incomplete recovered prefix");goto Finish;}
    if(K.WallMilliseconds<S->Info.LastWallMilliseconds){Result=xmqttSError(XMQTT_STORE_STATE_ERROR,"trusted recovery wall clock went backwards");goto Finish;}
    if(!ProjectExpiry){Result=XMQTT_STORE_DONE;goto Finish;}
    S->Info.LiveAtCut=S->Live;
    if((S->Live&&!S->Expiry)||xmqttSExpired(S,K.WallMilliseconds)){
        xmqttSClear(S);
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
        S->ProjectionDirty=true;
#endif
    }
    if(S->Info.HasSession)S->Info.RemainingMilliseconds=(S->Live||S->Expiry==UINT32_MAX)?UINT64_MAX:(uint64)S->Expiry*1000u-(K.WallMilliseconds-S->ClosedWall);
    size_t Counts[3]={0};for(xmqttstoreentry* E=S->Head;E;E=E->Next){if(E->Out)S->Views[0][Counts[0]++]=E;if(E->Receive)S->Views[1][Counts[1]++]=E;if(E->View.InboxPending)S->Views[2][Counts[2]++]=E;}
    S->Info.MemoryBytes-=Ops+Props;
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    xmqttjournalinfo FinalJournal;if(!xmqttJournalInfo(J,&FinalJournal)){Result=XMQTT_STORE_ERROR;goto Finish;}
    S->JournalChain=FinalJournal.Chain;S->JournalEnd=FinalJournal.CommittedEnd;
    if(!S->Info.ClientId.Size&&K.ClientId.Size){
        if(K.ClientId.Size>xmqttSAvailable(S)){Result=XMQTT_STORE_LIMIT;goto Finish;}
        char* Expected=xrtMalloc(K.ClientId.Size);if(!Expected){Result=XMQTT_STORE_ERROR;goto Finish;}
        memcpy(Expected,K.ClientId.Data,K.ClientId.Size);S->ExpectedId=(xstrview){Expected,K.ClientId.Size};S->Info.MemoryBytes+=K.ClientId.Size;xmqttSPeak(S);
        S->Config.ClientId=S->ExpectedId;
    }else S->Config.ClientId=S->Info.ClientId;
#else
    S->Config.ClientId=(xstrview){0};
#endif
    Result=XMQTT_STORE_DONE;
    S->Lowest=(uintptr_t)S;S->Highest=S->Lowest+Base;
    if(S->Info.ClientId.Size){uintptr_t A=(uintptr_t)S->Info.ClientId.Data;if(A<S->Lowest)S->Lowest=A;if(A+S->Info.ClientId.Size>S->Highest)S->Highest=A+S->Info.ClientId.Size;}
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
    if(S->ExpectedId.Size){uintptr_t A=(uintptr_t)S->ExpectedId.Data;if(A<S->Lowest)S->Lowest=A;if(A+S->ExpectedId.Size>S->Highest)S->Highest=A+S->ExpectedId.Size;}
#endif
    for(xmqttstoreentry* E=S->Head;E;E=E->Next){uintptr_t A=(uintptr_t)E;if(A<S->Lowest)S->Lowest=A;if(A+E->Allocation>S->Highest)S->Highest=A+E->Allocation;}
Finish:
    xrtFree(D);xrtFree(P);
    if(Result==XMQTT_STORE_DONE||Result==XMQTT_STORE_LIMIT){xerror* Previous=NULL;
        if(!xrtTrySwapErrorTake(Saved,&Previous)){xrtErrorFree(Saved);Result=XMQTT_STORE_ERROR;}xrtErrorFree(Previous);
    }else xrtErrorFree(Saved);
    if(Result==XMQTT_STORE_DONE)*Out=S;else xmqttStoreStateDestroy(S);return Result;
}

XMQTT_API xmqttstorestatus xmqttStoreStateRecover(xmqttjournal* J,const xmqttstorestateconfig* Config,xmqttstorestate** Out)
{return xmqttSRecover(J,Config,Out,true);}

#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT)
static void xmqttSCPut(unsigned char* P,uint64 Value,unsigned Bytes)
{
    for(unsigned I=Bytes;I;--I){P[I-1]=(unsigned char)Value;Value>>=8;}
}
static xmqttjournalstatus xmqttSCAppend(xmqttjournal* J,const xmqttstorestateconfig* C,
    const unsigned char* Buffer,size_t Size,uint64* Sequence,uint64* ReplayBytes)
{
    if(*Sequence>=C->MaxTransactions||Size>C->MaxReplayBytes-*ReplayBytes)return XMQTT_JOURNAL_LIMIT;
    xmqttjournalstatus Status=xmqttJournalAppend(J,(xbytesview){Buffer,Size},Sequence);
    if(Status==XMQTT_JOURNAL_DONE)*ReplayBytes+=Size;
    /* Uncertain writes to the inactive data file cannot select it. */
    return Status==XMQTT_JOURNAL_UNCERTAIN?XMQTT_JOURNAL_IO:Status;
}
static bool xmqttSCFits(const xmqttstoreentry* E,size_t Capacity,size_t At,size_t Rows,size_t Limit)
{
    return Rows<Limit&&Capacity-At>=XMQTT_CK_ENTRY&&E->View.Wire.Size<=Capacity-At-XMQTT_CK_ENTRY;
}
static xmqttjournalstatus xmqttSCWrite(xmqttjournal* J,const xmqttstorestate* S,
    unsigned char* Buffer,size_t Capacity,uint64 Count,bool (*Stop)(ptr),ptr StopContext)
{
    if(!S->Info.ClientId.Size)return XMQTT_JOURNAL_DONE;
    if(Stop&&Stop(StopContext))return XMQTT_JOURNAL_EMPTY;
    uint64 Sequence=0,ReplayBytes=0;
    if(!S->Config.Delta.MaxOperations||Count>=S->Config.MaxOperations)return XMQTT_JOURNAL_LIMIT;

    memset(Buffer,0,XMQTT_CK_HEADER);
    memcpy(Buffer,"XMQCKPT",8);
    Buffer[8]=1;Buffer[9]=(unsigned char)S->Info.Version;
    Buffer[10]=(S->Info.HasSession?XMQTT_CK_SESSION:0u)|(S->Opened?XMQTT_CK_OPENED:0u)|(S->Live?XMQTT_CK_LIVE:0u);
    xmqttSCPut(Buffer+12,S->Info.ClientId.Size,4);
    xmqttSCPut(Buffer+16,S->Info.Epoch,8);
    xmqttSCPut(Buffer+24,S->Info.LastKey,8);
    xmqttSCPut(Buffer+32,S->Info.LastWallMilliseconds,8);
    xmqttSCPut(Buffer+40,S->ClosedWall,8);
    xmqttSCPut(Buffer+48,Count,8);
    xmqttSCPut(Buffer+56,S->Expiry,4);
    memcpy(Buffer+XMQTT_CK_HEADER,S->Info.ClientId.Data,S->Info.ClientId.Size);
    xmqttjournalstatus Status=xmqttSCAppend(J,&S->Config,Buffer,XMQTT_CK_HEADER+S->Info.ClientId.Size,&Sequence,&ReplayBytes);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;

    memcpy(Buffer,"XMQCROW",8);
    memset(Buffer+12,0,4);
    const xmqttstoreentry* E=S->Head;
    while(E){
        size_t At=XMQTT_CK_CHUNK,Rows=0;
        if(Stop&&Stop(StopContext))return XMQTT_JOURNAL_EMPTY;
        while(E&&xmqttSCFits(E,Capacity,At,Rows,S->Config.Delta.MaxOperations)){
            const xmqttstoreview* V=&E->View;
            unsigned char* H=Buffer+At;
            xmqttSCPut(H,V->Key,8);
            xmqttSCPut(H+8,V->Epoch,8);
            xmqttSCPut(H+16,V->Wire.Size,4);
            xmqttSCPut(H+20,V->Identifier,2);
            H[22]=(E->Out?XMQTT_CK_OUT:0u)|(V->Attempted?XMQTT_CK_ATTEMPT:0u)|(V->Sent?XMQTT_CK_SENT:0u)|
                (V->Pubrel?XMQTT_CK_PUBREL:0u)|(V->InboxPending?XMQTT_CK_INBOX:0u)|(E->Receive?XMQTT_CK_RECEIVE:0u);
            H[23]=V->PubrecReason;
            memcpy(H+XMQTT_CK_ENTRY,V->Wire.Data,V->Wire.Size);
            At+=XMQTT_CK_ENTRY+V->Wire.Size;
            ++Rows;
            E=E->Next;
        }
        if(!Rows)return XMQTT_JOURNAL_LIMIT;
        xmqttSCPut(Buffer+8,Rows,4);
        Status=xmqttSCAppend(J,&S->Config,Buffer,At,&Sequence,&ReplayBytes);
        if(Status!=XMQTT_JOURNAL_DONE)return Status;
    }
    return XMQTT_JOURNAL_DONE;
}
static xmqttjournalstatus xmqttSCRetire(xmqttjournal* Journal,xmqttjournalstatus Status)
{
    if(xmqttJournalDestroy(Journal))return Status;
    (void)xmqttJournalDestroy(Journal);
    return Status==XMQTT_JOURNAL_UNCERTAIN?Status:XMQTT_JOURNAL_IO;
}
static xmqttjournalstatus xmqttSCRecoverStatus(xmqttstorestatus Status)
{
    switch(Status){
    case XMQTT_STORE_DONE:return XMQTT_JOURNAL_DONE;
    case XMQTT_STORE_LIMIT:return XMQTT_JOURNAL_LIMIT;
    case XMQTT_STORE_CORRUPT:return XMQTT_JOURNAL_CORRUPT;
    case XMQTT_STORE_STATE_ERROR:return XMQTT_JOURNAL_STATE;
    case XMQTT_STORE_IO:return XMQTT_JOURNAL_IO;
    default:return XMQTT_JOURNAL_ERROR;
    }
}
XMQTT_API xmqttjournalstatus xmqttStoreCompact(xmqttjournalslot* Slot,
    const xmqttstorestateconfig* Config,size_t MaxScratchBytes)
{
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    xmqttjournal* J=NULL;
    xmqttstorestate* S=NULL;
    unsigned char* Buffer=NULL;
    xmqttjournalstatus Status=xmqttJournalSlotOpenCurrent(Slot,&J);
    if(Status!=XMQTT_JOURNAL_DONE)goto Finish;
    Status=xmqttSCRecoverStatus(xmqttSRecover(J,Config,&S,false));
    size_t Capacity=xmqttJournalReplayTransactionLimit(J);
    Status=xmqttSCRetire(J,Status);
    J=NULL;
    if(Status!=XMQTT_JOURNAL_DONE)goto Finish;
    if(!S->Info.Transactions){Status=XMQTT_JOURNAL_EMPTY;goto Finish;}

    if(S->Info.ClientId.Size){
        if(Capacity>S->Config.Delta.MaxBytes)Capacity=S->Config.Delta.MaxBytes;
        if(Capacity>MaxScratchBytes)Capacity=MaxScratchBytes;
        if(Capacity<XMQTT_CK_HEADER+S->Info.ClientId.Size){Status=XMQTT_JOURNAL_LIMIT;goto Finish;}
        Buffer=xrtMalloc(Capacity);
        if(!Buffer){Status=XMQTT_JOURNAL_ERROR;goto Finish;}
    }
    Status=xmqttJournalSlotPrepare(Slot,&J);
    if(Status!=XMQTT_JOURNAL_DONE)goto Finish;
    uint64 Count=0;
    for(const xmqttstoreentry* E=S->Head;E;E=E->Next)++Count;
    Status=xmqttSCWrite(J,S,Buffer,Capacity,Count,NULL,NULL);
    if(Status==XMQTT_JOURNAL_DONE)Status=xmqttJournalSlotPublish(Slot,J);
Finish:
    Status=xmqttSCRetire(J,Status);
    xrtFree(Buffer);
    xmqttStoreStateDestroy(S);
    if(Status>=XMQTT_JOURNAL_EMPTY){
        xerror* Discard=NULL;
        if(!xrtTrySwapErrorTake(Saved,&Discard)){xrtErrorFree(Saved);return XMQTT_JOURNAL_ERROR;}
        xrtErrorFree(Discard);
    }else xrtErrorFree(Saved);
    return Status;
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
typedef struct xmqttcheckpointwork {
    const xmqttstorestate* State;
    unsigned char* Buffer;
    size_t Capacity;
    uint64 Rows;
    xmqttcompactstop Stop;
    ptr StopContext;
} xmqttcheckpointwork;

static xmqttjournalstatus xmqttSCMeasure(const xmqttcheckpointwork* Work,size_t MaxEntries,
    xmqttcheckpointstats* Stats,uint64* Rows)
{
    const xmqttstorestate* S=Work->State;
    if(!S->Info.ClientId.Size)return XMQTT_JOURNAL_DONE;
    size_t Capacity=Work->Capacity,Head=XMQTT_CK_HEADER+S->Info.ClientId.Size;
    if(Head>Capacity||Head>S->Config.MaxReplayBytes||!S->Config.MaxTransactions||
       !S->Config.MaxOperations||!S->Config.Delta.MaxOperations)return XMQTT_JOURNAL_LIMIT;

    *Stats=(xmqttcheckpointstats){1,1,Head};
    const xmqttstoreentry* E=S->Head;
    while(E){
        if(Work->Stop(Work->StopContext))return XMQTT_JOURNAL_EMPTY;
        size_t At=XMQTT_CK_CHUNK,InBlock=0;
        while(E&&xmqttSCFits(E,Capacity,At,InBlock,S->Config.Delta.MaxOperations)){
            if(*Rows>=MaxEntries||Stats->Operations==S->Config.MaxOperations)
                return XMQTT_JOURNAL_LIMIT;
            ++*Rows;
            ++Stats->Operations;
            ++InBlock;
            At+=XMQTT_CK_ENTRY+E->View.Wire.Size;
            E=E->Next;
        }
        if(!InBlock||Stats->Transactions==S->Config.MaxTransactions||
           At>S->Config.MaxReplayBytes-Stats->ReplayBytes)return XMQTT_JOURNAL_LIMIT;
        ++Stats->Transactions;
        Stats->ReplayBytes+=At;
    }
    return XMQTT_JOURNAL_DONE;
}
static xmqttjournalstatus xmqttSCEncode(xmqttjournal* Candidate,ptr Context)
{
    const xmqttcheckpointwork* Work=Context;
    return xmqttSCWrite(Candidate,Work->State,Work->Buffer,Work->Capacity,Work->Rows,
                        Work->Stop,Work->StopContext);
}
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCompactModel(const xmqttstorestate* S,
    const void* StateOwner,xmqttjournal** Journal,const void* Owner,const xmqttstorecompactconfig* Config,
    xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result,xmqttcheckpointstats* Stats,
    xmqttcompactionrequest* Request)
{
    if(!S||S->WriteOwner!=StateOwner||S->Plan||S->Retiring)abort();
    xmqttjournalstatus Status=xmqttInternalJournalCompactCheck(*Journal,Owner,0,0,Config->MinReclaimBytes,Result);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    if(S->ProjectionDirty){xmqttSError(XMQTT_STORE_STATE_ERROR,"durable reset required after recovery-time expiry");
        return XMQTT_JOURNAL_STATE;}
    size_t Capacity=xmqttJournalReplayTransactionLimit(*Journal);
    if(Capacity>S->Config.Delta.MaxBytes)Capacity=S->Config.Delta.MaxBytes;
    if(Capacity>Config->MaxScratchBytes)Capacity=Config->MaxScratchBytes;
    xmqttcheckpointwork Work={.State=S,.Capacity=Capacity,.Stop=Stop,.StopContext=StopContext};
    Status=xmqttSCMeasure(&Work,Config->MaxEntries,Stats,&Work.Rows);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    Status=xmqttInternalJournalCompactCheck(*Journal,Owner,Stats->Transactions,Stats->ReplayBytes,Config->MinReclaimBytes,Result);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    if(Request&&(Stats->Transactions==S->Config.MaxTransactions||
       Request->Operations>S->Config.MaxOperations-Stats->Operations||
       Request->Bytes>S->Config.MaxReplayBytes-Stats->ReplayBytes||
       !xmqttInternalJournalCompactFits(*Journal,Stats->Transactions,Stats->ReplayBytes,Request->Bytes))){
        Request->NoRoom=true;
        return XMQTT_JOURNAL_LIMIT;
    }
    if(S->Info.ClientId.Size){
        Work.Buffer=xrtMalloc(Capacity);
        if(!Work.Buffer)return XMQTT_JOURNAL_ERROR;
    }
    Status=xmqttInternalJournalCompactReplace(Journal,Owner,xmqttSCEncode,&Work,Stop,StopContext,Result);
    xrtFree(Work.Buffer);
    return Status;
}
XMQTT_INTERNAL void xmqttInternalStoreCompactRebase(xmqttstorestate* S,const void* Owner,
    const xmqttjournalinfo* J,const xmqttstorecompactresult* Result,const xmqttcheckpointstats* Stats)
{
    if(S->WriteOwner!=Owner||S->Plan||S->Retiring||S->ProjectionDirty||
       J->Sequence!=Stats->Transactions||J->Sequence!=Result->Sequence||
       J->CommittedEnd!=Result->Bytes||Result->Generation!=S->Config.Generation+1u)abort();
    S->Info.Transactions=Stats->Transactions;
    S->Info.Operations=Stats->Operations;
    S->Info.ReplayBytes=Stats->ReplayBytes;
    S->Config.Generation=Result->Generation;
    S->JournalChain=J->Chain;
    S->JournalEnd=J->CommittedEnd;
}
#endif

#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE)
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteIncomingKey(xmqttstorestate* S,const void* Owner,uint16 Id)
{
    if(!S||S->WriteOwner!=Owner||S->Plan)abort();
    const xmqttstoreentry* E=xmqttSIdFindBase(S,Id,false);
    return E?E->View.Key:0;
}
XMQTT_INTERNAL uint64 xmqttInternalStoreWriteOutgoingKey(xmqttstorestate* S,const void* Owner,
    uint16 Id,xbytesview Wire,bool Pubrel,bool Sent,uint8 PubrecReason,bool* Attempted)
{
    if(!S||S->WriteOwner!=Owner||S->Plan)abort();
    const xmqttstoreentry* E=xmqttSIdFindBase(S,Id,true);
    /* A full local send can be newer than the write-ahead attempt. */
    if(!E||E->View.Pubrel!=Pubrel||(E->View.Sent&&!Sent)||(Sent&&!E->View.Attempted)||
       E->View.PubrecReason!=PubrecReason||E->View.Wire.Size!=Wire.Size||
       memcmp(E->View.Wire.Data,Wire.Data,Wire.Size))return 0;
    *Attempted=E->View.Attempted;
    return E->View.Key;
}
XMQTT_INTERNAL bool xmqttInternalStoreWriteEverOpened(xmqttstorestate* S,const void* Owner)
{if(!S||S->WriteOwner!=Owner)abort();return S->Opened;}
#endif
XMQTT_INTERNAL bool xmqttInternalStoreWriteClaim(xmqttstorestate* S,xmqttjournal* J,const void* Owner,void* Output,size_t Bytes,xmqttstoredeltaconfig* Delta,size_t* MinimumPlanBytes)
{
    if(!S||!Owner||S->WriteOwner||!xmqttSOutput(S,Output,Bytes)){
        xmqttSError(XMQTT_STORE_STATE_ERROR,"writer requires exclusive recovered State and disjoint output");return false;}
    if(!xmqttJournalWriteStart(J,S->Config.Delta.Version,S->Config.Binding,S->Config.Generation,S->Info.Transactions,S->JournalChain,S->JournalEnd,Output,Bytes))return false;
    size_t Slots=S->Config.MaxOutgoing+S->Config.MaxIncoming+S->Config.MaxInbox,Buckets=0,PlanBytes=0;
    if(S->Config.Delta.MaxOperations>SIZE_MAX-Slots||!xmqttSPlanSize(Slots+S->Config.Delta.MaxOperations,&Buckets,&PlanBytes)){
        xmqttSError(XMQTT_STORE_ERROR,"configured model reset metadata size overflows");return false;}
    S->WriteOwner=Owner;*Delta=S->Config.Delta;*MinimumPlanBytes=PlanBytes;return true;
}
XMQTT_INTERNAL void xmqttInternalStoreWriteRelease(xmqttstorestate* S,const void* Owner,bool Destroy)
{if(!S||S->WriteOwner!=Owner||S->Plan)abort();S->WriteOwner=NULL;if(Destroy)xmqttStoreStateDestroy(S);}
XMQTT_INTERNAL bool xmqttInternalStoreWriteOutput(xmqttstorestate* S,const void* Owner,void* P,size_t N)
{return S&&S->WriteOwner==Owner&&xmqttSOutput(S,P,N);}
static void xmqttSPlanFree(xmqttstorestate* S,xmqttstoreplan* P)
{
    if(!P)return;
    for(xmqttstoreentry* E=P->Retired,*Next;E;E=Next){Next=E->Next;xrtFree(E);}
    for(size_t I=0;I<P->Count;++I)xrtFree(P->Nodes[I].Fresh);
    if(P->Shadow.Info.ClientId.Data!=S->Info.ClientId.Data&&P->Shadow.Info.ClientId.Data!=S->ExpectedId.Data)xrtFree((ptr)P->Shadow.Info.ClientId.Data);
    xrtFree(P);
}
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreWritePrepare(xmqttstorestate* S,const void* Owner,const xmqttstoredeltadocument* D,size_t MaxWork,size_t MaxPlanBytes,xmqttstoreplan** Out XMQTT_SW_PRESSURE_PARAM)
{
    if(!S||S->WriteOwner!=Owner||S->Plan)abort();
    if(D->Count>MaxWork)return XMQTT_STORE_LIMIT;
    bool ReplayFull=S->Info.Transactions>=S->Config.MaxTransactions||
        D->Count>S->Config.MaxOperations-S->Info.Operations||D->Raw.Size>S->Config.MaxReplayBytes-S->Info.ReplayBytes;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    if(ReplayFull&&(!Request||!S->Config.MaxTransactions||D->Count>S->Config.MaxOperations||
                    D->Raw.Size>S->Config.MaxReplayBytes))return XMQTT_STORE_LIMIT;
#else
    if(ReplayFull)return XMQTT_STORE_LIMIT;
#endif
    bool Bulk=false;
    for(size_t I=0;I<D->Count;++I){const xmqttstoredelta* O=D->Operations+I;
        if((O->Type==XMQTT_STORE_OPEN&&!(O->Flags&2u))||O->Type==XMQTT_STORE_CLEAR||(O->Type==XMQTT_STORE_END&&!O->Expiry))Bulk=true;
        if((O->Type==XMQTT_STORE_BIND||O->Type==XMQTT_STORE_OPEN||O->Type==XMQTT_STORE_END||O->Type==XMQTT_STORE_CLEAR)&&O->WallMilliseconds<S->Config.WallMilliseconds)
            return xmqttSError(XMQTT_STORE_STATE_ERROR,"live transaction wall clock precedes trusted recovery/commit floor");
    }
    size_t BaseCount=S->Info.Outgoing+S->Info.Incoming+S->Info.Inbox,Cap=D->Count;
    if(Bulk){if(BaseCount>SIZE_MAX-Cap)return XMQTT_STORE_LIMIT;Cap+=BaseCount;}
    if(Cap>MaxWork)Cap=MaxWork; /* every touched node consumes a record or reset visit */
    size_t Buckets=0,N=0;if(!xmqttSPlanSize(Cap,&Buckets,&N)||N>MaxPlanBytes)return XMQTT_STORE_LIMIT;
    xmqttstoreplan* P=xrtCalloc(1,N);if(!P)return XMQTT_STORE_ERROR;
    P->Allocation=N;P->PhysicalBytes=S->Info.MemoryBytes;P->Work=D->Count;P->MaxWork=MaxWork;P->Capacity=Cap;P->Buckets=Buckets;P->Nodes=(xmqttstorechange*)(P+1);P->Keys=(xmqttstorechange**)(P->Nodes+Cap);P->Ids=P->Keys+Buckets;
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
    memcpy(&P->Shadow,S,offsetof(xmqttstorestate,InboxReadyHead));
#else
    P->Shadow=*S;
#endif
    P->Shadow.Plan=P;xmqttstorestatus Status=XMQTT_STORE_DONE;
    for(size_t I=0;I<D->Count;++I){Status=xmqttSApply(&P->Shadow,D->Operations+I);if(P->Exhausted)Status=XMQTT_STORE_LIMIT;if(Status!=XMQTT_STORE_DONE)break;}
    if(Status!=XMQTT_STORE_DONE){xmqttSPlanFree(S,P);return Status;}
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    if(Request)*Request=(xmqttcompactionrequest){.Operations=D->Count,.Bytes=D->Raw.Size,.ReplayBlocked=ReplayFull};
    if(ReplayFull){
        xmqttSPlanFree(S,P);
        return XMQTT_STORE_LIMIT;
    }
#endif
    ++P->Shadow.Info.Transactions;P->Shadow.Info.Operations+=D->Count;P->Shadow.Info.ReplayBytes+=D->Raw.Size;
    if(P->PhysicalBytes>P->Shadow.Info.PeakMemoryBytes)P->Shadow.Info.PeakMemoryBytes=P->PhysicalBytes;
    *Out=P;return XMQTT_STORE_DONE;
}
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
static void xmqttSInboxReady(xmqttstorestate* S,xmqttstoreentry* E)
{
    if(E->ReadyNext||E->Lease||!E->View.InboxPending)abort();
    if(S->InboxReadyTail)S->InboxReadyTail->ReadyNext=E;
    else S->InboxReadyHead=E;
    S->InboxReadyTail=E;
}
XMQTT_INTERNAL void xmqttInternalStoreInboxEnable(xmqttstorestate* S,const void* Owner)
{
    if(S->WriteOwner!=Owner||S->InboxEnabled)abort();
    for(xmqttstoreentry* E=S->Head;E;E=E->Next)
        if(E->View.InboxPending)xmqttSInboxReady(S,E);
    S->InboxEnabled=true;
}
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreInboxTake(xmqttstorestate* S,const void* Owner,
    ptr Buffer,size_t Capacity,uint64 Serial,xmqttstoreview* View,size_t* Required)
{
    if(S->WriteOwner!=Owner||!S->InboxEnabled)abort();
    xmqttstoreentry* E=S->InboxReadyHead;
    if(!E)return XMQTT_STORE_EMPTY;
    size_t Bytes=E->View.Wire.Size;
    if(Capacity<Bytes){*Required=Bytes;return XMQTT_STORE_LIMIT;}
    if(!Serial||Serial>=XMQTT_INBOX_ACKING)return XMQTT_STORE_STATE_ERROR;
    if(E->Lease)abort();
    memcpy(Buffer,E->View.Wire.Data,Bytes);
    S->InboxReadyHead=E->ReadyNext;
    if(!S->InboxReadyHead)S->InboxReadyTail=NULL;
    E->ReadyNext=NULL;
    E->Lease=Serial;
    *View=E->View;
    View->Wire.Data=Buffer;
    *Required=Bytes;
    return XMQTT_STORE_DONE;
}
static xmqttstoreentry* xmqttSInboxFind(xmqttstorestate* S,const void* Owner,uint64 Key,uint64 Serial)
{
    if(S->WriteOwner!=Owner||!S->InboxEnabled)abort();
    if(!Key||!Serial||Serial>=XMQTT_INBOX_ACKING)return NULL;
    xmqttstoreentry* E=xmqttSFindBase(S,Key);
    return E&&E->View.InboxPending&&E->Lease==Serial?E:NULL;
}
XMQTT_INTERNAL bool xmqttInternalStoreInboxRelease(xmqttstorestate* S,const void* Owner,uint64 Key,uint64 Serial)
{
    xmqttstoreentry* E=xmqttSInboxFind(S,Owner,Key,Serial);
    if(!E)return false;
    E->Lease=0;
    xmqttSInboxReady(S,E);
    return true;
}
XMQTT_INTERNAL bool xmqttInternalStoreInboxBegin(xmqttstorestate* S,const void* Owner,uint64 Key,uint64 Serial,uint64* Epoch)
{
    xmqttstoreentry* E=xmqttSInboxFind(S,Owner,Key,Serial);
    if(!E)return false;
    E->Lease|=XMQTT_INBOX_ACKING;
    *Epoch=E->View.Epoch;
    return true;
}
XMQTT_INTERNAL void xmqttInternalStoreInboxUndo(xmqttstorestate* S,const void* Owner,uint64 Key,bool Requeue)
{
    if(S->WriteOwner!=Owner||!S->InboxEnabled)abort();
    xmqttstoreentry* E=xmqttSFindBase(S,Key);
    if(!E||!(E->Lease&XMQTT_INBOX_ACKING)||!E->View.InboxPending)abort();
    E->Lease=Requeue?0:E->Lease&~XMQTT_INBOX_ACKING;
    if(Requeue)xmqttSInboxReady(S,E);
}
#endif
XMQTT_INTERNAL void xmqttInternalStoreWriteFinish(xmqttstorestate* S,const void* Owner,xmqttstoreplan* P,bool Committed,uint64 Sequence)
{
    if(!S||S->WriteOwner!=Owner||S->Plan)abort();
    if(!P)return;
    if(!Committed)return; /* dispose outside the model gate */
    if(Sequence!=P->Shadow.Info.Transactions||Sequence!=S->Info.Transactions+1u)abort();
    S->Retiring=P;
    for(size_t I=0;I<P->Count;++I){xmqttstorechange* N=P->Nodes+I;xmqttstoreentry* E=N->Base;
        if(E){
            if(!N->Alive){xmqttSDelete(S,E);continue;}
            if(E->Receive&&!N->Value.Receive)xmqttSUnlinkId(S,E);
            if(E->View.InboxPending&&!N->Value.View.InboxPending){
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
                if(S->InboxEnabled&&!(E->Lease&XMQTT_INBOX_ACKING))abort();
                E->Lease=0;
#endif
                --S->Info.Inbox;
            }
            E->View=N->Value.View;E->Receive=N->Value.Receive;
        }else if(N->Alive){
            E=N->Fresh;if(!E)abort();E->View=N->Value.View;E->Receive=N->Value.Receive;E->Out=N->Value.Out;
            size_t B=xmqttSKeyBucket(S,E->View.Key);E->KeyNext=S->Keys[B];S->Keys[B]=E;
            if(E->Out||E->Receive){B=xmqttSIdBucket(S,E->View.Identifier,E->Out);E->IdNext=S->Ids[B];S->Ids[B]=E;if(E->Out)++S->Info.Outgoing;else ++S->Info.Incoming;}
            if(E->View.InboxPending){
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
                if(S->InboxEnabled)xmqttSInboxReady(S,E);
#endif
                ++S->Info.Inbox;
            }
            E->Previous=S->Tail;E->Next=NULL;if(S->Tail)S->Tail->Next=E;else S->Head=E;S->Tail=E;N->Fresh=NULL;
        }
        if(E&&N->Alive){uintptr_t A=(uintptr_t)E;if(A<S->Lowest)S->Lowest=A;if(A+E->Allocation>S->Highest)S->Highest=A+E->Allocation;}
    }
    S->Retiring=NULL;P->Published=true;P->Refund=P->PhysicalBytes-P->Shadow.Info.MemoryBytes;
    S->Info=P->Shadow.Info;S->Info.MemoryBytes=P->PhysicalBytes;S->Expiry=P->Shadow.Expiry;S->ClosedWall=P->Shadow.ClosedWall;S->Opened=P->Shadow.Opened;S->Live=P->Shadow.Live;S->WriteEstablished=P->Shadow.WriteEstablished;
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
    S->ProjectionDirty=P->Shadow.ProjectionDirty;
#endif
    if(S->Info.LastWallMilliseconds>S->Config.WallMilliseconds)S->Config.WallMilliseconds=S->Info.LastWallMilliseconds;
    if(S->ExpectedId.Size&&S->Info.ClientId.Data==S->ExpectedId.Data){S->ExpectedId=(xstrview){0};S->Config.ClientId=S->Info.ClientId;}
    S->Info.LiveAtCut=S->Live;S->Info.RemainingMilliseconds=0;
    if(S->Info.HasSession)S->Info.RemainingMilliseconds=(S->Live||S->Expiry==UINT32_MAX)?UINT64_MAX:(uint64)S->Expiry*1000u-(S->Config.WallMilliseconds-S->ClosedWall);
    if(S->Info.ClientId.Size){uintptr_t A=(uintptr_t)S->Info.ClientId.Data;if(A<S->Lowest)S->Lowest=A;if(A+S->Info.ClientId.Size>S->Highest)S->Highest=A+S->Info.ClientId.Size;}
}
XMQTT_INTERNAL size_t xmqttInternalStoreWriteDispose(xmqttstorestate* S,const void* Owner,xmqttstoreplan* P)
{
    if(!S||S->WriteOwner!=Owner||S->Plan||S->Retiring)abort();
    size_t Bytes=P&&P->Published?P->Refund:0;xmqttSPlanFree(S,P);return Bytes;
}
XMQTT_INTERNAL void xmqttInternalStoreWriteRefund(xmqttstorestate* S,const void* Owner,size_t Bytes)
{if(!S||S->WriteOwner!=Owner||Bytes>S->Info.MemoryBytes)abort();S->Info.MemoryBytes-=Bytes;}
XMQTT_INTERNAL void xmqttInternalStoreWriteInfo(xmqttstorestate* S,const void* Owner,xmqttstorestateinfo* Info)
{if(!S||S->WriteOwner!=Owner)abort();*Info=S->Info;}
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreWriteRead(xmqttstorestate* S,const void* Owner,uint64 Key,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required)
{
    if(!S||S->WriteOwner!=Owner)abort();
    if(!Key||!xmqttSOutput(S,Buffer,Capacity)||!xmqttSOutput(S,View,sizeof(*View))||!xmqttSOutput(S,Required,sizeof(*Required))||
       !xmqttSSeparate(Buffer,Capacity,View,sizeof(*View))||!xmqttSSeparate(Buffer,Capacity,Required,sizeof(*Required))||!xmqttSSeparate(View,sizeof(*View),Required,sizeof(*Required)))return XMQTT_STORE_ERROR;
    xmqttstoreentry* E=xmqttSFindBase(S,Key);if(!E)return XMQTT_STORE_EMPTY;
    *Required=E->View.Wire.Size;if(Capacity<E->View.Wire.Size)return XMQTT_STORE_LIMIT;
    if(E->View.Wire.Size)memcpy(Buffer,E->View.Wire.Data,E->View.Wire.Size);
    *View=E->View;View->Wire.Data=Buffer;return XMQTT_STORE_DONE;
}
#endif
#endif
