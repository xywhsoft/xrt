#ifndef XRPC_HTTP_SERVER_H
#define XRPC_HTTP_SERVER_H
#include <xrpc/server.h>
#include <xrt/tcp.h>
#include <xrt/http1.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER) && \
    (!defined(XRPC_FEATURE_XRPC_SERVER) || !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_HTTP1_BODY) || \
     !defined(XRT_FEATURE_HTTP_EXPECT) || !defined(XRT_FEATURE_HTTP_TARGET) || !defined(XRT_FEATURE_HTTP_PARAM) || \
     !defined(XRT_FEATURE_HTTP_TRAILER) || !defined(XRT_FEATURE_TIME_TEXT))
#error "xrpc HTTP server requires server, TCP, HTTP body/expect/target/param/trailer and time text"
#endif
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER)
typedef struct xrpchttpserver xrpchttpserver;
typedef enum xrpchttpserverstate {
    XRPC_HTTP_SERVER_CREATED=0,XRPC_HTTP_SERVER_RUNNING,
    XRPC_HTTP_SERVER_CLOSING,XRPC_HTTP_SERVER_CLOSED
} xrpchttpserverstate;
typedef enum xrpchttpservererror {
    XRPC_HTTP_SERVER_ARGUMENT=11001,XRPC_HTTP_SERVER_STATE,
    XRPC_HTTP_SERVER_MEMORY,XRPC_HTTP_SERVER_RESOURCE,XRPC_HTTP_SERVER_IO
} xrpchttpservererror;
typedef struct xrpchttpserverconfig {
    size_t Size;
    xnetlistenconfig Listen;
    xrpcserverconfig Execution;
    xhttp1limits Head;
    xhttp1bodylimits Body;
    xstrview Path;
    size_t MaxConnections,MaxOperations,MaxBufferBytes,MaxQueuedBytes,MaxRequestWireBytes;
    size_t InitialInputBytes,ReadBytes,WriteBytes,WriteChunkBytes;
    int64 IdleTimeout,RequestTimeout;
} xrpchttpserverconfig;
typedef struct xrpchttpserverinfo {
    size_t Size;
    xrpchttpserverstate State;
    size_t Connections,Operations,BufferBytes,QueuedBytes;
    uint64 Accepted,Rejected,OperationRejected,Requests,Dispatched,Responses,ProtocolErrors,Timeouts,TransportErrors;
    xrpcserverinfo Execution; /* Separate instant, not one atomic joint snapshot. */
} xrpchttpserverinfo;
XRT_EXTERN_C_BEGIN
/* Loopback ephemeral listener, /rpc, two execution workers, round-robin network
 * workers. 1024 connections/256 transport operations, 16 MiB private buffers,
 * 8 MiB referenced send bytes, 2 MiB request wire, 1 MiB decoded body.
 * Read/write work 64 KiB per turn, 16 KiB send chunks, 30s idle/request timeout. */
XRPC_API bool xrpcHttpServerConfigInit(xrpchttpserverconfig* Config);
/* Creates execution resources but does NOT bind/listen or invoke handlers.
 * Copies config/path. The running Engine is pinned through successful Destroy.
 * Callbacks/User remain borrowed through Destroy; initialize them before Start. */
XRPC_API bool xrpcHttpServerCreate(xnetengine* Engine,const xrpchttpserverconfig* Config,xrpchttpserver** Server);
/* Serialized creator operation. Synchronous numeric bind followed by async
 * accepts. All application state must be ready; handlers can run before return.
 * Failure leaves the created handle for retry or Close/Destroy. The factory
 * chooses plain TCP or the explicit TLS policy. */
XRPC_API bool xrpcHttpServerStart(xrpchttpserver* Server);
XRPC_API bool xrpcHttpServerLocal(xrpchttpserver* Server,xnetaddr* Address);
XRPC_API bool xrpcHttpServerInfo(xrpchttpserver* Server,xrpchttpserverinfo* Info);
/* Stops accepts, aborts peers and requests cooperative execution cancellation.
 * Nonblocking internal work is posted to owning workers. User token watches may
 * run on the caller and must not block. Body/result bytes are never replayed.
 * May be called by a handler; blocking Wait/Destroy may not. */
XRPC_API bool xrpcHttpServerClose(xrpchttpserver* Server);
/* Closes and waits for listener/peer/post/timer/watch/send-reference duties AND
 * executor publication/drop tails. One monotonic wait deadline. Rejects engine
 * workers and own execution/drop callbacks. Noncooperative handlers can delay
 * drain; timeout preserves all owner rights. Retained mechanical return tails
 * retire at host engine join. */
XRPC_API xwaitresult xrpcHttpServerWaitFor(xrpchttpserver* Server,int64 Timeout);
/* Stop concurrent native API entries/Start first. Success joins execution
 * workers, unpins Engine and consumes the unique owner. NULL succeeds;
 * failure leaves it valid for retry. No collector admission of opaque graphs. */
XRPC_API bool xrpcHttpServerDestroy(xrpchttpserver* Server);
XRT_EXTERN_C_END
#endif
#endif
