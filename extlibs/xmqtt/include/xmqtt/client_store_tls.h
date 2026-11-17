#ifndef XMQTT_CLIENT_STORE_TLS_H
#define XMQTT_CLIENT_STORE_TLS_H
#include <xmqtt/client_store.h>
#include <xmqtt/client_tls.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS))
#error "MQTT durable TLS Client requires durable Client and verified TLS"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
XRT_EXTERN_C_BEGIN
/* Durable Client over verified TLS for MQTT 5.0 and 3.1.1. Ownership,
 * matching Session/Writer, wall clock and Ready/END follow Client Store.
 * The original SetupTimeout includes native Dial/TLS and the durable OPEN.
 * Verification and identity follow Client TLS. Numeric/prepared endpoints need
 * an explicit peer name. Hostname dialing can infer it. Keep the Resolver and
 * native retained callback contexts live through physical retirement.
 * CONNACK and MQTT protocol state require a verified TLS connection.
 * Successful graceful Disconnect waits for authenticated TLS close and
 * committed END. Outcome separates storage status from network status.
 * TLS credentials, session tickets and endpoint addresses stay outside the
 * MQTT journal. Use a Writer bound to the same logical broker/principal/slot.
 * Client output changes only on success. */
XMQTT_API bool xmqttClientTlsConnectStore(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialStore(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesStore(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* Verified TLS followed by MQTT 5 enhanced authentication and durable OPEN.
 * TLS peer verification and final MQTT proof are independent prerequisites.
 * Auth and CONNECT are copied at admission. Use the existing auth ticket API. */
XMQTT_API bool xmqttClientTlsConnectStoreAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialStoreAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesStoreAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
