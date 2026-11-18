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
/* The Connect factory handles configured and learned Sentinel addresses. Apply
 * endpoint/credential/TLS policy before dialing. To skip an address, return
 * UNREACHABLE without transferring a Client. Endpoint bytes borrow the call. */
typedef xredissentinelmasterconnect xredissentinelseedendpointconnect;
typedef struct xredissentinelpoolseedconfig {
    size_t Size;
    const xredissentinelendpoint* Seeds; /* Discovery.SeedCount endpoints */
    xredissentinelseedendpointconnect Connect;
    size_t MaxSeeds, MaxPeerRows;
    int64 PeerTimeout, PeerRefreshMin;
} xredissentinelpoolseedconfig;
typedef struct xredissentinelpoolseedinfo {
    size_t Size, Configured, Known, Capacity;
} xredissentinelpoolseedinfo;
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
/* Automatic pool pinned to one ROLE-verified, connected replica at a time.
 * Use the same pool ownership and retirement contract. ConnectReplica supplies
 * the data factory; Discovery.ConnectMaster may be NULL. MaxReplicas1..256
 * bounds rows per discovery. Initial discovery chooses the first usable replica.
 * After that, each repair tries the current host/port first when eligible; a
 * different verified replica switches the entire generation and fences old
 * leases. Bootstrap establishes one endpoint before parallel refill starts.
 * NO_REPLICA also retries with bounded jitter. Missing members trigger discovery.
 * Take ordinary native leases; command permissions and replication
 * lag are governed by the Redis deployment and the caller. */
XREDIS_API xredisadmission xredisSentinelReplicaPoolCreate(xnetengine* Engine,uint64 Affinity,size_t Count,
    const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,xredissentinelreplicaconnect ConnectReplica,
    size_t MaxReplicas,xredispool** Output);
/* Defaults: 16 total seeds, 64 rows, 100ms query and at least 1s
 * between scans. Limits: 1..256 seeds/rows, 1..60000ms query cap,
 * 100..86400000ms spacing. Seeds and their hosts are copied on acceptance. */
XREDIS_API bool xredisSentinelPoolSeedConfigInit(xredissentinelpoolseedconfig* Config);
/* Endpoint-based master/replica constructors. Discovery.ConnectSeed is ignored.
 * Initial addresses must be distinct. A successful topology lookup can query
 * SENTINEL SENTINELS on the same connection within its remaining seed budget.
 * Append only validated, eligible endpoints after target ROLE succeeds. Keep
 * configured seeds and preserve exact address/index identity. A failed optional
 * list leaves the primary discovery result intact. No automatic eviction. */
XREDIS_API xredisadmission xredisSentinelPoolCreateWithSeeds(xnetengine* Engine,uint64 Affinity,size_t Count,
    const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,const xredissentinelpoolseedconfig* Seeds,xredispool** Output);
XREDIS_API xredisadmission xredisSentinelReplicaPoolCreateWithSeeds(xnetengine* Engine,uint64 Affinity,size_t Count,
    const xredispoolconfig* Config,const xredissentinelpoolconfig* Sentinel,const xredissentinelpoolseedconfig* Seeds,
    xredissentinelreplicaconnect ConnectReplica,size_t MaxReplicas,xredispool** Output);
/* Thread-safe counts. Also available after physical close. */
XREDIS_API bool xredisSentinelPoolSeedInfo(xredispool* Pool,xredissentinelpoolseedinfo* Info);
/* Coalesce a hint for a fresh Sentinel query and ROLE verification on the pool
 * worker. Hints can come from Pub/Sub; no endpoint from the event is used.
 * One probe runs on a fully connected pool, with at most one follow-up hint.
 * Refill takes precedence. An unchanged endpoint preserves all members.
 * A changed verified endpoint fences the old generation before any new grant.
 * A failed probe updates LastStatus and keeps existing members. No result
 * Future or per-call allocation. Thread-safe, including Notify reentry. */
XREDIS_API xredisadmission xredisSentinelPoolRefresh(xredispool* Pool);
/* Optional monotonic refresh interval (0 disables, otherwise 100..86400000ms).
 * Each interval starts after the prior probe's physical retirement. Disabling
 * periodic probes does not revoke accepted hints or cancel an in-flight probe.
 * Both refresh APIs return CLOSED after closing and ERROR for ordinary pools. */
XREDIS_API xredisadmission xredisSentinelPoolSetRefreshInterval(xredispool* Pool,int64 Interval);
XREDIS_API bool xredisSentinelPoolInfo(xredispool* Pool,xredissentinelpoolinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
