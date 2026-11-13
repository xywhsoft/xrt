#include <xrpc/tcp.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
static xatomic32 Ready;static xrpctcp* Peer;static xrpctcpconfig Config;
static bool tcpInteropEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));}
static bool tcpInteropAccept(xnetlistener* L,xnetstream* S,ptr U)
{(void)L;(void)U;if(!xrpcTcpAttach(S,&Config,&Peer))return false;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);return true;}
int main(int Argc,char** Argv)
{
    if(Argc!=2)return 1;
    unsigned Format=(unsigned)strtoul(Argv[1],NULL,10);if(Format<1 || Format>2)return 2;
    xrtAtomic32Init(&Ready,0);xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=2;xnetengine* E=xrtNetEngineCreate(&C);if(!E || !xrtNetEngineStart(E))return 3;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tcpInteropEcho;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 4;
    xrpcTcpConfigInit(&Config);Config.Duplex.Server=Server;Config.Frame.Format=(xrpcframeformat)Format;Config.ReadChunkBytes=7;Config.ReadBytes=37;Config.WriteChunkBytes=11;Config.WriteBytes=43;Config.WorkFrames=3;
    xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={tcpInteropAccept,NULL,NULL};xnetlistener* Listener=xrtNetListen(E,&L,&Events,NULL,NULL);xnetaddr Address;
    if(!Listener || !xrtNetListenerLocal(Listener,&Address))return 5;
    printf("PORT %u\n",(unsigned)Address.Port);fflush(stdout);double End=xrtTimer()+15;
    while(!xrtAtomic32Load(&Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End)return 6;xrtSleep(1);}
    xfuture* F=NULL;if(xrpcTcpCall(Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&F)!=XRPC_DUPLEX_ACCEPTED || xrtFutureWaitFor(F,15000)!=XWAIT_OK)return 7;
    const xrpccallresult* R=xrtFutureValue(F);if(!R || R->Status!=XRPC_CALL_OK || R->Delivery!=XRPC_REPLIED)return 8;
    if(xrpcTcpWaitFor(Peer,15000)!=XWAIT_OK || !xrpcTcpDestroy(Peer))return 9;
    xrtFutureDestroy(F);xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);if(!xrpcServerDestroy(Server))return 10;
    End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(E))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 11;xrtSleep(1);}if(Q!=XNET_RETIRE_READY)return 12;
    puts("PASS native framed TCP peer");return 0;
}
