#ifndef XRPC_SERVER_INTERNAL_H
#define XRPC_SERVER_INTERNAL_H
#include <xrpc/server.h>
#if defined(XRPC_FEATURE_XRPC_SERVER)
/* Product-private: transport first-byte deadline origin, without ms rounding. */
xrpcserveradmission xrpcServerSubmitEntered(xrpcserver* Server,xstrview Input,
    int64 Timeout,xfuture** Future,double Entered);
/* Copied context with the same transport-origin deadline. */
xrpcserveradmission xrpcServerSubmitContextEntered(xrpcserver* Server,xstrview Input,
    xbytesview Context,int64 Timeout,xfuture** Future,double Entered);
/* Transport work borrows the existing executor. Caller embeds this node and
 * transfers one Data reference only on ACCEPTED. Cancel is owned by Data. Drop
 * runs once after Call returns or queued cancellation; Core tracks its physical
 * tail even after auxiliary admission credit is returned. No Future/scratch. */
typedef struct xrpcserverwork xrpcserverwork;
struct xrpcserverwork {
    xrpcserver* Server;size_t Bytes;double Deadline;xcancel* Cancel;
    void (*Call)(xrpcserverwork*,xrpcserverstatus);
    void (*Drop)(xrpcserverwork*);
    bool Called;
};
xcancel* xrpcServerCancelChild(xrpcserver* Server);
xrpcserveradmission xrpcServerWorkSubmit(xrpcserver* Server,xrpcserverwork* Work);
#endif
#endif
