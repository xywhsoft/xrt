#ifndef XMQTT_CLIENT_SESSION_DIAL_H
#define XMQTT_CLIENT_SESSION_DIAL_H
#include <xmqtt/client_session.h>
#include <xmqtt/client_dial.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL))
#error "MQTT Dial Session requires Client Session and Dial"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
XRT_EXTERN_C_BEGIN
/* Explicit TCP Session/Inbox with hostname or prepared endpoints. All passive
 * ownership, identity, replay and physical-retirement rules follow Client Session.
 * Dial copies host/config/CONNECT and retains prepared endpoints before return.
 * Resolver stays alive through physical Client retirement; detached shared DNS
 * jobs remain Resolver-owned. One entry setup deadline spans DNS, TCP and MQTT,
 * including application proof when Auth is selected. Failed setup/denied proof
 * preserves offline state/expiry. Caller binds all endpoints to the same broker/
 * security principal; fallback is connection setup, not automatic reconnect.
 * Native Dial/Resolver bounds remain separate from MQTT private/request bounds.
 * Output changes only on success. Neither passive owner stores credentials.
 * TCP has no transport encryption; select client_session_tls for TLS. */
XMQTT_API bool xmqttClientDialSession(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesSession(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT5 challenge/final-proof/reauthentication follow Client Auth. Application
 * final proof gates Session opening and replay; all deadlines share factory entry. */
XMQTT_API bool xmqttClientDialSessionAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesSessionAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
