#include "../examples/duplex/app_dispatch.h"
#include <stdio.h>
#include <stdlib.h>

static bool transfer(xrpcduplex* From,xrpcduplex* To)
{
    uint64 Token;
    xrpcduplexoutput* Output=NULL;
    if(xrpcDuplexNext(From,&Output,&Token)!=1) return false;
    bool OK=xrpcDuplexStarted(From,Token) && xrpcDuplexSent(From,Token) &&
        xrpcDuplexReceive(To,xrpcDuplexOutputWire(Output),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(Output);
    return OK;
}

int main(int Argc,char** Argv)
{
    unsigned long Count=Argc>1?strtoul(Argv[1],NULL,10):100000;
    if(!Count || Count>10000000) return 1;
    xrpcduplexconfig Config;
    xrpcDuplexConfigInit(&Config);
    apprequest Job={0};
    Config.OnRequest=appTakeRequest;
    Config.User=&Job;
    xrpcduplex *Client=NULL,*Server=NULL;
    if(!xrpcDuplexCreate(NULL,&Client) || !xrpcDuplexCreate(&Config,&Server)) return 2;
    double Start=xrtTimer();
    for(unsigned long I=0;I<Count;++I){
        xfuture* Call=NULL;
        if(xrpcDuplexCall(Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[42]"),5000,&Call)!=XRPC_DUPLEX_ACCEPTED ||
           !xrpcDuplexStep(Client) || !transfer(Client,Server) || !appFinish(Server,&Job) ||
           !transfer(Server,Client)) return 3;
        const xrpccallresult* Result=xrtFutureValue(Call);
        if(!Result || Result->Status!=XRPC_CALL_OK || Result->Delivery!=XRPC_REPLIED) return 4;
        xrtFutureDestroy(Call);
    }
    double Seconds=xrtTimer()-Start;
    if(Seconds<=0 || !xrpcDuplexClose(Client,XRPC_CALL_CLOSED) ||
       !xrpcDuplexClose(Server,XRPC_CALL_CLOSED) ||
       !xrpcDuplexStep(Client) || !xrpcDuplexStep(Server) ||
       !xrpcDuplexDestroy(Client) || !xrpcDuplexDestroy(Server)) return 5;
    printf("calls_per_second: %.3f\n", Count/Seconds);
    printf("calls: %lu\n",Count);
    return 0;
}
