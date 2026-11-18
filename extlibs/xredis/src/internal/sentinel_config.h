#ifndef XREDIS_SENTINEL_CONFIG_INTERNAL_H
#define XREDIS_SENTINEL_CONFIG_INTERNAL_H
#include <xredis/sentinel.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
typedef struct xredissentinelpeerdiscovery {
    const xredissentinelendpoint* Seeds;
    xredissentinelmasterconnect Connect;
    size_t Configured, MaxRows;
    int64 Timeout;
} xredissentinelpeerdiscovery;
xredisadmission __xredisSentinelDiscoveryCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,xredissentineldiscovery** Output);
xredisadmission __xredisSentinelReplicaCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    size_t MaxReplicas,const xredissentinelendpoint* Prefer,xredissentineldiscovery** Output);
bool __xredisSentinelDiscoveryRetire(xredissentineldiscovery* Discovery);
xredisadmission __xredisSentinelDiscoveryCreateWithPeers(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    size_t MaxReplicas,const xredissentinelendpoint* Prefer,const xredissentinelpeerdiscovery* Peers,xredissentineldiscovery** Output);
const xredissentinelendpoint* __xredisSentinelDiscoveredPeers(xredissentineldiscovery* Discovery,size_t* Count);
bool __xredisSentinelConfigValid(const xredissentinelconfig* Config,bool EndpointSeeds,ptr Output,size_t Bytes);
bool __xredisSentinelEndpointValid(xredissentinelendpoint Endpoint,size_t MaxHostBytes);
#endif
#endif
