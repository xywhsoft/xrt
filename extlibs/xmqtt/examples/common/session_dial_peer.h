#ifndef XMQTT_EXAMPLE_SESSION_DIAL_PEER_H
#define XMQTT_EXAMPLE_SESSION_DIAL_PEER_H
#include "session_peer.h"
/* Literal offline Session/Auth peer. Fixed m/B/C/D tokens are test assertions,
 * not an authentication algorithm. DNS is local and never reaches the network. */
enum {MQTT_SESSION_DNS_BLOCK=1,MQTT_SESSION_DNS_MISSING=2};
typedef struct mqttSessionDialPeer {
    mqttResumePeer Peer;bool Auth,Incoming;xatomic32 Auths;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    xnetresolver* Resolver;xnetsocket Refused;xnetaddr RefusedAddress;
    unsigned DNS;xatomic32 Entered,Gate,Exited;
#endif
} mqttSessionDialPeer;
static inline void mqttSessionDialConnack(mqttSessionDialPeer* D)
{
    mqttResumePeer* P=&D->Peer;
    if(D->Auth){unsigned char W[]={0x20,11,P->Present?1:0,0,8,0x15,0,1,'m',0x16,0,1,'C'};mqttResumeSend(P,W,sizeof(W));}
    else mqttResumeConnack(P);
    if(D->Incoming){unsigned char A[]={P->Present?0x3c:0x34,10,0,2,'i','n',0,77,0,'x',0,'z'};
        unsigned char B[]={P->Present?0x3c:0x34,9,0,2,'i','n',0,77,'x',0,'z'};mqttResumeSend(P,P->Version==XMQTT_VERSION_5?A:B,P->Version==XMQTT_VERSION_5?sizeof(A):sizeof(B));}
}
static inline void mqttSessionDialFrame(mqttResumePeer* P,xnetstream* S,xmqttframe F)
{
    mqttSessionDialPeer* D=P->FrameData;const unsigned char* W=F.Wire.Data;
    if(F.Header.Type==XMQTT_PACKET_CONNECT){
        mqttExampleCheck(F.Wire.Size>9 && !(W[9]&2),"persistent literal CONNECT");
        xrtAtomic32FetchAdd(&P->Connects,1,XMEMORY_RELEASE);
        if(D->Auth){static const unsigned char A[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,'B'};mqttResumeSend(P,A,sizeof(A));}
        else if(!(P->Mode&MQTT_RESUME_HOLD_READY))mqttSessionDialConnack(D);
    }else if(F.Header.Type==XMQTT_PACKET_AUTH){
        mqttExampleCheck(D->Auth && F.Wire.Size>=3,"Session AUTH frame");
        if(W[2]==0x19){static const unsigned char A[]={0xf0,6,0,4,0x15,0,1,'m'};mqttResumeSend(P,A,sizeof(A));}
        else {static const unsigned char Expected[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,'D'};mqttExampleCheck(F.Wire.Size==sizeof(Expected) && !memcmp(W,Expected,sizeof(Expected)),"immutable challenge response");mqttSessionDialConnack(D);}
        xrtAtomic32FetchAdd(&D->Auths,1,XMEMORY_RELEASE);
    }else {
        mqttResumeFrame(P,S,F);
        if(F.Header.Type==XMQTT_PACKET_PUBREC && D->Incoming && P->Present)mqttResumeAck(P,XMQTT_PACKET_PUBREL,77);
    }
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
static inline xnetaddrlist* mqttSessionDialLookup(cstr Host,xnetfamily Family,ptr Data)
{
    mqttSessionDialPeer* D=Data;mqttExampleCheck(!strcmp(Host,"mqtt.fixture"),"Session copied DNS host");xrtAtomic32Store(&D->Entered,1,XMEMORY_RELEASE);
    if(D->DNS&MQTT_SESSION_DNS_BLOCK){double End=xrtTimer()+15;while(!xrtAtomic32Load(&D->Gate,XMEMORY_ACQUIRE)){mqttExampleCheck(xrtTimer()<End,"release Session DNS barrier");xrtThreadYield();}}
    xnetaddrlist* L=NULL;
    if(D->DNS&MQTT_SESSION_DNS_MISSING)xrtSetErrorInfo(XERR_NOT_FOUND,"fixture.mqtt.session.dns",71,"Session fixture host missing");
    else {xnetaddr A=D->Peer.Address;A.Port=0;L=xrtNetAddrListCreate(&A,Family==XNET_FAMILY_IPV6?0:1);}
    xrtAtomic32Store(&D->Exited,1,XMEMORY_RELEASE);return L;
}
#endif
static inline void mqttSessionDialStart(mqttSessionDialPeer* D,xmqttversion V,unsigned Mode,bool Present,bool Auth,bool Incoming,unsigned DNS)
{
    memset(D,0,sizeof(*D));D->Auth=Auth;D->Incoming=Incoming;xrtAtomic32Init(&D->Auths,0);
    mqttResumeStartHook(&D->Peer,V,Mode,Present,NULL,mqttSessionDialFrame,D);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    D->DNS=DNS;xrtAtomic32Init(&D->Entered,0);xrtAtomic32Init(&D->Gate,0);xrtAtomic32Init(&D->Exited,0);
    D->Refused=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);xnetaddr A;xrtNetAddrLoopback(&A,XNET_FAMILY_IPV4,0);
    mqttExampleCheck(D->Refused && xrtNetSocketBind(D->Refused,&A) && xrtNetSocketLocal(D->Refused,&D->RefusedAddress),"Session bound refusal endpoint");
    xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.Lookup=mqttSessionDialLookup;K.LookupData=D;D->Resolver=xrtNetResolverCreate(&K);mqttExampleCheck(D->Resolver!=NULL,"Session Resolver");
#else
    (void)DNS;
#endif
}
static inline void mqttSessionDialStop(mqttSessionDialPeer* D)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
    xrtAtomic32Store(&D->Gate,1,XMEMORY_RELEASE);mqttExampleCheck(xrtNetResolverDestroy(D->Resolver) && xrtNetSocketClose(D->Refused),"Session DNS/refusal retirement");
#endif
    mqttResumeStop(&D->Peer);
}
#endif
