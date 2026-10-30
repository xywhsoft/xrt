#include <xrpc/http_server.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <stdlib.h>
static void require(bool OK)
{if(!OK){fprintf(stderr,"HTTP server example: %s\n",xrtErrorMessage(xrtGetError()));exit(1);}}
static bool echo(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr Data)
{
    (void)Data;
    if(xrtCancelRequested(Context->Cancel) || xrtTimer()>=Context->Deadline)return false;
    return xrpcReplyResult(Reply,R->Params.Raw.Size?R->Params.Raw:XRT_STR_LITERAL("null"));
}
int main(int argc,char** argv)
{
    /* No-argument smoke binds an ephemeral loopback port for 100 ms.
     * For an external client: example_http_server 8080 60000 (milliseconds). */
    char *End=NULL;unsigned long Port=argc>1?strtoul(argv[1],&End,10):0;
    if(argc>3 || (argc>1 && (!*argv[1] || *End)) || Port>65535)return 2;
    unsigned long Duration=argc>2?strtoul(argv[2],&End,10):100;
    if((argc>2 && (!*argv[2] || *End)) || !Duration || Duration>3600000)return 2;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;
    xnetengine* Engine=xrtNetEngineCreate(&E);require(Engine && xrtNetEngineStart(Engine));
    xrpchttpserverconfig K;xrpcHttpServerConfigInit(&K);K.Execution.Call=echo;
    require(xrtNetAddrParse(&K.Listen.Address,"127.0.0.1",(uint16)Port));xrpchttpserver* Server=NULL;
    require(xrpcHttpServerCreate(Engine,&K,&Server) && xrpcHttpServerStart(Server));
    xnetaddr Address;char Endpoint[80];require(xrpcHttpServerLocal(Server,&Address));
    require(xrtNetAddrEndpointText(&Address,Endpoint,sizeof(Endpoint))<sizeof(Endpoint));
    printf("http://%s/rpc\n",Endpoint);fflush(stdout);xrtSleep((int64)Duration);
    require(xrpcHttpServerClose(Server) && xrpcHttpServerWaitFor(Server,10000)==XWAIT_OK && xrpcHttpServerDestroy(Server));
    require(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine));return 0;
}
