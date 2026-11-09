#ifndef XMQTT_CLIENT_DIAL_H
#define XMQTT_CLIENT_DIAL_H
#include <xmqtt/client.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
#include <xmqtt/client_auth.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "MQTT Dial requires client and net_tcp_dial"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
#define XMQTT_DIAL_ADDRESSES_MAX 64u
#define XMQTT_DIAL_HOST_MAX 65535u
/* Network policy only. Config.Stream remains the only TCP buffer/timeout policy.
 * Defaults: dual stack, 30000ms additional Dial timeout, 250ms fallback, eight
 * candidates, 1024 host bytes. Timeout=0 disables the additional Dial timer;
 * -1 is infinite. The finite original SetupTimeout still spans all setup. */
typedef struct xmqttclientdialconfig {
    size_t Size;xnetfamily Family;int64 Timeout,FallbackDelay;
    uint32 MaxAttempts;size_t MaxHostBytes;
} xmqttclientdialconfig;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientDialConfigInit(xmqttclientdialconfig* Config);
/* Bounded copied nonempty host view, no embedded NUL, nonzero port. Resolver is
 * borrowed through physical Client retirement. Engine/Resolver remain alive;
 * Close/Wait/Destroy the unique Client before their destruction. Output changes
 * only on success and must not overlap Dial or borrowed host/endpoint storage.
 * CONNECT/config are copied before return. Ready reports typed DNS/connect/setup
 * failures. Cancel/Close wakes the owner; physical Wait includes all client
 * callbacks and losing candidates. A detached shared DNS job may continue under
 * Resolver ownership; Resolver Destroy joins it. No request/session replay or reconnect.
 * Initial host scratch uses MaxPrivateBytes and retires before return; native
 * Dial/Resolver/Stream storage uses their own bounded Core configurations. */
XMQTT_API bool xmqttClientDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,
    uint16 Port,uint64 Affinity,const xmqttclientconfig* Config,
    const xmqttclientdialconfig* Dial,const xmqttpacket* Connect,xmqttclient** Client);
/* Immutable prepared endpoints, no Resolver/DNS. At most64, nonzero eligible
 * ports; Family filters and MaxAttempts bounds selected attempts. The list is
 * retained by Core during admission; caller may release it immediately. */
XMQTT_API bool xmqttClientDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* Same Dial/setup deadline with the enhanced-auth ticket and final-proof gates. */
XMQTT_API bool xmqttClientDialAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,
    uint16 Port,uint64 Affinity,const xmqttclientconfig* Config,
    const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesAuth(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,
    const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
