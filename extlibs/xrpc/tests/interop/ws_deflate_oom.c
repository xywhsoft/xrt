#include <xrpc/ws_deflate.h>
#include "../test.h"
static xallocationbudget* DeflateBudget;
static xatomic32 DeflateArmed,DeflateHits;
/* Real production TU and actual public native call. Only the original current
 * allocation scope is bounded; no transport result, wake or IO is mocked. */
static xnetresult wsDeflateBudgetOffer(xwsstream* S,xwsopcode Opcode,xbytesview Payload)
{
    if(!xrtAtomic32Exchange(&DeflateArmed,0,XMEMORY_ACQ_REL))return xrtWsStreamSendCompressed(S,Opcode,Payload);
    xallocationscope Scope={0};testRequire(xrtAllocationBudgetEnter(DeflateBudget,&Scope),"enter native compression original budget");
    xnetresult R=xrtWsStreamSendCompressed(S,Opcode,Payload);xerror* E=xrtTakeError();
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave native compression budget");xrtSetErrorTake(E);xrtAtomic32FetchAdd(&DeflateHits,1,XMEMORY_RELEASE);return R;
}
#define xrtWsStreamSendCompressed wsDeflateBudgetOffer
#include "../../src/ws/ws.c"
#undef xrtWsStreamSendCompressed
#define main wsDeflateRegularMain
#include "../test_ws_deflate.c"
#undef main
int main(int Argc,char** Argv)
{
    xrtAtomic32Init(&DeflateArmed,1);xrtAtomic32Init(&DeflateHits,0);testRequire(xrtAllocationBudgetCreate(1,&DeflateBudget),"native original budget1");
    xrpcwsconfig C;xrpcWsConfigInit(&C);C.CompressOutput=true;C.CompressMinBytes=0;
    xrpcwshttpconfig Client,Server;xrpcWsHttpConfigInit(&Client);xrpcWsHttpConfigInit(&Server);Client.Host=Server.Host=XRT_STR_LITERAL("rpc.fixture");Client.Client.EnableDeflate=Server.Server.EnableDeflate=true;
    rpcWsHttpService S;rpcWsHttpServiceStartEx(&S,
        #if defined(XRPC_FEATURE_XRPC_WS_HTTP_TLS)
        true,
        #else
        false,
        #endif
        &C,false,0,&Client,&Server,XNET_PORT_SELECT);
    const xrpccallresult* R=rpcWsHttpServiceResult(S.Initial);testRequire(R->Status==XRPC_CALL_MEMORY&&R->Delivery==XRPC_MAY_HAVE_EXECUTED,"actual refused send offer keeps conservative delivery and typed MEMORY");
    rpcWsHttpServiceStop(&S);xrtFutureDestroy(S.Initial);xallocationbudgetinfo B={.Size=sizeof(B)};
    testRequire(xrtAllocationBudgetInfo(DeflateBudget,&B)&&B.Rejections&&!B.LiveBytes&&!B.LiveCount&&xrtAtomic32Load(&DeflateHits,XMEMORY_ACQUIRE)==1&&!xrtAtomic32Load(&S.Calls,XMEMORY_ACQUIRE),"one actual rejection, no business execution, balanced physical owners/allocations");
    xrtAllocationBudgetFree(DeflateBudget);puts("[PASS] actual budget1 native compression refusal and full physical rollback");
    return wsDeflateRegularMain(Argc,Argv);
}
