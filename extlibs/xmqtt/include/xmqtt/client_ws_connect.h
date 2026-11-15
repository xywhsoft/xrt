#ifndef XMQTT_CLIENT_WS_CONNECT_H
#define XMQTT_CLIENT_WS_CONNECT_H
#include <xmqtt/client_ws.h>
#include <xmqtt/client_dial.h>
#include <xrt/websocket_upgrade.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
#include <xmqtt/client_tls.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_WS) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) || !defined(XRT_FEATURE_WEBSOCKET_UPGRADE))
#error "MQTT WS connect requires native WS attachment, Dial and strict Upgrade"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS))
#error "MQTT WSS connect requires WS connect, WSS attachment and verified TLS"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT)
#define XMQTT_WS_CONNECT_HEAD_MAX 16384u
#define XMQTT_WS_CONNECT_FIELDS_MAX 128u
#define XMQTT_WS_CONNECT_EXTRA_MAX 32u
#define XMQTT_WS_CONNECT_WRITE_MAX 65536u
typedef struct xmqttclientwsconnectconfig {
    size_t Size;xmqttclientwsconfig WebSocket;xmqttclientdialconfig Dial;
    xhttp1limits Head;size_t MaxWriteBytes,MaxInformationalResponses;
    xstrview Host,Target,Origin;const xhttpfield* Fields;size_t FieldCount;
} xmqttclientwsconnectconfig;
typedef struct xmqttclientwsconnectinfo {
    size_t Size;double Deadline;bool Secure,Upgraded;uint16 Status;
    size_t RequestBytes,ResponseBytes,HeaderBytes,InformationalResponses;
    size_t HeadCopyBytes,ColdBytes,PeakColdBytes;
} xmqttclientwsconnectinfo;
XRT_EXTERN_C_BEGIN
/* Exact mqtt Binary/no-compression policy, existing WS/Dial defaults, /mqtt,
 * 16KiB response head/128 fields,64KiB request cap and four interim responses.
 * Host is required, explicit HTTP authority (including a nondefault port).
 * <=32 extra fields /16KiB name+value bytes; managed Upgrade/framing/Origin
 * fields and repeated Authorization are rejected. Snapshots complete request
 * wire before return; caller may overwrite strings/fields/CONNECT afterwards. */
XMQTT_API bool xmqttClientWsConnectConfigInit(xmqttclientwsconnectconfig* Config);
/* One ordinary Client from API entry: original Config.SetupTimeout spans all
 * snapshots,DNS/candidates,TCP,TLS,HTTP Upgrade and MQTT CONNACK/local proof.
 * No MQTT bytes before strict 101 and exact mqtt selection. No redirects,
 * credential retry, proxy or downgrade. Host/Target/Origin are HTTP metadata;
 * Dial Host is the counted network name, separately used for default TLS SNI.
 * Numeric/prepared WSS requires explicit TLS peer identity. Config.Dial controls
 * WS and WSS candidates (the TLS argument supplies Session/Stream/metadata).
 * Optional WebSocket.Auth/Session/Inbox compose existing selected features.
 * API success returns an owned Client in CONNECTING; Ready reports typed setup
 * outcomes. Accepted request deadlines include setup queueing. False leaves
 * output unchanged and transfers no caller resources. Resolver is borrowed
 * through Client retirement; prepared endpoints may be released on return.
 * Existing Close/Wait/Destroy join all native and losing-candidate duties. */
XMQTT_API bool xmqttClientWsConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientWsDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientWsDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
/* Uses the existing trusted native Client TLS Session/Stream policy and optional
 * client authentication. TLS credential capture after WS handoff is not exposed
 * by this stage; ResumeLimit must be zero. A supplied Resume can still be used. */
XMQTT_API bool xmqttClientWssConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientWssDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientWssDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
#endif
/* Atomic checked snapshot, available after setup failure/cold cleanup. State is
 * local HTTP/native progress, not MQTT readiness. Ordinary raw attachment Clients
 * return STATE unchanged. Native buffers/allocator/TLS memory are separately
 * bounded; ColdBytes counts the HTTP state/request/descriptors/optional cache. */
XMQTT_API bool xmqttClientWsConnectInfo(xmqttclient* Client,xmqttclientwsconnectinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
