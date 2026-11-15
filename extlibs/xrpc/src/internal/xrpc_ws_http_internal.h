#ifndef XRPC_WS_HTTP_INTERNAL_H
#define XRPC_WS_HTTP_INTERNAL_H
#include <xrpc/ws_http.h>
#include "xrpc_server_internal.h"
#if defined(XRPC_FEATURE_XRPC_WS_HTTP)
typedef struct xrpcwsauthorization xrpcwsauthorization;
#if defined(XRPC_FEATURE_XRPC_WS_DIAL)
typedef struct xrpcwsdialdata xrpcwsdialdata;
#endif
typedef struct xrpcwsupgrade {
    xrpcwshttpconfig Config;
    ptr Native;
    size_t Bytes,WireBytes,WireOffset,HeaderBytes;
    double Deadline;
    bool TLS,Server,EventGone,NativeClosed,InputEnded,Paused,Checked,Rejected,Closing;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL)
    bool Establishing,BindTLS;xrpcwsdialdata* Dial;
    #endif
    xrpcwsreason Reason;
    xhttpfield* Fields;
    unsigned char* Wire;
    unsigned char* ZeroContext;
    unsigned char* Cache;size_t CacheUsed,ScanOffset,ScanLineStart,ScanLine;
    xwsupgrade Negotiated;
    char Key[XWS_KEY_CAPACITY],Protocol[256];
    xrpcwsauthorization* Authorization;
} xrpcwsupgrade;
struct xrpcwsauthorization {
    xrpcserverwork Work;
    xatomic32 References,Ready;
    xrpcws* Peer;
    xrpcwshttpauthorize Call;
    ptr User;
    size_t Bytes,Capacity,ContextBytes;
    bytes Context;
    uint16 Status;
    xrpcwshttpauthrequest Request;
    xhttp1head Head;
};
#endif
#endif
