#ifndef XRPC_HTTP_SERVER_TLS_H
#define XRPC_HTTP_SERVER_TLS_H
#include <xrpc/http_server.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER_TLS)
#include <xrt/tls_stream.h>
#if !defined(XRPC_FEATURE_XRPC_HTTP_SERVER) || !defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_TLS_SERVER_RESUME)
#error "xrpc HTTP TLS server requires HTTP server, native TLS stream and server resume"
#endif
#if !defined(XRT_FEATURE_TLS_IDENTITY_RSA) || !defined(XRT_FEATURE_TLS_IDENTITY_P256) || !defined(XRT_FEATURE_TLS_IDENTITY_P384) || !defined(XRT_FEATURE_TLS_IDENTITY_ED25519) || \
    !defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
    !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
#error "xrpc HTTP TLS server requires its declared identity/cipher/schedule/key-exchange backends"
#endif
typedef void (*xrpchttpserverhandshakeproc)(xnetresult Result,const xerror* Cause,ptr Context);
typedef struct xrpchttpservertlsconfig {
    size_t Size;
    xtlsserverconfig Session;
    xtlsstreamconfig Stream;
    size_t MaxHandshakes,MaxMetadataBytes;
    xrpchttpserverhandshakeproc HandshakeError;
    ptr Context;
} xrpchttpservertlsconfig;
typedef struct xrpchttpservertlsinfo {
    size_t Size,ActiveHandshakes,PeakHandshakes;
    uint64 Admitted,Completed,Failed,Rejected;
} xrpchttpservertlsinfo;
XRT_EXTERN_C_BEGIN
/* Native TLS limits, 128 concurrent handshakes, 64 KiB copied metadata,
 * HTTP/1.1 ALPN (no ALPN is allowed). Supply Identity or a native SNI selector.
 * Metadata hard limit 1 MiB; connection and handshake bounds are independent. */
XRPC_API bool xrpcHttpServerTlsConfigInit(xrpchttpservertlsconfig* Config);
/* Create does not bind or invoke business handlers. Native Context/Identity
 * references and ALPN bytes are snapshotted before return; callback code/context
 * remain borrowed through Destroy and must permit concurrent network workers.
 * HandshakeError borrows Cause for the call, runs without the server gate on
 * the peer worker, must not block, and may Close (not Wait/Destroy) this server.
 * Successful construction uses the ordinary Start/Local/Info/Close/Wait/Destroy
 * API. All admitted peers, including handshakes, count against MaxConnections
 * and private-buffer bounds before a native TLS session is allocated.
 * Stream.HandshakeTimeout starts at TCP admission; zero disables it. HTTP
 * idle/request deadlines begin after TLS readiness/first plaintext respectively.
 * Required TLS, no cleartext fallback or operation replay; mutual client and
 * HTTP application authentication are separate policies. Failure preserves Out.
 * MaxQueuedBytes charges conservative TLS application-record bounds until drain;
 * handshake/control bytes and native buffers have separate Core/count bounds. */
XRPC_API bool xrpcHttpServerTlsCreate(xnetengine* Engine,const xrpchttpserverconfig* HTTP,
    const xrpchttpservertlsconfig* TLS,xrpchttpserver** Out);
XRPC_API bool xrpcHttpServerTlsInfo(xrpchttpserver* Server,xrpchttpservertlsinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
