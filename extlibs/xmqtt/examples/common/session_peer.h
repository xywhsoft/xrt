#ifndef XMQTT_EXAMPLE_SESSION_PEER_H
#define XMQTT_EXAMPLE_SESSION_PEER_H
#include "client_peer.h"
/* Local literal peer for Session examples/fault tests, not a broker or protocol
 * implementation used by the product. Each instance accepts one connection. */
enum {MQTT_RESUME_HOLD_READY=1,MQTT_RESUME_HOLD_ACK=2,MQTT_RESUME_HOLD_REC=4,MQTT_RESUME_HOLD_COMP=8,MQTT_RESUME_EARLY_ACK=16};
typedef struct mqttResumeCaps {uint16 ReceiveMaximum,AliasMaximum;uint32 PacketMaximum;uint8 MaximumQos,Reason;uint16 EarlyId;} mqttResumeCaps;
typedef struct mqttResumePeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xmqttversion Version;unsigned Mode;bool Present;
    uint16 ReceiveMaximum,AliasMaximum;uint32 PacketMaximum;uint8 MaximumQos,Reason;
    xatomicptr Stream;xatomic32 Closed,ListenerClosed,Connects,Publishes,Releases,Pubacks,Pubrecs,Pubcomps,Disconnects,LogCount;
    struct {uint16 Id,Alias;uint8 Qos,Flags,Users;size_t Topic,Payload;unsigned char First,TopicFirst,LogicalFirst;uint64 UserHash;} Log[32];
    unsigned char AliasFirst;
    unsigned char Input[65536];size_t Used;uint16 EarlyId;
} mqttResumePeer;
static inline void mqttResumeWait(xatomic32* A,uint32 N)
{double End=xrtTimer()+8;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<N){mqttExampleCheck(xrtTimer()<End,"Session peer progress");xrtSleep(1);}}
static inline void mqttResumeSend(mqttResumePeer* P,const void* Data,size_t Size)
{mqttExampleCheck(xrtNetStreamSend(xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE),Data,Size)==XNET_RESULT_OK,"Session peer output");}
static inline xmqttproperty mqttResumeNumber(xmqttpropertyid Id,uint32 Number)
{xmqttproperty P={0};P.Id=Id;P.Type=Id==XMQTT_PROPERTY_RECEIVE_MAXIMUM || Id==XMQTT_PROPERTY_TOPIC_ALIAS || Id==XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM?XMQTT_PROPERTY_UINT16:Id==XMQTT_PROPERTY_MAXIMUM_QOS?XMQTT_PROPERTY_BYTE:XMQTT_PROPERTY_UINT32;P.Value.Number=Number;return P;}
static inline void mqttResumePacket(mqttResumePeer* P,xmqttpacket* Q)
{
    unsigned char Wire[2048];size_t N=0;bool OK=P->Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,sizeof(Wire),Q,XMQTT_SERVER_TO_CLIENT,NULL,&N):xmqttPacketWrite311(Wire,sizeof(Wire),Q,XMQTT_SERVER_TO_CLIENT,NULL,&N);
    mqttExampleCheck(OK,"Session peer encode");mqttResumeSend(P,Wire,N);
}
static inline void mqttResumeAck(mqttResumePeer* P,xmqttpackettype Type,uint16 Id)
{xmqttpacket Q;xmqttPacketInit(Type,&Q);Q.Identifier=Id;mqttResumePacket(P,&Q);}
static inline void mqttResumeConnack(mqttResumePeer* P)
{
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNACK,&Q);Q.SessionPresent=P->Present;Q.Reason=P->Reason;xmqttproperty Props[4];size_t N=0;
    if(P->Version==XMQTT_VERSION_5){if(P->ReceiveMaximum)Props[N++]=mqttResumeNumber(XMQTT_PROPERTY_RECEIVE_MAXIMUM,P->ReceiveMaximum);if(P->AliasMaximum)Props[N++]=mqttResumeNumber(XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,P->AliasMaximum);if(P->PacketMaximum)Props[N++]=mqttResumeNumber(XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,P->PacketMaximum);if(P->MaximumQos<2)Props[N++]=mqttResumeNumber(XMQTT_PROPERTY_MAXIMUM_QOS,P->MaximumQos);}
    Q.Properties=(xmqttpropertylist){Props,N};mqttResumePacket(P,&Q);
}
static inline void mqttResumeRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)S;mqttResumePeer* P=Data;size_t N=xrtNetBufSize(Buffer);mqttExampleCheck(N<=sizeof(P->Input)-P->Used,"Session peer input capacity");P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
    if((P->Mode&MQTT_RESUME_EARLY_ACK) && P->EarlyId && P->Used && (P->Input[0]>>4)==3){uint16 Id=P->EarlyId;P->EarlyId=0;mqttResumeAck(P,XMQTT_PACKET_PUBACK,Id);}
    while(P->Used){xmqttwireconfig K;xmqttWireConfigInit(&K);K.Version=P->Version;xmqttframe F;xmqttwirestatus T=xmqttFrameRead((xbytesview){P->Input,P->Used},&K,&F);
        mqttExampleCheck(T!=XMQTT_WIRE_ERROR,"Session peer framing");if(T==XMQTT_WIRE_MORE)break;
        if(F.Header.Type==XMQTT_PACKET_CONNECT){xrtAtomic32FetchAdd(&P->Connects,1,XMEMORY_RELEASE);if(!(P->Mode&MQTT_RESUME_HOLD_READY))mqttResumeConnack(P);}
        else if(F.Header.Type==XMQTT_PACKET_PUBLISH){xmqttpacket Q;xmqttproperty Props[16];xmqttcodecbuffers B={Props,16,NULL,0};
            mqttExampleCheck((P->Version==XMQTT_VERSION_5?xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL):xmqttPacketRead311(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&B,&Q,NULL))==XMQTT_CODEC_DONE,"Session peer publication");
            uint32 At=xrtAtomic32Load(&P->LogCount,XMEMORY_RELAXED);mqttExampleCheck(At<32,"Session peer log bound");P->Log[At].Id=Q.Identifier;P->Log[At].Flags=Q.Flags;P->Log[At].Qos=(Q.Flags>>1)&3u;P->Log[At].Topic=Q.Body.Publish.Topic.Size;P->Log[At].Payload=Q.Body.Publish.Payload.Size;P->Log[At].First=Q.Body.Publish.Payload.Size?Q.Body.Publish.Payload.Data[0]:0;
            for(size_t I=0;I<Q.Properties.Count;++I)if(Props[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS)P->Log[At].Alias=(uint16)Props[I].Value.Number;
            P->Log[At].TopicFirst=Q.Body.Publish.Topic.Size?(unsigned char)Q.Body.Publish.Topic.Data[0]:0;
            if(P->Log[At].Alias==1 && Q.Body.Publish.Topic.Size)P->AliasFirst=P->Log[At].TopicFirst;
            P->Log[At].LogicalFirst=Q.Body.Publish.Topic.Size?P->Log[At].TopicFirst:P->Log[At].Alias==1?P->AliasFirst:0;
            for(size_t I=0;I<Q.Properties.Count;++I)if(Props[I].Id==XMQTT_PROPERTY_USER){++P->Log[At].Users;
                xstrview Views[]={Props[I].Value.Pair.Name,Props[I].Value.Pair.Value};for(unsigned J=0;J<2;++J){for(size_t K=0;K<Views[J].Size;++K)P->Log[At].UserHash=P->Log[At].UserHash*131u+(unsigned char)Views[J].Data[K];P->Log[At].UserHash*=131u;}}
            xrtAtomic32Store(&P->LogCount,At+1,XMEMORY_RELEASE);
            if(((Q.Flags>>1)&3u)==1 && !(P->Mode&MQTT_RESUME_HOLD_ACK))mqttResumeAck(P,XMQTT_PACKET_PUBACK,Q.Identifier);
            if(((Q.Flags>>1)&3u)==2 && !(P->Mode&MQTT_RESUME_HOLD_REC))mqttResumeAck(P,XMQTT_PACKET_PUBREC,Q.Identifier);
            xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);
        }else if(F.Header.Type==XMQTT_PACKET_PUBREL){const unsigned char* W=F.Wire.Data;uint16 Id=(uint16)(((uint16)W[2]<<8)|W[3]);if(!(P->Mode&MQTT_RESUME_HOLD_COMP))mqttResumeAck(P,XMQTT_PACKET_PUBCOMP,Id);xrtAtomic32FetchAdd(&P->Releases,1,XMEMORY_RELEASE);
        }else if(F.Header.Type==XMQTT_PACKET_PUBACK)xrtAtomic32FetchAdd(&P->Pubacks,1,XMEMORY_RELEASE);
        else if(F.Header.Type==XMQTT_PACKET_PUBREC)xrtAtomic32FetchAdd(&P->Pubrecs,1,XMEMORY_RELEASE);
        else if(F.Header.Type==XMQTT_PACKET_PUBCOMP)xrtAtomic32FetchAdd(&P->Pubcomps,1,XMEMORY_RELEASE);
        else if(F.Header.Type==XMQTT_PACKET_DISCONNECT){xrtAtomic32FetchAdd(&P->Disconnects,1,XMEMORY_RELEASE);mqttExampleCheck(xrtNetStreamClose(S),"Session peer disconnect close");}
        else if(F.Header.Type==XMQTT_PACKET_PINGREQ){const unsigned char Pong[]={0xd0,0};mqttResumeSend(P,Pong,sizeof(Pong));}
        else mqttExampleCheck(false,"Session peer packet type");
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static inline void mqttResumeEnd(xnetstream* S,ptr Data){(void)Data;xrtNetStreamAbort(S);}
static inline void mqttResumeClosed(xnetstream* S,xnetresult Result,const xerror* Error,ptr Data)
{(void)S;(void)Result;(void)Error;xrtAtomic32Store(&((mqttResumePeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static inline bool mqttResumeAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;mqttResumePeer* P=Data;static const xnetstreamevents E={NULL,mqttResumeRead,mqttResumeEnd,NULL,NULL,NULL,mqttResumeClosed};
    mqttExampleCheck(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&E,P),"Session peer one connection");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static inline void mqttResumeListenerClosed(xnetlistener* L,ptr Data){(void)L;xrtAtomic32Store(&((mqttResumePeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline void mqttResumeStartWith(mqttResumePeer* P,xmqttversion Version,unsigned Mode,bool Present,const mqttResumeCaps* Caps)
{
    memset(P,0,sizeof(*P));P->Version=Version;P->Mode=Mode;P->Present=Present;P->MaximumQos=2;xrtAtomicPtrInit(&P->Stream,NULL);
    if(Caps){P->ReceiveMaximum=Caps->ReceiveMaximum;P->AliasMaximum=Caps->AliasMaximum;P->PacketMaximum=Caps->PacketMaximum;P->MaximumQos=Caps->MaximumQos;P->Reason=Caps->Reason;P->EarlyId=Caps->EarlyId;}
    xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Connects,0);xrtAtomic32Init(&P->Publishes,0);xrtAtomic32Init(&P->Releases,0);xrtAtomic32Init(&P->Pubacks,0);xrtAtomic32Init(&P->Pubrecs,0);xrtAtomic32Init(&P->Pubcomps,0);xrtAtomic32Init(&P->Disconnects,0);xrtAtomic32Init(&P->LogCount,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);mqttExampleCheck(P->Engine && xrtNetEngineStart(P->Engine),"Session peer Engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Distribution=XNET_ACCEPT_LOCAL;static const xnetlistenerevents L={mqttResumeAccept,NULL,mqttResumeListenerClosed};P->Listener=xrtNetListen(P->Engine,&K,&L,NULL,P);mqttExampleCheck(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"Session peer listener");
}
static inline void mqttResumeStart(mqttResumePeer* P,xmqttversion Version,unsigned Mode,bool Present)
{mqttResumeStartWith(P,Version,Mode,Present,NULL);}
static inline void mqttResumeStop(mqttResumePeer* P)
{
    mqttExampleCheck(xrtNetListenerClose(P->Listener),"Session peer listener close");mqttResumeWait(&P->ListenerClosed,1);xnetstream* S=xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);if(S)mqttResumeWait(&P->Closed,1);
    xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);mqttExampleCheck(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"Session peer physical Engine retirement");
}
#endif
