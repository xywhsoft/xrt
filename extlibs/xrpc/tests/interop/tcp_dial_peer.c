#include <xrpc/tcp_dial.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
static bool tcpDialInteropEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr Data)
{(void)C;(void)Data;return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));}
int main(int Argc,char** Argv)
{
    if(Argc!=4)return 1;
    unsigned Format=(unsigned)strtoul(Argv[1],NULL,10),Mode=(unsigned)strtoul(Argv[2],NULL,10),Port=(unsigned)strtoul(Argv[3],NULL,10);if(Format<1 || Format>2 || Mode>2 || !Port || Port>65535)return 2;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);if(!Engine || !xrtNetEngineStart(Engine))return 3;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tcpDialInteropEcho;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 4;
    xrpctcpconfig C;xrpcTcpConfigInit(&C);C.Duplex.Server=Server;C.Frame.Format=(xrpcframeformat)Format;C.ReadChunkBytes=7;C.ReadBytes=37;C.WriteChunkBytes=11;C.WriteBytes=43;C.WorkFrames=3;
    xnetaddr Address;xrtNetAddrParse(&Address,"127.0.0.1",(uint16)Port);xnetresolver* Resolver=NULL;xnetaddrlist* List=NULL;
    if(Mode==2){xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.CacheEntries=0;Resolver=xrtNetResolverCreate(&K);if(!Resolver)return 5;}
    if(Mode==1){List=xrtNetAddrListCreate(&Address,1);if(!List)return 6;}
    xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;xrpctcp* Peer=NULL;
    bool OK=Mode==0?xrpcTcpConnect(Engine,&Address,1,&C,&Peer):Mode==1?xrpcTcpDialAddresses(Engine,List,1,&C,&D,&Peer):xrpcTcpDial(Engine,Resolver,XRT_STR_LITERAL("localhost"),(uint16)Port,1,&C,&D,&Peer);
    xrtNetAddrListDestroy(List);if(!OK)return 7;puts("READY");fflush(stdout);
    xfuture* F=NULL;if(xrpcTcpCall(Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&F)!=XRPC_DUPLEX_ACCEPTED || xrtFutureWaitFor(F,15000)!=XWAIT_OK)return 8;
    const xrpccallresult* R=xrtFutureValue(F);if(!R || R->Status!=XRPC_CALL_OK || R->Delivery!=XRPC_REPLIED)return 9;
    if(xrpcTcpWaitFor(Peer,15000)!=XWAIT_OK || !xrpcTcpDestroy(Peer))return 10;
    xrtFutureDestroy(F);if(Resolver && !xrtNetResolverDestroy(Resolver))return 11;if(!xrpcServerDestroy(Server))return 12;
    double End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(Engine))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 13;xrtSleep(1);}if(Q!=XNET_RETIRE_READY)return 14;
    puts("PASS native framed candidate TCP peer");return 0;
}
