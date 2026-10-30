#ifndef XRPC_HTTP_SERVER_TLS_INTERNAL_H
#define XRPC_HTTP_SERVER_TLS_INTERNAL_H
#include <xrpc/http_server.h>
typedef struct xrpchssecure xrpchssecure;
typedef struct xrpchssecurepeer xrpchssecurepeer;
bool xrpcHttpServerCreateSecure(xnetengine* Engine,const xrpchttpserverconfig* Config,xrpchssecure* Secure,xrpchttpserver** Out);
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER_TLS)
#include <xrpc/http_server_tls.h>
struct xrpchssecure {
    xrpchttpservertlsconfig Config;
    xrpchttpservertlsinfo Info;
    size_t Bytes;
};
struct xrpchssecurepeer {
    xtlsstream* Stream;
    size_t Reserved,ContinueOffset;
    bool Handshaking,Ready;
};
void xrpcHsSecureDrop(xrpchssecure* Secure);
#endif
#endif
