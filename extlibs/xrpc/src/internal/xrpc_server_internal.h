#ifndef XRPC_SERVER_INTERNAL_H
#define XRPC_SERVER_INTERNAL_H
#include <xrpc/server.h>
#if defined(XRPC_FEATURE_XRPC_SERVER)
/* Product-private: transport first-byte deadline origin, without ms rounding. */
xrpcserveradmission xrpcServerSubmitEntered(xrpcserver* Server,xstrview Input,
    int64 Timeout,xfuture** Future,double Entered);
#endif
#endif
