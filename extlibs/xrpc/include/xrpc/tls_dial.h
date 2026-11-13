#ifndef XRPC_TLS_DIAL_H
#define XRPC_TLS_DIAL_H
#include <xrpc/tls.h>
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL) && (!defined(XRPC_FEATURE_XRPC_TLS) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "xrpc TLS dial requires TLS and Core candidate dial"
#endif
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
typedef struct xrpctlsdialconfig {
    size_t Size;
    xnetfamily Family;
    int64 FallbackDelay;
    uint32 MaxAttempts;
    size_t MaxHostBytes;
} xrpctlsdialconfig;
XRT_EXTERN_C_BEGIN
/* Dual stack,250ms fallback,8 attempts,1024 host bytes; hard64 endpoints/
 * attempts and65535 name bytes. Config.Stream.ConnectTimeout is ONE original
 * factory-entry deadline across snapshot/copy/queue/DNS/candidate retirement
 * and verified TLS OPEN. Zero expires immediately, -1 disables that deadline;
 * the configured native TLS handshake limit remains separately effective. */
XRPC_API bool xrpcTlsDialConfigInit(xrpctlsdialconfig* Config);
/* Concurrent host-owned async DNS. DNSName routes TCP and is independent from
 * mandatory explicit Client.ServerName/VerifyName and its verification/resume
 * policy. No inferred identity, IDNA conversion or cleartext fallback. Copy
 * bounded nonempty DNSName (no NUL) and snapshot the native client Session before
 * return. Names/ALPN copied; native context/verifier/resume references retained.
 * Only the transferred TCP winner is handed to TLS; candidate callbacks never
 * publish facade readiness. RPC writes wait for verified TLS OPEN. No replay.
 * Success returns one peer owner: Close/WaitFor/Destroy as for numeric TLS.
 * Calls can queue and independently expire/cancel during DNS/TCP/handshake.
 * Keep Resolver valid through peer retirement; lookup code/data remains through
 * its separate Resolver join, including cancelled shared backend lookups.
 * All native TLS policy callback code/data remains alive through Destroy.
 * Cold source/controller owners retire after native owned Done/Drop. No new
 * Engine/Resolver/runtime/thread or hot IO vtable. False changes no output and
 * consumes none of the caller's native aliases. */
XRPC_API bool xrpcTlsDial(xnetengine* Engine,xnetresolver* Resolver,xstrview DNSName,
    uint16 Port,uint64 Affinity,const xtlsclientconfig* Client,
    const xrpctlsconfig* Config,const xrpctlsdialconfig* Dial,xrpctls** Peer);
/* Retain an immutable <=64 prepared list, preserve selected nonzero ports and
 * filter Family. No Resolver. Explicit TLS identity/trust remains mandatory;
 * caller may drop its list/context/verifier/resume aliases after success per
 * native ownership contracts. Borrowed callback contexts still outlive Destroy. */
XRPC_API bool xrpcTlsDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xtlsclientconfig* Client,const xrpctlsconfig* Config,
    const xrpctlsdialconfig* Dial,xrpctls** Peer);
XRT_EXTERN_C_END
#endif
#endif
