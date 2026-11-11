#ifndef XREDIS_SENTINEL_H
#define XREDIS_SENTINEL_H
#include <xredis/client.h>
#include <xredis/role.h>
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_ROLE))
#error "Sentinel discovery requires native managed clients and ROLE inspection"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
typedef struct xredissentineldiscovery xredissentineldiscovery;
typedef struct xredissentinelendpoint {xbytesview Host;uint16 Port;} xredissentinelendpoint;
typedef enum xredissentinelstatus {
    XREDIS_SENTINEL_PENDING=0,XREDIS_SENTINEL_OK,XREDIS_SENTINEL_UNREACHABLE,
    XREDIS_SENTINEL_UNKNOWN_SERVICE,XREDIS_SENTINEL_STALE_ROLE,XREDIS_SENTINEL_TIMEOUT,
    XREDIS_SENTINEL_CANCELLED,XREDIS_SENTINEL_AUTH_FAILED,XREDIS_SENTINEL_SERVER_ERROR,
    XREDIS_SENTINEL_PROTOCOL,XREDIS_SENTINEL_LIMIT,XREDIS_SENTINEL_MEMORY,XREDIS_SENTINEL_FACTORY
} xredissentinelstatus;
typedef enum xredissentinelstep {
    XREDIS_SENTINEL_STEP_ERROR=-1,XREDIS_SENTINEL_WAIT=0,XREDIS_SENTINEL_MORE=1,XREDIS_SENTINEL_DONE=2
} xredissentinelstep;
/* Nonblocking factories run on the Step caller. OK transfers one fresh,
 * PRIVATE managed Client reference on Engine; failure transfers nothing.
 * Select numeric/DNS/prepared/TLS, Sentinel/data credentials and trust separately.
 * Respect Timeout as the entire native setup limit; copy borrowed Endpoint input
 * before returning. No business call, external sharing or control is permitted.
 * Return a typed non-OK status on failure (no transferred reference); transient
 * TIMEOUT/UNREACHABLE retries, other statuses fail closed. The discovery performs
 * its own ROLE command; factories need no role flag. */
typedef xredissentinelstatus (*xredissentinelseedconnect)(xnetengine* Engine,size_t Seed,int64 Timeout,ptr Data,xredisclient** Client);
typedef xredissentinelstatus (*xredissentinelmasterconnect)(xnetengine* Engine,const xredissentinelendpoint* Endpoint,int64 Timeout,ptr Data,xredisclient** Client);
typedef struct xredissentinelconfig {
    size_t Size; xbytesview Service;
    size_t SeedCount,PreferredSeed,MaxRounds,MaxServiceBytes,MaxHostBytes;
    int64 Timeout,SeedTimeout,RetryDelay;
    xredisprotocol Protocol;xredisrolelimits Role;
    xredissentinelseedconnect ConnectSeed;xredissentinelmasterconnect ConnectMaster;ptr Data;
} xredissentinelconfig;
/* Host borrows Discovery until Destroy, copied once from the successful query.
 * PreferredSeed identifies the most recent positive query, for the next discovery.
 * Native outcomes are last observations, never an ambient error/peer diagnostic.
 * OK verifies a point-in-time master identity, not freshness or consensus. */
typedef struct xredissentinelresult {
    size_t Size;xredissentinelstatus Status;xredissentinelendpoint Endpoint;
    size_t PreferredSeed,Rounds,SeedAttempts,MasterAttempts,UnknownReplies,StaleReplies;
    bool ReadyObserved,RequestObserved;
    xredisreadystatus ReadyStatus;xredisrequeststatus RequestStatus;
} xredissentinelresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 5s total, 300ms per seed connect+query, 100ms stale/transport retry,
 * 3 rounds, 1 seed, 64KiB service and 1024-byte host. Two factories mandatory.
 * Timeout 1..24h, SeedTimeout/RetryDelay 1..60s; seeds1..4096/rounds1..1024;
 * service1..64KiB, host1..64KiB. All limits include caller scheduling delay. */
XREDIS_API bool xredisSentinelConfigInit(xredissentinelconfig* Config);
/* Unique caller serialization for every API; no reentry on this Discovery from
 * a factory. Copies config/service, allocates one bounded host buffer and pins
 * existing Engine. Data is borrowed through Destroy. Failure consumes nothing
 * and preserves Output. No new runtime, worker, timer or wrapper Future.
 * Successful factories' transport callbacks retain their own context/security
 * material through native physical close; Discovery never assumes their duty. */
XREDIS_API xredisadmission xredisSentinelDiscoveryCreate(xnetengine* Engine,const xredissentinelconfig* Config,xredissentineldiscovery** Output);
/* Each Step advances one phase, never blocks. MORE schedules another turn.
 * WAIT observes Pending or NextWake (whichever comes first). Never busy-poll or
 * block an Engine worker. One original monotonic deadline spans all rounds,
 * factory/Ready/query/ROLE/backoff and retirement. Failed candidates physically
 * retire before another factory starts. Nil query tries next seed; wrong ROLE
 * waits then rediscovers. ACL/auth/protocol/limit/OOM fail closed. All-null seed
 * round is UNKNOWN_SERVICE; mixed unavailable/null is UNREACHABLE. No INFO
 * fallback for old servers without ROLE, business replay or implicit downgrade. */
XREDIS_API xredissentinelstep xredisSentinelDiscoveryStep(xredissentineldiscovery* Discovery);
/* One retained unresolved native Ready/command/Closed observer, otherwise NULL.
 * Release it normally. Cancelling a command observer asks that native operation
 * to stop; cancellation of the shared readiness/close observer does not abort
 * discovery. Use DiscoveryCancel for that. No notification runs a helper callback. */
XREDIS_API xfuture* xredisSentinelDiscoveryPending(xredissentineldiscovery* Discovery);
/* Absolute monotonic seconds (xrtTimer); zero on DONE, includes physical-tail
 * polling at >=1ms. Pending completion can require an earlier turn. */
XREDIS_API double xredisSentinelDiscoveryNextWake(xredissentineldiscovery* Discovery);
XREDIS_API bool xredisSentinelDiscoveryResult(xredissentineldiscovery* Discovery,xredissentinelresult* Result);
/* Once only, after DONE/OK, transfers native master reference and close duty.
 * Destroy otherwise closes it. Future reconnects must run discovery again.
 * No proxy/extra allocation/copy/synchronization on ordinary business calls. */
XREDIS_API bool xredisSentinelDiscoveryTakeClient(xredissentineldiscovery* Discovery,xredisclient** Client);
XREDIS_API bool xredisSentinelDiscoveryCancel(xredissentineldiscovery* Discovery);
/* Nonblocking abort: closes unreleased candidate, cancels/releases owned command
 * observer, unpins Engine and frees helper storage. Native physical duties own
 * copied wire and no Discovery pointer. Drive Cancel to DONE first if observing
 * physical retirement is required. All retained native observers remain valid. */
XREDIS_API void xredisSentinelDiscoveryDestroy(xredissentineldiscovery* Discovery);
XRT_EXTERN_C_END
#endif
#endif
