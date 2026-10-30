#ifndef XRPC_HTTP_TLS_H
#define XRPC_HTTP_TLS_H
#include <xrpc/http_dial.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_TLS)
#include <xrt/tls_stream.h>
#if !defined(XRPC_FEATURE_XRPC_HTTP_DIAL) || !defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME)
#error "xrpc HTTP TLS requires HTTP dial, TLS stream and verified/resumable client"
#endif
#if !defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || \
    !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
    !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
#error "xrpc HTTP TLS requires its declared cipher/schedule/key-exchange backends"
#endif
typedef struct xrpchttptlsconfig {
    size_t Size;
    xtlsclientconfig Session;
    xtlsstreamconfig Stream;
    xrpchttpdialconfig Dial;
    size_t MaxMetadataBytes,WriteBytes;
} xrpchttptlsconfig;
XRT_EXTERN_C_BEGIN
/* Native TLS defaults, HTTP dial defaults, 64 KiB metadata and plaintext work
 * per turn, HTTP/1.1 ALPN. Require explicit Session.ServerName or VerifyName,
 * and a Verifier (or explicit ResumeOnly). IP identities use VerifyName only.
 * DNS name, HTTP authority and certificate identity are independent inputs.
 * Views are snapshotted and shared Context/Verifier/Resume retained at factory
 * entry; callback code/context follows Core verifier lifetime through join.
 * Only HTTP/1.1 or no ALPN is supported; h2 is rejected, never misparsed. */
XRPC_API bool xrpcHttpTlsConfigInit(xrpchttptlsconfig* Config);
/* Required TLS, no cleartext fallback or application replay. The one Dial
 * deadline begins at API entry and covers construction, DNS, TCP and TLS.
 * Queued calls/notifications/batches retain their own deadlines/cancellation.
 * Partial accepted plaintext marks MAY_HAVE_EXECUTED immediately. Closed waits
 * for actual TCP/TLS retirement; retained results do not pin the connection.
 * Resolver stays application-owned through Closed and is joined separately.
 * Success owns one creator ref: Close then Release. Failure preserves Client. */
XRPC_API bool xrpcHttpTlsDial(xnetengine* Engine,xnetresolver* Resolver,xstrview DNSName,uint16 Port,
    uint64 Affinity,xstrview Host,xstrview Target,const xrpchttpconfig* HTTP,const xrpchttptlsconfig* TLS,xrpchttpclient** Client);
XRPC_API bool xrpcHttpTlsDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* HTTP,const xrpchttptlsconfig* TLS,xrpchttpclient** Client);
XRPC_API bool xrpcHttpTlsConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* HTTP,const xrpchttptlsconfig* TLS,xrpchttpclient** Client);
XRT_EXTERN_C_END
#endif
#endif
