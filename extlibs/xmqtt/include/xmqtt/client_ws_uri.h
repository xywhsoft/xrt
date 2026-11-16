#ifndef XMQTT_CLIENT_WS_URI_H
#define XMQTT_CLIENT_WS_URI_H
#include <xmqtt/features.h>
#include <xmqtt/export.h>
#include <xrt/http.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI)
#include <xmqtt/client_ws_connect.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_WS_URI_PARSE) && !defined(XRT_FEATURE_HTTP_TARGET)
#error "MQTT WS URI parsing requires HTTP target"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI) && (!defined(XMQTT_FEATURE_XMQTT_WS_URI_PARSE) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT))
#error "MQTT WS URI requires URI parsing and native HTTP connection setup"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS))
#error "MQTT WSS URI requires URI setup and verified TLS"
#endif
#if defined(XMQTT_FEATURE_XMQTT_WS_URI_PARSE)
#define XMQTT_WS_URI_MAX_BYTES 8192u
#define XMQTT_WS_URI_MAX_AUTHORITY 1024u
#define XMQTT_WS_URI_MAX_TARGET 4096u
typedef enum xmqttwsurihostkind {XMQTT_WS_URI_NAME=0,XMQTT_WS_URI_IPV4,XMQTT_WS_URI_IPV6,XMQTT_WS_URI_IPVFUTURE} xmqttwsurihostkind;
typedef struct xmqttwsuri {
    size_t Size;bool Secure,HasQuery;uint16 Port;xmqttwsurihostkind HostKind;
    xstrview Authority,Host,Path,Query;size_t RouteBytes,TargetBytes;
} xmqttwsuri;
XRT_EXTERN_C_BEGIN
/* Strict absolute case-insensitive ws/wss, positive uint16 network port, default
 * 80/443 for absent/empty port. No userinfo, fragment, raw non-ASCII or malformed
 * escaping. Borrowed raw views; Host excludes brackets/port. Empty path maps to
 * /; only a nonempty query adds ? to the eventual resource-name (RFC6455).
 * HasQuery also records a present empty query. Valid IPvFuture is inspectable;
 * factories reject its unsupported routing mechanism. RouteBytes is decoded
 * host octets, not IDNA size. Valid parsing has no heap allocation. Checked Size
 * and disjoint complete output/input; failure preserves every output byte. */
XMQTT_API bool xmqttWsUriParse(xstrview URI,xmqttwsuri* Output);
XRT_EXTERN_C_END
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI)
XRT_EXTERN_C_BEGIN
/* ws:// only. URI replaces WebSocket.Host/Target; NULL WebSocket selects defaults.
 * Remaining HTTP/native WS/Dial and optional Auth/Session/Inbox policies apply.
 * Host decodes once to valid UTF-8, rejecting ASCII controls and routing
 * delimiters. Encoded path/query stay byte-exact. Strict numeric IP bypasses
 * Resolver; names borrow Resolver under its existing lifetime contract. No
 * IRI/IDNA conversion; host name-service policy belongs to Resolver. All inputs
 * snapshot before return. Original Client.SetupTimeout begins at URI API entry;
 * no extra clock, runtime, URI heap owner, redirects/retry/downgrade or replay.
 * Output changes only on success; input/output spans must be disjoint. Normal
 * Client Ready/Close/Wait/Destroy still cover all protocol/native duties. */
XMQTT_API bool xmqttClientWsConnectUri(xnetengine* Engine,xnetresolver* Resolver,xstrview URI,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
/* wss:// only, explicit TLS trust policy required. With ServerNameFromHost true,
 * empty VerifyName derives from decoded URI host; empty SNI derives only for a
 * name, not an IP. Explicit nonempty identity/SNI overrides remain caller policy.
 * With false, caller supplies native identity. Existing metadata/resumption/
 * client-auth limits remain; ResumeLimit must currently be zero, and newly issued
 * post-Upgrade credentials are not exposed. A supplied Resume remains allowed.
 * No cleartext fallback. */
XMQTT_API bool xmqttClientWssConnectUri(xnetengine* Engine,xnetresolver* Resolver,xstrview URI,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientwsconnectconfig* WebSocket,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif
