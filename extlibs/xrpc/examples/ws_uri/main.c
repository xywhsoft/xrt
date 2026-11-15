#include <xrpc/ws_uri.h>
#define XRPC_WS_HTTP_SERVICE_LISTEN_ONLY
#include "../common/ws_http_service.h"
int main(void)
{
    rpcWsHttpService S;xrpcwshttpconfig Server;xrpcWsHttpConfigInit(&Server);rpcWsHttpServiceListen(&S,false,NULL,false,0,NULL,&Server,XNET_PORT_SELECT);char URI[100];int N=snprintf(URI,sizeof(URI),"ws://127.0.0.1:%u/rpc",(unsigned)S.Address.Port);xrpcwsuri Parsed={.Size=sizeof(Parsed)};rpcWsHttpServiceRequire(N>0&&xrpcWsUriParse((xstrview){URI,(size_t)N},&Parsed)&&xrpcWsConnectUri(S.Engine,NULL,(xstrview){URI,(size_t)N},1,&S.ClientConfig,NULL,NULL,&S.Client),"bounded URI/client factory");xfuture* F=NULL;rpcWsHttpServiceRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"queue call during URI setup");const xrpccallresult* R=rpcWsHttpServiceResult(F);rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED,"URI echo result");printf("URI=%s port=%u resource_bytes=%zu status=%u\n",URI,(unsigned)Parsed.Port,Parsed.TargetBytes,(unsigned)R->Status);rpcWsHttpServiceStop(&S);xrtFutureDestroy(F);return 0;
}
