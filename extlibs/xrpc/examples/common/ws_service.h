#ifndef XRPC_EXAMPLE_WS_SERVICE_H
#define XRPC_EXAMPLE_WS_SERVICE_H
#include <xrpc/ws.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
#include "../../../../tests/fixtures/tls_server.h"
#endif
typedef struct rpcWsService rpcWsService;
typedef struct rpcWsAttach {xnetpost Post;rpcWsService* Service;ptr Native;bool Server;} rpcWsAttach;
struct rpcWsService {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpcws* Client;xrpcws* Accepted;xnetaddr Address;
    xrpcwsconfig ClientConfig,ServerConfig;xatomic32 ClientReady,ServerReady,Calls,NativeRefs,HandlerEntered,HandlerPermit;
    bool TLS,HoldHandler;rpcWsAttach Attach[2];
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;xtlsserverconfig NativeServer;
#endif
};
static void rpcWsServiceRequire(bool OK,cstr M){if(!OK){fprintf(stderr,"%s: %s\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void rpcWsServiceAwait(xatomic32* A){double End=xrtTimer()+15;while(!xrtAtomic32Load(A,XMEMORY_ACQUIRE)){rpcWsServiceRequire(xrtTimer()<End,"WS fixture barrier");xrtSleep(1);}}
static bool rpcWsServiceEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{
    (void)C;rpcWsService* S=Data;rpcWsServiceRequire(!xrtNetEngineCurrent(S->Engine),"WS business outside network Worker");xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);
    if(S->HoldHandler){xrtAtomic32Store(&S->HandlerEntered,1,XMEMORY_RELEASE);rpcWsServiceAwait(&S->HandlerPermit);}
    return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static void rpcWsServiceAttach(rpcWsService* S,ptr Native,bool Server)
{
    xrpcws** P=Server?&S->Accepted:&S->Client;bool OK;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(S->TLS)OK=xrpcWsAttachTls(Native,NULL,Server?&S->ServerConfig:&S->ClientConfig,P);else
#endif
    OK=xrpcWsAttachTcp(Native,NULL,Server?&S->ServerConfig:&S->ClientConfig,P);
    rpcWsServiceRequire(OK,"WS product attach");xrtAtomic32Store(Server?&S->ServerReady:&S->ClientReady,1,XMEMORY_RELEASE);
}
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
static void rpcWsServiceAttachPost(xnetworker* W,ptr Data){(void)W;rpcWsAttach* P=Data;rpcWsServiceAttach(P->Service,P->Native,P->Server);}
static bool rpcWsServiceNativeCount(const void* D,size_t* N){*N=xrtAtomic32Load(&((const rpcWsService*)D)->NativeRefs,XMEMORY_ACQUIRE);return true;}
static bool rpcWsServiceNativeTrace(const void* D,xrtownershipvisitor V,ptr U){(void)D;(void)V;(void)U;return false;}
static void rpcWsServiceNativeDrop(const void* D){xrtAtomic32FetchSub(&((rpcWsService*)D)->NativeRefs,1,XMEMORY_RELEASE);}
static const xrtownershipops RpcWsServiceNativeOps={rpcWsServiceNativeCount,rpcWsServiceNativeTrace};
static void rpcWsServiceTLSOpen(xtlsstream* Native,ptr Data)
{
    rpcWsService* S=Data;bool Server=xrtTlsSessionRole(xrtTlsStreamSession(Native))==XTLS_SERVER;rpcWsAttach* P=S->Attach+(Server?1:0);P->Service=S;P->Native=Native;P->Server=Server;
    rpcWsServiceRequire(xrtNetPostInit(&P->Post)&&xrtNetPost(xrtNetStreamWorker(xrtTlsStreamTransport(Native)),&P->Post,rpcWsServiceAttachPost,P),"exclusive WSS handoff after Open");
}
static const xtlsstreamownershipv1 RpcWsServiceTLSPolicy={sizeof(xtlsstreamownershipv1),{.Open=rpcWsServiceTLSOpen},rpcWsServiceNativeDrop,&RpcWsServiceNativeOps};
static xtlsverifydecision rpcWsServiceVerify(const xtlspeer* P,ptr Data)
{rpcWsService* S=Data;xbytesview Pin={0};return P&&P->CertificateCount&&P->Name.Size==11&&!memcmp(P->Name.Data,"rpc.fixture",11)&&xrtTlsIdentityCertificate(S->Identity,0,&Pin)&&P->Certificates[0].Raw.Size==Pin.Size&&!memcmp(P->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
#endif
static bool rpcWsServiceAccept(xnetlistener* L,xnetstream* Native,ptr Data)
{
    (void)L;rpcWsService* S=Data;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(S->TLS){xtlsstream* T=NULL;xrtAtomic32FetchAdd(&S->NativeRefs,1,XMEMORY_RELAXED);rpcWsServiceRequire(xrtTlsStreamAcceptOwnedV1(Native,&S->NativeServer,NULL,S,&RpcWsServiceTLSPolicy,&T),"WSS native accept");return true;}
#endif
    rpcWsServiceAttach(S,Native,true);return true;
}
static void rpcWsServiceOpen(xnetstream* Native,ptr Data){rpcWsServiceAttach(Data,Native,false);}
static void rpcWsServiceStartEx(rpcWsService* S,bool TLS,const xrpcwsconfig* Config,bool Hold,size_t TimerLimit)
{
    memset(S,0,sizeof(*S));S->TLS=TLS;S->HoldHandler=Hold;xrtAtomic32Init(&S->ClientReady,0);xrtAtomic32Init(&S->ServerReady,0);xrtAtomic32Init(&S->Calls,0);xrtAtomic32Init(&S->NativeRefs,1);xrtAtomic32Init(&S->HandlerEntered,0);xrtAtomic32Init(&S->HandlerPermit,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;if(TimerLimit)E.TimerLimit=TimerLimit;S->Engine=xrtNetEngineCreate(&E);rpcWsServiceRequire(S->Engine&&xrtNetEngineStart(S->Engine),"WS host Engine");xrpcserverconfig C;xrpcServerConfigInit(&C);C.Call=rpcWsServiceEcho;C.User=S;rpcWsServiceRequire(xrpcServerCreate(&C,&S->Server),"WS shared Server");
    xrpcWsConfigInit(&S->ServerConfig);if(Config)S->ServerConfig=*Config;S->ServerConfig.Duplex.Server=S->Server;S->ServerConfig.Stream.Role=XWS_ROLE_SERVER;S->ServerConfig.Stream.Protocol=XRT_STR_LITERAL("jsonrpc");S->ClientConfig=S->ServerConfig;S->ClientConfig.Stream.Role=XWS_ROLE_CLIENT;
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){S->Context=testTlsServerContext();S->Identity=testTlsServerIdentity();rpcWsServiceRequire(S->Context&&S->Identity,"WSS public fixture");xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=rpcWsServiceVerify;V.Context=S;S->Verifier=xrtTlsVerifierCreate(&V);rpcWsServiceRequire(S->Verifier!=NULL,"explicit WSS DER pin");xrtTlsServerConfigInit(&S->NativeServer);S->NativeServer.Context=S->Context;S->NativeServer.Identity=S->Identity;}
#else
    rpcWsServiceRequire(!TLS,"TLS module not selected");
#endif
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents LE={rpcWsServiceAccept,NULL,NULL};S->Listener=xrtNetListen(S->Engine,&L,&LE,NULL,S);rpcWsServiceRequire(S->Listener&&xrtNetListenerLocal(S->Listener,&S->Address),"WS local listener");
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    if(TLS){xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=S->Context;K.Verifier=S->Verifier;K.ServerName=XRT_STR_LITERAL("rpc.fixture");xrtAtomic32FetchAdd(&S->NativeRefs,1,XMEMORY_RELAXED);rpcWsServiceRequire(xrtTlsStreamConnectOwnedV1(S->Engine,&S->Address,1,NULL,&K,NULL,S,&RpcWsServiceTLSPolicy)!=NULL,"WSS native connect");}
    else
#endif
    {xnetstreamevents Events={.Open=rpcWsServiceOpen};rpcWsServiceRequire(xrtNetStreamConnect(S->Engine,&S->Address,1,NULL,&Events,S)!=NULL,"WS native connect");}
    rpcWsServiceAwait(&S->ClientReady);rpcWsServiceAwait(&S->ServerReady);
}
static void rpcWsServiceStart(rpcWsService* S,bool TLS,const xrpcwsconfig* Config,bool Hold)
{rpcWsServiceStartEx(S,TLS,Config,Hold,0);}
static const xrpccallresult* rpcWsServiceResult(xfuture* F)
{rpcWsServiceRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK,"WS call wait");const xrpccallresult* R=xrtFutureValue(F);rpcWsServiceRequire(R!=NULL,"WS typed call result");return R;}
static void rpcWsServicePeerClose(xrpcws* P){if(P)rpcWsServiceRequire(xrpcWsClose(P)&&xrpcWsWaitFor(P,10000)==XWAIT_OK&&xrpcWsDestroy(P),"WS physical peer retirement");}
static void rpcWsServiceStop(rpcWsService* S)
{
    rpcWsServicePeerClose(S->Client);rpcWsServicePeerClose(S->Accepted);rpcWsServiceRequire(xrtNetListenerClose(S->Listener),"WS listener close");xrtNetListenerDestroy(S->Listener);rpcWsServiceRequire(xrpcServerDestroy(S->Server),"WS shared Server join");double End=xrtTimer()+10;xnetretireresult R;while((R=xrtNetEngineTryDestroy(S->Engine))==XNET_RETIRE_BUSY){rpcWsServiceRequire(xrtTimer()<End,"WS host Engine tail");xrtSleep(1);}rpcWsServiceRequire(R==XNET_RETIRE_READY,"WS Engine join");rpcWsServiceRequire(xrtAtomic32Load(&S->NativeRefs,XMEMORY_ACQUIRE)==1,"WSS native context credits");
#if defined(XRPC_FEATURE_XRPC_WS_TLS)
    xrtTlsVerifierRelease(S->Verifier);xrtTlsIdentityRelease(S->Identity);xrtTlsContextRelease(S->Context);
#endif
}
#endif
