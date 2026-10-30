#ifndef XRPC_EXAMPLE_HTTP_TLS_PEER_H
#define XRPC_EXAMPLE_HTTP_TLS_PEER_H
/* Bounded single-connection test/demo TLS 1.3 peer, not the production server.
 * Historical public test identity plus explicit custom fixture trust. This
 * verifier is deliberately unsuitable for arbitrary remote services. */
#include "http_peer.h"
#include <xrpc/http_tls.h>
#include "../../../../tests/fixtures/tls_server.h"
typedef struct rpcTlsPeer {
    xnetengine* Engine;xnetlistener* Listener;xnetaddr Address;xatomicptr TLS,Raw;
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;
    xatomic32 Closed,ListenerClosed,Commands,PlainBytes,Verified,Early,VerifyGate;
    bool HoldHandshake,Reject,Truncate;
    unsigned char Input[65536],Output[131072];size_t Used,OutAt,OutEnd;
} rpcTlsPeer;
static inline xtlsstream* rpcTlsPeerStream(rpcTlsPeer* P)
{return xrtAtomicPtrLoad(&P->TLS,XMEMORY_ACQUIRE);}
static inline void rpcTlsPeerPump(xtlsstream* S,ptr Data)
{
    rpcTlsPeer* P=Data;
    while(P->OutAt<P->OutEnd){size_t N=0;xtlsresult R=xrtTlsStreamSend(S,P->Output+P->OutAt,P->OutEnd-P->OutAt,&N);P->OutAt+=N;
        if(R!=XTLS_OK || !N)return;}
    if(P->Truncate)(void)xrtNetStreamShutdownWrite(xrtTlsStreamTransport(S));
}
static inline void rpcTlsPeerReply(rpcTlsPeer* P,xstrview Input)
{
    xrpcmessage M[256];xrpcdocument D={0};xrpcdecodeconfig K;xrpcDecodeConfigInit(&K);K.Mode=XRPC_DECODE_REQUESTS;
    rpcHttpPeerRequire(xrpcDecode(Input,&K,M,256,&D)==XRPC_DECODE_DONE,"TLS peer RPC envelope");
    xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);
    if(rpcHttpMethod(&M[0],"hold"))return;
    if(rpcHttpMethod(&M[0],"drop")){(void)xrtTlsStreamAbort(rpcTlsPeerStream(P));return;}
    rpcHttpPeerRequire(P->OutAt==P->OutEnd,"TLS peer response queue");size_t Count=0,Used=512;
    if(D.Batch)P->Output[Used++]='[';
    for(size_t I=0;I<D.Count;++I){const xrpcmessage* V=&M[I];if(V->Kind!=XRPC_MESSAGE_CALL)continue;if(Count++)P->Output[Used++]=',';
        int N=rpcHttpMethod(V,"error")?
            snprintf((char*)P->Output+Used,sizeof(P->Output)-Used,"{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"error\":{\"code\":-1,\"message\":\"failure\"}}",(int)V->Id.Raw.Size,V->Id.Raw.Data):
            snprintf((char*)P->Output+Used,sizeof(P->Output)-Used,"{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"result\":%.*s}",(int)V->Id.Raw.Size,V->Id.Raw.Data,(int)(V->Params.Raw.Size?V->Params.Raw.Size:4),V->Params.Raw.Size?V->Params.Raw.Data:"null");
        rpcHttpPeerRequire(N>0 && (size_t)N<sizeof(P->Output)-Used,"TLS peer bounded reply");Used+=(size_t)N;
    }
    if(D.Batch)P->Output[Used++]=']';
    if(!Count)Used=512;
    char Head[256];int N=snprintf(Head,sizeof(Head),"HTTP/1.1 %s\r\nContent-Length: %zu\r\n\r\n",Count?"200 OK":"204 No Content",Used-512);
    rpcHttpPeerRequire(N>0 && N<256,"TLS peer head");memcpy(P->Output+512-(size_t)N,Head,(size_t)N);P->OutAt=512-(size_t)N;P->OutEnd=Used;
    if(rpcHttpMethod(&M[0],"trunc")){P->OutEnd=512+1;P->Truncate=true;}
    rpcTlsPeerPump(rpcTlsPeerStream(P),P);
}
static inline void rpcTlsPeerRead(xtlsstream* S,const xnetbuf* Buffer,ptr Data)
{
    (void)Buffer;rpcTlsPeer* P=Data;size_t N=0;rpcHttpPeerRequire(xrtTlsStreamRead(S,P->Input+P->Used,sizeof(P->Input)-P->Used,&N)!=XTLS_ERROR,"TLS peer read");
    P->Used+=N;xrtAtomic32FetchAdd(&P->PlainBytes,(uint32)N,XMEMORY_RELEASE);
    if(xrtAtomic32Exchange(&P->Early,0,XMEMORY_ACQ_REL)){
        cstr Body="{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"result\":null}";char Head[128];int H=snprintf(Head,sizeof(Head),"HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n",strlen(Body));
        memcpy(P->Output,Head,(size_t)H);memcpy(P->Output+H,Body,strlen(Body));P->OutAt=0;P->OutEnd=(size_t)H+strlen(Body);rpcTlsPeerPump(S,P);return;
    }
    for(;;){xhttpfield Fields[32];xhttp1head H;xrtHttp1HeadInit(&H,Fields,32);xhttp1status R=xrtHttp1RequestParse((xbytesview){P->Input,P->Used},&H,NULL,NULL);
        rpcHttpPeerRequire(R==XHTTP1_MORE || R==XHTTP1_READY,"TLS peer HTTP head");if(R==XHTTP1_MORE)return;
        xhttp1bodyplan Plan;rpcHttpPeerRequire(xrtHttp1RequestBodyPlan(&H,&Plan) && Plan.Mode==XHTTP1_BODY_FIXED && Plan.Length<=sizeof(P->Input)-H.Bytes,"TLS peer request bound");
        if(Plan.Length>P->Used-H.Bytes)return;
        rpcHttpPeerRequire(H.MethodCode==XHTTP_METHOD_POST && H.Target.Size==4 && !memcmp(H.Target.Data,"/rpc",4),"TLS peer target");
        rpcTlsPeerReply(P,(xstrview){(cstr)P->Input+H.Bytes,(size_t)Plan.Length});size_t Used=H.Bytes+(size_t)Plan.Length;
        memmove(P->Input,P->Input+Used,P->Used-Used);P->Used-=Used;if(!P->Used)return;
    }
}
static inline void rpcTlsPeerEnd(xtlsstream* S,ptr Data){(void)Data;(void)xrtTlsStreamAbort(S);}
static inline void rpcTlsPeerClosed(xtlsstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;(void)R;(void)E;xrtAtomic32Store(&((rpcTlsPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static inline void rpcTlsPeerRawClosed(xnetstream* S,xnetresult R,const xerror* E,ptr Data)
{(void)S;(void)R;(void)E;xrtAtomic32Store(&((rpcTlsPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static inline bool rpcTlsPeerAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;rpcTlsPeer* P=Data;rpcHttpPeerRequire(!rpcTlsPeerStream(P) && !xrtAtomicPtrLoad(&P->Raw,XMEMORY_ACQUIRE),"TLS peer unexpected replay/connection");
    if(P->HoldHandshake){static const xnetstreamevents Events={.Close=rpcTlsPeerRawClosed};rpcHttpPeerRequire(xrtNetStreamSetEvents(S,&Events,P),"held TLS peer events");xrtAtomicPtrStore(&P->Raw,S,XMEMORY_RELEASE);return true;}
    xtlsserverconfig K;xrtTlsServerConfigInit(&K);K.Context=P->Context;K.Identity=P->Identity;static const xstrview Protocol={"http/1.1",8};K.Protocols=&Protocol;K.ProtocolCount=1;
    xtlsstreamconfig C;xrtTlsStreamConfigInit(&C);C.CloseTimeout=200;
    static const xtlsstreamevents Events={.Read=rpcTlsPeerRead,.End=rpcTlsPeerEnd,.Writable=rpcTlsPeerPump,.Drain=rpcTlsPeerPump,.Close=rpcTlsPeerClosed};xtlsstream* TLS=NULL;
    rpcHttpPeerRequire(xrtTlsStreamAccept(S,&K,&C,&Events,P,&TLS),"TLS peer accept");xrtAtomicPtrStore(&P->TLS,TLS,XMEMORY_RELEASE);return true;
}
static inline xtlsverifydecision rpcTlsPeerVerify(const xtlspeer* V,ptr Data)
{
    rpcTlsPeer* P=Data;rpcHttpPeerRequire(V && V->Role==XTLS_SERVER && V->CertificateCount && V->Name.Size==11 && !memcmp(V->Name.Data,"rpc.fixture",11),"TLS copied verification identity");
    xrtAtomic32Store(&P->Verified,1,XMEMORY_RELEASE);
    while(xrtAtomic32Load(&P->VerifyGate,XMEMORY_ACQUIRE)==1)xrtSleep(1);
    return P->Reject?XTLS_VERIFY_REJECT:XTLS_VERIFY_ACCEPT;
}
static inline void rpcTlsPeerListenerClose(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((rpcTlsPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline void rpcTlsPeerStart(rpcTlsPeer* P,bool Hold,bool Reject)
{
    memset(P,0,sizeof(*P));xrtAtomicPtrInit(&P->TLS,NULL);xrtAtomicPtrInit(&P->Raw,NULL);
    xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);xrtAtomic32Init(&P->Commands,0);xrtAtomic32Init(&P->PlainBytes,0);xrtAtomic32Init(&P->Verified,0);xrtAtomic32Init(&P->Early,0);xrtAtomic32Init(&P->VerifyGate,0);P->HoldHandshake=Hold;P->Reject=Reject;
    P->Context=testTlsServerContext();P->Identity=testTlsServerIdentity();xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcTlsPeerVerify;V.Context=P;xtlsverifylimits VL;xrtTlsVerifyLimitsInit(&VL);
    P->Verifier=xrtTlsVerifierCreateLimited(&V,&VL);rpcHttpPeerRequire(P->Context && P->Identity && P->Verifier,"TLS peer identity/policy/verifier");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);rpcHttpPeerRequire(P->Engine && xrtNetEngineStart(P->Engine),"TLS peer engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Affinity=0;K.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents Events={rpcTlsPeerAccept,NULL,rpcTlsPeerListenerClose};P->Listener=xrtNetListen(P->Engine,&K,&Events,NULL,P);
    rpcHttpPeerRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"TLS peer listen");
}
static inline void rpcTlsPeerConfig(rpcTlsPeer* P,xrpchttptlsconfig* K)
{xrpcHttpTlsConfigInit(K);K->Session.Context=P->Context;K->Session.Verifier=P->Verifier;K->Session.VerifyName=(xstrview)XRT_STR_LITERAL("rpc.fixture");K->Stream.CloseTimeout=200;}
static inline void rpcTlsPeerStop(rpcTlsPeer* P)
{
    rpcHttpPeerRequire(xrtNetListenerClose(P->Listener),"TLS peer listener close");rpcHttpWait(&P->ListenerClosed,1);
    xtlsstream* S=rpcTlsPeerStream(P);xnetstream* Raw=xrtAtomicPtrLoad(&P->Raw,XMEMORY_ACQUIRE);if(S || Raw)rpcHttpWait(&P->Closed,1);
    xrtTlsStreamDestroy(S);xrtNetStreamDestroy(Raw);xrtNetListenerDestroy(P->Listener);
    rpcHttpPeerRequire(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"TLS peer physical engine duties");
    xrtTlsVerifierRelease(P->Verifier);xrtTlsIdentityRelease(P->Identity);xrtTlsContextRelease(P->Context);
}
static inline void rpcTlsClientClose(xrpchttpclient* C)
{xfuture* F=xrpcHttpClosed(C);rpcHttpPeerRequire(F && xrpcHttpClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"TLS client physical close");xrpcHttpRelease(C);xrtFutureDestroy(F);}
#endif
