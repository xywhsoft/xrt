#include <xrpc/ws_deflate.h>
#include "../common/ws_http_service.h"
int main(int Argc,char** Argv)
{
    bool TLS=Argc>1&&strtoul(Argv[1],NULL,10);bool Compression=Argc<3||strcmp(Argv[2],"0");rpcWsHttpService S;
    if(Compression){
        xrpcwsconfig C;xrpcWsConfigInit(&C);C.CompressOutput=true;
        xrpcwshttpconfig Client,Server;xrpcWsHttpConfigInit(&Client);xrpcWsHttpConfigInit(&Server);Client.Host=Server.Host=XRT_STR_LITERAL("rpc.fixture");Client.Client.EnableDeflate=Server.Server.EnableDeflate=true;
        rpcWsHttpServiceStartEx(&S,TLS,&C,false,0,&Client,&Server,XNET_PORT_SELECT);
    }else rpcWsHttpServiceStart(&S,TLS,NULL,false);
    rpcWsHttpServiceRequire(rpcWsHttpServiceResult(S.Initial)->Status==XRPC_CALL_OK,"small initial call");
    char Params[4100];Params[0]='[';Params[1]='"';memset(Params+2,'a',4096);Params[4098]='"';Params[4099]=']';xfuture* F=NULL;
    rpcWsHttpServiceRequire(xrpcWsCall(S.Client,XRT_STR_LITERAL("echo"),(xstrview){Params,sizeof(Params)},5000,&F)==XRPC_DUPLEX_ACCEPTED,"bounded call");
    const xrpccallresult* R=rpcWsHttpServiceResult(F);rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK&&R->Delivery==XRPC_REPLIED,"compressed result");
    xrpcwsdeflateinfo I={.Size=sizeof(I)};rpcWsHttpServiceRequire(xrpcWsDeflateInfo(S.Client,&I)&&I.Enabled==Compression,"actual negotiated facts");
    printf("negotiated=%u compress_output=%u min_bytes=%zu\n",(unsigned)I.Enabled,(unsigned)I.CompressOutput,I.CompressMinBytes);
    rpcWsHttpServiceStop(&S);rpcWsHttpServiceRequire(R->Status==XRPC_CALL_OK,"retained result survives native/host joins");xrtFutureDestroy(F);xrtFutureDestroy(S.Initial);return 0;
}
