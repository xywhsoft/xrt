#ifndef XRPC_WS_DEFLATE_H
#define XRPC_WS_DEFLATE_H
#include <xrpc/ws.h>
#if defined(XRPC_FEATURE_XRPC_WS_DEFLATE) && (!defined(XRPC_FEATURE_XRPC_WS) || !defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE))
#error "RPC WS compression requires WS and Core compressed streams"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_HTTP_DEFLATE) && (!defined(XRPC_FEATURE_XRPC_WS_HTTP) || !defined(XRPC_FEATURE_XRPC_WS_DEFLATE) || !defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE))
#error "RPC compressed HTTP Upgrade requires HTTP WS and Core negotiation"
#endif
#if defined(XRPC_FEATURE_XRPC_WS_DEFLATE)
typedef struct xrpcwsdeflateinfo {
    size_t Size;
    xwsdeflate Negotiated;
    size_t CompressMinBytes;
    bool Enabled,CompressOutput;
} xrpcwsdeflateinfo;
XRT_EXTERN_C_BEGIN
/* Copied facts after native WS attach/HTTP handoff, including after Close.
 * Enabled is the actual negotiation; CompressOutput is the caller's policy,
 * applied only when Enabled and message bytes >= CompressMinBytes. Never
 * exposes codec/dictionary pointers. No negotiation yet returns STATE and
 * preserves output. Keep the peer owner stable through this concurrent call. */
XRPC_API bool xrpcWsDeflateInfo(xrpcws*,xrpcwsdeflateinfo*);
XRT_EXTERN_C_END
#endif
#endif
