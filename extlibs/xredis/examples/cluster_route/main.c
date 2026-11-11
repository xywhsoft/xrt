#include <xredis.h>
#include "../../tests/support/cluster_route_peer.h"
int main(void)
{
    crfixture* F=crStart();crMode(F,0,CR_MOVED,1);crMode(F,1,CR_ASK,2);
    xredisclustertopology* T=crTopology(F->Peer.Peers[0].Address.Port,"127.0.0.1",0,16383);
    xredisclusterrouteconfig C;crConfig(F,&C);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("route\0bytes")};xredisclusterroute* R=NULL;
    if(xredisClusterRouteCreate(T,(xbytesview)XRT_BYTES_LITERAL("127.0.0.1"),1,A,2,&C,&R)!=XREDIS_ADMISSION_ACCEPTED)return 1;
    xredisClusterTopologyRelease(T);xredisclusterrouteresult Result=crDrive(R);
    if(Result.Status!=XREDIS_CLUSTER_ROUTE_COMPLETE || Result.Query.Status!=XREDIS_REQUEST_OK || Result.Attempts!=3 || Result.Moved!=1 || Result.Asks!=1)return 2;
    xfuture* Native=xredisClusterRouteResultFuture(R);xredisClusterRouteDestroy(R);crStop(F);
    const xredisrequestresult* Q=xrtFutureValue(Native);xredisreplyvalue V;
    if(!Q || !xredisReplyGet(Q->Reply,0,&V) || V.Data.Size!=11 || memcmp(V.Data.Data,"route\0bytes",11))return 3;
    xrtFutureDestroy(Native);puts("Topology -> MOVED -> ASKING/query, one original budget; retained native reply survives engines");return 0;
}
