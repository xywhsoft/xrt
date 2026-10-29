#ifndef XREDIS_POOL_H
#define XREDIS_POOL_H
#include <xredis/client.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_POOL requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispool xredispool;
typedef struct xredislease xredislease;
typedef enum xredispoolstate { XREDIS_POOL_OPEN, XREDIS_POOL_CLOSING, XREDIS_POOL_CLOSED } xredispoolstate;
typedef struct xredispoolconfig {
    size_t Size;
    /* Includes concurrent acquire constructors and queued acquisitions. */
    size_t MaxPending;
    size_t MaxRequests;
    size_t MaxCommandBytes;
    /* Finite maximum lease hold, including calls and application think time.
     * Default 30 s, hard maximum 24 hours. No renewal on calls. */
    int64 LeaseTimeout;
    /* Graceful Return seals admission and drains, default 5 s, max 60 s.
     * Expiry retires a still-busy connection. */
    int64 ReturnTimeout;
    size_t WorkItems;
} xredispoolconfig;
typedef struct xredispoolinfo {
    size_t Size;
    xredispoolstate State;
    size_t Members, Live, Idle, Leased, Pending;
    uint64 Requests, CommandBytes, Acquired, Rejected, Returned, Retired;
} xredispoolinfo;
/* Borrowed from the acquisition Future. Only OK has a Lease. Retain it before
 * dropping that Future. Failed acquisition performs no Redis command. */
typedef struct xredisleaseresult {
    xredisrequeststatus Status;
    xredislease* Lease;
} xredisleaseresult;
typedef struct xredisleaseclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xredisleaseclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config);
/* Fixed membership, 1..65536 distinct OPEN/ready/physically idle managed
 * clients with the same negotiated RESP version on this running Engine.
 * The caller supplies a common service/credentials/database/session profile;
 * the pool cannot infer erased credentials or server-side profile changes.
 * Existing numeric/DNS/TLS factories
 * supply members. Success holds refs and exclusively claims them; caller refs
 * are borrowed, never consumed. Failure rolls back every partial claim and
 * leaves output unchanged. Busy members return OVERLOADED. No connection is
 * created/reconnected/replayed by this core. PoolClose closes its members.
 * Do not use member clients directly while pooled; Close observers and an
 * external close remain valid. Count/argument storage is borrowed for Create.
 * Limits are hard across all members, including request constructor/wire/
 * timer/cancel tails. Result/reply storage retained by callers remains theirs.
 * Each native parser/push/work limit also remains in force. */
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool);
XREDIS_API bool xredisPoolRetain(xredispool* Pool);
/* Caller must Close then Release; Release alone does not close a live pool. */
XREDIS_API void xredisPoolRelease(xredispool* Pool);
/* Bounded FIFO admission. Timeout (-1 forever, 0 due) includes allocation,
 * queue wait and grant. A separate finite lease hold starts at grant. Cancel
 * requests stop; every accepted acquisition resolves a typed immutable result.
 * No live member yields DISCONNECTED for fixed membership; optional automatic
 * maintenance instead waits within the original deadline through outages.
 * Output changes only on ACCEPTED.
 * Sync rejects every worker of the same engine. */
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info);
XREDIS_API bool xredisPoolClose(xredispool* Pool);
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool);
XREDIS_API bool xredisLeaseRetain(xredislease* Lease);
XREDIS_API void xredisLeaseRelease(xredislease* Lease);
/* Direct native request/Future, with no second per-call Future or wire clone.
 * Ordinary one-reply and blocking commands use the same FIFO and physical
 * obligations as the client. Effective deadline is min(call deadline, lease
 * hold deadline). Connection/mode/transaction controls require their own APIs.
 * Local cancel/timeout does not release a sent request's FIFO obligation. */
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent, nonblocking seal. Return drains within ReturnTimeout and the
 * original hold deadline; Abort retires the connection. Releasing a handle
 * alone does not return it; finite hold expiry recovers dropped observers.
 * Closed resolves only after API constructors and native request storage
 * retire. Reusable certifies the slot-release boundary; it does not promise
 * another caller has not acquired it since. Completed retained metadata pins
 * no client/pool/engine. Join the engine before unloading callback code. */
XREDIS_API bool xredisLeaseReturn(xredislease* Lease);
XREDIS_API bool xredisLeaseAbort(xredislease* Lease);
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease);
XRT_EXTERN_C_END
#endif
#endif
