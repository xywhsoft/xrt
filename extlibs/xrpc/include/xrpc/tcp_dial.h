#ifndef XRPC_TCP_DIAL_H
#define XRPC_TCP_DIAL_H
#include <xrpc/tcp.h>
#if defined(XRPC_FEATURE_XRPC_TCP_DIAL) && (!defined(XRPC_FEATURE_XRPC_TCP) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "xrpc TCP dial requires TCP and Core candidate dial"
#endif
#if defined(XRPC_FEATURE_XRPC_TCP_DIAL)
typedef struct xrpctcpdialconfig {
    size_t Size;
    xnetfamily Family;
    int64 FallbackDelay;
    uint32 MaxAttempts;
    size_t MaxHostBytes;
} xrpctcpdialconfig;
XRT_EXTERN_C_BEGIN
/* Dual stack,250ms fallback,8 attempts,1024 host bytes; hard64 endpoints/
 * attempts and65535 name bytes. Config.Stream.ConnectTimeout is the ONE
 * original factory-entry deadline across copying, queueing, DNS and candidate
 * retirement, with the same zero/-1 semantics as numeric xrpcTcpConnect. */
XRPC_API bool xrpcTcpDialConfigInit(xrpctcpdialconfig* Config);
/* Concurrent async DNS via a borrowed host Resolver. Bounded nonempty name is
 * copied/NUL-terminated before return; embedded NUL rejected, no IDNA conversion.
 * Calls/notifications/batches queue using their own original entry deadlines.
 * Candidate sockets have no facade callbacks; only the transferred winner gets
 * the owned protocol binding. No RPC replay/reconnect or implicit TLS fallback.
 * Success returns one peer owner, Close/WaitFor/Destroy as for numeric connect.
 * Keep Resolver valid through peer retirement. Lookup code/data remains alive
 * through its separate Resolver join, including cancelled queries still running.
 * Core can retire this dial's owned callback before a shared backend lookup exits;
 * peer retirement does not join that independently owned host thread.
 * Cold source/list/name/controller ownership is released after its owned Done
 * callback physically retires, rather than retained on the open connection.
 * False changes no output and consumes no caller input references. */
XRPC_API bool xrpcTcpDial(xnetengine* Engine,xnetresolver* Resolver,xstrview DNSName,
    uint16 Port,uint64 Affinity,const xrpctcpconfig* Config,
    const xrpctcpdialconfig* Dial,xrpctcp** Peer);
/* Immutable <=64 prepared endpoints, selected ports nonzero; no Resolver.
 * Each endpoint port is preserved, Family filters candidates. The factory
 * retains the list before return, so the caller may release its alias at once.
 * Deadline, typed failure/native diagnostics and physical close rules match. */
XRPC_API bool xrpcTcpDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xrpctcpconfig* Config,const xrpctcpdialconfig* Dial,xrpctcp** Peer);
XRT_EXTERN_C_END
#endif
#endif
