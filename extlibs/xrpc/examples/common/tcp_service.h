#ifndef XRPC_EXAMPLE_TCP_SERVICE_H
#define XRPC_EXAMPLE_TCP_SERVICE_H
#include <xrpc/tcp.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Repository example/benchmark fixture, not library runtime. Early failure
 * terminates the process; embedding applications unwind each acquired owner. */
typedef struct rpcTcpService {
    xnetengine* Engine;xnetlistener* Listener;xrpcserver* Server;xrpctcp* Accepted;
    xnetaddr Address;xrpctcpconfig Config;xatomic32 Ready,Calls;
} rpcTcpService;
static void rpcTcpServiceRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"%s: %s\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static bool rpcTcpServiceEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{
    (void)C;rpcTcpService* S=Data;rpcTcpServiceRequire(!xrtNetEngineCurrent(S->Engine),"business must run off network worker");
    xrtAtomic32FetchAdd(&S->Calls,1,XMEMORY_RELAXED);return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));
}
static bool rpcTcpServiceAccept(xnetlistener* L,xnetstream* Stream,ptr Data)
{(void)L;rpcTcpService* S=Data;if(!xrpcTcpAttach(Stream,&S->Config,&S->Accepted))return false;xrtAtomic32Store(&S->Ready,1,XMEMORY_RELEASE);return true;}
static void rpcTcpServiceStart(rpcTcpService* S)
{
    memset(S,0,sizeof(*S));xrtAtomic32Init(&S->Ready,0);xrtAtomic32Init(&S->Calls,0);xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;
    S->Engine=xrtNetEngineCreate(&E);rpcTcpServiceRequire(S->Engine && xrtNetEngineStart(S->Engine),"Engine start");
    xrpcserverconfig C;xrpcServerConfigInit(&C);C.Call=rpcTcpServiceEcho;C.User=S;rpcTcpServiceRequire(xrpcServerCreate(&C,&S->Server),"shared business Server");
    xrpcTcpConfigInit(&S->Config);S->Config.Duplex.Server=S->Server;xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);
    xnetlistenerevents Events={rpcTcpServiceAccept,NULL,NULL};S->Listener=xrtNetListen(S->Engine,&L,&Events,NULL,S);
    rpcTcpServiceRequire(S->Listener && xrtNetListenerLocal(S->Listener,&S->Address),"loopback listener");
}
static const xrpccallresult* rpcTcpServiceResult(xfuture* F)
{
    rpcTcpServiceRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK,"call wait");const xrpccallresult* R=xrtFutureValue(F);rpcTcpServiceRequire(R!=NULL,"typed call result");return R;
}
static void rpcTcpServicePeerClose(xrpctcp* Peer)
{if(Peer)rpcTcpServiceRequire(xrpcTcpClose(Peer) && xrpcTcpWaitFor(Peer,10000)==XWAIT_OK && xrpcTcpDestroy(Peer),"physical peer close");}
static void rpcTcpServiceCycleClose(rpcTcpService* S,xrpctcp* Peer)
{rpcTcpServicePeerClose(Peer);if(xrtAtomic32Load(&S->Ready,XMEMORY_ACQUIRE))rpcTcpServicePeerClose(S->Accepted);S->Accepted=NULL;xrtAtomic32Store(&S->Ready,0,XMEMORY_RELEASE);}
static void rpcTcpServiceStop(rpcTcpService* S)
{
    rpcTcpServiceRequire(!S->Accepted && xrtNetListenerClose(S->Listener),"listener close after peer owners");xrtNetListenerDestroy(S->Listener);
    rpcTcpServiceRequire(xrpcServerDestroy(S->Server),"business Server join");double End=xrtTimer()+10;xnetretireresult R;
    while((R=xrtNetEngineTryDestroy(S->Engine))==XNET_RETIRE_BUSY){rpcTcpServiceRequire(xrtTimer()<End,"Engine native resource/pin tail");xrtSleep(1);}rpcTcpServiceRequire(R==XNET_RETIRE_READY,"Engine physical join");
}
#endif
