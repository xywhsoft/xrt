#ifndef XRPC_EXAMPLE_WS_HTTP_SERVICE_H
#define XRPC_EXAMPLE_WS_HTTP_SERVICE_H
#include <xrpc/ws_http.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
#include "../../../../tests/fixtures/tls_server.h"
#endif
typedef struct rpcWsHttpService rpcWsHttpService;
typedef struct rpcWsHttpAttach {xnetpost Post;rpcWsHttpService* Service;ptr Native;bool Server;} rpcWsHttpAttach;
struct rpcWsHttpService {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpcws* Client;xrpcws* Accepted;xnetaddr Address;
    xrpcwsconfig ClientConfig,ServerConfig;xrpcwshttpconfig ClientHTTP,ServerHTTP;xfuture* Initial;xatomic32 ClientReady,ServerReady,Calls,NativeRefs,HandlerEntered,HandlerPermit;
    bool TLS,HoldHandler;rpcWsHttpAttach Attach[2];
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig NativeServer;
#endif
};
static void rpcWsHttpServiceRequire(bool OK,cstr M){if(!OK){fprintf(stderr,"%s: %s\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void rpcWsHttpServiceAwait(xatomic32* A){double End=xrtTimer()+15;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){rpcWsHttpServiceRequire(xrtTimer()<End,"WS fixture barrier");xrtSleep(1);}}
static bool rpcWsHttpServiceEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{
    rpcWsHttpService* S=Data;rpcWsHttpServiceRequire(!xrtNetEngineCurrent(S->Engine),"WS business outside network Worker");xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);
    if(S->HoldHandler){xrtAtomic32Store(&S->HandlerEntered,1,XMEMORY_RELEASE);rpcWsHttpServiceAwait(&S->HandlerPermit);}
    if(M->Method.Raw.Size==5&&!memcmp(M->Method.Raw.Data,"\"who\"",5))return xrpcReplyResult(R,C->Data.Size?(xstrview){(cstr)C->Data.Data,C->Data.Size}:XRT_STR_LITERAL("null"));
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static void rpcWsHttpServiceAttach(rpcWsHttpService* S,ptr Native,bool Server)
{
    xrpcws** P=Server?&S->Accepted:&S->Client;bool OK;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(S->TLS)OK=Server?xrpcWsHttpServerTls(Native,&S->ServerConfig,&S->ServerHTTP,P):xrpcWsHttpClientTls(Native,&S->ClientConfig,&S->ClientHTTP,P);else
#endif
    OK=Server?xrpcWsHttpServerTcp(Native,&S->ServerConfig,&S->ServerHTTP,P):xrpcWsHttpClientTcp(Native,&S->ClientConfig,&S->ClientHTTP,P);
    rpcWsHttpServiceRequire(OK,"HTTP Upgrade factory");if(!Server)rpcWsHttpServiceRequire(xrpcWsCall(*P,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,&S->Initial)==XRPC_DUPLEX_ACCEPTED,"first call queues before handshake owner post");xrtAtomic32Store(Server?&S->ServerReady:&S->ClientReady,1,XMEMORY_RELEASE);
}
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
static void rpcWsHttpServiceAttachPost(xnetworker* W,ptr Data){(void)W;rpcWsHttpAttach* P=Data;rpcWsHttpServiceAttach(P->Service,P->Native,P->Server);}
static bool rpcWsHttpServiceNativeCount(const void* D,size_t* N){*N=xrtAtomic32Load(&((const rpcWsHttpService*)D)->NativeRefs,XMEMORY_ACQUIRE);return true;}
static bool rpcWsHttpServiceNativeTrace(const void* D,xrtownershipvisitor V,ptr U){(void)D;(void)V;(void)U;return false;}
static void rpcWsHttpServiceNativeDrop(const void* D){xrtAtomic32FetchSub(&((rpcWsHttpService*)D)->NativeRefs,1,XMEMORY_RELEASE);}
static const xrtownershipops RpcWsHttpServiceNativeOps={rpcWsHttpServiceNativeCount,rpcWsHttpServiceNativeTrace};
static void rpcWsHttpServiceTLSOpen(xtlsstream* Native,ptr Data)
{
    rpcWsHttpService* S=Data;bool Server=xrtTlsSessionRole(xrtTlsStreamSession(Native))==XTLS_SERVER;rpcWsHttpAttach* P=S->Attach+(Server?1:0);P->Service=S;P->Native=Native;P->Server=Server;
    rpcWsHttpServiceRequire(xrtNetPostInit(&P->Post)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(Native)),&P->Post,rpcWsHttpServiceAttachPost,P),"exclusive WSS handoff after Open");
}
static const xtlsstreamownershipv1 RpcWsHttpServiceTLSPolicy={sizeof(xtlsstreamownershipv1),{.Open=rpcWsHttpServiceTLSOpen},rpcWsHttpServiceNativeDrop,&RpcWsHttpServiceNativeOps};
static xtlsverifydecision rpcWsHttpServiceVerify(const xtlspeer* P,ptr Data)
{rpcWsHttpService* S=Data;xbytesview Pin={0};return P&&P->CertificateCount&&P->Name.Size==11&&!memcmp(P->Name.Data,"rpc.fixture",11)&&xrtTlsIdentityCertificate(S->Identity,0,&Pin)&&P->Certificates[0].Raw.Size==Pin.Size&&!memcmp(P->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
#endif
static bool rpcWsHttpServiceAccept(xnetlistener* L,xnetstream* Native,ptr Data)
{
    (void)L;rpcWsHttpService* S=Data;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(S->TLS){xtlsstream* T=NULL;xrtAtomic32FetchAdd(&S->NativeRefs,1,XMEMORY_RELAXED);rpcWsHttpServiceRequire(xrtTlsStreamAcceptOwnedV1(Native,&S->NativeServer,NULL,S,&RpcWsHttpServiceTLSPolicy,&T),"WSS native accept");return true;}
#endif
    rpcWsHttpServiceAttach(S,Native,true);return true;
}
static void rpcWsHttpServiceOpen(xnetstream* Native,ptr Data){rpcWsHttpServiceAttach(Data,Native,false);}
static void rpcWsHttpServiceStartEx(rpcWsHttpService* S,bool TLS,const xrpcwsconfig* Config,bool Hold,size_t TimerLimit,const xrpcwshttpconfig* ClientHTTP,const xrpcwshttpconfig* ServerHTTP,xnetportbackend Backend)
{
    memset(S,0,sizeof(*S));S->TLS=TLS;S->HoldHandler=Hold;xrtAtomic32Init(&S->ClientReady,0);xrtAtomic32Init(&S->ServerReady,0);xrtAtomic32Init(&S->Calls,0);xrtAtomic32Init(&S->NativeRefs,1);xrtAtomic32Init(&S->HandlerEntered,0);xrtAtomic32Init(&S->HandlerPermit,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;if(TimerLimit)E.TimerLimit=TimerLimit;S->Engine=xrtNetEngineCreate(&E);rpcWsHttpServiceRequire(S->Engine&&xrtNetEngineStart(S->Engine),"WS host Engine");xrpcserverconfig C;xrpcServerConfigInit(&C);C.Call=rpcWsHttpServiceEcho;C.User=S;rpcWsHttpServiceRequire(xrpcServerCreate(&C,&S->Server),"WS shared Server");
    xrpcWsConfigInit(&S->ServerConfig);if(Config)S->ServerConfig=*Config;S->ServerConfig.Duplex.Server=S->Server;S->ServerConfig.Stream.Role=XWS_ROLE_SERVER;S->ServerConfig.Stream.Protocol=XRT_STR_LITERAL("jsonrpc");S->ClientConfig=S->ServerConfig;S->ClientConfig.Stream.Role=XWS_ROLE_CLIENT;
    xrpcWsHttpConfigInit(&S->ClientHTTP);S->ClientHTTP.Host=XRT_STR_LITERAL("rpc.fixture");xrpcWsHttpConfigInit(&S->ServerHTTP);S->ServerHTTP.Host=XRT_STR_LITERAL("rpc.fixture");if(ClientHTTP)S->ClientHTTP=*ClientHTTP;if(ServerHTTP)S->ServerHTTP=*ServerHTTP;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){S->Context=testTlsServerContext();S->Identity=testTlsServerIdentity();rpcWsHttpServiceRequire(S->Context&&S->Identity,"WSS public fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcWsHttpServiceVerify;V.Context=S;S->Verifier=xrtTlsVerifierCreate(&V);rpcWsHttpServiceRequire(S->Verifier!=NULL,"explicit WSS DER pin");xrtTlsServerConfigInit(&S->NativeServer);S->NativeServer.Context=S->Context;S->NativeServer.Identity=S->Identity;}
#else
    rpcWsHttpServiceRequire(!TLS,"TLS module not selected");
#endif
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents LE={rpcWsHttpServiceAccept,NULL,NULL};S->Listener=xrtNetListen(S->Engine,&L,&LE,NULL,S);rpcWsHttpServiceRequire(S->Listener&&xrtNetListenerLocal(S->Listener,&S->Address),"WS local listener");
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=S->Context;K.Verifier=S->Verifier;K.ServerName=XRT_STR_LITERAL("rpc.fixture");xrtAtomic32FetchAdd(&S->NativeRefs,1,XMEMORY_RELAXED);rpcWsHttpServiceRequire(xrtTlsStreamConnectOwnedV1(S->Engine,&S->Address,1,NULL,&K,NULL,S,&RpcWsHttpServiceTLSPolicy)!=NULL,"WSS native connect");}
    else
#endif
    {xnetstreamevents Events={.Open=rpcWsHttpServiceOpen};rpcWsHttpServiceRequire(xrtNetStreamConnect(S->Engine,&S->Address,1,NULL,&Events,S)!=NULL,"WS native connect");}
    rpcWsHttpServiceAwait(&S->ClientReady);rpcWsHttpServiceAwait(&S->ServerReady);
}
static void rpcWsHttpServiceStart(rpcWsHttpService* S,bool TLS,const xrpcwsconfig* Config,bool Hold)
{rpcWsHttpServiceStartEx(S,TLS,Config,Hold,0,NULL,NULL,XNET_PORT_SELECT);}
static const xrpccallresult* rpcWsHttpServiceResult(xfuture* F)
{rpcWsHttpServiceRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK,"WS call wait");const xrpccallresult* R=xrtFutureValue(F);rpcWsHttpServiceRequire(R!=NULL,"WS typed call result");return R;}
static void rpcWsHttpServicePeerClose(xrpcws* P){if(P)rpcWsHttpServiceRequire(xrpcWsClose(P)&&xrpcWsWaitFor(P,10000)==XWAIT_OK&&xrpcWsDestroy(P),"WS physical peer retirement");}
static void rpcWsHttpServiceStop(rpcWsHttpService* S)
{
    rpcWsHttpServicePeerClose(S->Client);rpcWsHttpServicePeerClose(S->Accepted);rpcWsHttpServiceRequire(xrtNetListenerClose(S->Listener),"WS listener close");xrtNetListenerDestroy(S->Listener);rpcWsHttpServiceRequire(xrpcServerDestroy(S->Server),"WS shared Server join");double End=xrtTimer()+10;xnetretireresult R;while((R=xrtNetEngineTryDestroy(S->Engine))==XNET_RETIRE_BUSY){rpcWsHttpServiceRequire(xrtTimer()<End,"WS host Engine tail");xrtSleep(1);}rpcWsHttpServiceRequire(R==XNET_RETIRE_READY,"WS Engine join");rpcWsHttpServiceRequire(xrtAtomic32Load(&S->NativeRefs,XMEMORY_ACQUIRE)==1,"WSS native context credits");
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    xrtTlsVerifierRelease(S->Verifier);xrtTlsIdentityRelease(S->Identity);xrtTlsContextRelease(S->Context);
#endif
}
#endif
