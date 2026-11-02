#include "../common/http_peer.h"
int main(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);
    xrpchttpitem Items[]={
        {XRPC_MESSAGE_CALL,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]")},
        {XRPC_MESSAGE_NOTIFICATION,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[]")},
        {XRPC_MESSAGE_CALL,XRT_STR_LITERAL("error"),{0}}
    };
    xfuture* F=NULL;rpcHttpPeerRequire(xrpcHttpBatchAsync(C,Items,3,5000,&F)==XRPC_ADMISSION_ACCEPTED,"batch admission");
    const xrpcbatchcallresult* R=(const xrpcbatchcallresult*)rpcHttpResult(F);
    rpcHttpPeerRequire(R->Call.Status==XRPC_CALL_BATCH_DONE && R->Count==3 && R->Indices[1]==SIZE_MAX,"batch completion");
    rpcHttpClose(&P,C); /* Result remains live after physical engine cleanup. */
    xrpcresponseinfo I={0};I.Size=sizeof(I);rpcHttpPeerRequire(xrpcResponsesInfo(R->Call.Responses,&I),"owned batch snapshot");
    rpcHttpPeerRequire(I.Messages[R->Indices[0]].Kind==XRPC_MESSAGE_RESULT && I.Messages[R->Indices[2]].Kind==XRPC_MESSAGE_ERROR,"mapped business outcomes");
    puts("call 0: result; item 1: notification (no business confirmation); call 2: remote error");
    xrtFutureDestroy(F);return 0;
}
