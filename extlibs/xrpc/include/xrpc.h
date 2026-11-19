#ifndef XRPC_H
#define XRPC_H
#include <xrpc/features.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DECODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DECODE"
#endif
#include <xrpc/decode.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ENCODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ENCODE"
#endif
#include <xrpc/encode.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DISPATCH)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DISPATCH"
#endif
#include <xrpc/dispatch.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_METHODS)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_METHODS"
#endif
#include <xrpc/methods.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ID)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ID"
#endif
#include <xrpc/id.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_PENDING)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_PENDING"
#endif
#include <xrpc/pending.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_RESPONSE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_RESPONSE"
#endif
#include <xrpc/response.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_RECEIVE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_RECEIVE"
#endif
#include <xrpc/receive.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_CALL)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_CALL"
#endif
#include <xrpc/call.h>

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DUPLEX)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DUPLEX"
#endif
#include <xrpc/duplex.h>

#endif
