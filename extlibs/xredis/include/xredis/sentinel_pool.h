#ifndef XREDIS_SENTINEL_POOL_H
#define XREDIS_SENTINEL_POOL_H
#include <xredis/sentinel.h>
#include <xredis/pool_connect.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL) && (!defined(XREDIS_FEATURE_XREDIS_SENTINEL) || !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT))
#error "Sentinel pools require discovery and native pool maintenance"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
typedef enum xredissentinelpoolphase {
    XREDIS_SENTINEL_POOL_DISCOVERING=0,XREDIS_SENTINEL_POOL_ONLINE,
    XREDIS_SENTINEL_POOL_RECONFIGURING,XREDIS_SENTINEL_POOL_CLOSING,XREDIS_SENTINEL_POOL_CLOSED
} xredissentinelpoolphase;
typedef struct xredissentinelpoolinfo {
    size_t Size;xredissentinelpoolphase Phase;
    uint64 Generation,Changes;size_t PreferredSeed;uint16 Port;
    xredissentinelstatus LastStatus;xredispoolconnectinfo Connect;
} xredissentinelpoolinfo;
/* Coalesced snapshots on the existing pool worker, outside gates. Must not block.
 * Reentry through async/native snapshot/close APIs is allowed. Snapshot is borrowed
 * through return; retain escaping objects. Callback code/Data survive through Drop. */
typedef void (*xredissentinelpoolnotify)(xredispool* Pool,const xredissentinelpoolinfo* Info,ptr Data);
typedef struct xredissentinelpoolconfig {
    size_t Size;xredissentinelconfig Discovery;size_t MaxConnecting;
    int64 RetryMin,RetryMax;xredissentinelpoolnotify Notify;xredispoolconnectdrop Drop;
} xredissentinelpoolconfig;
XRT_EXTERN_C_BEGIN
/* Defaults: native discovery defaults, four complete concurrent discovery/retire
 * duties, jittered100ms..10s retry. Discovery.Timeout1..60000ms; other discovery
 * bounds apply. Retry1..60000ms, min<=max. Service bytes are copied once here and
 * once per bounded active discovery. Native factories keep their own limits. */
XREDIS_API bool xredisSentinelPoolConfigInit(xredissentinelpoolconfig* Config);
/* Automatic Count1..65536 native pool slots. Every missing member runs fresh
 * Sentinel discovery and target ROLE, including same-address reconnects.
 * One existing pool worker/heap/timer owns all cold state. MaxConnecting counts
 * physical failure/cancellation tails. No factory runs inline on this caller.
 * ACCEPTED consumes Discovery.Data/Drop duty; failure consumes nothing and
 * preserves Output. Factories/Notify/Drop run serially on the pool worker.
 * Discovery.Service is snapshotted; other referenced trust/resolver/materials
 * and context remain valid through Drop. Each attempt shares the discovery's
 * original deadline; acquisitions keep their own outage-inclusive deadlines.
 * TIMEOUT/UNREACHABLE/STALE_ROLE retry with bounded jitter; unknown service,
 * auth/server/protocol/limit/memory/factory failures fail the pool closed.
 * LastStatus preserves the maintenance outcome after physical close.
 *
 * A changed verified host/port atomically fences ALL old lease admissions before
 * retiring members in WorkItems batches. Already accepted calls keep native
 * delivery uncertainty; no business command is replayed. Old generations and
 * discovery/watch/cancel tails physically retire before the fence reopens.
 * An unchanged endpoint refills only missing slots. Host comparison is exact
 * bytes; no DNS alias/consensus/freshness inference is made. ROLE is point-in-time.
 * Use the ordinary PoolAcquire/LeaseCall/Return/Abort/Close/Closed APIs directly.
 * Explicit Close retires all candidates/callbacks before one Drop and Closed;
 * Context access retires before Drop; final watch/post release frames may finish
 * afterwards. Join the engine before unloading callback code. Retained closed
 * snapshots/outcomes keep no engine/context reference. */
XREDIS_API xredisadmission xredisSentinelPoolCreate(xnetengine* Engine,uint64 Affinity,size_t Count,
    const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,xredispool** Output);
XREDIS_API bool xredisSentinelPoolInfo(xredispool* Pool,xredissentinelpoolinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
