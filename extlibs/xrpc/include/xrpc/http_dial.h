#ifndef XRPC_HTTP_DIAL_H
#define XRPC_HTTP_DIAL_H
#include <xrpc/http_client.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_DIAL) && \
    (!defined(XRPC_FEATURE_XRPC_HTTP_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "xrpc HTTP dial requires HTTP client and Core TCP dial"
#endif
#if defined(XRPC_FEATURE_XRPC_HTTP_DIAL)
typedef struct xrpchttpdialconfig {
    size_t Size;
    xnetfamily Family;
    int64 Timeout,FallbackDelay;
    uint32 MaxAttempts;
    size_t MaxHostBytes;
} xrpchttpdialconfig;
XRT_EXTERN_C_BEGIN
/* Dual stack, 30s complete factory-to-connection deadline, 250ms fallback,
 * eight candidates, 1024 host bytes. Timeout 0 is immediate, -1 infinite.
 * Hard limits: 64 prepared endpoints/candidates and 65535 host bytes.
 * Config.Stream in the HTTP client remains the TCP buffer/limit policy. */
XRPC_API bool xrpcHttpDialConfigInit(xrpchttpdialconfig* Config);
/* DNSName is distinct from the explicit HTTP Host authority and Target.
 * Bounded non-NUL DNSName is copied before return; no IDNA conversion.
 * Core supplies resolver admission, family ordering, candidate race and losing
 * socket retirement. No application request is replayed. Calls/notifications/
 * batches can queue, expire or cancel through connection setup.
 * The application-owned Resolver must outlive Closed; retire/join it separately.
 * A running uncooperative lookup can delay physical Closed after logical cancel.
 * Success returns one creator ref: Close then Release. All inputs are borrowed
 * only during this factory except Resolver. Failure leaves Client unchanged.
 * Plain TCP only; TLS is a separate explicit policy, never inferred. */
XRPC_API bool xrpcHttpDial(xnetengine* Engine,xnetresolver* Resolver,xstrview DNSName,
    uint16 Port,uint64 Affinity,xstrview Host,xstrview Target,const xrpchttpconfig* Config,
    const xrpchttpdialconfig* Dial,xrpchttpclient** Client);
/* Immutable prepared endpoints, each port preserved; no Resolver is consulted.
 * At most 64 entries. Family filters candidates; selected endpoints need a
 * nonzero port. Factory retains the list before return, so caller may release
 * its reference immediately. Connection/client deadline and close rules match. */
XRPC_API bool xrpcHttpDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* Config,const xrpchttpdialconfig* Dial,
    xrpchttpclient** Client);
XRT_EXTERN_C_END
#endif
#endif
