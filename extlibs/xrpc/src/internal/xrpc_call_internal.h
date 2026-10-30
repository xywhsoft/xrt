#ifndef XRPC_CALL_INTERNAL_H
#define XRPC_CALL_INTERNAL_H
#include <xrpc/call.h>
#include "xrpc_result_quota.h"
#if defined(XRPC_FEATURE_XRPC_CALL)
/* Product-internal constructor shares an API-entry monotonic origin without
 * quantizing away pre-admission/encoding time. No new public ABI. */
bool xrpcCallCreateEntered(const xrpccallconfig* Config, xrpccall** Call,
    xfuture** Future, double Entered);
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config, size_t Count,
    xrpccall** Call, xfuture** Future, double Entered);
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
bool xrpcCallCreateQuota(const xrpccallconfig* Config,size_t Count,
    xrpccall** Call,xfuture** Future,double Entered,xrpcresultquota* Quota,bool* Limited);
#endif
#endif
#endif
