#include <xrpc/ws_dial.h>
#define XRPC_WS_HTTP_SERVICE_LISTEN_ONLY
#include "../common/ws_http_service.h"
int main(int Argc,char** Argv)
{
    bool TLS=Argc>1&&strtoul(Argv[1],NULL,10);rpcWsHttpService S;rpcWsHttpServiceListen(&S,TLS,NULL,false,0,NULL,NULL,XNET_PORT_SELECT);bool OK;
    #if defined(XRPC_FEATURE_XRPC_WS_DIAL_TLS)
    if(TLS){xtlsclientconfig T;xrtTlsClientConfigInit(&T);T.Context=S.Context;T.Verifier=S.Verifier;T.ServerName=XRT_STR_LITERAL("rpc.fixture");OK=xrpcWsConnectTls(S.Engine,&S.Address,1,&T,NULL,&S.ClientConfig,&S.ClientHTTP,NULL,&S.Client);}else
    #endif
    OK=xrpcWsConnect(S.Engine,&S.Address,1,&S.ClientConfig,&S.ClientHTTP,NULL,&S.Client);
    rpcWsHttpServiceRequire(OK,"numeric client with native establishment and checked HTTP Upgrade");xfuture* F=NULL;rpcWsHttpServiceRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"call queues during setup");const xrpccallresult* R=rpcWsHttpServiceResult(F);rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED,"checked RPC reply");
    xrpcresponseinfo I={.Size=sizeof(I)};rpcWsHttpServiceRequire(xrpcResponsesInfo(R->Responses,&I)&&R->Index<I.Count,"retained response view");xstrview Value=I.Messages[R->Index].Result.Raw;printf("result=%.*s\n",(int)Value.Size,Value.Data);rpcWsHttpServiceStop(&S);rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK,"retained result after all host joins");xrtFutureDestroy(F);return 0;
}
