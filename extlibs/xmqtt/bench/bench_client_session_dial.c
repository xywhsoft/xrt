/* Same steady-state Session QoS1 echo, with real prepared-address Dial setup. */
#include <xmqtt.h>
static bool benchSessionDial(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    xnetaddrlist* L=xrtNetAddrListCreate(R,1);if(!L)return false;
    bool OK=xmqttClientDialAddressesSession(E,L,A,C,NULL,S,I,Q,O);xrtNetAddrListDestroy(L);return OK;
}
#define xmqttClientConnectSession benchSessionDial
#include "bench_client_session.c"
