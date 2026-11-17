#ifndef XMQTT_CLIENT_STORE_WS_H
#define XMQTT_CLIENT_STORE_WS_H
#include <xmqtt/client_store.h>
#include <xmqtt/client_ws_uri.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI))
#error "MQTT durable WebSocket requires Client Store and native URI Upgrade"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
XRT_EXTERN_C_BEGIN
/* Durable MQTT 5.0 and 3.1.1 over ws://. Use the matching Session and
 * Inbox-mode Writer as in Client Store. WebSocket may be NULL for defaults.
 * Its embedded volatile Session and Inbox must be NULL. Optional Auth uses
 * the existing ticket API and gates OPEN on acceptance of final proof.
 * The original SetupTimeout includes DNS, Upgrade, MQTT and durable OPEN.
 * Graceful Disconnect waits for clean WebSocket closure and durable END.
 * Use a trusted endpoint for the store's broker/principal/slot. A false
 * return preserves output. The Resolver must outlive Client retirement. */
XMQTT_API bool xmqttClientWsConnectUriStore(xnetengine* Engine,xnetresolver* Resolver,xstrview URI,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
/* wss:// with explicit native TLS trust policy, using the existing URI
 * verification and limits. A successful final Disconnect additionally
 * requires authenticated TLS transport closure. */
XMQTT_API bool xmqttClientWssConnectUriStore(xnetengine* Engine,xnetresolver* Resolver,xstrview URI,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttclienttlsconfig* TLS,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
