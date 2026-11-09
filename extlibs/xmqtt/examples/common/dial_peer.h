#ifndef XMQTT_EXAMPLE_DIAL_PEER_H
#define XMQTT_EXAMPLE_DIAL_PEER_H
#include "client_peer.h"
enum {MQTT_DIAL_BLOCK=1,MQTT_DIAL_MISSING=2,MQTT_DIAL_DUAL=4};
typedef struct mqttDialPeer {
    mqttExamplePeer Peer;xnetresolver* Resolver;xnetsocket Refused;xnetaddr RefusedAddress;
    unsigned Flags;xatomic32 Lookups,Entered,Gate,Exited;
} mqttDialPeer;
static inline xnetaddrlist* mqttDialLookup(cstr Host,xnetfamily Family,ptr Data)
{
    mqttDialPeer* P=Data;mqttExampleCheck(!strcmp(Host,"mqtt.fixture"),"copied hostname");
    xrtAtomic32FetchAdd(&P->Lookups,1,XMEMORY_RELEASE);xrtAtomic32Store(&P->Entered,1,XMEMORY_RELEASE);
    if(P->Flags&MQTT_DIAL_BLOCK){double End=xrtTimer()+10;while(!xrtAtomic32Load(&P->Gate,XMEMORY_ACQUIRE)){mqttExampleCheck(xrtTimer()<End,"release blocked resolver");xrtThreadYield();}}
    xnetaddrlist* List=NULL;
    if(P->Flags&MQTT_DIAL_MISSING)xrtSetErrorInfo(XERR_NOT_FOUND,"fixture.mqtt.dns",71,"fixture hostname missing");
    else {xnetaddr A[2];size_t N=0;if(Family!=XNET_FAMILY_IPV4 && (P->Flags&MQTT_DIAL_DUAL))xrtNetAddrLoopback(&A[N++],XNET_FAMILY_IPV6,0);
        if(Family!=XNET_FAMILY_IPV6){A[N]=P->Peer.Address;A[N++].Port=0;}List=xrtNetAddrListCreate(A,N);}
    xrtAtomic32Store(&P->Exited,1,XMEMORY_RELEASE);return List;
}
static inline void mqttDialStart(mqttDialPeer* P,xmqttversion Version,unsigned Flags)
{
    memset(P,0,sizeof(*P));P->Flags=Flags;mqttExampleStart(&P->Peer,Version);
    xrtAtomic32Init(&P->Lookups,0);xrtAtomic32Init(&P->Entered,0);xrtAtomic32Init(&P->Exited,0);xrtAtomic32Init(&P->Gate,0);
    P->Refused=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);xnetaddr A;xrtNetAddrLoopback(&A,XNET_FAMILY_IPV4,0);
    mqttExampleCheck(P->Refused && xrtNetSocketBind(P->Refused,&A) && xrtNetSocketLocal(P->Refused,&P->RefusedAddress),"bound refusal socket");
    xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.Lookup=mqttDialLookup;K.LookupData=P;P->Resolver=xrtNetResolverCreate(&K);mqttExampleCheck(P->Resolver!=NULL,"fixture resolver");
}
static inline void mqttDialStop(mqttDialPeer* P)
{
    xrtAtomic32Store(&P->Gate,1,XMEMORY_RELEASE);mqttExampleCheck(xrtNetResolverDestroy(P->Resolver),"resolver retirement");
    if(!xrtAtomicPtrLoad(&P->Peer.Stream,XMEMORY_ACQUIRE))xrtAtomic32Store(&P->Peer.Closed,1,XMEMORY_RELEASE);
    mqttExampleStop(&P->Peer);mqttExampleCheck(xrtNetSocketClose(P->Refused),"refusal socket retirement");
}
#endif
