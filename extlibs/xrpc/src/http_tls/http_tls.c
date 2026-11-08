#include <xrpc/http_tls.h>
#include "../internal/xrpc_http_tls.h"
#include <xrt/memory.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_TLS)
static bool xrpcTlsArgument(cstr Message)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.rpc",XRPC_HTTP_ARGUMENT,Message);return false;}
static const xstrview xrpcTlsHttp11={"http/1.1",8};
XRPC_API bool xrpcHttpTlsConfigInit(xrpchttptlsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xrpcTlsArgument("invalid HTTP TLS config output");
    *K=(xrpchttptlsconfig){0};K->Size=sizeof(*K);xrtTlsClientConfigInit(&K->Session);xrtTlsStreamConfigInit(&K->Stream);
    xrpcHttpDialConfigInit(&K->Dial);K->Session.Protocols=&xrpcTlsHttp11;K->Session.ProtocolCount=1;
    K->MaxMetadataBytes=65536;K->WriteBytes=65536;return true;
}
void xrpcHttpSecureDrop(xrpchttpsecure* S)
{if(S){xrtTlsStreamDestroy(S->Stream);xrtTlsSessionDestroy(S->Session);xrtFree(S);}}
static bool xrpcTlsCreate(xnetengine* E,xnetresolver* R,xnetaddrlist* A,xstrview N,uint16 P,uint64 F,xstrview H,xstrview T,
    const xrpchttpconfig* HTTP,const xrpchttptlsconfig* TLS,xrpchttpclient** Out,double Began)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || !TLS || !xrtMemRangeValid(TLS,sizeof(*TLS)) ||
        xrtMemRangesOverlap(TLS,sizeof(*TLS),Out,sizeof(*Out)))return xrpcTlsArgument("invalid HTTP TLS descriptor/output");
    xrpchttptlsconfig K=*TLS;
    if(K.Size!=sizeof(K) || !K.MaxMetadataBytes || K.MaxMetadataBytes>1048576u || !K.WriteBytes || K.WriteBytes>1048576u ||
        K.Stream.HandshakeTimeout<0 || K.Stream.CloseTimeout<0 || !K.Stream.AsyncBytesLimit || !K.Stream.AsyncCountLimit || !K.Stream.AsyncBatch ||
        K.Session.ProtocolCount>1 || (!K.Session.ServerName.Size && !K.Session.VerifyName.Size))return xrpcTlsArgument("invalid HTTP TLS policy/identity/bounds");
    size_t Bytes=K.Session.ProtocolCount*sizeof(xstrview);
    if(!xrtMemRangeValid(K.Session.Protocols,Bytes) || xrtMemRangesOverlap(K.Session.Protocols,Bytes,Out,sizeof(*Out)) || Bytes>K.MaxMetadataBytes)
        return xrpcTlsArgument("invalid HTTP TLS ALPN descriptors");
    for(size_t I=0;I<K.Session.ProtocolCount+2;++I){xstrview V=I==0?K.Session.ServerName:I==1?K.Session.VerifyName:K.Session.Protocols[I-2];
        if(V.Size>K.MaxMetadataBytes-Bytes || !xrtMemRangeValid(V.Data,V.Size) || xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out)) ||
            (I<2 && V.Size && memchr(V.Data,0,V.Size)))return xrpcTlsArgument("invalid HTTP TLS name/metadata");
        if(I>=2 && (V.Size!=8 || memcmp(V.Data,"http/1.1",8)))return xrpcTlsArgument("HTTP TLS supports HTTP/1.1 ALPN only");
        Bytes+=V.Size;
    }
    xrpchttpsecure* S=xrtCalloc(1,sizeof(*S));if(!S)return false;S->Config=K.Stream;S->WriteBytes=K.WriteBytes;
    S->Session=xrtTlsClientCreate(&K.Session,NULL);if(!S->Session){xrpcHttpSecureDrop(S);return false;}
    if(xrpcHttpDialCreate(E,R,A,N,P,F,H,T,HTTP,&K.Dial,Out,Began,S))return true;
    xerror* Error=xrtTakeError();xrpcHttpSecureDrop(S);xrtClearError();xrtSetErrorTake(Error);return false;
}
XRPC_API bool xrpcHttpTlsDial(xnetengine* E,xnetresolver* R,xstrview N,uint16 P,uint64 F,xstrview H,xstrview T,const xrpchttpconfig* C,const xrpchttptlsconfig* K,xrpchttpclient** O)
{return xrpcTlsCreate(E,R,NULL,N,P,F,H,T,C,K,O,xrtTimer());}
XRPC_API bool xrpcHttpTlsDialAddresses(xnetengine* E,xnetaddrlist* A,uint64 F,xstrview H,xstrview T,const xrpchttpconfig* C,const xrpchttptlsconfig* K,xrpchttpclient** O)
{double Began=xrtTimer();if(!A)return xrpcTlsArgument("HTTP TLS endpoint list is NULL");return xrpcTlsCreate(E,NULL,A,(xstrview){0},0,F,H,T,C,K,O,Began);}
XRPC_API bool xrpcHttpTlsConnect(xnetengine* E,const xnetaddr* A,uint64 F,xstrview H,xstrview T,const xrpchttpconfig* C,const xrpchttptlsconfig* K,xrpchttpclient** O)
{
    double Began=xrtTimer();if(!A || !O || !xrtMemRangeValid(A,sizeof(*A)) || !xrtMemRangeValid(O,sizeof(*O)) ||
        xrtMemRangesOverlap(A,sizeof(*A),O,sizeof(*O)))return xrpcTlsArgument("invalid HTTP TLS numeric endpoint/output");
    xnetaddrlist* List=xrtNetAddrListCreate(A,1);if(!List)return false;
    bool OK=xrpcTlsCreate(E,NULL,List,(xstrview){0},0,F,H,T,C,K,O,Began);xerror* Error=OK?NULL:xrtTakeError();
    xrtNetAddrListDestroy(List);if(!OK){xrtClearError();xrtSetErrorTake(Error);}return OK;
}
#endif
