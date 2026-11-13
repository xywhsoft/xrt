#ifndef XRPC_TCP_CONNECT_INTERNAL_H
#define XRPC_TCP_CONNECT_INTERNAL_H
#include <xrpc/tcp.h>
#if defined(XRPC_FEATURE_XRPC_TCP_DIAL)
/* Product-private cold setup only. Creator Data transfers on factory success.
 * Start/Cancel/Drop and Connected/Retired run on this peer's host Worker.
 * Start success owns a real native callback credit and must report Connected
 * then physical Retired exactly once; failure transfers none. No IO vtable. */
typedef struct xrpctcpconnect {
    ptr Data;
    bool (*Start)(ptr,xrpctcp*,xnetengine*,uint64,const xnetstreamconfig*);
    void (*Cancel)(ptr);
    void (*Drop)(ptr);
    bool Pending;
} xrpctcpconnect;
bool xrpcTcpCreatePlanned(xnetengine*,uint64,const xrpctcpconfig*,xrpctcpconnect*,xrpctcp**,double);
void xrpcTcpConnected(xrpctcp*,xnetresult,xnetstream*,const xerror*);
void xrpcTcpConnectRetired(xrpctcp*);
#endif
#endif
