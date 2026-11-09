/* Same independent byte peer and assertions, through each new Dial+Auth factory. */
#define XMQTT_MODULE_XMQTT
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
static xnetresolver* apDialResolver;
static bool apDialConnect(xnetengine* E,const xnetaddr* A,uint64 Affinity,const xmqttclientconfig* K,
    const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** C)
{
#if defined(AP_DIAL_PREPARED)
    xnetaddrlist* L=xrtNetAddrListCreate(A,1);if(!L)return false;
    bool OK=xmqttClientDialAddressesAuth(E,L,Affinity,K,NULL,Auth,Q,C);xrtNetAddrListDestroy(L);return OK;
#else
    xnetresolverconfig R;xrtNetResolverConfigInit(&R);apDialResolver=xrtNetResolverCreate(&R);if(!apDialResolver)return false;
    return xmqttClientDialAuth(E,apDialResolver,XRT_STR_LITERAL("127.0.0.1"),A->Port,Affinity,K,NULL,Auth,Q,C);
#endif
}
static bool apDialStop(xnetengine* E)
{if(apDialResolver && !xrtNetResolverDestroy(apDialResolver))return false;return xrtNetEngineStop(E);}
#define xmqttClientConnectAuth apDialConnect
#define xrtNetEngineStop apDialStop
#include "auth_probe.c"
