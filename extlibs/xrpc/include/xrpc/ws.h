#ifndef XRPC_WS_H
#define XRPC_WS_H
#include <xrpc/duplex.h>
#include <xrt/websocket_stream.h>
#if defined(XRPC_FEATURE_XRPC_WS) && (!defined(XRPC_FEATURE_XRPC_DUPLEX) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_RETIRE) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_REF))
#error "RPC WebSocket requires duplex and native WebSocket reference send/retirement"
#endif
#if defined(XRPC_FEATURE_XRPC_WS)
typedef struct xrpcws xrpcws;
typedef enum xrpcwsstate {XRPC_WS_OPEN=0,XRPC_WS_CLOSING,XRPC_WS_CLOSED,XRPC_WS_UPGRADING} xrpcwsstate;
typedef enum xrpcwsreason {XRPC_WS_LOCAL=0,XRPC_WS_TRANSPORT,XRPC_WS_PROTOCOL,XRPC_WS_LIMIT,XRPC_WS_MEMORY,XRPC_WS_TIMEOUT,XRPC_WS_ENGINE,XRPC_WS_HTTP,XRPC_WS_AUTH} xrpcwsreason;
typedef struct xrpcwsconfig {
    size_t Size;
    xrpcduplexconfig Duplex;
    xwsstreamconfig Stream;
    size_t MaxMessageBytes,MaxReceiveBytes,KeepReceiveBytes,ReadBytes,WriteBytes,WorkMessages,MaxSubmitters;
    int64 MessageTimeout;
    bool AllowBinary,CompressOutput;
    xbytesview RequestContext;
    size_t CompressMinBytes;
} xrpcwsconfig;
typedef struct xrpcwsinfo {
    size_t Size;
    xrpcwsstate State;xrpcwsreason Reason;
    uint64 ReadBytes,WriteBytes,ReadMessages,WriteMessages,TimerFailures,PostFailures,Overloaded,ReceiveAllocations;
    size_t Submitters,FixedBytes,ReceiveBytes,ReceiveCapacity,PeakReceiveBytes;
    uint32 NativeCloseFlags;uint16 LocalCode,RemoteCode;
    xerrkind NativeErrorKind;int32 NativeErrorCode;bool NativeErrorDomainTruncated;
    char NativeErrorDomain[64];
    bool Encrypted;uint16 Version,Cipher;
    size_t ProtocolBytes;char Protocol[256];
    xrpcduplexinfo Duplex;
} xrpcwsinfo;
XRT_EXTERN_C_BEGIN
/* Default: one UTF-8 Text JSON-RPC document per WS message,1MiB message limit,
 * 3MiB receive growth peak,4KiB retained receive capacity,64KiB read-yield
 * threshold (plus at most one native callback chunk),1MiB write work,
 * 32 messages/turn,128 submitters,30s from MessageBegin through owner admission.
 * Binary input is opt-in; output is always Text. Native masking/UTF-8/control
 * handling is delegated to Core. Duplex.Wake/User are reserved and must be NULL.
 * RequestContext copies once (<=16KiB). Native HTTP/TLS and read/idle limits are
 * supplied by the host before attach; partial WS header time is not observable
 * through native MessageBegin. No new Engine/Server/Resolver/thread/runtime. */
/* CompressOutput defaults false, CompressMinBytes256. Select xrpc_ws_deflate
 * to enable output compression, which additionally requires negotiated
 * Stream.DeflateEnabled. Smaller messages remain ordinary Text. Decoded input
 * is bounded by MaxMessageBytes even if native Inflater.OutputLimit is larger.
 * Config gained two unconditional fields: rebuild checked-Size consumers. */
XRPC_API bool xrpcWsConfigInit(xrpcwsconfig* Config);
/* Worker-only exclusive OPEN transport after validated HTTP Upgrade. Initial is
 * optional exclusive owned read-ahead after its header. Stream.Role/Protocol and
 * compression must match that negotiation. Success consumes ONE transport ref
 * and Initial blocks, snapshots native protocol, and returns one peer owner.
 * False changes no output and consumes neither source nor Initial. No replay.
 * Raw TCP attach works in Accept; owned TLS attach must be posted after Open/
 * drive returns, per Core's transactional binding contract. */
XRPC_API bool xrpcWsAttachTcp(xnetstream* Stream,xnetbuf* Initial,const xrpcwsconfig* Config,xrpcws** Peer);
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
XRPC_API bool xrpcWsAttachTls(xtlsstream* Stream,xnetbuf* Initial,const xrpcwsconfig* Config,xrpcws** Peer);
#endif
/* Concurrent bounded admissions, original API-entry timeout and native typed
 * call/batch results. Inputs copied/encoded before ACCEPTED; only it writes
 * Future. Notification completion is native message acceptance, not business ACK.
 * Immutable output references survive physical send; unmasked TCP server avoids
 * an adapter payload copy, clients mask and TLS copies in Core. Native opaque
 * reference graphs may refuse collection until drained. No zero-copy claim. */
XRPC_API xrpcduplexadmission xrpcWsCall(xrpcws*,xstrview,xstrview,int64,xfuture**);
XRPC_API xrpcduplexadmission xrpcWsNotify(xrpcws*,xstrview,xstrview,int64,xfuture**);
XRPC_API xrpcduplexadmission xrpcWsBatch(xrpcws*,const xrpcduplexitem*,size_t,int64,xfuture**);
/* Aborting concurrent close; no implicit graceful handshake or replay. Complete
 * buffered messages precede remote EOF failure. Overload/framing/resource/native
 * scheduling failure closes with counted typed outcomes. Message assembly is
 * incrementally paused; callback work is bounded by native chunks/MaxMessage.
 * Keep host Engine, shared Server, callback code and external callers alive
 * through their separate joins. Retained result/wire references survive peer. */
XRPC_API bool xrpcWsClose(xrpcws*);
XRPC_API bool xrpcWsInfo(xrpcws*,xrpcwsinfo*);
XRPC_API xwaitresult xrpcWsPollRetired(xrpcws*);
/* One budget; rejects every same-Engine Worker. Poll nudges close on timer/post
 * exhaustion; successful physical retirement includes native WS/TCP/send tails. */
XRPC_API xwaitresult xrpcWsWaitFor(xrpcws*,int64);
/* NULL succeeds, otherwise Close+retired required; false retains ownership.
 * Stop concurrent entries before successful Destroy. Engine pin released here. */
XRPC_API bool xrpcWsDestroy(xrpcws*);
XRT_EXTERN_C_END
#endif
#endif
