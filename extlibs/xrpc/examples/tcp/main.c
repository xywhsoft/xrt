#include <xrpc/tcp.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <string.h>
/* Loopback demo with one host-owned Engine and one shared business executor. */
static xrpctcp* Accepted;static xatomic32 Ready;static xrpctcpconfig Config;
static bool echo(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));}
static bool acceptPeer(xnetlistener* L,xnetstream* S,ptr U)
{(void)L;(void)U;if(!xrpcTcpAttach(S,&Config,&Accepted))return false;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);return true;}
int main(void)
{
    xrtAtomic32Init(&Ready,0);xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* E=xrtNetEngineCreate(&K);if(!E || !xrtNetEngineStart(E))return 1;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=echo;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 2;
    xrpcTcpConfigInit(&Config);Config.Duplex.Server=Server;xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);
    xnetlistenerevents Events={acceptPeer,NULL,NULL};xnetlistener* Listener=xrtNetListen(E,&L,&Events,NULL,NULL);xnetaddr Remote;if(!Listener || !xrtNetListenerLocal(Listener,&Remote))return 3;
    xrpctcp* Client=NULL;if(!xrpcTcpConnect(E,&Remote,1,&Config,&Client))return 4;double End=xrtTimer()+10;while(!xrtAtomic32Load(&Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End)return 10;xrtSleep(1);}
    xfuture *A=NULL,*B=NULL;if(xrpcTcpCall(Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2]"),5000,&A)!=XRPC_DUPLEX_ACCEPTED || xrpcTcpCall(Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[3,4]"),5000,&B)!=XRPC_DUPLEX_ACCEPTED)return 5;
    if(xrtFutureWaitFor(A,10000)!=XWAIT_OK || xrtFutureWaitFor(B,10000)!=XWAIT_OK)return 6;
    const xrpccallresult *RA=xrtFutureValue(A),*RB=xrtFutureValue(B);if(!RA || !RB || RA->Status!=XRPC_CALL_OK || RB->Status!=XRPC_CALL_OK)return 7;
    puts("both TCP endpoints called each other using Content-Length framing");
    if(!xrpcTcpClose(Client) || !xrpcTcpClose(Accepted) || xrpcTcpWaitFor(Client,10000)!=XWAIT_OK || xrpcTcpWaitFor(Accepted,10000)!=XWAIT_OK || !xrpcTcpDestroy(Client) || !xrpcTcpDestroy(Accepted))return 8;
    xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);if(!xrpcServerDestroy(Server))return 9;End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(E))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 11;xrtSleep(1);}if(Q!=XNET_RETIRE_READY)return 12;xrtFutureDestroy(A);xrtFutureDestroy(B);return 0;
}
