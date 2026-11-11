#ifndef XREDIS_SENTINEL_CLIENT_INTERNAL_H
#define XREDIS_SENTINEL_CLIENT_INTERNAL_H
#include <xredis/sentinel.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
bool __xredisSentinelClientProfile(xredisclient* Client,xnetengine* Engine);
#endif
#endif
