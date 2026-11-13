#ifndef XRPC_TLS_H
#define XRPC_TLS_H
#include <xrpc/duplex.h>
#include <xrpc/frame.h>
#include <xrt/tls_stream.h>
#if defined(XRPC_FEATURE_XRPC_TLS) && (!defined(XRPC_FEATURE_XRPC_DUPLEX) || !defined(XRPC_FEATURE_XRPC_FRAME) || !defined(XRT_FEATURE_TLS_STREAM_RETIRE))
#error "xrpc TLS requires duplex, frame and native TLS retirement"
#endif
#if defined(XRPC_FEATURE_XRPC_TLS)
typedef struct xrpctls xrpctls;
typedef enum xrpctlsstate {XRPC_TLS_CONNECTING=0,XRPC_TLS_HANDSHAKING,XRPC_TLS_OPEN,XRPC_TLS_CLOSING,XRPC_TLS_CLOSED} xrpctlsstate;
typedef enum xrpctlsreason {XRPC_TLS_LOCAL=0,XRPC_TLS_TRANSPORT,XRPC_TLS_PROTOCOL,XRPC_TLS_LIMIT,XRPC_TLS_MEMORY,XRPC_TLS_TIMEOUT,XRPC_TLS_ENGINE,XRPC_TLS_HANDSHAKE} xrpctlsreason;
typedef struct xrpctlsconfig {
    size_t Size;
    xrpcduplexconfig Duplex;
    xrpcframeconfig Frame;
    xnetstreamconfig Stream;
    xtlsstreamconfig TLS;
    size_t ReadChunkBytes,ReadBytes,WriteChunkBytes,WriteBytes,WorkFrames,MaxSubmitters;
    size_t MaxCipherBytes,MaxMetadataBytes;
    int64 FrameTimeout;
    xbytesview RequestContext;
} xrpctlsconfig;
typedef struct xrpctlsinfo {
    size_t Size;
    xrpctlsstate State;
    xrpctlsreason Reason;
    uint64 ReadBytes,WriteBytes,ReadFrames,WriteFrames,TimerFailures,PostFailures,Overloaded,ShortWrites,BlockedWrites;
    size_t Submitters,FixedBytes,FrameStorageBytes,CipherBytes,MaxObservedCipherBytes;
    xtlsversion Version;
    xtlscipher Cipher;
    bool Resumed;
    xerrkind NativeErrorKind;
    int32 NativeErrorCode;
    bool NativeErrorDomainTruncated;
    char NativeErrorDomain[64];
    size_t ProtocolBytes;
    unsigned char Protocol[256]; /* Copied opaque negotiated ALPN, not a C string. */
    xrpcduplexinfo Duplex;
} xrpctlsinfo;
XRT_EXTERN_C_BEGIN
/* Borrow the host Engine and shared Server, no new runtime/thread. Defaults:
 * Content-Length, 4KiB scratch,64KiB read/write work,16KiB chunks,32 frames/turn,
 *128 submitters,1MiB RPC ciphertext offer ceiling,64KiB TLS name/ALPN metadata,
 *30s incomplete frame timeout. Duplex.Wake/User are reserved. RequestContext is
 * copied once (<=16KiB); align all codec/frame/shared Server limits.
 * MaxCipherBytes gates RPC offers against total native pending bytes; Core TLS
 * control traffic remains bounded separately by Context.SendLimit + TCP limits.
 * Native TLS performs record/ciphertext copies; SendVec may create two records. */
XRPC_API bool xrpcTlsConfigInit(xrpctlsconfig* Config);
/* Concurrent numeric connect. Snapshot a native client Session before return:
 * names/ALPN are copied, context/verifier/resume references use Core ownership.
 * Explicit verification identity required; Core validates verifier/resume trust.
 * Original API entry + Stream.ConnectTimeout covers construction, TCP and TLS.
 * Native TLS config callback contexts/code remain borrowed through Destroy.
 * Calls can queue before OPEN with their own original API-entry deadlines. */
XRPC_API bool xrpcTlsConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xtlsclientconfig* Client,const xrpctlsconfig* Config,xrpctls** Peer);
/* Worker-only exclusive OPEN TCP stream. Snapshot the native server policy and
 * start its TLS handshake. Success takes one TCP caller reference; false takes
 * nothing and leaves its event binding unchanged. Accept callback may return
 * true after success. Policy callback contexts/code survive through Destroy. */
XRPC_API bool xrpcTlsAccept(xnetstream* Stream,const xtlsserverconfig* Server,
    const xrpctlsconfig* Config,xrpctls** Peer);
/* Adopt an OPEN owned TLS stream with exclusive IO, no previous event handoff,
 * no active callback or read Future. Post from Open to its Worker first.
 * Success takes one TLS caller reference and binds an owned event context;
 * false leaves reference/binding unchanged. Original session context survives
 * through this peer's Destroy; existing native socket/TLS settings are retained.
 * Already buffered plaintext is driven explicitly, never assumed replayed. */
XRPC_API bool xrpcTlsAttach(xtlsstream* Stream,const xrpctlsconfig* Config,xrpctls** Peer);
XRPC_API xrpcduplexadmission xrpcTlsCall(xrpctls* Peer,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcTlsNotify(xrpctls* Peer,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcTlsBatch(xrpctls* Peer,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future);
/* Concurrent aborting close, no reconnect/replay. Whole plaintext acceptance is
 * not delivery/business ACK. Once an offer starts, cancellation/timeout retains
 * MAY_HAVE_EXECUTED, including refusal/partial record acceptance. Protocol,
 * resource, scheduling and channel failures close the connection. Native TLS
 * errors are copied as kind/code; incoming final plaintext is drained first. */
XRPC_API bool xrpcTlsClose(xrpctls* Peer);
XRPC_API bool xrpcTlsInfo(xrpctls* Peer,xrpctlsinfo* Info);
/* Nonblocking physical query for facade/duplex/post/timer/native TLS+TCP tails.
 * Keep the sole caller owner stable, stop entries before final success. Poll
 * nudges closing work; bounded cleanup timers back off, exhausted timer quota
 * needs Poll/WaitFor progress. Wait rejects every same-Engine Worker. */
XRPC_API xwaitresult xrpcTlsPollRetired(xrpctls* Peer);
XRPC_API xwaitresult xrpcTlsWaitFor(xrpctls* Peer,int64 Timeout);
/* NULL succeeds; requires Close + retirement, never blocks, false retains owner.
 * Engine stays pinned through successful Destroy; keep it RUNNING. Shared
 * Server/Engine/external callers join separately before unloading code.
 * Retained result Futures/response payloads survive peer destruction. */
XRPC_API bool xrpcTlsDestroy(xrpctls* Peer);
XRT_EXTERN_C_END
#endif
#endif
