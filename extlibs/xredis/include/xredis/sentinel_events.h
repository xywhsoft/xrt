#ifndef XREDIS_SENTINEL_EVENTS_H
#define XREDIS_SENTINEL_EVENTS_H
#include <xredis/sentinel_pool.h>
#include <xredis/subscription_session.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS) && \
    (!defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL) || !defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION) || \
     !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY))
#error "Sentinel events require native Sentinel pools, subscription sessions and receive notifications"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_EVENTS)
typedef struct xredissentineleventconfig {
    size_t Size, WorkMessages;
    int64 AttemptTimeout, RetryMin, RetryMax, HintInterval;
} xredissentineleventconfig;
typedef struct xredissentineleventinfo {
    size_t Size, Seed;
    xredissubscriptionsessioninfo Session;
    xredissentinelstatus LastSeedStatus;
    uint64 Matched, Ignored, Hints;
} xredissentineleventinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: 16 messages/turn, 5s setup, 100ms..10s jitter and 50ms
 * between hints. WorkMessages 1..64; all times 1..60000ms. */
XREDIS_API bool xredisSentinelEventConfigInit(xredissentineleventconfig* Config);
/* Start one optional, reconnecting subscription to +switch-master.
 * Uses the pool's configured/learned seed factories on its worker and their
 * native push limits. The service name must be a single event token.
 * Factory/readiness/subscription share one original attempt deadline.
 * ACCEPTED copies Config and schedules setup, including when called via Notify.
 * Repeated registration returns OVERLOADED; closed pools return CLOSED.
 * Each valid event is only a paced hint for full query plus ROLE verification.
 * The first acknowledged subscription and each restored one also queue a hint.
 * Session failures surface via Info while existing data leases keep working.
 * Close retires the subscriber before the pool context Drop. Periodic
 * topology refresh remains independently configurable. */
XREDIS_API xredisadmission xredisSentinelPoolListen(xredispool* Pool,const xredissentineleventconfig* Config);
/* Thread-safe snapshot also available after physical close. */
XREDIS_API bool xredisSentinelPoolEventInfo(xredispool* Pool,xredissentineleventinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
