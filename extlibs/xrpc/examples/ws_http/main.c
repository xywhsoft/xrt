#include <xrpc/ws_http.h>
#include "../common/ws_http_service.h"
int main(int Argc,char** Argv)
{
    bool TLS=Argc>1&&strtoul(Argv[1],NULL,10);rpcWsHttpService S;
    rpcWsHttpServiceStart(&S,TLS,NULL,false);
    const xrpccallresult* R=rpcWsHttpServiceResult(S.Initial);
    rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED,"queued call replies after checked HTTP Upgrade");
    xrpcwshttpinfo H={.Size=sizeof(H)};
    rpcWsHttpServiceRequire(xrpcWsHttpInfo(S.Client,&H)&&H.Outcome==XRPC_WS_HTTP_OPEN&&H.Status==101,"checked Upgrade snapshot");
    rpcWsHttpServiceStop(&S);
    rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK,"result survives physical host joins");
    xrtFutureDestroy(S.Initial);puts("[PASS] actual HTTP Upgrade, queued RPC and physical retirement");return 0;
}
