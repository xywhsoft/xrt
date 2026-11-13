#ifndef XRPC_TLS_CONNECT_INTERNAL_H
#define XRPC_TLS_CONNECT_INTERNAL_H
#include <xrpc/tls.h>
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
/* Product-private cold TCP source. Success-only creator Data transfer.
 * All methods and Connected/Retired run on the peer Worker. Start success
 * transfers a real native callback credit and reports transferred winner/failure
 * then owned callback retirement, once; failure transfers none. No IO vtable. */
typedef struct xrpctlsconnect {
    ptr Data;
    bool (*Start)(ptr,xrpctls*,xnetengine*,uint64,const xnetstreamconfig*);
    void (*Cancel)(ptr);
    void (*Drop)(ptr);
    bool Pending;
} xrpctlsconnect;
bool xrpcTlsCreatePlanned(xnetengine*,uint64,const xtlsclientconfig*,const xrpctlsconfig*,xrpctlsconnect*,xrpctls**,double);
void xrpcTlsConnected(xrpctls*,xnetresult,xnetstream*,const xerror*);
void xrpcTlsConnectRetired(xrpctls*);
#endif
#endif
