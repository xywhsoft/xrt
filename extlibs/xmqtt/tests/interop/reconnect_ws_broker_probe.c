#include "../../examples/reconnect_ws/endpoint.h"
#include <stdlib.h>
static mqttWsEndpoint ManagedWsEndpoint;
static xmqttclientstatus wsBrokerFactory(xnetengine* E,uint64 A,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,ptr Data,xmqttclient** C)
{(void)Data;return mqttWsEndpointFactory(E,A,K,S,I,Q,&ManagedWsEndpoint,C);}
static bool wsBrokerEndpointClear(void){return mqttWsEndpointClear(&ManagedWsEndpoint);}
#define XMQTT_RECONNECT_BROKER_FACTORY wsBrokerFactory
#define XMQTT_RECONNECT_BROKER_BEFORE_ENGINE_DESTROY wsBrokerEndpointClear
#define main managedWsBrokerInner
#include "reconnect_broker_probe.c"
#undef main
int main(int Count,char** Args)
{
    if(Count!=8){return 2;}bool TLS=atoi(Args[6])!=0;char URI[256];int N=snprintf(URI,sizeof(URI),"%s://%s:%s/mqtt?managed=1",TLS?"wss":"ws",Args[5],Args[1]);if(N<=0||(size_t)N>=sizeof(URI))return 2;
    bool OK=mqttWsEndpointInit(&ManagedWsEndpoint,(xstrview){URI,(size_t)N},TLS?Args[7]:NULL);if(!OK){mqttWsEndpointClear(&ManagedWsEndpoint);return 2;}int Code=managedWsBrokerInner(6,Args);if(!mqttWsEndpointClear(&ManagedWsEndpoint))return 1;return Code;
}
