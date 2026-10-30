#include "../common/http_peer.h"
static void idle(xrpchttpclient* C,size_t Blocks)
{
    double End=xrtTimer()+10;
    for(;;){xrpchttpinfo I={0};I.Size=sizeof(I);rpcHttpPeerRequire(xrpcHttpInfo(C,&I),"quota info");
        if(!I.Requests){rpcHttpPeerRequire(I.ResultBlocks==Blocks,"result credits");return;}
        rpcHttpPeerRequire(xrtTimer()<End,"request retirement");xrtSleep(1);}
}
int main(void)
{
    /* Self-contained local TCP peer; the production client owns its quota. */
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpconfig K;xrpcHttpConfigInit(&K);K.MaxResultBlocks=2;
    xrpchttpclient* C=rpcHttpConnect(&P,&K);xfuture* F=rpcHttpSubmit(C,"echo","[7]",5000);
    rpcHttpPeerRequire(rpcHttpResult(F)->Status==XRPC_CALL_OK,"first reply");idle(C,2);
    xfuture* Next=NULL;rpcHttpPeerRequire(xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]"),5000,&Next)==XRPC_ADMISSION_OVERLOADED && !Next,"retained result backpressure");
    xrtFutureDestroy(F);idle(C,0);Next=rpcHttpSubmit(C,"echo","[]",5000);
    rpcHttpPeerRequire(rpcHttpResult(Next)->Status==XRPC_CALL_OK,"returned quota reuse");xrtFutureDestroy(Next);idle(C,0);
    rpcHttpClose(&P,C);puts("retained result quota refused excess work and returned credits after release");return 0;
}
