#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <xrt/atomic.h>
#include <stdio.h>
#include <string.h>
/* One serialized message owner. The peer runs business on XRT's executor. */
typedef struct duplexExampleCancellation {
    xatomic32 Entered,Cancelled;
} duplexExampleCancellation;

static bool duplexExampleEcho(const xrpcmessage* Request,xrpcreply* Reply,
    const xrpcservercontext* Context,ptr User)
{
    duplexExampleCancellation* C=User;
    if(Request->Method.Raw.Size==6 && !memcmp(Request->Method.Raw.Data,"\"hold\"",6)){
        xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);
        while(!xrtCancelRequested(Context->Cancel) && xrtTimer()<Context->Deadline)xrtSleep(1);
        if(xrtCancelRequested(Context->Cancel))xrtAtomic32Store(&C->Cancelled,1,XMEMORY_RELEASE);
        return false;
    }
    return xrpcReplyResult(Reply,Request->Params.Raw.Size?Request->Params.Raw:XRT_STR_LITERAL("null"));
}

static bool duplexExampleTransfer(xrpcduplex* From,xrpcduplex* To)
{
    xrpcduplexoutput* Output=NULL;uint64 Token=0;int N=xrpcDuplexNext(From,&Output,&Token);if(N<0)return false;if(!N)return true;
    bool OK=xrpcDuplexStarted(From,Token) && xrpcDuplexSent(From,Token) &&
        xrpcDuplexReceive(To,xrpcDuplexOutputWire(Output),(xbytesview){0})==XRPC_DUPLEX_ACCEPTED;
    xrpcDuplexOutputRelease(Output);return OK;
}

static bool duplexExamplePump(xrpcduplex* A,xrpcduplex* B)
{
    return xrpcDuplexStep(A) && xrpcDuplexStep(B) &&
        duplexExampleTransfer(A,B) && duplexExampleTransfer(B,A);
}

static bool duplexExampleCancel(xrpcduplex* A,xrpcduplex* B,duplexExampleCancellation* State)
{
    xfuture* Offer=NULL;
    if(!xrpcDuplexEnableCancellation(A) || !xrpcDuplexEnableCancellation(B) ||
       xrpcDuplexNegotiateCancellation(A,5000,&Offer)!=XRPC_DUPLEX_ACCEPTED)return false;
    double Until=xrtTimer()+5;
    while(!xrtFutureDone(Offer)){
        if(xrtTimer()>=Until || !duplexExamplePump(A,B)){xrtFutureDestroy(Offer);return false;}
        xrtSleep(1);
    }
    const xrpccallresult* Agreed=xrtFutureValue(Offer);
    xrpcresponseinfo R={.Size=sizeof(R)};
    bool OK=Agreed && Agreed->Status==XRPC_CALL_OK && xrpcResponsesInfo(Agreed->Responses,&R) &&
        R.Count==1 && R.Messages[0].Result.Raw.Size==4 &&
        !memcmp(R.Messages[0].Result.Raw.Data,"true",4);
    xrtFutureDestroy(Offer);
    if(!OK)return false;

    xfuture* Call=NULL;
    if(xrpcDuplexCall(A,XRT_STR_LITERAL("hold"),(xstrview){0},5000,&Call)!=XRPC_DUPLEX_ACCEPTED)
        return false;
    Until=xrtTimer()+5;
    while(!xrtAtomic32Load(&State->Entered,XMEMORY_ACQUIRE)){
        if(xrtTimer()>=Until || !duplexExamplePump(A,B)){xrtFutureDestroy(Call);return false;}
        xrtSleep(1);
    }
    if(!xrtFutureCancel(Call)){xrtFutureDestroy(Call);return false;}
    while(!xrtFutureDone(Call) || !xrtAtomic32Load(&State->Cancelled,XMEMORY_ACQUIRE)){
        if(xrtTimer()>=Until || !duplexExamplePump(A,B)){xrtFutureDestroy(Call);return false;}
        xrtSleep(1);
    }
    const xrpccallresult* Stopped=xrtFutureValue(Call);
    OK=Stopped && Stopped->Status==XRPC_CALL_CANCELLED && Stopped->Delivery==XRPC_MAY_HAVE_EXECUTED;
    xrtFutureDestroy(Call);
    return OK;
}

int main(void)
{
    duplexExampleCancellation Cancel={0};
    xrpcserverconfig SConfig;xrpcServerConfigInit(&SConfig);SConfig.Call=duplexExampleEcho;SConfig.User=&Cancel;
    xrpcserver* Server=NULL;
    if(!xrpcServerCreate(&SConfig,&Server))return 1;
    xrpcduplexconfig Config;xrpcDuplexConfigInit(&Config);Config.Server=Server;xrpcduplex *A=NULL,*B=NULL;
    if(!xrpcDuplexCreate(&Config,&A) || !xrpcDuplexCreate(&Config,&B))return 2;
    if(!duplexExampleCancel(A,B,&Cancel))return 10;
    puts("the peer observed negotiated cancellation");
    xfuture *Left=NULL,*Right=NULL;
    if(xrpcDuplexCall(A,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2]"),5000,&Left)!=XRPC_DUPLEX_ACCEPTED ||
       xrpcDuplexCall(B,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("{\"reverse\":true}"),5000,&Right)!=XRPC_DUPLEX_ACCEPTED)return 3;
    while(!xrtFutureDone(Left) || !xrtFutureDone(Right)){
        if(!duplexExamplePump(A,B))return 4;
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
