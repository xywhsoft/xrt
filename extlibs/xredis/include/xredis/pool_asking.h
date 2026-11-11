#ifndef XREDIS_POOL_ASKING_H
#define XREDIS_POOL_ASKING_H
#include <xredis/pool.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "Pooled ASKING requires native pools"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
/* Both outcomes borrow their Future. Query is the ordinary result prefix.
 * ControlReceived/Control are immutable snapshots at logical completion, even
 * when a sent cancellation/timeout still has physical FIFO obligations. No
 * result retains the lease/client/engine; retain the Future for escaping views. */
typedef struct xredisaskingresult {xredisrequestresult Query,Control;bool ControlReceived;} xredisaskingresult;
XRT_EXTERN_C_BEGIN
/* Copies ASKING + one ordinary command into ONE adjacent native wire group,
 * request and Future, with no extra payload copy/Future/controller. Producers
 * sharing this exclusive lease cannot interleave between the two commands.
 * Both commands are pipelined: Query MAY EXECUTE even if Control is SERVER_ERROR.
 * Inspect both outcomes; a control error never authorizes replay of the query.
 * No address selection, redirect following, reconnect or business replay occurs.
 * Managed ready pooled clients only. Known connection/transaction/subscription
 * controls, CLIENT, QUIT, READONLY/READWRITE and ASKING are refused as queries.
 * Inputs borrow only during admission. MaxCommandBytes/Total and pool quotas
 * include the prefix and aligned private state. Timeout starts at API entry,
 * covers construction/both replies, and is capped by the original lease hold.
 * Sent cancel/timeout conservatively reports MAY_HAVE_EXECUTED even when only
 * the prefix may have gone out. Native FIFO drains both replies, with no reuse
 * until physical retirement; drain expiry/disconnect/protocol errors retire the
 * connection, so a pending one-shot ASKING flag cannot escape to a later lease.
 * Control must reply simple OK or a server error; malformed control fails the
 * connection. A validated Query reply has ordinary OK/SERVER_ERROR semantics.
 * Reply/limits/OOM/push/TLS bounds remain native. Return/Abort the lease normally.
 * Output changes only on ACCEPTED; sync rejects every worker of the same Engine. */
XREDIS_API xredisadmission xredisLeaseAskingCallAsync(xredislease* Lease,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisLeaseAskingCall(xredislease* Lease,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
