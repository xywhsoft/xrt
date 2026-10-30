#ifndef XRPC_HTTP_TLS_INTERNAL_H
#define XRPC_HTTP_TLS_INTERNAL_H
#include "xrpc_http_connect.h"
#include <xrpc/http_dial.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_DIAL)
bool xrpcHttpDialCreate(xnetengine* Engine,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Name,uint16 Port,
    uint64 Affinity,xstrview Host,xstrview Target,const xrpchttpconfig* Config,const xrpchttpdialconfig* Dial,
    xrpchttpclient** Result,double Began,xrpchttpsecure* Secure);
#endif
#if defined(XRPC_FEATURE_XRPC_HTTP_TLS)
#include <xrpc/http_tls.h>
struct xrpchttpsecure {
    xtlssession* Session;
    xtlsstream* Stream;
    xtlsstreamconfig Config;
    size_t WriteBytes,Written;
    bool Ready;
};
void xrpcHttpSecureDrop(xrpchttpsecure* Secure);
#endif
#endif
