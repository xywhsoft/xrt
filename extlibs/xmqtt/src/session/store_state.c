#include <xmqtt/store_state.h>
#include <xrt/memory.h>
#include "../internal/xmqtt_journal_replay.h"
#include "../internal/xmqtt_internal.h"
#include "../internal/xmqtt_store_session_source.h"
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
typedef struct xmqttstoreentry {
    struct xmqttstoreentry *Next,*Previous,*KeyNext,*IdNext;
    xmqttstoreview View;size_t Allocation;unsigned Receive;bool Out;
} xmqttstoreentry;
struct xmqttstorestate {
    xmqttstorestateconfig Config;xmqttstorestateinfo Info;
    xmqttstoreentry *Head,*Tail,**Keys,**Ids,**Views[3];size_t Buckets;
    uint32 Expiry;uint64 ClosedWall;bool Opened,Live;
    uintptr_t Lowest,Highest;
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
static xmqttstoreentry* xmqttSFind(const xmqttstorestate* S,uint64 Key)
{if(S->Buckets)for(xmqttstoreentry* E=S->Keys[xmqttSKeyBucket(S,Key)];E;E=E->KeyNext)if(E->View.Key==Key)return E;return NULL;}
static bool xmqttSIdUsed(const xmqttstorestate* S,uint16 Id,bool Out)
{if(S->Buckets)for(xmqttstoreentry* E=S->Ids[xmqttSIdBucket(S,Id,Out)];E;E=E->IdNext)if(E->View.Identifier==Id&&E->Out==Out)return true;return false;}
static void xmqttSUnlinkId(xmqttstorestate* S,xmqttstoreentry* E)
{
    xmqttstoreentry** P=&S->Ids[xmqttSIdBucket(S,E->View.Identifier,E->Out)];while(*P!=E)P=&(*P)->IdNext;*P=E->IdNext;E->IdNext=NULL;
    if(E->Out)--S->Info.Outgoing;else {--S->Info.Incoming;E->Receive=0;}
}
static void xmqttSDelete(xmqttstorestate* S,xmqttstoreentry* E)
{
    if(E->Out||E->Receive)xmqttSUnlinkId(S,E);
    if(E->View.InboxPending)--S->Info.Inbox;
    xmqttstoreentry** P=&S->Keys[xmqttSKeyBucket(S,E->View.Key)];while(*P!=E)P=&(*P)->KeyNext;*P=E->KeyNext;
    if(E->Previous)E->Previous->Next=E->Next;else S->Head=E->Next;
    if(E->Next)E->Next->Previous=E->Previous;else S->Tail=E->Previous;
    S->Info.MemoryBytes-=E->Allocation;xrtFree(E);
}
static void xmqttSClear(xmqttstorestate* S)
{
    for(xmqttstoreentry* E=S->Head,*Next;E;E=Next){Next=E->Next;if(E->Out)xmqttSDelete(S,E);
        else {if(E->Receive)xmqttSUnlinkId(S,E);if(!E->View.InboxPending)xmqttSDelete(S,E);}}
    S->Info.HasSession=false;
}
static void xmqttSPeak(xmqttstorestate* S)
{if(S->Info.MemoryBytes>S->Info.PeakMemoryBytes)S->Info.PeakMemoryBytes=S->Info.MemoryBytes;}
static bool xmqttSExpired(const xmqttstorestate* S,uint64 Wall)
{return S->Info.HasSession&&!S->Live&&S->Expiry!=UINT32_MAX&&Wall-S->ClosedWall>=(uint64)S->Expiry*1000u;}
static xmqttstorestatus xmqttSInsert(xmqttstorestate* S,const xmqttstoredelta* D)
{
    bool Out=D->Type==XMQTT_STORE_OUT_PUT;unsigned Qos=(D->Data.Data[0]>>1)&3u;bool Incoming=!Out&&Qos==2;
    if(D->Key<=S->Info.LastKey)return xmqttSError(XMQTT_STORE_CORRUPT,"reused or reversed persistent flow key");
    if((Out?S->Info.Outgoing==S->Config.MaxOutgoing:S->Info.Inbox==S->Config.MaxInbox)||
       (Incoming&&S->Info.Incoming==S->Config.MaxIncoming))return XMQTT_STORE_LIMIT;
    if((Out||Incoming)&&xmqttSIdUsed(S,D->Identifier,Out))return xmqttSError(XMQTT_STORE_CORRUPT,"duplicate active MQTT identifier");
    if(D->Data.Size>SIZE_MAX-sizeof(xmqttstoreentry)||sizeof(xmqttstoreentry)+D->Data.Size>S->Config.MaxMemoryBytes-S->Info.MemoryBytes)return XMQTT_STORE_LIMIT;
    size_t N=sizeof(xmqttstoreentry)+D->Data.Size;xmqttstoreentry* E=xrtMalloc(N);if(!E)return XMQTT_STORE_ERROR;memset(E,0,sizeof(*E));
    E->Allocation=N;E->Out=Out;E->Receive=Incoming?2u:0u;E->View=(xmqttstoreview){.Epoch=D->Epoch,.Key=D->Key,.Identifier=D->Identifier,.Qos=(uint8)Qos,
        .Attempted=(D->Flags&1u)!=0,.Sent=(D->Flags&2u)!=0,.InboxPending=!Out,.Wire={(const unsigned char*)(E+1),D->Data.Size}};
    memcpy(E+1,D->Data.Data,D->Data.Size);size_t B=xmqttSKeyBucket(S,D->Key);E->KeyNext=S->Keys[B];S->Keys[B]=E;
    if(Out||Incoming){B=xmqttSIdBucket(S,D->Identifier,Out);E->IdNext=S->Ids[B];S->Ids[B]=E;if(Out)++S->Info.Outgoing;else ++S->Info.Incoming;}
    if(!Out)++S->Info.Inbox;
    E->Previous=S->Tail;if(S->Tail)S->Tail->Next=E;else S->Head=E;S->Tail=E;
    S->Info.LastKey=D->Key;S->Info.MemoryBytes+=N;xmqttSPeak(S);return XMQTT_STORE_DONE;
}
static xmqttstorestatus xmqttSApply(xmqttstorestate* S,const xmqttstoredelta* D)
{
    if(D->Type==XMQTT_STORE_BIND){
        if(S->Info.ClientId.Size)return xmqttSError(XMQTT_STORE_CORRUPT,"identity must be bound exactly once");
        if(S->Config.ClientId.Size&&(S->Config.ClientId.Size!=D->Data.Size||memcmp(S->Config.ClientId.Data,D->Data.Data,D->Data.Size)))return xmqttSError(XMQTT_STORE_STATE_ERROR,"bound ClientId differs");
        if(D->Data.Size>S->Config.MaxMemoryBytes-S->Info.MemoryBytes)return XMQTT_STORE_LIMIT;
        char* Id=xrtMalloc(D->Data.Size);if(!Id)return XMQTT_STORE_ERROR;memcpy(Id,D->Data.Data,D->Data.Size);
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
    }else if(D->Type==XMQTT_STORE_INBOX_ACK){
        xmqttstoreentry* E=xmqttSFind(S,D->Key);
        if(!E||E->Out||!E->View.InboxPending||E->View.Epoch!=D->Epoch)return xmqttSError(XMQTT_STORE_CORRUPT,"stale or repeated application delivery acknowledgement");
        E->View.InboxPending=false;--S->Info.Inbox;if(!E->Receive)xmqttSDelete(S,E);
    }else {
        if(D->Epoch!=S->Info.Epoch)return xmqttSError(XMQTT_STORE_CORRUPT,"stale protocol epoch");
        if(D->Type==XMQTT_STORE_END){
            if(!S->Live||(!(D->Flags&1u)&&D->Expiry!=S->Expiry)||(!S->Expiry&&D->Expiry)||
               (S->Config.Delta.Version==XMQTT_VERSION_311&&D->Expiry!=S->Expiry))return xmqttSError(XMQTT_STORE_CORRUPT,"invalid connection-end expiry transition");
            S->Live=false;S->Expiry=D->Expiry;S->ClosedWall=D->WallMilliseconds;if(!S->Expiry)xmqttSClear(S);
        }else if(D->Type==XMQTT_STORE_CLEAR){
            if(S->Live||(D->Reason==1&&!xmqttSExpired(S,D->WallMilliseconds)))return xmqttSError(XMQTT_STORE_CORRUPT,"invalid live or premature expiry reset");
            xmqttSClear(S);
        }else {
            if(!S->Live)return xmqttSError(XMQTT_STORE_CORRUPT,"protocol transition while offline");
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
    for(xmqttstoreentry* E=S->Head,*Next;E;E=Next){Next=E->Next;xrtFree(E);}
    xrtFree((ptr)S->Info.ClientId.Data);xrtFree(S);
}
static bool xmqttSOutput(const xmqttstorestate* S,void* P,size_t N)
{
    if(!S||!xmqttSSpan(P,N))return false;
    uintptr_t A=(uintptr_t)P;
    if(A>=S->Highest||A+N<=S->Lowest)return true;
    if(!xmqttSSeparate(P,N,S,S->Info.BaseBytes)||!xmqttSSeparate(P,N,S->Info.ClientId.Data,S->Info.ClientId.Size))return false;
    for(xmqttstoreentry* E=S->Head;E;E=E->Next)if(!xmqttSSeparate(P,N,E,E->Allocation))return false;
    return true;
}
XMQTT_API bool xmqttStoreStateInfo(const xmqttstorestate* S,xmqttstorestateinfo* I)
{if(!xmqttSOutput(S,I,sizeof(*I))){xmqttSError(XMQTT_STORE_ERROR,"invalid recovery info output");return false;}*I=S->Info;return true;}
XMQTT_API xmqttstorestatus xmqttStoreStateView(const xmqttstorestate* S,xmqttstoreviewkind K,size_t Index,xmqttstoreview* V)
{
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
    if(!xmqttSOutput(S,Output,Bytes))return xmqttSError(XMQTT_STORE_ERROR,"import output overlaps recovered state");
    if(S->Config.Generation!=Generation||S->Config.Binding.Size!=Binding.Size||memcmp(S->Config.Binding.Data,Binding.Data,Binding.Size))
        return xmqttSError(XMQTT_STORE_STATE_ERROR,"import broker/principal/slot or storage generation differs");
    *RecoveryWall=S->Config.WallMilliseconds;return XMQTT_STORE_DONE;
}
#endif
XMQTT_API xmqttstorestatus xmqttStoreStateRecover(xmqttjournal* J,const xmqttstorestateconfig* Config,xmqttstorestate** Out)
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
    for(;;){
        xmqttjournalrecord R;xmqttjournalstatus JS=xmqttJournalNext(J,&R);
        if(JS==XMQTT_JOURNAL_EMPTY)break;
        if(JS!=XMQTT_JOURNAL_DONE){Result=JS==XMQTT_JOURNAL_LIMIT?XMQTT_STORE_LIMIT:JS==XMQTT_JOURNAL_CORRUPT?XMQTT_STORE_CORRUPT:JS==XMQTT_JOURNAL_IO||JS==XMQTT_JOURNAL_UNCERTAIN?XMQTT_STORE_IO:XMQTT_STORE_ERROR;goto Finish;}
        if(R.Sequence!=S->Info.Transactions+1u||R.Sequence>Transactions){Result=xmqttSError(XMQTT_STORE_CORRUPT,"invalid recovery transaction sequence");goto Finish;}
        if(R.Data.Size>K.MaxReplayBytes-S->Info.ReplayBytes){Result=XMQTT_STORE_LIMIT;goto Finish;}
        xmqttstoredeltadocument Doc;xmqttstoredeltastatus DS=xmqttStoreDeltaRead(&K.Delta,R.Data,D,K.Delta.MaxOperations,P,K.Delta.Codec.MaxProperties,&Doc);
        if(DS!=XMQTT_STORE_DELTA_DONE){Result=DS==XMQTT_STORE_DELTA_LIMIT?XMQTT_STORE_LIMIT:DS==XMQTT_STORE_DELTA_ERROR?XMQTT_STORE_ERROR:XMQTT_STORE_CORRUPT;goto Finish;}
        if(Doc.Count>K.MaxOperations-S->Info.Operations){Result=XMQTT_STORE_LIMIT;goto Finish;}
        for(size_t I=0;I<Doc.Count;++I){Result=xmqttSApply(S,D+I);if(Result!=XMQTT_STORE_DONE)goto Finish;}
        ++S->Info.Transactions;S->Info.Operations+=Doc.Count;S->Info.ReplayBytes+=R.Data.Size;
    }
    if(S->Info.Transactions!=Transactions){Result=xmqttSError(XMQTT_STORE_CORRUPT,"incomplete recovered prefix");goto Finish;}
    if(K.WallMilliseconds<S->Info.LastWallMilliseconds){Result=xmqttSError(XMQTT_STORE_STATE_ERROR,"trusted recovery wall clock went backwards");goto Finish;}
    S->Info.LiveAtCut=S->Live;
    if((S->Live&&!S->Expiry)||xmqttSExpired(S,K.WallMilliseconds))xmqttSClear(S);
    if(S->Info.HasSession)S->Info.RemainingMilliseconds=(S->Live||S->Expiry==UINT32_MAX)?UINT64_MAX:(uint64)S->Expiry*1000u-(K.WallMilliseconds-S->ClosedWall);
    size_t Counts[3]={0};for(xmqttstoreentry* E=S->Head;E;E=E->Next){if(E->Out)S->Views[0][Counts[0]++]=E;if(E->Receive)S->Views[1][Counts[1]++]=E;if(E->View.InboxPending)S->Views[2][Counts[2]++]=E;}
    S->Info.MemoryBytes-=Ops+Props;S->Config.ClientId=(xstrview){0};Result=XMQTT_STORE_DONE;
    S->Lowest=(uintptr_t)S;S->Highest=S->Lowest+Base;
    if(S->Info.ClientId.Size){uintptr_t A=(uintptr_t)S->Info.ClientId.Data;if(A<S->Lowest)S->Lowest=A;if(A+S->Info.ClientId.Size>S->Highest)S->Highest=A+S->Info.ClientId.Size;}
    for(xmqttstoreentry* E=S->Head;E;E=E->Next){uintptr_t A=(uintptr_t)E;if(A<S->Lowest)S->Lowest=A;if(A+E->Allocation>S->Highest)S->Highest=A+E->Allocation;}
Finish:
    xrtFree(D);xrtFree(P);
    if(Result==XMQTT_STORE_DONE||Result==XMQTT_STORE_LIMIT){xerror* Previous=NULL;
        if(!xrtTrySwapErrorTake(Saved,&Previous)){xrtErrorFree(Saved);Result=XMQTT_STORE_ERROR;}xrtErrorFree(Previous);
    }else xrtErrorFree(Saved);
    if(Result==XMQTT_STORE_DONE)*Out=S;else xmqttStoreStateDestroy(S);return Result;
}
#endif
