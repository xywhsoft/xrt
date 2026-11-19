#ifndef XREDIS_SENTINEL_CONFIG_INTERNAL_H
#define XREDIS_SENTINEL_CONFIG_INTERNAL_H
#include <xredis/sentinel.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
xredisadmission __xredisSentinelDiscoveryCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,xredissentineldiscovery** Output);
xredisadmission __xredisSentinelReplicaCreateUntil(xnetengine* Engine,const xredissentinelconfig* Config,double Deadline,
    size_t MaxReplicas,const xredissentinelendpoint* Prefer,xredissentineldiscovery** Output);
bool __xredisSentinelDiscoveryRetire(xredissentineldiscovery* Discovery);
bool __xredisSentinelConfigValid(const xredissentinelconfig* Config,ptr Output,size_t Bytes);
bool __xredisSentinelEndpointValid(xredissentinelendpoint Endpoint,size_t MaxHostBytes);
#endif
#endif
