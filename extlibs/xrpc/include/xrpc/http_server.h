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
typedef enum xrpchttpauthstatus {
    XRPC_HTTP_AUTH_ALLOW=0,XRPC_HTTP_AUTH_UNAUTHORIZED,
    XRPC_HTTP_AUTH_FORBIDDEN,XRPC_HTTP_AUTH_ERROR
} xrpchttpauthstatus;
typedef struct xrpchttpauthrequest {
    const xhttp1head* Head; /* Immutable owned snapshot, callback-borrowed. */
    xnetaddr Remote;
    xcancel* Cancel; /* Child token; poll only, never destroy/request it. */
    double Deadline; /* Whole request deadline, not a new authorization clock. */
    bool Secure; /* Verified transport mode; no client identity is implied. */
} xrpchttpauthrequest;
/* Executor callback, never an I/O worker. Context is bounded writable storage;
 * set ContextBytes within Capacity (initially zero). ALLOW copies those opaque
 * identity bytes to every business handler's Context->Data. Borrow neither input
 * nor output after return. Poll Cancel/Deadline; noncooperative callbacks delay
 * physical drain. Callback/User can run concurrently and live through Destroy. */
typedef xrpchttpauthstatus (*xrpchttpauthorize)(const xrpchttpauthrequest* Request,
    bytes Context,size_t Capacity,size_t* ContextBytes,ptr User);
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
    xrpchttpauthorize Authorize;
    ptr AuthorizationUser;
    xstrview UnauthorizedChallenge; /* Copied valid RFC challenge, max 128 bytes. */
    size_t MaxAuthorizations,MaxAuthorizationContextBytes;
} xrpchttpserverconfig;
typedef struct xrpchttpserverinfo {
    size_t Size;
    xrpchttpserverstate State;
    size_t Connections,Operations,BufferBytes,QueuedBytes;
    uint64 Accepted,Rejected,OperationRejected,Requests,Dispatched,Responses,ProtocolErrors,Timeouts,TransportErrors;
    xrpcserverinfo Execution; /* Separate instant, not one atomic joint snapshot. */
    size_t Authorizations; /* Pending and retained head/context snapshots. */
    uint64 AuthorizationAccepted,AuthorizationRejected,AuthorizationAllowed,AuthorizationDenied,AuthorizationErrors;
} xrpchttpserverinfo;
XRT_EXTERN_C_BEGIN
/* Loopback ephemeral listener, /rpc, two execution workers, round-robin network
 * workers. 1024 connections/256 transport operations, 16 MiB private buffers,
 * 8 MiB referenced send bytes, 2 MiB request wire, 1 MiB decoded body.
 * Read/write work 64 KiB per turn, 16 KiB send chunks, 30s idle/request timeout.
 * Authorization disabled, 128 snapshot slots / 256 identity bytes if enabled. */
XRPC_API bool xrpcHttpServerConfigInit(xrpchttpserverconfig* Config);
/* Creates execution resources but does NOT bind/listen or invoke handlers.
 * Copies config/path. The running Engine is pinned through successful Destroy.
 * Callbacks/User remain borrowed through Destroy; initialize them before Start.
 * Optional authorization copies a validated head, uses the SAME bounded Core
 * executor, and delays Continue/body processing/business until ALLOW. Refusals
 * close the connection: 401 with copied configured challenge, 403, or 503/500.
 * Duplicate Authorization is 400 before user code. No credential negotiation,
 * replay, proxy trust, mTLS identity or token refresh is implied. Snapshot and
 * authorized receive storage are wiped on real retirement/growth; native/system
 * copies and returned business values are excluded. MaxAuthorizations includes
 * retained metadata during body wait, under MaxBufferBytes. Executor work also
 * shares MaxOperations/MaxRequestBytesTotal until completion publication.
 * Recompile users for config/info and nested execution-info layout changes. */
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
