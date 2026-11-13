#ifndef XRPC_TCP_H
#define XRPC_TCP_H
#include <xrpc/duplex.h>
#include <xrpc/frame.h>
#include <xrt/tcp.h>
#if defined(XRPC_FEATURE_XRPC_TCP) && (!defined(XRPC_FEATURE_XRPC_DUPLEX) || !defined(XRPC_FEATURE_XRPC_FRAME) || !defined(XRT_FEATURE_NET_TCP_RETIRE))
#error "xrpc TCP requires duplex, frame and native TCP retirement"
#endif
#if defined(XRPC_FEATURE_XRPC_TCP)
typedef struct xrpctcp xrpctcp;
typedef enum xrpctcpstate {XRPC_TCP_CONNECTING=0,XRPC_TCP_OPEN,XRPC_TCP_CLOSING,XRPC_TCP_CLOSED} xrpctcpstate;
typedef enum xrpctcpreason {XRPC_TCP_LOCAL=0,XRPC_TCP_TRANSPORT,XRPC_TCP_PROTOCOL,XRPC_TCP_LIMIT,XRPC_TCP_MEMORY,XRPC_TCP_TIMEOUT,XRPC_TCP_ENGINE} xrpctcpreason;
typedef struct xrpctcpconfig {
    size_t Size;
    xrpcduplexconfig Duplex;
    xrpcframeconfig Frame;
    xnetstreamconfig Stream;
    size_t ReadChunkBytes,ReadBytes,WriteChunkBytes,WriteBytes,WorkFrames,MaxSubmitters;
    int64 FrameTimeout;
    xbytesview RequestContext;
} xrpctcpconfig;
typedef struct xrpctcpinfo {
    size_t Size;
    xrpctcpstate State;
    xrpctcpreason Reason;
    uint64 ReadBytes,WriteBytes,ReadFrames,WriteFrames,TimerFailures,PostFailures,Overloaded;
    size_t Submitters,FixedBytes,FrameStorageBytes;
    xrpcduplexinfo Duplex; /* Last owner snapshot, not simultaneous with facade counters. */
} xrpctcpinfo;
XRT_EXTERN_C_BEGIN
/* Existing Engine + borrowed shared Server; no new runtime/thread. Defaults:
 * Content-Length/1 MiB messages, 4 KiB read scratch, 64 KiB read/write work,
 * 16 KiB write chunks, 32 frames/turn,128 concurrent submitters,30s frame timeout.
 * Duplex.Wake/User are reserved and must be NULL. RequestContext is copied
 * (<=16 KiB) once and supplied to each incoming Server job; it is not deep copied.
 * Align Frame, Duplex codec and shared Server limits. */
XRPC_API bool xrpcTcpConfigInit(xrpctcpconfig* Config);
/* Concurrent factory; numeric IPv4/IPv6 only. Caller owns Peer on true.
 * Native connect/queue time shares the original ConnectTimeout. Calls may be
 * submitted before Open; each retains its own API-entry monotonic deadline.
 * Keep Engine running and Server/code alive through WaitFor/Destroy. */
XRPC_API bool xrpcTcpConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xrpctcpconfig* Config,xrpctcp** Peer);
/* Worker-only, OPEN stream with exclusive protocol/read/write ownership.
 * Success takes exactly the caller's one stream reference and replaces its
 * event binding; false takes nothing and leaves the existing binding intact.
 * Accept callback can return true after successful Attach. Buffered input is
 * explicitly driven; native SetEvents does not replay it. No reconnect/replay. */
XRPC_API bool xrpcTcpAttach(xnetstream* Stream,const xrpctcpconfig* Config,xrpctcp** Peer);
/* Concurrent bounded admission; input copied before return; only ACCEPTED
 * writes Future. API entry time includes facade gate/admission and encoding.
 * Native typed call/batch results and independent retained payloads are used. */
XRPC_API xrpcduplexadmission xrpcTcpCall(xrpctcp* Peer,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcTcpNotify(xrpctcp* Peer,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcTcpBatch(xrpctcp* Peer,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** Future);
/* Concurrent aborting close. Accepted calls preserve delivery uncertainty;
 * notifications mean whole-message native queue acceptance, not business ACK.
 * Protocol/framing/EOF truncation, resource overload and scheduling failures
 * close the channel. Incoming overload is counted fail-close, no silent drop
 * or implicit retry. Frame timeout runs from first buffered-byte observation;
 * coalesced queued frames conservatively share that observation origin. */
XRPC_API bool xrpcTcpClose(xrpctcp* Peer);
XRPC_API bool xrpcTcpInfo(xrpctcp* Peer,xrpctcpinfo* Info);
/* Nonblocking barrier for message/runtime/post/timer/event/native stream tails.
 * Poll may nudge pending close work. Timer exhaustion switches close progress
 * to Poll/WaitFor nudges; no unbounded worker spin is installed. Keep the sole
 * caller owner stable. Stop concurrent API entries before treating OK as final.
 * WaitFor rejects every worker of the same Engine; one timeout covers all duties. */
XRPC_API xwaitresult xrpcTcpPollRetired(xrpctcp* Peer);
XRPC_API xwaitresult xrpcTcpWaitFor(xrpctcp* Peer,int64 Timeout);
/* NULL succeeds. Requires Close + retired; never blocks, failure retains owner.
 * Engine pin is released on successful Destroy, then host can stop Engine.
 * Shared Server's physical worker join and external caller quiescence remain
 * separate duties before code unload. Retained result Futures survive. */
XRPC_API bool xrpcTcpDestroy(xrpctcp* Peer);
XRT_EXTERN_C_END
#endif
#endif
