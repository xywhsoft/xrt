#ifndef XRPC_H
#define XRPC_H
#include <xrpc/features.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DECODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DECODE"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ENCODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ENCODE"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DISPATCH)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DISPATCH"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_METHODS)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_METHODS"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ID)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ID"
#endif
#include <xrpc/decode.h>
#include <xrpc/encode.h>
#include <xrpc/dispatch.h>
#include <xrpc/methods.h>
#include <xrpc/id.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_PENDING)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_PENDING"
#endif
#include <xrpc/pending.h>
#if defined(XRPC_FEATURE_XRPC) && (!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRPC_FEATURE_XRPC_RECEIVE))
#error "XRPC_FEATURE_XRPC requires response and receive"
#endif
#include <xrpc/response.h>
#include <xrpc/receive.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_CALL)
#error "XRPC_FEATURE_XRPC requires call"
#endif
#include <xrpc/call.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
#error "XRPC_FEATURE_XRPC requires http_client"
#endif
#include <xrpc/http_client.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_SERVER)
#error "XRPC_FEATURE_XRPC requires server"
#endif
#include <xrpc/server.h>
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_HTTP_SERVER)
#error "XRPC_FEATURE_XRPC requires http_server"
#endif
#include <xrpc/http_server.h>
#endif
