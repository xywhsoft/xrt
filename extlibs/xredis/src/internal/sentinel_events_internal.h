#ifndef XREDIS_SENTINEL_EVENTS_INTERNAL_H
#define XREDIS_SENTINEL_EVENTS_INTERNAL_H
#include <xredis/sentinel_events.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
xredisadmission __xredisSentinelPoolListen(xredispool*,const xredissentineleventconfig*);
bool __xredisSentinelPoolEventInfo(xredispool*,xredissentineleventinfo*);
bool __xredisSentinelSwitchEvent(const xredisreply*,xbytesview,size_t);
#endif
#endif
