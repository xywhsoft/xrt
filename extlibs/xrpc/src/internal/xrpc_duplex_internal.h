#ifndef XRPC_DUPLEX_INTERNAL_H
#define XRPC_DUPLEX_INTERNAL_H
#include <xrpc/duplex.h>
#if defined(XRPC_FEATURE_XRPC_STDIO)
void xrpcDuplexEndInput(xrpcduplex*);
#endif
#if defined(XRPC_FEATURE_XRPC_TCP) || defined(XRPC_FEATURE_XRPC_TLS) || defined(XRPC_FEATURE_XRPC_WS) || defined(XRPC_FEATURE_XRPC_STDIO)
xrpcduplexadmission xrpcDuplexSubmitEntered(xrpcduplex*,const xrpcduplexitem*,size_t,bool,int64,xfuture**,double);
xrpcduplexadmission xrpcDuplexReceiveEntered(xrpcduplex*,xstrview,xbytesview,double);
#endif
#if defined(XRPC_FEATURE_XRPC_TCP) || defined(XRPC_FEATURE_XRPC_TLS) || defined(XRPC_FEATURE_XRPC_WS)
/* Concurrent internal admissions. The transport pins its facade through return. */
bool xrpcDuplexEnableExtensions(xrpcduplex*,uint32,size_t);
xrpcduplexadmission xrpcDuplexNegotiateExtensionEntered(xrpcduplex*,xrpcduplexextension,int64,xfuture**,double);
#endif
#if defined(XRPC_FEATURE_XRPC_TCP) || defined(XRPC_FEATURE_XRPC_TLS) || defined(XRPC_FEATURE_XRPC_WS)
/* Owner-only, no new wire reference. Check write credit before measuring expiry. */
size_t xrpcDuplexNextSize(const xrpcduplex*);
#endif
#if defined(XRPC_FEATURE_XRPC_TCP)
/* A transport can still own the wire after the owner revoked its selection. */
bool xrpcDuplexSelectionIs(const xrpcduplex*,uint64);
#endif
#endif
