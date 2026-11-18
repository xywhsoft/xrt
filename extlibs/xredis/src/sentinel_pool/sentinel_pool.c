#include <xredis/sentinel_pool.h>
#include "../internal/sentinel_pool_internal.h"
#include <xrt/memory.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
XREDIS_API bool xredisSentinelPoolConfigInit(xredissentinelpoolconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid Sentinel pool config");return false;}
    *K=(xredissentinelpoolconfig){0};K->Size=sizeof(*K);K->MaxConnecting=4;K->RetryMin=100;K->RetryMax=10000;return xredisSentinelConfigInit(&K->Discovery);
}
XREDIS_API xredisadmission xredisSentinelPoolCreate(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,const xredissentinelpoolconfig* S,xredispool** P)
{return __xredisSentinelPoolCreate(E,A,N,C,S,NULL,NULL,0,P);}
static xredisadmission xredisSentinelPoolCreateReplica(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,
    const xredissentinelpoolconfig* S,const xredissentinelpoolseedconfig* Seeds,
    xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,xredispool** P)
{
    if(!ConnectReplica||!MaxReplicas||MaxReplicas>256){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid Sentinel replica pool factory or limit");
        return XREDIS_ADMISSION_ERROR;
    }
    return __xredisSentinelPoolCreate(E,A,N,C,S,Seeds,ConnectReplica,MaxReplicas,P);
}
XREDIS_API xredisadmission xredisSentinelReplicaPoolCreate(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,
    const xredissentinelpoolconfig* S,xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,xredispool** P)
{return xredisSentinelPoolCreateReplica(E,A,N,C,S,NULL,ConnectReplica,MaxReplicas,P);}
XREDIS_API bool xredisSentinelPoolSeedConfigInit(xredissentinelpoolseedconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid Sentinel seed config");
        return false;
    }
    *K=(xredissentinelpoolseedconfig){.Size=sizeof(*K),.MaxSeeds=16,.MaxPeerRows=64,.PeerTimeout=100,.PeerRefreshMin=1000};
    return true;
}
XREDIS_API xredisadmission xredisSentinelPoolCreateWithSeeds(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,
    const xredissentinelpoolconfig* S,const xredissentinelpoolseedconfig* Seeds,xredispool** P)
{
    if(!Seeds){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"missing Sentinel seed config");
        return XREDIS_ADMISSION_ERROR;
    }
    return __xredisSentinelPoolCreate(E,A,N,C,S,Seeds,NULL,0,P);
}
XREDIS_API xredisadmission xredisSentinelReplicaPoolCreateWithSeeds(xnetengine* E,uint64 A,size_t N,const xredispoolconfig* C,
    const xredissentinelpoolconfig* S,const xredissentinelpoolseedconfig* Seeds,
    xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,xredispool** P)
{
    if(!Seeds){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"missing Sentinel seed config");
        return XREDIS_ADMISSION_ERROR;
    }
    return xredisSentinelPoolCreateReplica(E,A,N,C,S,Seeds,ConnectReplica,MaxReplicas,P);
}
XREDIS_API bool xredisSentinelPoolSeedInfo(xredispool* P,xredissentinelpoolseedinfo* I)
{return __xredisSentinelPoolSeedInfo(P,I);}
XREDIS_API xredisadmission xredisSentinelPoolRefresh(xredispool* P)
{return __xredisSentinelPoolRefresh(P,-1);}
XREDIS_API xredisadmission xredisSentinelPoolSetRefreshInterval(xredispool* P,int64 Interval)
{
    if(Interval<0 || (Interval && Interval<100) || Interval>86400000){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid Sentinel refresh interval");
        return XREDIS_ADMISSION_ERROR;
    }
    return __xredisSentinelPoolRefresh(P,Interval);
}
XREDIS_API bool xredisSentinelPoolInfo(xredispool* P,xredissentinelpoolinfo* I)
{return __xredisSentinelPoolInfo(P,I);}
#endif
