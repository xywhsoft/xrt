#include <xmqtt/session.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include "../internal/xmqtt_internal.h"
#include <math.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
enum {XMQTT_SESSION_ID_BYTES=8192,XMQTT_SESSION_RX_BYTES=16384};
struct xmqttsessionpacket {
    xatomic32 Refs;xmqttsession* Owner;
    xmqttsessionpacket *Next,*Previous,*HashNext,*ReadyNext,*ReadyPrevious;
    size_t Allocation,Bytes;uint16 Identifier;uint8 Qos,Reason,PubrecReason,ReadyKind,SelectedType;
    bool Active,Sent,Attempted,ReleaseSent,PubrecReceived,Need,Retry;
    xmqttsessionstage Stage;unsigned char Duplicate,Release[4];
    xmqttpacket Packet;xmqttproperty Properties[];
};
struct xmqttsession {
    xatomic32 Refs;xmutex Gate;xmqttsessionconfig Config;xmqttnegotiated Peer;
    size_t BaseBytes,LiveBytes,Objects,Active,RxCount,Prepared,Buckets;
    xmqttsessionpacket **Hash,*Head,*Tail,*ReadyHead[2],*ReadyTail[2],*Selected;
    unsigned char *Ids,*Receive;uint16 Next,Quota,InitialQuota;uint64 Generation;
    bool Connected,HasState,Persistent,InfiniteExpiry;uint32 Expiry;double LastNow,ExpiresAt,ClosedAt;
    unsigned char Data[];
};
static xmqttsessionstatus xmqttSessionFailure(xmqttsessionstatus Status,cstr Text)
{
    xerrkind K=Status==XMQTT_SESSION_STATE_ERROR?XERR_STATE:Status==XMQTT_SESSION_PROTOCOL_ERROR?XERR_PROTOCOL:Status==XMQTT_SESSION_RESTRICTED?XERR_RANGE:XERR_ARGUMENT;
    xrtSetErrorInfo(K,"xrt.mqtt",6001-(int32)Status,Text);return Status;
}
static bool xmqttSessionSpan(const void* P,size_t N)
{return (P || !N) && xrtMemRangeValid(P,N);}
static bool xmqttSessionSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N || !M || !xrtMemRangesOverlap(A,N,B,M);}
static bool xmqttSessionOutput(xmqttsession* S,const void* P,size_t N)
{return xmqttSessionSpan(P,N) && xmqttSessionSeparate(P,N,S,S->BaseBytes);}
/* Descriptor validation precedes this traversal. */
static bool xmqttSessionPropertiesOutput(const void* O,size_t N,xmqttpropertylist L)
{
    if(!xmqttSessionSeparate(O,N,L.Data,L.Count*sizeof(*L.Data)))return false;
    for(size_t I=0;I<L.Count;++I){const xmqttproperty* P=L.Data+I;
        if(P->Type==XMQTT_PROPERTY_STRING && !xmqttSessionSeparate(O,N,P->Value.Text.Data,P->Value.Text.Size))return false;
        if(P->Type==XMQTT_PROPERTY_BINARY && !xmqttSessionSeparate(O,N,P->Value.Binary.Data,P->Value.Binary.Size))return false;
        if(P->Type==XMQTT_PROPERTY_STRING_PAIR && (!xmqttSessionSeparate(O,N,P->Value.Pair.Name.Data,P->Value.Pair.Name.Size) || !xmqttSessionSeparate(O,N,P->Value.Pair.Value.Data,P->Value.Pair.Value.Size)))return false;
    }return true;
}
static bool xmqttSessionPacketOutput(const void* O,size_t N,const xmqttpacket* P)
{
    if(!xmqttSessionSeparate(O,N,P,sizeof(*P)) || !xmqttSessionPropertiesOutput(O,N,P->Properties))return false;
    if(P->Type==XMQTT_PACKET_PUBLISH)return xmqttSessionSeparate(O,N,P->Body.Publish.Topic.Data,P->Body.Publish.Topic.Size) && xmqttSessionSeparate(O,N,P->Body.Publish.Payload.Data,P->Body.Publish.Payload.Size);
    if(P->Type==XMQTT_PACKET_CONNECT){const xmqttconnectpacket* C=&P->Body.Connect;
        if(!xmqttSessionSeparate(O,N,C->ClientId.Data,C->ClientId.Size))return false;
        if(C->HasUsername && !xmqttSessionSeparate(O,N,C->Username.Data,C->Username.Size))return false;
        if(C->HasPassword && !xmqttSessionSeparate(O,N,C->Password.Data,C->Password.Size))return false;
        if(C->HasWill && (!xmqttSessionSeparate(O,N,C->WillTopic.Data,C->WillTopic.Size) || !xmqttSessionSeparate(O,N,C->WillPayload.Data,C->WillPayload.Size) || !xmqttSessionPropertiesOutput(O,N,C->WillProperties)))return false;
    }return true;
}
static bool xmqttSessionSizePacket(xmqttsession* S,const xmqttpacket* P,xmqttdirection D,size_t* N)
{return S->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(P,D,&S->Config.Codec,N):xmqttPacketSize311(P,D,&S->Config.Codec,N);}
static void xmqttSessionDrop(xmqttsession* S)
{if(xrtAtomic32FetchSub(&S->Refs,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&S->Gate);xrtFree(S);}}
static void xmqttSessionPacketRef(xmqttsessionpacket* P)
{(void)xrtAtomic32FetchAdd(&P->Refs,1,XMEMORY_RELAXED);}
XMQTT_API bool xmqttSessionPacketRetain(const xmqttsessionpacket* Packet)
{
    if(!Packet){xmqttSessionFailure(XMQTT_SESSION_ERROR,"session packet is NULL");return false;}
    xmqttsessionpacket* P=(xmqttsessionpacket*)Packet;uint32 N=xrtAtomic32Load(&P->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&P->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session packet reference limit");return false;
}
XMQTT_API void xmqttSessionPacketRelease(const xmqttsessionpacket* Packet)
{
    xmqttsessionpacket* P=(xmqttsessionpacket*)Packet;if(!P || xrtAtomic32FetchSub(&P->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttsession* S=P->Owner;xrtMutexLock(&S->Gate);S->LiveBytes-=P->Allocation;--S->Objects;S->Ids[P->Identifier>>3]&=(unsigned char)~(1u<<(P->Identifier&7u));xrtMutexUnlock(&S->Gate);
    xrtFree(P);xmqttSessionDrop(S);
}
static void xmqttSessionListRelease(xmqttsessionpacket* P)
{while(P){xmqttsessionpacket* Next=P->Next;xmqttSessionPacketRelease(P);P=Next;}}
static unsigned xmqttSessionReceiveState(const xmqttsession* S,uint16 Id)
{return S->Receive?(S->Receive[Id>>2]>>((Id&3u)*2u))&3u:0;}
static void xmqttSessionReceiveSet(xmqttsession* S,uint16 Id,unsigned State)
{unsigned Shift=(Id&3u)*2u;S->Receive[Id>>2]=(unsigned char)((S->Receive[Id>>2]&~(3u<<Shift))|(State<<Shift));}
static size_t xmqttSessionHash(const xmqttsession* S,uint16 Id)
{return ((uint32)Id*2654435761u)&(S->Buckets-1u);}
static xmqttsessionpacket* xmqttSessionFind(xmqttsession* S,uint16 Id)
{
    if(!S->Hash)return NULL;
    for(xmqttsessionpacket* P=S->Hash[xmqttSessionHash(S,Id)];P;P=P->HashNext)if(P->Identifier==Id)return P;
    return NULL;
}
static void xmqttSessionReadyRemove(xmqttsession* S,xmqttsessionpacket* P)
{
    if(!P->ReadyKind)return;
    unsigned K=P->ReadyKind-1u;
    if(P->ReadyPrevious)P->ReadyPrevious->ReadyNext=P->ReadyNext;else S->ReadyHead[K]=P->ReadyNext;
    if(P->ReadyNext)P->ReadyNext->ReadyPrevious=P->ReadyPrevious;else S->ReadyTail[K]=P->ReadyPrevious;
    P->ReadyKind=0;P->ReadyNext=P->ReadyPrevious=NULL;
}
static void xmqttSessionReadyAdd(xmqttsession* S,xmqttsessionpacket* P)
{
    P->Need=true;if(P->ReadyKind || S->Selected==P)return;
    unsigned K=P->Stage==XMQTT_SESSION_PUBREL?1u:0u;P->ReadyKind=(uint8)(K+1u);P->ReadyPrevious=S->ReadyTail[K];P->ReadyNext=NULL;
    if(S->ReadyTail[K])S->ReadyTail[K]->ReadyNext=P;else S->ReadyHead[K]=P;S->ReadyTail[K]=P;
}
static void xmqttSessionUnlink(xmqttsession* S,xmqttsessionpacket* P)
{
    xmqttSessionReadyRemove(S,P);xmqttsessionpacket** Link=S->Hash+xmqttSessionHash(S,P->Identifier);while(*Link!=P)Link=&(*Link)->HashNext;*Link=P->HashNext;
    if(P->Previous)P->Previous->Next=P->Next;else S->Head=P->Next;
    if(P->Next)P->Next->Previous=P->Previous;else S->Tail=P->Previous;
    P->Next=P->Previous=P->HashNext=NULL;P->Active=false;P->Need=false;--S->Active;
}
static xmqttsessionpacket* xmqttSessionClearLocked(xmqttsession* S)
{
    xmqttsessionpacket* P=S->Head;S->Head=S->Tail=NULL;S->Active=0;
    for(xmqttsessionpacket* Q=P;Q;Q=Q->Next){Q->Active=Q->Need=false;Q->ReadyKind=0;Q->Stage=XMQTT_SESSION_DISCARDED;}
    if(S->Hash)memset(S->Hash,0,S->Buckets*sizeof(*S->Hash));
    if(S->Receive)memset(S->Receive,0,XMQTT_SESSION_RX_BYTES);
    S->ReadyHead[0]=S->ReadyHead[1]=S->ReadyTail[0]=S->ReadyTail[1]=NULL;
    S->RxCount=S->Prepared=0;S->HasState=false;S->ExpiresAt=INFINITY;return P;
}
static bool xmqttSessionTime(xmqttsession* S,double Now)
{return isfinite(Now) && Now>=0 && Now>=S->LastNow;}
static bool xmqttSessionAvailableAt(const xmqttsession* S,double Now)
{return S->HasState && (S->InfiniteExpiry || Now-S->ClosedAt<(double)S->Expiry);}
XMQTT_API bool xmqttSessionConfigInit(xmqttsessionconfig* C)
{
    if(!xmqttSessionSpan(C,sizeof(*C))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session config output");return false;}
    *C=(xmqttsessionconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;xmqttCodecConfigInit(&C->Codec);C->MaxOutgoing=128;C->MaxIncoming=64;C->MaxBytes=16u*1024u*1024u;return true;
}
XMQTT_API bool xmqttSessionCreate(const xmqttsessionconfig* Config,xmqttsession** Out)
{
    if(!xmqttSessionSpan(Config,sizeof(*Config)) || !xmqttSessionSpan(Out,sizeof(*Out)) || !xmqttSessionSeparate(Config,sizeof(*Config),Out,sizeof(*Out))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session config/output");return false;}
    xmqttsessionconfig C=*Config;xmqttcodecconfig Codec;
    if(C.Size!=sizeof(C) || (C.Version!=XMQTT_VERSION_5 && C.Version!=XMQTT_VERSION_311) || C.MaxOutgoing>65535 || C.MaxIncoming>65535 || !C.ClientId.Size || C.ClientId.Size>65535 ||
       !xmqttSessionSpan(C.ClientId.Data,C.ClientId.Size) || !xmqttSessionSeparate(Out,sizeof(*Out),C.ClientId.Data,C.ClientId.Size) ||
       !xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&C.Codec,&Codec) || !xmqttUtf8StringCheck(C.ClientId,C.Codec.Utf8Flags,NULL)){
        xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session settings/identity");return false;}
    size_t Buckets=0;if(C.MaxOutgoing){Buckets=2;while(Buckets<C.MaxOutgoing*2u)Buckets*=2u;}
    size_t Base=sizeof(xmqttsession)+Buckets*sizeof(xmqttsessionpacket*)+(C.MaxOutgoing?XMQTT_SESSION_ID_BYTES:0u)+(C.MaxIncoming?XMQTT_SESSION_RX_BYTES:0u)+C.ClientId.Size;
    if(Base>C.MaxBytes){xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session fixed maps exceed byte budget");return false;}
    xmqttsession* S=xrtCalloc(1,Base);if(!S)return false;
    if(!xrtMutexInit(&S->Gate)){xrtFree(S);return false;}
    xrtAtomic32Init(&S->Refs,1);S->Config=C;S->Config.Codec=Codec;S->BaseBytes=S->LiveBytes=Base;S->Buckets=Buckets;S->Next=1;S->ExpiresAt=INFINITY;
    unsigned char* At=S->Data;
    if(Buckets){S->Hash=(xmqttsessionpacket**)At;At+=Buckets*sizeof(*S->Hash);S->Ids=At;At+=XMQTT_SESSION_ID_BYTES;}
    if(C.MaxIncoming){S->Receive=At;At+=XMQTT_SESSION_RX_BYTES;}
    memcpy(At,C.ClientId.Data,C.ClientId.Size);S->Config.ClientId=(xstrview){(const char*)At,C.ClientId.Size};*Out=S;return true;
}
XMQTT_API xmqttsessionstatus xmqttSessionOpen(xmqttsession* S,const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out)))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session negotiation output");
    xrtMutexLock(&S->Gate);bool Available=xmqttSessionAvailableAt(S,Now);
    if(S->Connected || !xmqttSessionTime(S,Now) || S->Generation==UINT64_MAX){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"invalid session generation/time");}
    xmqttnegotiationconfig C;xmqttNegotiationConfigInit(&C);C.Version=S->Config.Version;C.Codec=S->Config.Codec;C.HasSessionState=Available;
    xmqttnegotiated N;xmqttnegotiationstatus Status=xmqttNegotiate(Connect,Connack,&C,&N,NULL);
    if(Status<0){xrtMutexUnlock(&S->Gate);return Status==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_SESSION_STATE_ERROR:Status==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_SESSION_ERROR:XMQTT_SESSION_PROTOCOL_ERROR;}
    if(!xmqttSessionPacketOutput(Out,sizeof(*Out),Connect) || !xmqttSessionPacketOutput(Out,sizeof(*Out),Connack)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"overlapping negotiation output");}
    if(Connect->Body.Connect.ClientId.Size!=S->Config.ClientId.Size || memcmp(Connect->Body.Connect.ClientId.Data,S->Config.ClientId.Data,S->Config.ClientId.Size)){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session ClientId mismatch");}
    if(Status==XMQTT_NEGOTIATION_REJECTED){*Out=N;xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_REJECTED;}
    if(N.SessionPresent && !Available){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"server session without local state");}
    if(S->Config.Version==XMQTT_VERSION_311 && Available && !Connect->Body.Connect.CleanStart && !N.SessionPresent){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"311 server lost session; explicit reset required");}
    bool Reset=S->HasState && (!Available || Connect->Body.Connect.CleanStart || !N.SessionPresent);
    xmqttsessionpacket* Drop=Reset?xmqttSessionClearLocked(S):NULL;
    S->Connected=S->HasState=true;S->Persistent=!Connect->Body.Connect.CleanStart;S->Peer=N;
    S->Peer.ClientId=S->Config.ClientId;S->Peer.AuthMethod=S->Peer.ServerReference=S->Peer.ResponseInfo=(xstrview){0};S->Peer.HasAuthMethod=false;
    S->LastNow=Now;S->ExpiresAt=INFINITY;S->InfiniteExpiry=true;++S->Generation;S->Quota=S->InitialQuota=N.SendReceiveMaximum;
    for(xmqttsessionpacket* P=S->Head;P;P=P->Next){P->ReleaseSent=false;xmqttSessionReadyAdd(S,P);}
    *Out=N;xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return Reset?XMQTT_SESSION_RESET:XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionEnd(xmqttsession* S,const xmqttpacket* Disconnect,double Now)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);
    if(!S->Connected || !xmqttSessionTime(S,Now)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session is not connected or time went backwards");}
    uint32 Expiry=S->Peer.SessionExpiry;
    if(Disconnect){
        if(!xmqttSessionSpan(Disconnect,sizeof(*Disconnect)) || Disconnect->Type!=XMQTT_PACKET_DISCONNECT){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid ended DISCONNECT");}
        if(xmqttNegotiatedPacketCheck(&S->Peer,Disconnect,&S->Config.Codec,NULL,NULL)!=XMQTT_NEGOTIATION_DONE){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
        for(size_t I=0;I<Disconnect->Properties.Count;++I)if(Disconnect->Properties.Data[I].Id==XMQTT_PROPERTY_SESSION_EXPIRY)Expiry=Disconnect->Properties.Data[I].Value.Number;
    }
    S->Connected=false;S->LastNow=Now;S->Quota=S->InitialQuota=0;xmqttsessionpacket* Selection=S->Selected;S->Selected=NULL;
    S->ReadyHead[0]=S->ReadyHead[1]=S->ReadyTail[0]=S->ReadyTail[1]=NULL;
    for(xmqttsessionpacket* P=S->Head;P;P=P->Next)P->ReadyKind=0;
    bool Retain=S->Config.Version==XMQTT_VERSION_5?Expiry!=0:S->Persistent;
    xmqttsessionpacket* Drop=NULL;
    if(!Retain)Drop=xmqttSessionClearLocked(S);
    else {S->ClosedAt=Now;S->Expiry=Expiry;S->InfiniteExpiry=S->Config.Version==XMQTT_VERSION_311 || Expiry==UINT32_MAX;S->ExpiresAt=S->InfiniteExpiry?INFINITY:Now+(double)Expiry;
        if(S->Prepared){for(uint32 Id=1;Id<=65535;++Id)if(xmqttSessionReceiveState(S,(uint16)Id)==1){xmqttSessionReceiveSet(S,(uint16)Id,0);--S->RxCount;}S->Prepared=0;}}
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(Selection);xmqttSessionListRelease(Drop);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionExpire(xmqttsession* S,double Now)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);
    if(S->Connected || !xmqttSessionTime(S,Now)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot expire live session or reverse time");}
    S->LastNow=Now;bool Expired=S->HasState && !xmqttSessionAvailableAt(S,Now);xmqttsessionpacket* Drop=Expired?xmqttSessionClearLocked(S):NULL;
    xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return Expired?XMQTT_SESSION_DONE:XMQTT_SESSION_EMPTY;
}
XMQTT_API xmqttsessionstatus xmqttSessionReset(xmqttsession* S)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);if(S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot reset live session");}
    xmqttsessionpacket* Drop=xmqttSessionClearLocked(S);xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionPublish(xmqttsession* S,const xmqttpacket* Publish,const xmqttsessionpacket** Out)
{
    if(!S || !xmqttSessionSpan(Publish,sizeof(*Publish)) || !xmqttSessionOutput(S,Out,sizeof(*Out)) || Publish->Type!=XMQTT_PACKET_PUBLISH || (Publish->Flags&8u) || ((Publish->Flags>>1)&3u)==0 || ((Publish->Flags>>1)&3u)>2 || !Publish->Body.Publish.Topic.Size)
        return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session publish requires new QoS1/2 and full logical topic");
    xmqttpacket P=*Publish;if(!P.Identifier)P.Identifier=1;size_t Capacity;
    if(!xmqttSessionSizePacket(S,&P,XMQTT_CLIENT_TO_SERVER,&Capacity))return XMQTT_SESSION_ERROR;
    if(!xmqttSessionPacketOutput(Out,sizeof(*Out),Publish))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"overlapping packet output");
    size_t Count=P.Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttsessionpacket))/sizeof(xmqttproperty) || Capacity>SIZE_MAX-sizeof(xmqttsessionpacket)-Count*sizeof(xmqttproperty))return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session packet size overflow");
    size_t Allocation=sizeof(xmqttsessionpacket)+Count*sizeof(xmqttproperty)+Capacity;
    xrtMutexLock(&S->Gate);
    if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot admit packet offline");}
    if(S->Objects>=S->Config.MaxOutgoing || Allocation>S->Config.MaxBytes-S->LiveBytes){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}
    uint16 Id=Publish->Identifier;
    if(Id && (S->Ids[Id>>3]&(1u<<(Id&7u)))){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}
    if(!Id){Id=S->Next;while(S->Ids[Id>>3]&(1u<<(Id&7u)))Id=Id==65535?1:(uint16)(Id+1u);}
    xmqttsessionpacket* R=xrtMalloc(Allocation);if(!R){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
    memset(R,0,sizeof(*R));R->Owner=S;R->Identifier=P.Identifier=Id;R->Qos=(P.Flags>>1)&3u;R->Allocation=Allocation;R->Active=true;R->Stage=XMQTT_SESSION_PUBLISH;
    size_t Properties=0;for(size_t I=0;I<Count;++I)if(P.Properties.Data[I].Id!=XMQTT_PROPERTY_TOPIC_ALIAS)R->Properties[Properties++]=P.Properties.Data[I];
    P.Properties=(xmqttpropertylist){R->Properties,Properties};unsigned char* Wire=(unsigned char*)(R->Properties+Count);size_t Bytes=0;
    bool Written=S->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,Capacity,&P,XMQTT_CLIENT_TO_SERVER,&S->Config.Codec,&Bytes):xmqttPacketWrite311(Wire,Capacity,&P,XMQTT_CLIENT_TO_SERVER,&S->Config.Codec,&Bytes);
    xmqttcodecbuffers Buffers={R->Properties,Count,NULL,0};
    xmqttcodecstatus Read=Written?(S->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,&S->Config.Codec,&Buffers,&R->Packet,NULL):xmqttPacketRead311((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,&S->Config.Codec,&Buffers,&R->Packet,NULL)):XMQTT_CODEC_LOCAL_ERROR;
    if(Read!=XMQTT_CODEC_DONE){xrtFree(R);xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
    if(Bytes>S->Peer.SendPacketBytes || R->Qos>S->Peer.MaximumQos || ((P.Flags&1u) && !S->Peer.RetainAvailable)){
        xrtFree(R);xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"publish exceeds negotiated peer capability");}
    R->Bytes=Bytes;R->Duplicate=(unsigned char)(Wire[0]|8u);R->Release[0]=0x62;R->Release[1]=2;R->Release[2]=(unsigned char)(Id>>8);R->Release[3]=(unsigned char)Id;
    R->Previous=S->Tail;if(S->Tail)S->Tail->Next=R;else S->Head=R;S->Tail=R;
    size_t Bucket=xmqttSessionHash(S,Id);R->HashNext=S->Hash[Bucket];S->Hash[Bucket]=R;
    S->Ids[Id>>3]|=(unsigned char)(1u<<(Id&7u));S->Next=Id==65535?1:(uint16)(Id+1u);++S->Objects;++S->Active;S->LiveBytes+=Allocation;
    (void)xrtAtomic32FetchAdd(&S->Refs,1,XMEMORY_RELAXED);xrtAtomic32Init(&R->Refs,2);xmqttSessionReadyAdd(S,R);
    xrtMutexUnlock(&S->Gate);*Out=R;return XMQTT_SESSION_DONE;
}
static xmqttsessionframe xmqttSessionFrame(xmqttsession* S,xmqttsessionpacket* P)
{
    xmqttsessionframe F={0};F.Size=sizeof(F);F.Packet=P;F.Generation=S->Generation;F.Type=(xmqttpackettype)P->SelectedType;F.Identifier=P->Identifier;
    if(F.Type==XMQTT_PACKET_PUBREL)F.Prefix=(xbytesview){P->Release,4};
    else {F.Prefix=(xbytesview){P->Retry?&P->Duplicate:P->Packet.Wire.Data,1};F.Tail=(xbytesview){P->Packet.Wire.Data+1,P->Bytes-1};}
    return F;
}
XMQTT_API xmqttsessionstatus xmqttSessionNext(xmqttsession* S,xmqttsessionframe* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out)))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session frame output");
    xrtMutexLock(&S->Gate);if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot select frame offline");}
    xmqttsessionpacket* P=S->ReadyHead[1]?S->ReadyHead[1]:S->Quota?S->ReadyHead[0]:NULL;
    if(S->Selected || !P){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_EMPTY;}
    if(!xmqttSessionSeparate(Out,sizeof(*Out),P,P->Allocation)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"frame output overlaps owned packet");}
    bool Release=P->Stage==XMQTT_SESSION_PUBREL;
    if((Release?4:P->Bytes)>S->Peer.SendPacketBytes || (!Release && (P->Qos>S->Peer.MaximumQos || ((P->Packet.Flags&1u) && !S->Peer.RetainAvailable)))){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"resumed packet exceeds new peer capability");}
    xmqttSessionReadyRemove(S,P);P->Need=false;P->SelectedType=Release?XMQTT_PACKET_PUBREL:XMQTT_PACKET_PUBLISH;P->Retry=P->Attempted;S->Selected=P;
    if(!Release){--S->Quota;P->Attempted=true;}
    xmqttSessionPacketRef(P);xmqttSessionPacketRef(P);*Out=xmqttSessionFrame(S,P);
    xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionSent(xmqttsession* S,const xmqttsessionframe* F)
{
    if(!S || !xmqttSessionSpan(F,sizeof(*F)) || F->Size!=sizeof(*F))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session frame token");
    xrtMutexLock(&S->Gate);xmqttsessionpacket* P=S->Selected;
    if(!S->Connected || !P || F->Packet!=P || F->Generation!=S->Generation){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"stale session frame generation");}
    xmqttsessionframe Expected=xmqttSessionFrame(S,P);
    if(F->Type!=Expected.Type || F->Identifier!=Expected.Identifier || F->Prefix.Data!=Expected.Prefix.Data || F->Prefix.Size!=Expected.Prefix.Size || F->Tail.Data!=Expected.Tail.Data || F->Tail.Size!=Expected.Tail.Size){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"altered session frame token");}
    S->Selected=NULL;if(F->Type==XMQTT_PACKET_PUBLISH)P->Sent=true;else P->ReleaseSent=true;
    if(P->Active && P->Need)xmqttSessionReadyAdd(S,P);
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(P);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionAcknowledge(xmqttsession* S,const xmqttpacket* Ack)
{
    if(!S || !xmqttSessionSpan(Ack,sizeof(*Ack)) || (Ack->Type!=XMQTT_PACKET_PUBACK && Ack->Type!=XMQTT_PACKET_PUBREC && Ack->Type!=XMQTT_PACKET_PUBCOMP))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session acknowledgement");
    size_t Bytes;if(!xmqttSessionSizePacket(S,Ack,XMQTT_SERVER_TO_CLIENT,&Bytes))return XMQTT_SESSION_ERROR;
    xrtMutexLock(&S->Gate);if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"acknowledgement while offline");}
    xmqttsessionpacket* P=xmqttSessionFind(S,Ack->Identifier);
    if(Ack->Type==XMQTT_PACKET_PUBCOMP && (!P || P->Qos!=2 || P->Stage!=XMQTT_SESSION_PUBREL || !P->ReleaseSent)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DUPLICATE;}
    if(!P || !P->Sent || (Ack->Type==XMQTT_PACKET_PUBACK && P->Qos!=1) || (Ack->Type==XMQTT_PACKET_PUBREC && P->Qos!=2) || (Ack->Type==XMQTT_PACKET_PUBCOMP && !P->ReleaseSent)){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_PROTOCOL_ERROR,"acknowledgement does not match a sent flow");}
    if(Ack->Type==XMQTT_PACKET_PUBREC && P->Stage==XMQTT_SESSION_PUBREL){
        if(Ack->Reason>=0x80){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_PROTOCOL_ERROR,"negative PUBREC after accepted PUBREC");}
        xmqttSessionReadyAdd(S,P);xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DUPLICATE;
    }
    if(Ack->Type==XMQTT_PACKET_PUBREC){P->PubrecReceived=true;P->PubrecReason=Ack->Reason;
        if(Ack->Reason<0x80){P->Stage=XMQTT_SESSION_PUBREL;P->ReleaseSent=false;xmqttSessionReadyAdd(S,P);xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;}}
    P->Reason=Ack->Reason;P->Stage=XMQTT_SESSION_COMPLETE;xmqttSessionUnlink(S,P);
    if(S->Quota<S->InitialQuota)++S->Quota;
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(P);
    return Ack->Reason>=0x80?XMQTT_SESSION_REJECTED:XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionReceiveChange(xmqttsession* S,uint16 Id,unsigned Operation,uint8* Reason)
{
    if(!S || !Id || (Reason && !xmqttSessionOutput(S,Reason,sizeof(*Reason))))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid incoming session identifier/output");
    xrtMutexLock(&S->Gate);if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"incoming state while offline");}
    unsigned State=xmqttSessionReceiveState(S,Id);xmqttsessionstatus Result=XMQTT_SESSION_DONE;
    if(!Operation){
        if(State==1)Result=XMQTT_SESSION_STATE_ERROR;
        else if(State)Result=XMQTT_SESSION_DUPLICATE;
        else if(S->RxCount==S->Config.MaxIncoming)Result=XMQTT_SESSION_LIMIT;
        else {xmqttSessionReceiveSet(S,Id,1);++S->RxCount;++S->Prepared;}
    }else if(Operation==1){if(State!=1)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,2);--S->Prepared;}}
    else if(Operation==2){if(State!=1)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,0);--S->RxCount;--S->Prepared;}}
    else if(Operation==3){
        if(State==1)Result=XMQTT_SESSION_STATE_ERROR;
        else if(!State){*Reason=S->Config.Version==XMQTT_VERSION_5?0x92:0;Result=XMQTT_SESSION_EMPTY;}
        else {xmqttSessionReceiveSet(S,Id,3);*Reason=0;}
    }else {if(State!=3)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,0);--S->RxCount;}}
    xrtMutexUnlock(&S->Gate);return Result==XMQTT_SESSION_STATE_ERROR?xmqttSessionFailure(Result,"invalid incoming session transition"):Result;
}
XMQTT_API xmqttsessionstatus xmqttSessionReceivePrepare(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,Id,0,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveCommit(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,Id,1,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveAbort(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,Id,2,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveRelease(xmqttsession* S,uint16 Id,uint8* Reason)
{if(!Reason)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"PUBCOMP reason output is NULL");return xmqttSessionReceiveChange(S,Id,3,Reason);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveComplete(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,Id,4,NULL);}
XMQTT_API bool xmqttSessionInfo(xmqttsession* S,xmqttsessioninfo* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session info output");return false;}
    xrtMutexLock(&S->Gate);xmqttsessioninfo I={0};I.Size=sizeof(I);I.Version=S->Config.Version;I.Connected=S->Connected;I.HasState=S->HasState;I.ActiveOutgoing=S->Active;I.Objects=S->Objects;I.LiveBytes=S->LiveBytes;I.BaseBytes=S->BaseBytes;
    I.Incoming=S->RxCount-S->Prepared;I.Prepared=S->Prepared;I.SendQuota=S->Quota;I.InitialSendQuota=S->InitialQuota;I.Generation=S->Generation;I.ExpiresAt=S->ExpiresAt;*Out=I;xrtMutexUnlock(&S->Gate);return true;
}
XMQTT_API const xmqttpacket* xmqttSessionPacket(const xmqttsessionpacket* P)
{return P?&P->Packet:NULL;}
XMQTT_API bool xmqttSessionPacketInfo(const xmqttsessionpacket* P,xmqttsessionpacketinfo* Out)
{
    if(!P || !xmqttSessionOutput(P->Owner,Out,sizeof(*Out)) || !xmqttSessionSeparate(Out,sizeof(*Out),P,P->Allocation)){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session packet info output");return false;}
    xmqttsession* S=P->Owner;xrtMutexLock(&S->Gate);xmqttsessionpacketinfo I={0};I.Size=sizeof(I);I.Identifier=P->Identifier;I.Qos=P->Qos;I.Reason=P->Reason;I.PubrecReason=P->PubrecReason;I.PubrecReceived=P->PubrecReceived;I.Active=P->Active;I.Sent=P->Sent;I.Stage=P->Stage;I.WireBytes=P->Bytes;*Out=I;xrtMutexUnlock(&S->Gate);return true;
}
XMQTT_API bool xmqttSessionDestroy(xmqttsession* S)
{
    if(!S){xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");return false;}
    xrtMutexLock(&S->Gate);if(S->Connected){xrtMutexUnlock(&S->Gate);xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot destroy connected session");return false;}
    xmqttsessionpacket* Drop=xmqttSessionClearLocked(S);xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);xmqttSessionDrop(S);return true;
}
#endif
