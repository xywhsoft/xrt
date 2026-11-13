#include <xrpc/tcp_dial.h>
#include "../common/tcp_service.h"
int main(void)
{
    rpcTcpService S;rpcTcpServiceStart(&S);xnetresolverconfig C;xrtNetResolverConfigInit(&C);C.Workers=1;C.CacheEntries=0;
    xnetresolver* Resolver=xrtNetResolverCreate(&C);rpcTcpServiceRequire(Resolver!=NULL,"host Resolver");xrpctcpdialconfig D;xrpcTcpDialConfigInit(&D);D.Family=XNET_FAMILY_IPV4;
    xrpctcp* Peer=NULL;rpcTcpServiceRequire(xrpcTcpDial(S.Engine,Resolver,XRT_STR_LITERAL("localhost"),S.Address.Port,1,&S.Config,&D,&Peer),"DNS peer");
    xfuture* F=NULL;rpcTcpServiceRequire(xrpcTcpCall(Peer,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"queue before DNS completes");
    rpcTcpServiceRequire(rpcTcpServiceResult(F)->Status==XRPC_CALL_OK,"DNS call reply");xrtFutureDestroy(F);
    rpcTcpServiceRequire(xrpcTcpCall(S.Accepted,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[4,5]"),5000,&F)==XRPC_DUPLEX_ACCEPTED && rpcTcpServiceResult(F)->Status==XRPC_CALL_OK,"reverse call");
    rpcTcpServiceCycleClose(&S,Peer);rpcTcpServiceRequire(xrtNetResolverDestroy(Resolver),"separate host Resolver join");rpcTcpServiceStop(&S);xrtFutureDestroy(F);
    puts("DNS TCP peer: bidirectional RPC, owned candidate retirement, independent retained result");return 0;
}
