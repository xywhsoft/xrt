#ifndef MQTT_AUTH_EXAMPLE_PEER_H
#define MQTT_AUTH_EXAMPLE_PEER_H
#include <xmqtt/client_auth.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Offline protocol fixture. The fixed binary tokens are assertions, not an
 * authentication algorithm or a broker interoperability oracle. */
enum {MQTT_AUTH_DIRECT=1,MQTT_AUTH_EARLY=2,MQTT_AUTH_BAD_METHOD=4,
    MQTT_AUTH_MISSING_METHOD=8,MQTT_AUTH_REFUSE=16,MQTT_AUTH_HOLD=32,
    MQTT_AUTH_SHORT_SUCCESS=64,MQTT_AUTH_SERVER_REAUTH=128};
typedef struct mqttAuthPeer {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Stream;xnetaddr Address;
    xatomic32 Closed,ListenerClosed,Auths,Publishes,Acks,Pings;
    unsigned Mode,Round,RoundTarget;bool Reauth,Connected;uint32 PendingPings;char Method[64];size_t MethodBytes;
    unsigned char Input[16384];size_t Used;
} mqttAuthPeer;
static void mqttAuthCheck(bool OK,cstr What)
{if(!OK){fprintf(stderr,"MQTT authentication: %s (%s)\n",What,xrtErrorMessage(xrtGetError()));abort();}}
static void mqttAuthWait(xatomic32* A,uint32 Count)
{double End=xrtTimer()+10;while(xrtAtomic32Load(A,XMEMORY_ACQUIRE)<Count){mqttAuthCheck(xrtTimer()<End,"peer wait");xrtSleep(1);}}
static void mqttAuthSend(mqttAuthPeer* P,const void* Wire,size_t Size)
{
    xnetstream* S=xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);xnetresult R=xrtNetStreamSend(S,Wire,Size);
    mqttAuthCheck(R==XNET_RESULT_OK || xrtNetStreamState(S)>=XNET_STREAM_CLOSING,"fixture send");
}
static size_t mqttAuthEncode(mqttAuthPeer* P,xmqttpackettype Type,uint8 Reason,unsigned char* Wire,size_t Capacity)
{
    xmqttpacket Q;xmqttPacketInit(Type,&Q);Q.Reason=Reason;
    static const unsigned char Proof[]={'p',0,255,'f'};
    xmqttproperty Props[2]={{XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}},
        {XMQTT_PROPERTY_AUTH_DATA,XMQTT_PROPERTY_BINARY,{0}}};
    Props[0].Value.Text=(P->Mode&MQTT_AUTH_BAD_METHOD)?XRT_STR_LITERAL("wrong"):(xstrview){P->Method,P->MethodBytes};
    Props[1].Value.Binary=(xbytesview){Proof,sizeof(Proof)};
    if(!(P->Mode&MQTT_AUTH_MISSING_METHOD))Q.Properties=(xmqttpropertylist){Props,2};
    size_t N=0;mqttAuthCheck(xmqttPacketWrite5(Wire,Capacity,&Q,XMQTT_SERVER_TO_CLIENT,NULL,&N),"fixture encode AUTH/CONNACK");return N;
}
static void mqttAuthReply(mqttAuthPeer* P,xmqttpackettype Type,uint8 Reason)
{
    unsigned char Wire[512];size_t N=mqttAuthEncode(P,Type,Reason,Wire,sizeof(Wire));
    if(Type==XMQTT_PACKET_CONNACK && !Reason && (P->Mode&MQTT_AUTH_EARLY)){
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=77;
        Q.Body.Publish.Topic=XRT_STR_LITERAL("early");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("quarantine");size_t Extra=0;
        mqttAuthCheck(xmqttPacketWrite5(Wire+N,sizeof(Wire)-N,&Q,XMQTT_SERVER_TO_CLIENT,NULL,&Extra),"early publication");N+=Extra;
    }
    mqttAuthSend(P,Wire,N);
    if(Type==XMQTT_PACKET_CONNACK && !Reason){
        P->Connected=true;const unsigned char Pong[]={0xd0,0};
        while(P->PendingPings){mqttAuthSend(P,Pong,sizeof(Pong));--P->PendingPings;}
    }
}
static void mqttAuthRead(xnetstream* S,xnetbuf* B,ptr User)
{
    mqttAuthPeer* P=User;size_t N=xrtNetBufSize(B);mqttAuthCheck(N<=sizeof(P->Input)-P->Used,"fixture input bound");
    P->Used+=xrtNetBufRead(B,P->Input+P->Used,N);
    while(P->Used){xmqttframe F;xmqttwirestatus H=xmqttFrameRead((xbytesview){P->Input,P->Used},NULL,&F);
        mqttAuthCheck(H!=XMQTT_WIRE_ERROR,"fixture frame parse");if(H==XMQTT_WIRE_MORE)break;
        xmqttpacket Q;xmqttproperty Props[16];xmqttcodecbuffers Buffers={Props,16,NULL,0};
        xmqttcodecstatus Parsed=xmqttPacketRead5(F.Wire,XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Q,NULL);
        if(Parsed!=XMQTT_CODEC_DONE){fprintf(stderr,"fixture failure mode=%u reauth=%u round=%u type=%u bytes=%zu wire=",P->Mode,P->Reauth,P->Round,F.Header.Type,F.Wire.Size);for(size_t I=0;I<F.Wire.Size && I<64;++I)fprintf(stderr,"%02x",((const unsigned char*)F.Wire.Data)[I]);fputc('\n',stderr);}
        mqttAuthCheck(Parsed==XMQTT_CODEC_DONE,"fixture packet parse and frame boundaries");
        if(Q.Type==XMQTT_PACKET_CONNECT){
            for(size_t I=0;I<Q.Properties.Count;++I)if(Props[I].Id==XMQTT_PROPERTY_AUTH_METHOD){P->MethodBytes=Props[I].Value.Text.Size;mqttAuthCheck(P->MethodBytes<=sizeof(P->Method),"fixture method bound");memcpy(P->Method,Props[I].Value.Text.Data,P->MethodBytes);}
            if(P->Mode&MQTT_AUTH_REFUSE)mqttAuthReply(P,XMQTT_PACKET_CONNACK,0x87);
            else if(!(P->Mode&MQTT_AUTH_HOLD))mqttAuthReply(P,(P->Mode&MQTT_AUTH_DIRECT)?XMQTT_PACKET_CONNACK:XMQTT_PACKET_AUTH,(P->Mode&MQTT_AUTH_DIRECT)?0:0x18);
        }else if(Q.Type==XMQTT_PACKET_AUTH){
            mqttAuthCheck(Q.Reason==0x18 || Q.Reason==0x19,"client AUTH direction");
            if(Q.Reason==0x19){P->Reauth=true;P->Round=0;}
            else ++P->Round;
            if(!(P->Mode&MQTT_AUTH_HOLD)){
                if(P->Mode&MQTT_AUTH_SHORT_SUCCESS){const unsigned char W[]={0xf0,0};mqttAuthSend(P,W,sizeof(W));}
                else if(P->Mode&MQTT_AUTH_SERVER_REAUTH)mqttAuthReply(P,XMQTT_PACKET_AUTH,0x19);
                else if(Q.Reason==0x19 && !(P->Mode&MQTT_AUTH_DIRECT))mqttAuthReply(P,XMQTT_PACKET_AUTH,0x18);
                else if(P->Round<P->RoundTarget && !(P->Mode&MQTT_AUTH_DIRECT))mqttAuthReply(P,XMQTT_PACKET_AUTH,0x18);
                else mqttAuthReply(P,P->Reauth?XMQTT_PACKET_AUTH:XMQTT_PACKET_CONNACK,0);
            }
            xrtAtomic32FetchAdd(&P->Auths,1,XMEMORY_RELEASE);
        }else if(Q.Type==XMQTT_PACKET_PUBLISH){mqttAuthSend(P,F.Wire.Data,F.Wire.Size);xrtAtomic32FetchAdd(&P->Publishes,1,XMEMORY_RELEASE);}
        else if(Q.Type==XMQTT_PACKET_PUBACK)xrtAtomic32FetchAdd(&P->Acks,1,XMEMORY_RELEASE);
        else if(Q.Type==XMQTT_PACKET_PINGREQ){const unsigned char W[]={0xd0,0};if(P->Connected)mqttAuthSend(P,W,2);else ++P->PendingPings;xrtAtomic32FetchAdd(&P->Pings,1,XMEMORY_RELEASE);}
        else if(Q.Type==XMQTT_PACKET_DISCONNECT)mqttAuthCheck(xrtNetStreamClose(S),"diagnostic close");
        else mqttAuthCheck(false,"unexpected client packet");
        P->Used-=F.Wire.Size;if(P->Used)memmove(P->Input,P->Input+F.Wire.Size,P->Used);
    }
}
static void mqttAuthEnd(xnetstream* S,ptr User)
{(void)User;(void)xrtNetStreamAbort(S);}
static void mqttAuthClosed(xnetstream* S,xnetresult Status,const xerror* Error,ptr User)
{(void)S;(void)Status;(void)Error;xrtAtomic32Store(&((mqttAuthPeer*)User)->Closed,1,XMEMORY_RELEASE);}
static bool mqttAuthAccept(xnetlistener* L,xnetstream* S,ptr User)
{
    (void)L;mqttAuthPeer* P=User;static const xnetstreamevents E={NULL,mqttAuthRead,mqttAuthEnd,NULL,NULL,NULL,mqttAuthClosed};
    mqttAuthCheck(!xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE) && xrtNetStreamSetEvents(S,&E,P),"single fixture connection, no replay");
    xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;
}
static void mqttAuthListenerClosed(xnetlistener* L,ptr User)
{(void)L;xrtAtomic32Store(&((mqttAuthPeer*)User)->ListenerClosed,1,XMEMORY_RELEASE);}
static void mqttAuthStart(mqttAuthPeer* P,unsigned Mode)
{
    memset(P,0,sizeof(*P));P->Mode=Mode;P->RoundTarget=2;xrtAtomicPtrInit(&P->Stream,NULL);
    xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Auths,0);xrtAtomic32Init(&P->Publishes,0);xrtAtomic32Init(&P->Acks,0);xrtAtomic32Init(&P->Pings,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);mqttAuthCheck(P->Engine && xrtNetEngineStart(P->Engine),"fixture engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents Events={mqttAuthAccept,NULL,mqttAuthListenerClosed};
    P->Listener=xrtNetListen(P->Engine,&K,&Events,NULL,P);mqttAuthCheck(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"fixture listener");
}
static void mqttAuthStop(mqttAuthPeer* P)
{
    mqttAuthCheck(xrtNetListenerClose(P->Listener),"listener close");mqttAuthWait(&P->ListenerClosed,1);
    xnetstream* S=xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);if(S)mqttAuthWait(&P->Closed,1);
    xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);mqttAuthCheck(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"engine physical retirement");
}
static xmqttpacket mqttAuthPacket(xmqttpackettype Type,uint8 Reason,xmqttproperty Props[2],bool Empty)
{
    xmqttpacket Q;xmqttPacketInit(Type,&Q);Q.Reason=Reason;
    static const unsigned char Token[]={'a',0,255,'z'};
    Props[0]=(xmqttproperty){XMQTT_PROPERTY_AUTH_METHOD,XMQTT_PROPERTY_STRING,{0}};Props[0].Value.Text=Empty?XRT_STR_LITERAL(""):XRT_STR_LITERAL("fixture");
    Props[1]=(xmqttproperty){XMQTT_PROPERTY_AUTH_DATA,XMQTT_PROPERTY_BINARY,{0}};Props[1].Value.Binary=(xbytesview){Token,sizeof(Token)};Q.Properties=(xmqttpropertylist){Props,2};
    if(Type==XMQTT_PACKET_CONNECT){Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("auth-client");}
    return Q;
}
static inline xmqttauthticket* mqttAuthTicket(xmqttclient* C)
{
    xmqttauthticket* T=NULL;double End=xrtTimer()+10;
    while(!xmqttClientTakeAuth(C,&T)){mqttAuthCheck(xrtTimer()<End,"authentication ticket wait");xrtSleep(1);}return T;
}
static void mqttAuthAnswer(xmqttauthticket* T,bool Empty)
{
    mqttAuthCheck(xmqttAuthMessage(T)!=NULL,"owned authentication message");
    if(xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE){xmqttproperty Props[2];xmqttpacket Q=mqttAuthPacket(XMQTT_PACKET_AUTH,0x18,Props,Empty);mqttAuthCheck(xmqttAuthRespond(T,&Q)==XMQTT_ADMISSION_ACCEPTED,"challenge response");}
    else mqttAuthCheck(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"verify final server proof");
    xmqttAuthRelease(T);
}
#endif
