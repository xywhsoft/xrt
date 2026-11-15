#ifndef XRPC_WS_URI_H
#define XRPC_WS_URI_H
#include <xrpc/features.h>
#include <xrpc/export.h>
#include <xrt/http.h>
#if defined(XRPC_FEATURE_XRPC_WS_URI)
#include <xrpc/ws_dial.h>
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI_PARSE) && !defined(XRT_FEATURE_HTTP_TARGET)
#error "RPC WS URI parsing requires Core HTTP target"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI) && (!defined(XRPC_FEATURE_XRPC_WS_URI_PARSE) || !defined(XRPC_FEATURE_XRPC_WS_DIAL))
#error "RPC WS URI requires URI parsing and WS dial"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI_TLS) && (!defined(XRPC_FEATURE_XRPC_WS_URI) || !defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS))
#error "RPC WSS URI requires URI and verified TLS dial"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI_PARSE)
#define XRPC_WS_URI_MAX_BYTES 8192u
#define XRPC_WS_URI_MAX_AUTHORITY 1024u
#define XRPC_WS_URI_MAX_TARGET 4096u
typedef enum xrpcwsurihostkind {XRPC_WS_URI_NAME=0,XRPC_WS_URI_IPV4,XRPC_WS_URI_IPV6,XRPC_WS_URI_IPVFUTURE} xrpcwsurihostkind;
typedef struct xrpcwsuri {
    size_t Size;
    bool Secure,HasQuery;
    uint16 Port;
    xrpcwsurihostkind HostKind;
    xstrview Authority,Host,Path,Query;
    size_t RouteBytes,TargetBytes;
} xrpcwsuri;
XRT_EXTERN_C_BEGIN
/* Checked Size; failure preserves all output bytes. Borrowed raw URI views,
 * host excludes brackets/port. Absolute case-insensitive ws/wss only, nonempty
 * authority, no userinfo/fragment/raw non-ASCII or malformed percent triplet.
 * Explicit positive network port<=65535, absent/empty port defaults80/443.
 * Empty path maps to /; resource-name includes ? only for NONEMPTY query per
 * RFC6455. HasQuery separately preserves a syntactically present empty query.
 * HostKind uses strict IPv4/IPv6 recognition; valid IPvFuture is inspectable.
 * RouteBytes is decoded host octets, not IDNA size; TargetBytes is the eventual
 * resource-name. Hard limits above. No heap allocation on valid parsing. */
XRPC_API bool xrpcWsUriParse(xstrview URI,xrpcwsuri* Output);
XRT_EXTERN_C_END
#endif
#if defined(XRPC_FEATURE_XRPC_WS_URI)
XRT_EXTERN_C_BEGIN
/* Concurrent ws:// URI factory, with the ordinary WS peer/close contract.
 * URI owns HTTP.Host/Target (those two config views are replaced); all remaining
 * HTTP policies apply. Routing and resource bytes are snapshotted before return.
 * Numeric IP (including an escaped strict IPv4 name) bypasses Resolver; names
 * require a borrowed host Resolver. Host is percent-decoded ONCE, valid UTF-8 and
 * non-control/non-delimiter; path/query retain exact encoded bytes. IPvFuture
 * routing is UNSUPPORTED, never guessed as DNS. No IRI/IDNA conversion; host
 * Resolver owns its name-service policy. ASCII DNS A-labels work directly.
 * No redirects, userinfo auth, retries, downgrade or replay. Original API-entry
 * HTTP setup deadline includes URI validation/assembly and all native setup.
 * False preserves Peer and consumes no aliases; input/output must be disjoint.
 * Bounded temporary stack storage, no extra URI heap owner or shared runtime. */
XRPC_API bool xrpcWsConnectUri(xnetengine*,xnetresolver*,xstrview URI,uint64 Affinity,
    const xrpcwsconfig*,const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws** Peer);
#if defined(XRPC_FEATURE_XRPC_WS_URI_TLS)
/* wss:// only. Explicit native client/trust policy is required. Empty VerifyName
 * derives from decoded URI host; empty ServerName derives only for a name, not
 * an IP literal. Explicit nonempty identity/SNI overrides remain intentional
 * caller policy. For international names, supply name-service/IDNA policy and
 * appropriate native ASCII identity as required by Core. No cleartext fallback.
 * Native client/metadata/stream/Resolver lifetime rules match WS dial. */
XRPC_API bool xrpcWsConnectUriTls(xnetengine*,xnetresolver*,xstrview URI,uint64 Affinity,
    const xtlsclientconfig*,const xtlsstreamconfig*,const xrpcwsconfig*,
    const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws** Peer);
#endif
XRT_EXTERN_C_END
#endif
#endif
