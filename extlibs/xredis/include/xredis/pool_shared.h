#ifndef XREDIS_POOL_SHARED_H
#define XREDIS_POOL_SHARED_H
#include <xredis/pool.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "Shared pools require native pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
XRT_EXTERN_C_BEGIN
/* Fixed ready managed members, same ownership/profile/rollback rules as
 * PoolCreate. Uses the native pool handle and PoolInfo/Close/Closed/Retain/
 * Release. Exclusively claims members, but multiplexes ordinary calls without
 * allocating a lease. Acquire APIs reject this mode. Public config validation
 * is unchanged; MaxPending/LeaseTimeout/ReturnTimeout do not govern shared
 * calls. There is no acquisition wait queue, reconnect or business replay.
 * PoolInfo.Idle counts dispatch-eligible members, including busy ones; Leased,
 * Pending, Acquired and Returned stay zero. Requests/CommandBytes retain hard
 * native constructor/wire/timer/cancel-tail accounting. */
XREDIS_API xredisadmission xredisSharedPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool);
/* One native request/Future. Round-robin selection probes at most
 * min(dispatch-eligible members, WorkItems) candidates. Only nonaccepted
 * admissions can try another member; accepted calls never move or replay.
 * No ready/capacity candidate returns OVERLOADED without changing output.
 * Timeout starts at API entry, including selection and constructor work.
 * Ordinary client restrictions, limits, FIFO and sent-stop drain apply.
 * A blocking command also blocks later replies on its selected connection;
 * isolate blocking traffic in a separate pool. Known connection/mode/session
 * controls require exclusive APIs and are refused here. No cross-connection
 * order or key affinity is promised. Caller retains Pool across each API.
 * Result storage outlives the pool/engine. Sync rejects every engine worker. */
XREDIS_API xredisadmission xredisSharedPoolCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisSharedPoolCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
