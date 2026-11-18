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
    bool Hold=Request->Method.Raw.Size==6 && !memcmp(Request->Method.Raw.Data,"\"hold\"",6);
    bool Expire=Request->Method.Raw.Size==8 && !memcmp(Request->Method.Raw.Data,"\"expire\"",8);
    if(Hold || Expire){
        if(Hold)xrtAtomic32Store(&C->Entered,1,XMEMORY_RELEASE);
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

static bool duplexExampleAwait(xrpcduplex* A,xrpcduplex* B,xfuture* Future)
{
    double Until=xrtTimer()+5.;
    while(!xrtFutureDone(Future)){
        if(xrtTimer()>=Until || !duplexExamplePump(A,B))return false;
        xrtSleep(1);
    }
    return true;
}

static bool duplexExampleAgreed(xrpcduplex* A,xrpcduplex* B,xfuture* Offer)
{
    bool Ready=duplexExampleAwait(A,B,Offer);
    const xrpccallresult* Agreed=Ready?xrtFutureValue(Offer):NULL;
    xrpcresponseinfo R={.Size=sizeof(R)};
    bool OK=Agreed && Agreed->Status==XRPC_CALL_OK && xrpcResponsesInfo(Agreed->Responses,&R) &&
        R.Count==1 && R.Messages[0].Result.Raw.Size==4 &&
        !memcmp(R.Messages[0].Result.Raw.Data,"true",4);
    xrtFutureDestroy(Offer);
    return OK;
}

static bool duplexExampleDeadline(xrpcduplex* A,xrpcduplex* B)
{
    xfuture* Future=NULL;
    if(!xrpcDuplexEnableDeadline(A) || !xrpcDuplexEnableDeadline(B) ||
       xrpcDuplexNegotiateDeadline(A,5000,&Future)!=XRPC_DUPLEX_ACCEPTED ||
       !duplexExampleAgreed(A,B,Future))return false;
    if(xrpcDuplexCall(A,XRT_STR_LITERAL("expire"),(xstrview){0},5000,&Future)!=XRPC_DUPLEX_ACCEPTED)
        return false;
    bool Ready=duplexExampleAwait(A,B,Future);
    const xrpccallresult* Result=Ready?xrtFutureValue(Future):NULL;
    xrpcresponseinfo R={.Size=sizeof(R)};
    bool OK=Result && Result->Status==XRPC_CALL_REMOTE_ERROR && Result->Delivery==XRPC_REPLIED &&
        xrpcResponsesInfo(Result->Responses,&R) && R.Count==1 &&
        R.Messages[0].ErrorCode.Raw.Size==6 && !memcmp(R.Messages[0].ErrorCode.Raw.Data,"-32001",6);
    xrtFutureDestroy(Future);
    return OK;
}

static bool duplexExampleCancel(xrpcduplex* A,xrpcduplex* B,duplexExampleCancellation* State)
{
    xfuture* Offer=NULL;
    if(!xrpcDuplexEnableCancellation(A) || !xrpcDuplexEnableCancellation(B) ||
       xrpcDuplexNegotiateCancellation(A,5000,&Offer)!=XRPC_DUPLEX_ACCEPTED)return false;
    if(!duplexExampleAgreed(A,B,Offer))return false;

    xfuture* Call=NULL;
    if(xrpcDuplexCall(A,XRT_STR_LITERAL("hold"),(xstrview){0},5000,&Call)!=XRPC_DUPLEX_ACCEPTED)
        return false;
    double Until=xrtTimer()+5.;
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
    bool OK=Stopped && Stopped->Status==XRPC_CALL_CANCELLED && Stopped->Delivery==XRPC_MAY_HAVE_EXECUTED;
    xrtFutureDestroy(Call);
    return OK;
}

int main(void)
{
    duplexExampleCancellation Cancel={0};
    xrpcserverconfig SConfig;xrpcServerConfigInit(&SConfig);SConfig.Call=duplexExampleEcho;SConfig.User=&Cancel;
    xrpcserver* Server=NULL;
    if(!xrpcServerCreate(&SConfig,&Server))return 1;
    xrpcduplexconfig Config;xrpcDuplexConfigInit(&Config);Config.Server=Server;Config.IncomingTimeout=500;xrpcduplex *A=NULL,*B=NULL;
    if(!xrpcDuplexCreate(&Config,&A) || !xrpcDuplexCreate(&Config,&B))return 2;
    if(!duplexExampleCancel(A,B,&Cancel))return 10;
    puts("the peer observed negotiated cancellation");
    if(!duplexExampleDeadline(A,B))return 11;
    puts("the peer returned the negotiated deadline error");
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
