/* Offline HTTP/TCP call example; no external service or credentials required.
 * Run tools/build.py with xrpc manifest, --suite xrpc_http_client --no-single.
 * Asserts the copied request's result, then closes/releases client, peer and
 * engine. The retained typed Future still owns its payload after that cleanup. */
#include "../common/http_peer.h"
int main(void)
{
    rpcHttpPeer P;rpcHttpPeerStart(&P);xrpchttpclient* C=rpcHttpConnect(&P,NULL);
    xfuture* F=rpcHttpSubmit(C,"echo","[1,2]",5000);const xrpccallresult* R=rpcHttpResult(F);
    rpcHttpPeerRequire(R->Status==XRPC_CALL_OK && R->Delivery==XRPC_REPLIED,"example typed response");
    rpcHttpClose(&P,C);xrpcresponseinfo I={0};I.Size=sizeof(I);
    rpcHttpPeerRequire(xrpcResponsesInfo(R->Responses,&I) && I.Messages[0].Result.Raw.Size==5 &&
        !memcmp(I.Messages[0].Result.Raw.Data,"[1,2]",5),"example retained payload");
    xrtFutureDestroy(F);puts("xrpc HTTP: typed reply and retained payload verified after physical close");return 0;
}
