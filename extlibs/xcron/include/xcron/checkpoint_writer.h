#ifndef XCRON_CHECKPOINT_WRITER_H
#define XCRON_CHECKPOINT_WRITER_H
#include <xcron/checkpoint_group.h>
#include <xrt/executor.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER) && (!defined(XCRON_FEATURE_XCRON_CHECKPOINT_GROUP) || !defined(XRT_FEATURE_EXECUTOR))
#error "cron checkpoint writer requires the checkpoint group and XRT executor"
#endif
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
typedef struct xcroncheckpointwriter xcroncheckpointwriter;
typedef uint64 xcroncheckpointwriteticket;
typedef struct xcroncheckpointwriterreply {
    xcroncheckpointstorestatus Status;
    uint64 Generation;
    xerror* Error;
} xcroncheckpointwriterreply;

XRT_EXTERN_C_BEGIN
/* Success consumes one open group handle. Failure leaves Store owned by the
 * caller and Writer untouched. Read any startup image before transferring the
 * store. The host keeps Executor alive through WriterDestroy. Use an executor
 * for storage work if job dispatch latency must be independent of disk I/O. */
XCRON_API bool xcronCheckpointWriterCreate(xcroncheckpointgroup* Store,xexecutor* Executor,xcroncheckpointwriter** Writer);
/* Admit one exclusive commit through collection. No record copy or allocation
 * occurs on successful admission. A rejected submission leaves Ticket and
 * input ownership intact. Keep Records readable and immutable until XWAIT_OK
 * from Collect, including if the executor is closed or cancelled. The worker
 * checks record content, capacity and generation before writing. */
XCRON_API bool xcronCheckpointWriterSubmit(xcroncheckpointwriter* Writer,uint64 ExpectedGeneration,
    const xcroncheckpoint* Records,uint32 Count,xcroncheckpointwriteticket* Ticket);
/* Timeout is relative milliseconds, zero to poll or -1 to wait indefinitely.
 * There is one successful collector per ticket. Only XWAIT_OK writes Reply
 * and releases Records. Error, if present, transfers to the collector. A
 * queued executor cancellation reports CANCELLED. A running commit reports
 * the native store result. Generation is set only for OK. Blocking on the
 * writer from its executor is refused. All outputs must be disjoint from the
 * writer, store, executor and borrowed input. */
XCRON_API xwaitresult xcronCheckpointWriterCollect(xcroncheckpointwriter* Writer,xcroncheckpointwriteticket Ticket,
    int64 Timeout,xcroncheckpointwriterreply* Reply);
/* Caller serializes destruction with all public calls. BUSY preserves a writer
 * with an uncollected ticket. Otherwise consumes Writer and closes its store,
 * returning the native close status. The executor remains host-owned. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointWriterDestroy(xcroncheckpointwriter* Writer);
XRT_EXTERN_C_END
#endif
#endif
