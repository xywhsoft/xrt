#ifndef XREDIS_POOL_SHARED_CONNECT_H
#define XREDIS_POOL_SHARED_CONNECT_H
#include <xredis/pool_shared.h>
#include <xredis/pool_connect.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT) && \
    (!defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT))
#error "Automatic shared pools require shared pools and native connection maintenance"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
XRT_EXTERN_C_BEGIN
/* Starts/refills Count shared slots using the same factory, bounded attempts,
 * readiness/profile checks, backoff, physical generation retirement, timer-OOM
 * failure and exactly-once Data/Drop contract as PoolConnect. No factory runs
 * inline. ACCEPTED consumes Data/Drop; failure preserves it and output.
 * Use SharedPoolCall[Async], PoolConnectInfo and ordinary PoolInfo/Close/Closed/
 * Retain/Release. Native request/Future/selection costs match fixed shared pools.
 * Ready members admit immediately; an outage or a saturated bounded probe
 * returns OVERLOADED with output untouched. There is NO outage wait queue or
 * automatic retry of accepted business requests. The call timeout does not
 * wait for maintenance. Acquisition APIs reject shared mode. Old logical stops
 * keep native FIFO/quota obligations; replacements wait for physical retirement.
 * Caller factory/context/code remain valid until Drop. Full Cluster topology,
 * address policy, redirects and failover selection are separate operations. */
XREDIS_API xredisadmission xredisSharedPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result);
XRT_EXTERN_C_END
#endif
#endif
