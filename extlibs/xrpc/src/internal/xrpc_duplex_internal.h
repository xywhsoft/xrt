#ifndef XRPC_DUPLEX_INTERNAL_H
#define XRPC_DUPLEX_INTERNAL_H
#include <xrpc/duplex.h>
#if defined(XRPC_FEATURE_XRPC_TCP)
xrpcduplexadmission xrpcDuplexSubmitEntered(xrpcduplex*,const xrpcduplexitem*,size_t,bool,int64,xfuture**,double);
xrpcduplexadmission xrpcDuplexReceiveEntered(xrpcduplex*,xstrview,xbytesview,double);
#endif
#endif
