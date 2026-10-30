#ifndef XRPC_CALL_INTERNAL_H
#define XRPC_CALL_INTERNAL_H
#include <xrpc/call.h>
#if defined(XRPC_FEATURE_XRPC_CALL)
/* Product-internal constructor shares an API-entry monotonic origin without
 * quantizing away pre-admission/encoding time. No new public ABI. */
bool xrpcCallCreateEntered(const xrpccallconfig* Config, xrpccall** Call,
    xfuture** Future, double Entered);
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config, size_t Count,
    xrpccall** Call, xfuture** Future, double Entered);
#endif
#endif
