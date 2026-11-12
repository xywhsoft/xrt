#ifndef XREDIS_CLUSTER_ROUTE_INTERNAL_H
#define XREDIS_CLUSTER_ROUTE_INTERNAL_H
#include <xredis/cluster_route.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
bool __xredisClusterRouteConfig(const xredisclusterrouteconfig* Config);
size_t __xredisClusterRouteBaseBytes(void);
xredisadmission __xredisClusterRouteCreate(const xredisclustertopology* Topology,
    xbytesview Origin,uint16 Slot,const xbytesview* Arguments,size_t Count,
    const xredisclusterrouteconfig* Config,xredisclusterroute** Route,double Entered,xredisclusterroutestatus* Failure);
xredisadmission __xredisClusterRouteRetryMissing(xredisclusterroute* Previous,
    const xredisclustertopology* Topology,xbytesview Origin,const xredisclusterrouteconfig* Config,xredisclusterroute** Route,xredisclusterroutestatus* Failure);
#endif
#endif
