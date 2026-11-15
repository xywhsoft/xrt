#ifndef XRPC_WS_HTTP_H
#define XRPC_WS_HTTP_H
#include <xrpc/ws.h>
#include <xrt/websocket_upgrade.h>
#if defined(XRPC_FEATURE_XRPC_WS_HTTP) && (!defined(XRPC_FEATURE_XRPC_WS) || !defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM))
#error "RPC WS HTTP requires WS and strict Core HTTP Upgrade"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS) && (!defined(XRPC_FEATURE_XRPC_WS_HTTP) || !defined(XRPC_FEATURE_XRPC_WS_TLS) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_EXCHANGE) || !defined(XRT_FEATURE_TLS_STREAM_RETIRE))
#error "RPC WSS HTTP requires WS HTTP, owned TLS exchange and physical TLS retirement"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_HTTP)
typedef enum xrpcwshttpauthstatus {
    XRPC_WS_HTTP_ALLOW=0,XRPC_WS_HTTP_UNAUTHORIZED,XRPC_WS_HTTP_FORBIDDEN,XRPC_WS_HTTP_AUTH_ERROR
} xrpcwshttpauthstatus;
typedef struct xrpcwshttpauthrequest {
    const xhttp1head* Head;
    xnetaddr Remote;
    xcancel* Cancel;
    double Deadline;
    bool Secure;
} xrpcwshttpauthrequest;
/* Existing shared Server executor, never an IO Worker. Head is an owned immutable
 * snapshot borrowed during this call; Cancel is poll-only. Return bounded opaque
 * context for subsequent RPC handlers. Poll the original Deadline/Cancel. Code/
 * User remain resident through physical peer retirement; noncooperative work
 * delays that retirement. No input/output borrow survives this callback. */
typedef xrpcwshttpauthstatus (*xrpcwshttpauthorize)(const xrpcwshttpauthrequest*,
    bytes Context,size_t Capacity,size_t* ContextBytes,ptr User);
typedef struct xrpcwshttpconfig {
    size_t Size;
    xhttp1limits Head;
    size_t MaxWriteBytes;
    int64 SetupTimeout;
    xstrview Host,Target,Origin,UnauthorizedChallenge;
    const xhttpfield* Fields;size_t FieldCount;
    xwsupgradeclientconfig Client;
    xwsupgradeserverconfig Server;
    bool RequireProtocol;
    xrpcwshttpauthorize Authorize;ptr AuthorizationUser;
    size_t MaxAuthorizationContextBytes;
} xrpcwshttpconfig;
typedef enum xrpcwshttpoutcome {
    XRPC_WS_HTTP_PENDING=0,XRPC_WS_HTTP_OPEN,XRPC_WS_HTTP_REJECTED,
    XRPC_WS_HTTP_FAILED,XRPC_WS_HTTP_TIMED_OUT,XRPC_WS_HTTP_CLOSED
} xrpcwshttpoutcome;
typedef struct xrpcwshttpinfo {
    size_t Size;
    xrpcwshttpoutcome Outcome;
    double Deadline;
    uint16 Status,AuthorizationStatus;
    size_t HeaderBytes,WriteBytes,ColdBytes,PeakColdBytes,HeaderCopyBytes;
    bool Server,Secure,AuthorizationPending;
} xrpcwshttpinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: strict Core head limits,32KiB output,10s whole setup deadline,
 * /rpc target,required jsonrpc subprotocol,both compression policies disabled.
 * Client Host is required. Server nonempty Host/Origin are exact allow rules
 * (Host ASCII case-insensitive); Target is exact including any query. Empty
 * Origin does not implement a browser-origin allow rule. Fields are client
 * request/server response extras; managed/framing/handshake fields are rejected.
 * <=32 extras,16KiB head,128 fields,64KiB output,16KiB auth context hard caps.
 * Strings/fields are copied before return, credentials erased at cold cleanup.
 * DeflateData and AuthorizationUser are borrowed through peer retirement. */
XRPC_API bool xrpcWsHttpConfigInit(xrpcwshttpconfig*);
/* Exclusive native OPEN Worker factories; role follows function. TLS must be
 * authenticated and outside its Open/Read driver. Success consumes one native
 * owner and returns the same xrpcws handle in UPGRADING. Existing buffered HTTP
 * bytes are processed, then only the validated header is consumed; its first WS
 * suffix is preserved. No JSON or handler dispatch before validated Upgrade.
 * All calls/batches/notifications use their original API-entry deadlines while
 * queued. SetupTimeout begins at factory entry; no clock refresh at 101/auth.
 * False changes no output/binding and consumes no native owner. No replay,
 * redirect, credential retry or implicit downgrade. DNS/TCP/TLS establishment
 * precedes these attach factories and has its own host ownership contract.
 * Authorize requires RPC.Duplex.Server and an empty RPC.RequestContext; its
 * allowed context becomes that connection's handler context. All setup and
 * executor tails join through ordinary Close/WaitFor/Destroy. */
XRPC_API bool xrpcWsHttpClientTcp(xnetstream*,const xrpcwsconfig*,const xrpcwshttpconfig*,xrpcws**);
XRPC_API bool xrpcWsHttpServerTcp(xnetstream*,const xrpcwsconfig*,const xrpcwshttpconfig*,xrpcws**);
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
XRPC_API bool xrpcWsHttpClientTls(xtlsstream*,const xrpcwsconfig*,const xrpcwshttpconfig*,xrpcws**);
XRPC_API bool xrpcWsHttpServerTls(xtlsstream*,const xrpcwsconfig*,const xrpcwshttpconfig*,xrpcws**);
#endif
/* Copied snapshot, including after cold setup storage has retired. Ordinary
 * post-Upgrade attach peers report STATE and leave output unchanged. */
XRPC_API bool xrpcWsHttpInfo(xrpcws*,xrpcwshttpinfo*);
XRT_EXTERN_C_END
#endif
#endif
