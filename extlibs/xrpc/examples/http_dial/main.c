#include "../common/http_peer.h"
#include <xrpc/http_dial.h>
int main(int argc,char** argv)
{
    /* No args: self-contained peer. One port argument: external localhost peer. */
    if(argc>2)return 2;
    char* End=NULL;unsigned long Port=argc==2?strtoul(argv[1],&End,10):0;
    if(argc==2 && (!*argv[1] || *End || !Port || Port>65535))return 2;
    rpcHttpPeer P={0};if(argc==1){rpcHttpPeerStart(&P);Port=P.Address.Port;}
    else{xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P.Engine=xrtNetEngineCreate(&E);rpcHttpPeerRequire(P.Engine && xrtNetEngineStart(P.Engine),"external-peer engine");}
    xnetresolverconfig K;xrtNetResolverConfigInit(&K);K.Workers=1;K.RequestLimit=16;K.QueryLimit=16;
    xnetresolver* Resolver=xrtNetResolverCreate(&K);rpcHttpPeerRequire(Resolver!=NULL,"resolver create");
    xrpchttpdialconfig D;xrpcHttpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;xrpchttpclient* C=NULL;
    rpcHttpPeerRequire(xrpcHttpDial(P.Engine,Resolver,XRT_STR_LITERAL("localhost"),(uint16)Port,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),NULL,&D,&C),"HTTP DNS dial");
    xfuture* F=rpcHttpSubmit(C,"echo","[7]",5000);const xrpccallresult* Reply=rpcHttpResult(F);xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
    rpcHttpPeerRequire(Reply->Status==XRPC_CALL_OK && xrpcResponsesInfo(Reply->Responses,&Info) && Info.Count==1 && Info.Messages[0].Result.Raw.Size==3 && !memcmp(Info.Messages[0].Result.Raw.Data,"[7]",3),"dial reply");
    xrtFutureDestroy(F);
    if(argc==1)rpcHttpClose(&P,C);
    else{xfuture* Closed=xrpcHttpClosed(C);rpcHttpPeerRequire(Closed && xrpcHttpClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"external-peer client close");xrpcHttpRelease(C);xrtFutureDestroy(Closed);}
    rpcHttpPeerRequire(xrtNetResolverDestroy(Resolver),"resolver join");
    if(argc==2)rpcHttpPeerRequire(xrtNetEngineStop(P.Engine) && xrtNetEngineDestroy(P.Engine),"external-peer engine join");
    puts("asynchronous localhost dial, request, close and resolver join completed");return 0;
}
