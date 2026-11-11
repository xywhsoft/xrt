#ifndef XREDIS_POOL_SHARED_ASKING_H
#define XREDIS_POOL_SHARED_ASKING_H
#include <xredis/pool_shared.h>
#include <xredis/pool_asking.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING) && \
    (!defined(XREDIS_FEATURE_XREDIS_POOL_SHARED) || !defined(XREDIS_FEATURE_XREDIS_POOL_ASKING))
#error "Shared ASKING requires shared pools and native ASKING groups"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
XRT_EXTERN_C_BEGIN
/* Native adjacent ASKING/query execution on fixed or maintained shared pools.
 * Returns ONE native request/Future borrowing an xredisaskingresult; no lease,
 * proxy Future or extra payload clone. Other producers cannot interleave
 * between this group's commands on its selected connection. Both commands are
 * pipelined: Query MAY EXECUTE even when Control is SERVER_ERROR. Inspect both
 * outcomes. No accepted query is retried/replayed, routed or refreshed here.
 * SharedPoolCall selection, bounded probes, immediate overload, entry deadline,
 * quota and close rules apply. Native ASKING command restrictions, two-reply
 * FIFO, immutable prefix knowledge, partial-wire completion and sent-stop drain
 * rules apply. Group prefix/aligned state count toward native and pool quotas.
 * A stopped group cannot leak its pending one-shot flag to following ordinary
 * calls; full wire/replies drain or the native member retires. Maintenance may
 * replace retired members but never replays the group. Completed outcomes pin
 * neither pool nor engine. Caller retains Pool across each API. No lease hold
 * applies. Output changes only on ACCEPTED; sync refuses every engine worker. */
XREDIS_API xredisadmission xredisSharedPoolAskingCallAsync(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisSharedPoolAskingCall(xredispool* Pool,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
