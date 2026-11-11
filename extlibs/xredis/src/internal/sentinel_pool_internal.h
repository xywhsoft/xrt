#ifndef XREDIS_SENTINEL_POOL_INTERNAL_H
#define XREDIS_SENTINEL_POOL_INTERNAL_H
#include <xredis/sentinel_pool.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
xredisadmission __xredisSentinelPoolCreate(xnetengine*,uint64,size_t,const xredispoolconfig*,const xredissentinelpoolconfig*,xredispool**);
bool __xredisSentinelPoolInfo(xredispool*,xredissentinelpoolinfo*);
#endif
#endif
