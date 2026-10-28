#ifndef XRPC_H
#define XRPC_H
#include <xrpc/features.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DECODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DECODE"
#endif
#include <xrpc/decode.h>
#endif
