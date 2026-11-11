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
{return __xredisSentinelPoolCreate(E,A,N,C,S,P);}
XREDIS_API bool xredisSentinelPoolInfo(xredispool* P,xredissentinelpoolinfo* I)
{return __xredisSentinelPoolInfo(P,I);}
#endif
