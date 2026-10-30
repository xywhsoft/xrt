#include <xrpc/http_server_tls.h>
#include "../internal/xrpc_http_server_tls.h"
#include <xrt/memory.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_SERVER_TLS)
static bool xrpcHsTlsArgument(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.rpc",XRPC_HTTP_SERVER_ARGUMENT,Text);return false;}
static const xstrview xrpcHsHttp11={"http/1.1",8};
XRPC_API bool xrpcHttpServerTlsConfigInit(xrpchttpservertlsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xrpcHsTlsArgument("invalid TLS server config output");
    *K=(xrpchttpservertlsconfig){0};K->Size=sizeof(*K);xrtTlsServerConfigInit(&K->Session);xrtTlsStreamConfigInit(&K->Stream);
    K->MaxHandshakes=128;K->MaxMetadataBytes=65536;K->Session.Protocols=&xrpcHsHttp11;K->Session.ProtocolCount=1;return true;
}
void xrpcHsSecureDrop(xrpchssecure* S)
{if(S){xrtTlsIdentityRelease((xtlsidentity*)S->Config.Session.Identity);xrtTlsContextRelease((xtlscontext*)S->Config.Session.Context);xrtFree(S);}}
XRPC_API bool xrpcHttpServerTlsCreate(xnetengine* E,const xrpchttpserverconfig* H,const xrpchttpservertlsconfig* K,xrpchttpserver** Out)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || !K || !xrtMemRangeValid(K,sizeof(*K)) ||
        xrtMemRangesOverlap(K,sizeof(*K),Out,sizeof(*Out)))return xrpcHsTlsArgument("invalid TLS server descriptor/output");
    xrpchttpservertlsconfig C=*K;
    if(C.Size!=sizeof(C) || !C.MaxHandshakes || C.MaxHandshakes>1000000u || !C.MaxMetadataBytes || C.MaxMetadataBytes>1048576u ||
        C.Stream.HandshakeTimeout<0 || C.Stream.CloseTimeout<0 || !C.Stream.AsyncBytesLimit || !C.Stream.AsyncCountLimit || !C.Stream.AsyncBatch ||
        (!C.Session.Identity && !C.Session.Select) || C.Session.ProtocolCount>1 || (C.Session.Resume && C.Session.ResumeOwnedV1))return xrpcHsTlsArgument("invalid TLS server identity/policy/bounds");
    size_t Bytes=C.Session.ProtocolCount*sizeof(xstrview);
    if(Bytes>C.MaxMetadataBytes || !xrtMemRangeValid(C.Session.Protocols,Bytes) || xrtMemRangesOverlap(C.Session.Protocols,Bytes,Out,sizeof(*Out)))return xrpcHsTlsArgument("invalid TLS server ALPN descriptors");
    for(size_t I=0;I<C.Session.ProtocolCount;++I){xstrview V=C.Session.Protocols[I];
        if(V.Size>C.MaxMetadataBytes-Bytes || !xrtMemRangeValid(V.Data,V.Size) || xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out)) || V.Size!=8 || memcmp(V.Data,"http/1.1",8))return xrpcHsTlsArgument("TLS HTTP server supports HTTP/1.1 ALPN only");
        Bytes+=V.Size;}
    xrpchssecure* S=xrtCalloc(1,sizeof(*S)+Bytes);if(!S)return false;S->Config=C;S->Bytes=sizeof(*S)+Bytes;S->Info.Size=sizeof(S->Info);
    S->Config.Session.Context=NULL;S->Config.Session.Identity=NULL;S->Config.Session.Protocols=NULL;
    if(C.Session.Context && !(S->Config.Session.Context=xrtTlsContextRetain(C.Session.Context)))goto Failed;
    if(C.Session.Identity && !(S->Config.Session.Identity=xrtTlsIdentityRetain(C.Session.Identity)))goto Failed;
    if(C.Session.ProtocolCount){xstrview* Views=(xstrview*)(S+1);unsigned char* Text=(unsigned char*)(Views+C.Session.ProtocolCount);
        Views[0]=(xstrview){(cstr)Text,8};memcpy(Text,C.Session.Protocols[0].Data,8);S->Config.Session.Protocols=Views;}
    if(xrpcHttpServerCreateSecure(E,H,S,Out))return true;
Failed:
    {xerror* Error=xrtTakeError();xrpcHsSecureDrop(S);xrtClearError();xrtSetErrorTake(Error);return false;}
}
#endif
