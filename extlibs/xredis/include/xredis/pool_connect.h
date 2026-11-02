#ifndef XREDIS_POOL_CONNECT_H
#define XREDIS_POOL_CONNECT_H
#include <xredis/pool.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS_POOL_CONNECT requires XREDIS_FEATURE_XREDIS_POOL"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
/* Called serially on the pool worker, without a pool/client gate. Must not
 * block. Start an existing managed numeric/DNS/TLS/auth factory with Timeout
 * as its total setup limit. Success transfers ONE private client reference;
 * failure transfers nothing and leaves Client unchanged. No business commands
 * or sharing/claims are permitted before transfer. Slot is stable, Attempt is
 * a saturating per-slot counter. Choose the same service/session/RESP profile
 * on every attempt. No command from an old connection is ever replayed. */
typedef bool (*xredispoolconnectproc)(xnetengine* Engine, size_t Slot,
    uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client);
typedef void (*xredispoolconnectdrop)(ptr Data);
typedef struct xredispoolconnectconfig {
    size_t Size;
    xredispoolconnectproc Connect;
    xredispoolconnectdrop Drop;
    ptr Data;
    xredisprotocol Protocol;
    size_t MaxConnecting;
    int64 AttemptTimeout;
    int64 RetryMin, RetryMax;
} xredispoolconnectconfig;
typedef struct xredispoolconnectinfo {
    size_t Size;
    size_t Connecting, Waiting;
    uint64 Attempts, Failures, Connected;
    xredisrequeststatus LastFailure;
} xredispoolconnectinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: RESP3, 4 simultaneous attempts, 5 s setup, 100 ms..10 s retry.
 * Connect is mandatory. Bounds: concurrency 1..65536 (capped by Count),
 * setup 1..60000 ms, retry 1..60000 ms, min <= max. */
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config);
/* Start/refill Count slots. No I/O/factory callback runs inline, although a
 * worker may run before this API returns. ACCEPTED consumes the Data/Drop
 * obligation; every failure preserves it and Result. Config values are copied.
 * Attempts, their physical failure tails and retry queues are bounded. Every
 * failed generation physically closes before its slot can reconnect. Backoff
 * is exponential with bounded non-cryptographic jitter, reset on readiness.
 * Acquisition deadlines include outage/retry wait; zero Live does not cause
 * DISCONNECTED in this mode. Each acquisition still has its own finite/forever
 * timeout and cancellation; it does not cancel shared maintenance.
 * PoolClose stops new attempts, closes candidates/members and retires timers/
 * callbacks before Drop and PoolClosed. Drop runs once on the pool worker,
 * outside gates, must not block and must outlive the pool. Context, resolver,
 * verifier/signer code and referenced material remain valid until Drop.
 * Maintenance timer OOM fails the pool closed; it never busy-retries OOM.
 * PoolInfo.Retired counts actual retired client generations (can exceed Count).
 * Retained closed pool metadata holds no context/client/engine reference. */
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result);
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
