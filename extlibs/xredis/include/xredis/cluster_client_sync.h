#ifndef XREDIS_CLUSTER_CLIENT_SYNC_H
#define XREDIS_CLUSTER_CLIENT_SYNC_H
#include <xredis/cluster_client.h>
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC) && !defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#error "Synchronous Cluster calls require the asynchronous client"
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT_SYNC)
/* ACCEPTED transfers Future even when the native wait fails. Wait is the actual
 * XRT wait result, not a Redis operation status. Inspect the Future's immutable
 * xredisclusterclientresult after XWAIT_OK. Destroy Future exactly once. */
typedef struct xredisclusterwait {xwaitresult Wait;xfuture* Future;} xredisclusterwait;
XRT_EXTERN_C_BEGIN
/* Blocking explicit-slot facade. Uses the existing async operation and one
 * function-entry deadline, including validation and worker checks. No second
 * relative timeout, thread, timer, command copy or result allocation is added.
 * All workers of either progress Engine or Nodes Engine are refused before
 * admission (ERROR/XERR_STATE), including workers other than the selected one.
 * An unrelated Engine worker may call, subject to application dependencies.
 * Only ACCEPTED changes Result. Refusals preserve it and all input ownership.
 * ACCEPTED means admitted, not successful Redis execution. Result.Wait records
 * platform wait failure; in that case Future may still be pending, and caller
 * retains it for later wait/cancel/destruction. Do not silently retry business.
 * Timeout/default/zero and resource/status semantics match Call. Close from
 * another thread can interrupt the operation. Local timeout/close does not join
 * outstanding native FIFO duties; Nodes/Engines retain physical ownership.
 * Result must be disjoint from Client/current map/input descriptors and bytes.
 * Keep a stable Client owner through return. No external cancellation token is
 * introduced; use asynchronous Call for independently cancellable submissions. */
XREDIS_API xredisadmission xredisClusterClientCallSync(xredisclusterclient* Client,
    uint16 Slot,const xbytesview* Arguments,size_t Count,int64 Timeout,
    xredisclusterwait* Result);
XRT_EXTERN_C_END
#endif
#endif
