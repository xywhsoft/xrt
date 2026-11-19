#ifndef XRPC_TEST_NETWORK_EXTENSIONS_H
#define XRPC_TEST_NETWORK_EXTENSIONS_H
#include "test.h"
#include <xrpc/duplex.h>
#include <xrt/thread.h>
#include <string.h>

#define RPC_TEST_EXTENSIONS (XRPC_DUPLEX_EXTENSION_CANCELLATION|XRPC_DUPLEX_EXTENSION_DEADLINE)
#define RPC_TEST_REMOTE_LIMIT 600

static inline void rpcTestNetworkWait(const xatomic32* Flag)
{
    double End=xrtTimer()+8.;
    while(!xrtAtomic32Load(Flag,XMEMORY_ACQUIRE)){
        testRequire(xrtTimer()<End,"network extension fixture barrier");
        xrtSleep(1);
    }
}

static inline const xrpccallresult* rpcTestNetworkReply(xfuture* F,xrpccallstatus Status,xrpcdelivery Delivery)
{
    testRequire(xrtFutureWaitFor(F,8000)==XWAIT_OK,"network extension Future completed");
    const xrpccallresult* R=xrtFutureValue(F);
    testRequire(R && R->Status==Status && R->Delivery==Delivery,"network extension typed outcome");
    return R;
}

static inline void rpcTestNetworkOfferResult(xfuture* F,bool Accepted)
{
    const xrpccallresult* R=rpcTestNetworkReply(F,Accepted?XRPC_CALL_OK:XRPC_CALL_REMOTE_ERROR,XRPC_REPLIED);
    xrpcresponseinfo I={.Size=sizeof(I)};
    testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Count==1,"network capability response");
    xrpcvalue Value=Accepted?I.Messages[0].Result:I.Messages[0].ErrorCode;
    cstr Expected=Accepted?"true":"-32601";
    testRequire(Value.Raw.Size==strlen(Expected) && !memcmp(Value.Raw.Data,Expected,Value.Raw.Size),
        "network capability reply value");
    xrtFutureDestroy(F);
}

static inline void rpcTestNetworkDeadlineResult(xfuture* F)
{
    const xrpccallresult* R=rpcTestNetworkReply(F,XRPC_CALL_REMOTE_ERROR,XRPC_REPLIED);
    xrpcresponseinfo I={.Size=sizeof(I)};
    testRequire(xrpcResponsesInfo(R->Responses,&I) && I.Count==1 &&
        I.Messages[0].ErrorCode.Raw.Size==6 && !memcmp(I.Messages[0].ErrorCode.Raw.Data,"-32001",6),
        "network peer returned the negotiated deadline error");
    xrtFutureDestroy(F);
}

static inline bool rpcTestNetworkHeld(const xrpcservercontext* Context,xatomic32* Entered,xatomic32* Cancelled)
{
    xrtAtomic32Store(Entered,1,XMEMORY_RELEASE);
    double End=xrtTimer()+8.;
    while(!xrtCancelRequested(Context->Cancel) && xrtTimer()<Context->Deadline){
        testRequire(xrtTimer()<End,"network worker context carries a finite deadline");
        xrtSleep(1);
    }
    if(xrtCancelRequested(Context->Cancel))
        xrtAtomic32Store(Cancelled,1,XMEMORY_RELEASE);
    return false;
}
#endif
