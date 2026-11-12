#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <stdio.h>
/* One message owner; an actual transport adapter replaces transfer(). */
static bool echo(const xrpcmessage* Request,xrpcreply* Reply,const xrpcservercontext* Context,ptr User)
{(void)Context;(void)User;return xrpcReplyResult(Reply,Request->Params.Raw.Size?Request->Params.Raw:XRT_STR_LITERAL("null"));}
static bool transfer(xrpcduplex* From,xrpcduplex* To)
{
    xrpcduplexoutput* Output=NULL;uint64 Token=0;int N=xrpcDuplexNext(From,&Output,&Token);if(N<0)return false;if(!N)return true;
    bool OK=xrpcDuplexStarted(From,Token) && xrpcDuplexSent(From,Token) &&
        xrpcDuplexReceive(To,xrpcDuplexOutputWire(Output),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(Output);return OK;
}
int main(void)
{
    xrpcserverconfig SConfig;xrpcServerConfigInit(&SConfig);SConfig.Call=echo;xrpcserver* Server=NULL;
    if(!xrpcServerCreate(&SConfig,&Server))return 1;
    xrpcduplexconfig Config;xrpcDuplexConfigInit(&Config);Config.Server=Server;xrpcduplex *A=NULL,*B=NULL;
    if(!xrpcDuplexCreate(&Config,&A) || !xrpcDuplexCreate(&Config,&B))return 2;
    xfuture *Left=NULL,*Right=NULL;
    if(xrpcDuplexCall(A,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2]"),5000,&Left)!=XRPC_DUPLEX_ACCEPTED ||
       xrpcDuplexCall(B,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("{\"reverse\":true}"),5000,&Right)!=XRPC_DUPLEX_ACCEPTED)return 3;
    while(!xrtFutureDone(Left) || !xrtFutureDone(Right)){
        if(!xrpcDuplexStep(A) || !xrpcDuplexStep(B) || !transfer(A,B) || !transfer(B,A))return 4;
        xrtSleep(0);
    }
    const xrpccallresult* L=xrtFutureValue(Left);const xrpccallresult* R=xrtFutureValue(Right);
    if(!L || !R || L->Status!=XRPC_CALL_OK || R->Status!=XRPC_CALL_OK)return 5;
    puts("both peers called each other; handlers ran on the shared executor");
    if(!xrpcDuplexClose(A,XRPC_CALL_CLOSED) || !xrpcDuplexClose(B,XRPC_CALL_CLOSED))return 6;
    for(;;){xrpcduplexinfo LI={0},RI={0};LI.Size=sizeof(LI);RI.Size=sizeof(RI);
        if(!xrpcDuplexStep(A) || !xrpcDuplexStep(B) || !xrpcDuplexInfo(A,&LI) || !xrpcDuplexInfo(B,&RI))return 7;
        if(LI.Drained && RI.Drained)break;
        xrtSleep(0);
    }
    if(!xrpcDuplexDestroy(A) || !xrpcDuplexDestroy(B) || !xrpcServerDestroy(Server))return 8;
    /* Independent result Futures still own their payloads after endpoint join. */
    if(((const xrpccallresult*)xrtFutureValue(Left))->Status!=XRPC_CALL_OK)return 9;
    xrtFutureDestroy(Left);xrtFutureDestroy(Right);return 0;
}
