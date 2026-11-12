#include <xredis.h>
#include "../../tests/support/cluster_refresh_peer.h"
int main(void)
{
    rffixture* F=rfStart(3);rfCreate(F,1,NULL);xfuture* Query=rfBegin(F,false);const xredisclusterrefreshresult* Result=rfDrive(F,Query);
    if(Result->Status!=XREDIS_REFRESH_OK || !Result->Published)return 1;
    xredisclustermap* Map=NULL;bool Stale=true;if(xredisClusterRefreshSnapshot(F->Refresh,&Map,&Stale)!=XREDIS_CLUSTER_VALUE || Stale)return 2;
    xredisclustermapinfo I={0};I.Size=sizeof(I);if(!xredisClusterMapInfo(Map,&I))return 3;
    if(xredisClusterRefreshDiscover(F->Refresh,Map)!=XREDIS_ADMISSION_ACCEPTED)return 5;
    xredisclusterrouteconfig C;xredisClusterRouteConfigInit(&C);C.GetPool=xredisClusterNodesGetPool;C.Data=F->Base->Cache;C.Tls=I.Origin.Tls;
    xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("fresh\0route")};xredisclusterroute* Route=NULL;
    if(xredisClusterRouteCreate(I.Topology,I.Origin.Host,123,A,2,&C,&Route)!=XREDIS_ADMISSION_ACCEPTED || crDrive(Route).Query.Status!=XREDIS_REQUEST_OK)return 4;
    xredisClusterRouteDestroy(Route);rfStop(F);xredisClusterMapRelease(Map);xrtFutureDestroy(Query);puts("Coalesced SHARDS refresh -> immutable map -> cached native route, explicit close");return 0;
}
