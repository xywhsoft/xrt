#ifndef XRPC_HTTP_CONNECT_INTERNAL_H
#define XRPC_HTTP_CONNECT_INTERNAL_H
#include <xrpc/http_client.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
/* Cold connection source only, not a second transport/runtime or hot IO layer.
 * Success-only transfer of Data's creator owner; Start/Cancel/Drop on the
 * client's network worker. Failed Start retains Data but transfers no callback
 * owner. Accepted Start must call Connected then Retired, once, on that worker.
 * Retired follows physical callback-owner Drop, not merely logical completion. */
typedef struct xrpchttpconnectplan {
    ptr Data;
    bool (*Start)(ptr Data,xrpchttpclient* Client,xnetengine* Engine,uint64 Affinity,
        const xnetstreamconfig* Stream,const xnetstreamevents* Events);
    void (*Cancel)(ptr Data);
    void (*Drop)(ptr Data);
    double Deadline;
    uint64 Timer;
    bool Pending;
} xrpchttpconnectplan;
typedef struct xrpchttpsecure xrpchttpsecure;
bool xrpcHttpCreatePlanned(xnetengine* Engine,uint64 Affinity,xstrview Host,xstrview Target,
    const xrpchttpconfig* Config,xrpchttpconnectplan* Plan,double Deadline,xrpchttpclient** Client);
bool xrpcHttpCreateSecurePlanned(xnetengine* Engine,uint64 Affinity,xstrview Host,xstrview Target,
    const xrpchttpconfig* Config,xrpchttpconnectplan* Plan,double Deadline,xrpchttpsecure* Secure,xrpchttpclient** Client);
void xrpcHttpConnected(xrpchttpclient* Client,xnetresult Result,xnetstream* Stream,const xerror* Cause);
void xrpcHttpConnectRetired(xrpchttpclient* Client);
#endif
#endif
