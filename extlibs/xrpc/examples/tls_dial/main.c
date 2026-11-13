#include <xrpc/tls_dial.h>
#include "../common/tls_service.h"
int main(void)
{
    rpcTlsService S;rpcTlsServiceStart(&S,1,false,false,false);xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;
    xnetresolver* Resolver=xrtNetResolverCreate(&C);rpcTlsServiceRequire(Resolver!=NULL,"host TLS Resolver");xrpctlsdialconfig D;xrpcTlsDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;
    xtlsclientconfig Client;rpcTlsServiceClient(&S,&Client);xrpctls* Peer=NULL;
    rpcTlsServiceRequire(xrpcTlsDial(S.Engine,Resolver,XRT_STR_LITERAL("localhost"),S.Address.Port,1,&Client,&S.Config,&D,&Peer),"DNS verified TLS peer");
    xfuture* F=NULL;rpcTlsServiceRequire(xrpcTlsCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"queue during DNS/TLS");
    rpcTlsServiceRequire(rpcTlsServiceResult(F)->Status==XRPC_CALL_OK,"verified DNS reply");xrtFutureDestroy(F);
    rpcTlsServiceRequire(xrpcTlsCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[4,5]"),5000,&F)==XRPC_DUPLEX_ACCEPTED && rpcTlsServiceResult(F)->Status==XRPC_CALL_OK,"verified reverse call");
    rpcTlsServiceCycleClose(&S,Peer);rpcTlsServiceRequire(xrtNetResolverDestroy(Resolver),"separate Resolver join");rpcTlsServiceStop(&S);xrtFutureDestroy(F);
    puts("TLS DNS peer: explicit fixture identity/pin, bidirectional RPC, physical candidate/TLS retirement");return 0;
}
