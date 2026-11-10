#ifndef XMQTT_CLIENT_SESSION_TLS_H
#define XMQTT_CLIENT_SESSION_TLS_H
#include <xmqtt/client_session.h>
#include <xmqtt/client_tls.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS))
#error "MQTT TLS Session requires Client Session and TLS"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS)
XRT_EXTERN_C_BEGIN
/* Explicit persistent protocol Session/Inbox with mandatory authenticated TLS.
 * Identity/codec/quota/exclusive claims follow Client Session; metadata snapshots,
 * verifier/identity retention, one entry setup deadline and actual ciphertext
 * retirement follow Client TLS. Caller binds passive state to one logical
 * broker/security principal. TLS credentials and native sessions are not stored
 * in the MQTT Session. Pre-acceptance failures preserve old state and expiry.
 * After MQTT/proof acceptance, local replay failure may follow a reset/resume.
 * Constructor output changes only on success; Open/replay waits for all gates.
 * Each reconnection creates a new TLS connection, explicitly after old Client
 * Destroy; optional TLS resumption is independent from MQTT SessionPresent.
 * QoS1/2 retain one canonical plaintext snapshot. TLS record encryption/copying,
 * request fence tail, ciphertext buffers and native retirement still have costs.
 * No cleartext fallback, automatic reconnect or durable store is implied. */
XMQTT_API bool xmqttClientTlsConnectSession(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialSession(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesSession(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT5 challenge/final-proof/reauthentication is independent of TLS mutual
 * authentication. Session opening and replay wait for initial proof acceptance. */
XMQTT_API bool xmqttClientTlsConnectSessionAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialSessionAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesSessionAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
