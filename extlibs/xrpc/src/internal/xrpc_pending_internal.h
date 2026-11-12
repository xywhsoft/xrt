#ifndef XRPC_PENDING_INTERNAL_H
#define XRPC_PENDING_INTERNAL_H
#include <xrpc/pending.h>
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
/* No removal. Duplicated live IDs remain MATCHED in this preflight. */
bool xrpcPendingProbeBatch(xrpcpending*,const xstrview*,size_t,xrpcpendingresolution*);
#endif
#endif
