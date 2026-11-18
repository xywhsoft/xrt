#ifndef XMQTT_JOURNAL_WORKER_H
#define XMQTT_JOURNAL_WORKER_H
#include <xmqtt/journal.h>
#include <xrt/task.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER) && (!defined(XMQTT_FEATURE_XMQTT_JOURNAL) || !defined(XRT_FEATURE_TASK_POOL) || !defined(XRT_FEATURE_COND))
#error "MQTT journal worker requires journal, host task pool and condition variables"
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
typedef struct xmqttjournalworker xmqttjournalworker;
typedef enum xmqttjournaladmission {XMQTT_JWORK_ERROR=-1,XMQTT_JWORK_ACCEPTED=0,XMQTT_JWORK_OVERLOADED,XMQTT_JWORK_CLOSED} xmqttjournaladmission;
typedef enum xmqttcommitstatus {XMQTT_COMMIT_OK=0,XMQTT_COMMIT_TIMEOUT,XMQTT_COMMIT_CANCELLED,XMQTT_COMMIT_CLOSED,XMQTT_COMMIT_LIMIT,XMQTT_COMMIT_STORAGE,XMQTT_COMMIT_INITIALIZATION,XMQTT_COMMIT_INVALID} xmqttcommitstatus;
typedef struct xmqttjournalworkerconfig {size_t Size,MaxRequests,MaxObjects,MaxBytes,MaxTasks,MaxTransactionBytes,WorkTransactions;} xmqttjournalworkerconfig;
typedef struct xmqttcommitresult {
    xmqttcommitstatus Status;xmqttjournalstatus AppendStatus;xmqttjournaleffect Effect;
    uint64 Sequence,AttemptSequence;bool AppendCalled;xbytesview Data;const xerror* Cause;
} xmqttcommitresult;
typedef struct xmqttjournalworkerinfo {
    size_t Size;bool Closed,Failed,Busy,Scheduling;size_t Requests,Queued,Objects,Bytes,BaseBytes,Tasks;
    uint64 Accepted,Rejected,Completed;xmqttjournalinfo Journal;
} xmqttjournalworkerinfo;
XRT_EXTERN_C_BEGIN
/* Defaults:128 outstanding admissions,256 retained objects,4MiB requested
 * extension memory,2 native pump tasks (including their retirement tails),
 * 1MiB transaction,32 admissions per pump turn. Zero limits are real caps. Journal allocation,
 * opaque Core Promise/token/task allocations and diagnostics are additional;
 * their counts are bounded by Objects/Tasks and the borrowed host pool. */
XMQTT_API bool xmqttJournalWorkerConfigInit(xmqttjournalworkerconfig* Config);
/* Success consumes exclusive ownership of a READY Journal, failure none.
 * Direct alias operations on that Journal are refused after transfer. No file
 * IO here. Borrow Pool through Destroy. Keep a borrowed Journal file/provider
 * live through Destroy. An owned, locked Journal closes its file at Destroy
 * after the worker's physical I/O and task tails have retired.
 * No thread/pool/shared runtime is created. Output changes only on success. */
XMQTT_API bool xmqttJournalWorkerCreate(xtaskpool* Pool,xmqttjournal* Journal,
    const xmqttjournalworkerconfig* Config,xmqttjournalworker** Worker);
/* Thread-safe, immediate bounded admission; immutable input copied once.
 * Payload is opaque (including empty); use the typed delta codec separately.
 * Queue order is successful admission order. One host pool task serially
 * appends; no IO or sync in the submitting thread. Timeout starts at API entry,
 * -1 means no deadline; cancellation is sampled before/after append. Pending
 * cancellation/expiry/Close prevents append. An active native call cannot be
 * interrupted: result waits for physical completion and retains actual Effect.
 * TIMEOUT/CANCELLED/CLOSED may therefore accompany COMMITTED. Local ERROR after
 * both syncs also preserves COMMITTED/Sequence. Any storage/local initialization
 * failure stops further writes; destroy/reopen/recover before retrying. LIMIT
 * does not poison. No automatic replay, MQTT ACK, semantic delta application,
 * namespace publication, group commit or durable delivery lease is performed.
 * All accepted Futures resolve to this typed immutable result. Cause may be
 * NULL after native error-slot failure. No stale caller diagnostic becomes Cause.
 * Data and its object/byte credit survive through final Future destruction,
 * including synchronous completion-watch callbacks. Dropping Future does not
 * cancel accepted work. Explicit Future/raw token cancellation is cooperative.
 * Future output changes only on ACCEPTED and cannot alias any owned allocation.
 * Scheduling/retirement contention may return normal OVERLOADED. Host pool
 * rejection returns ERROR; ownership/output remain caller's. A pump accepts
 * at most WorkTransactions across its entire turn, including replenishment.
 * Turn exhaustion returns OVERLOADED until admitted work drains and a fresh
 * pump is queued at the host tail. This bounds monopolization by a hot owner;
 * no automatic retry, fill wait or extra timer is introduced. Diagnostic handlers
 * and completion watches may reenter Info/Close/Append; Wait/Destroy reject
 * self calls. Completion normally runs on the host pool; cancellation of a
 * queued native task may complete on the pool-cancelling thread. */
XMQTT_API xmqttjournaladmission xmqttJournalWorkerAppendAsync(xmqttjournalworker* Worker,
    xbytesview Data,int64 Timeout,xfuture** Future);
/* Snapshot contains last physically completed Journal state; while Busy, a
 * newer in-flight commit may already have effects. Completed results remain
 * charged to Objects/Bytes. Native task tails are reaped by Info/admission/Wait.
 * Outputs may not alias any worker-owned allocation or retained result/Data. */
XMQTT_API bool xmqttJournalWorkerInfo(xmqttjournalworker* Worker,xmqttjournalworkerinfo* Info);
/* Idempotent stop admission. Queued requests finish CLOSED without IO; active
 * append finishes with its actual effect. Does not interrupt/close the file. */
XMQTT_API bool xmqttJournalWorkerClose(xmqttjournalworker* Worker);
/* Physical extension task-data retirement, not just Future logical completion.
 * Requires Close first; -1 infinite. Rejects any same-pool worker and own
 * callback/admission reentry. IO backends may block: timeout does not release
 * native duties. Retained result observers do not block retirement. */
XMQTT_API xwaitresult xmqttJournalWorkerWaitFor(xmqttjournalworker* Worker,int64 Timeout);
/* Close+infinite Wait, destroy transferred Journal, release creator reference.
 * Serialize Destroy with other API calls. Retained results keep only the inert
 * credit ledger/immutable payload alive, never Journal/file/pool ownership.
 * Keep extension/Core code resident through all result destructors and native
 * tails; host pool shutdown/join still belongs to the host. */
XMQTT_API bool xmqttJournalWorkerDestroy(xmqttjournalworker* Worker);
XRT_EXTERN_C_END
#endif
#endif
