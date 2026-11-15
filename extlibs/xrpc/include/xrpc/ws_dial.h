#ifndef XRPC_WS_DIAL_H
#define XRPC_WS_DIAL_H
#include <xrpc/ws_http.h>
#if defined(XRPC_FEATURE_XRPC_WS_DIAL) && (!defined(XRPC_FEATURE_XRPC_WS_HTTP) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "RPC WS dial requires HTTP Upgrade and Core candidate dial"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS) && (!defined(XRPC_FEATURE_XRPC_WS_DIAL) || !defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS) || !defined(XRT_FEATURE_TLS_CLIENT))
#error "RPC WSS dial requires WS dial, owned TLS Upgrade and TLS client"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
typedef struct xrpcwsdialconfig {
    size_t Size;
    xnetstreamconfig Stream;
    xnetfamily Family;
    int64 FallbackDelay;
    uint32 MaxAttempts;
    size_t MaxHostBytes,MaxMetadataBytes;
} xrpcwsdialconfig;
XRT_EXTERN_C_BEGIN
/* Native TCP defaults, dual stack,250ms fallback,8 attempts,1024 copied DNS
 * bytes,16KiB TLS name/ALPN metadata. Hard64 endpoints/attempts,65535 DNS and
 * 16KiB metadata. No Resolver/Engine/executor/thread is created. */
XRPC_API bool xrpcWsDialConfigInit(xrpcwsdialconfig*);
/* Concurrent factories return the SAME xrpcws handle in UPGRADING. HTTP.Host
 * and Target remain explicit and independent of the routing address/DNSName.
 * HTTP.SetupTimeout is ONE factory-entry monotonic deadline across copying,
 * worker queue, DNS, candidates, verified TLS, Upgrade and authorization;0
 * expires immediately,-1 disables that deadline. Native caps remain effective.
 * Calls/batches/notifications can queue with their own original entry clocks.
 * Only checked101 permits RPC bytes. No replay/reconnect, redirect, credential
 * retry, scheme inference or cleartext fallback. False preserves output and
 * consumes no aliases. Success copies inputs and returns one creator reference;
 * Close/WaitFor/Destroy joins physical peer/native/owned Done/Drop duties.
 * Resolver is borrowed through peer retirement, then joined separately by its
 * host; cancelled shared backend lookups may outlive this owned callback.
 * DNSName is bounded nonempty and NUL-free; no IDNA conversion is performed. */
XRPC_API bool xrpcWsConnect(xnetengine*,const xnetaddr*,uint64 Affinity,
    const xrpcwsconfig*,const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
XRPC_API bool xrpcWsDial(xnetengine*,xnetresolver*,xstrview DNSName,uint16 Port,uint64 Affinity,
    const xrpcwsconfig*,const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
/* Retains <=64 immutable endpoints before return; selected ports nonzero and
 * preserved, Family filters candidates. Caller may release its list at once. */
XRPC_API bool xrpcWsDialAddresses(xnetengine*,xnetaddrlist*,uint64 Affinity,
    const xrpcwsconfig*,const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
#if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
/* Explicit Client.ServerName/VerifyName is mandatory, distinct from HTTP Host
 * and DNS routing. Session snapshots names/ALPN and holds context/verifier/
 * resume aliases before return. All borrowed native callback data/code remains
 * resident through Destroy. Empty or http/1.1 negotiated ALPN only. Native TLS
 * limits are copied; NULL uses defaults. TCP candidates never begin TLS. */
XRPC_API bool xrpcWsConnectTls(xnetengine*,const xnetaddr*,uint64 Affinity,
    const xtlsclientconfig*,const xtlsstreamconfig*,const xrpcwsconfig*,
    const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
XRPC_API bool xrpcWsDialTls(xnetengine*,xnetresolver*,xstrview DNSName,uint16 Port,uint64 Affinity,
    const xtlsclientconfig*,const xtlsstreamconfig*,const xrpcwsconfig*,
    const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
XRPC_API bool xrpcWsDialAddressesTls(xnetengine*,xnetaddrlist*,uint64 Affinity,
    const xtlsclientconfig*,const xtlsstreamconfig*,const xrpcwsconfig*,
    const xrpcwshttpconfig*,const xrpcwsdialconfig*,xrpcws**);
#endif
XRT_EXTERN_C_END
#endif
#endif
