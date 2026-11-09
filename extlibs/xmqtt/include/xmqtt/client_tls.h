#ifndef XMQTT_CLIENT_TLS_H
#define XMQTT_CLIENT_TLS_H
#include <xmqtt/client_dial.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#include <xrt/tls_stream.h>
#include <xrt/tls_client_auth.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) || !defined(XRT_FEATURE_TLS_STREAM_WRITE_PROGRESS) || !defined(XRT_FEATURE_TLS_CLIENT_AUTH) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME))
#error "MQTT TLS requires Dial, native TLS write progress, verified/resumable client and client authentication"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS) && (!defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384))
#error "MQTT TLS requires its declared native cipher, schedule and key-exchange backends"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
/* Borrowed only during the factory. Native Session snapshots names/ALPN and
 * retains Context/Verifier/Resume and optional ClientAuth.Identity before DNS.
 * Their callback/code/context lifetime follows the native shared-object contract
 * through physical Client retirement. Library code stays resident through Engine
 * join before unloading. Signers run on the connection Worker.
 * Defaults: native TLS policy/limits, Dial defaults,64 KiB metadata, infer SNI
 * and VerifyName from host; numeric literals infer VerifyName without IP SNI.
 * Prepared/numeric endpoints require an explicit Session peer identity.
 * MaxMetadataBytes bounds names, ALPN view descriptors and ALPN bytes together;
 * native Session/ciphertext storage uses its separate bounded Core limits.
 * ClientAuth selects native full-handshake mutual authentication; its current
 * native contract rejects explicit resumption. No private-key backend is forced.
 * Layout is independent of other compile-time selections. */
typedef struct xmqttclienttlsconfig {
    size_t Size;xtlsclientconfig Session;xtlsstreamconfig Stream;
    xmqttclientdialconfig Dial;size_t MaxMetadataBytes;bool ServerNameFromHost;
    const xtlsclientauthconfigv1* ClientAuth;
} xmqttclienttlsconfig;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientTlsConfigInit(xmqttclienttlsconfig* Config);
/* TLS required; no cleartext fallback or request replay. Config/CONNECT/views
 * snapshot before return; output changes only on success. Original SetupTimeout
 * spans admission, DNS, TCP, TLS and MQTT authentication/CONNACK. Ready succeeds
 * only after all configured gates. Resolver is borrowed through Client retirement.
 * AcceptedBytes counts MQTT plaintext accepted by TLS, conservatively uncertain
 * even before ciphertext reaches TCP. QoS0 OK observes its saved native ciphertext
 * prefix retirement while open. ACK results may precede that physical duty;
 * request quota/id reservations remain held until its real prefix/close outcome.
 * TLS requests add one aligned fence tail (24 bytes plus padding on x64) under MaxPrivateBytes, with no
 * per-chunk watch allocation. Native ciphertext is not a second plaintext copy.
 * Each TLS owner turn shares WriteBytes plaintext and WorkPackets send-call
 * budgets across startup, control and application output.
 * Graceful Disconnect includes authenticated TLS close; it is bounded by its
 * original deadline. Close/Wait/Destroy retain all actual callback/session/native
 * tails, including final verifier/identity releases. Public Client layouts stay fixed. */
XMQTT_API bool xmqttClientTlsConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* Same TLS factories with the MQTT5 challenge/final-proof/reauthentication gates.
 * TLS mutual authentication and MQTT enhanced authentication are independent. */
XMQTT_API bool xmqttClientTlsConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
