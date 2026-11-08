#ifndef XCRON_EXECUTOR_SELECTED_H
#define XCRON_EXECUTOR_SELECTED_H
#include <xcron/executor.h>
#include <xcron/scheduler_selected.h>
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED) && (!defined(XCRON_FEATURE_XCRON_EXECUTOR) || !defined(XCRON_FEATURE_XCRON_SCHEDULER_SELECTED))
#error "selected executor requires executor and selected scheduling"
#endif
#if defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED)
typedef struct xcronexecutionlabel {xdatetime Local;size_t Candidates;} xcronexecutionlabel;
typedef struct xcronselectedcompletion {
    xcronselectedrun Run;xcronrunresult Result;xcronexecutorerror Code;xerror* Error;
} xcronselectedcompletion;
XRT_EXTERN_C_BEGIN
/* Uses the existing callback/config and owner, rejection, cancellation, Notify
 * tail and borrowed-executor contracts. One allocation includes immutable label
 * storage for every RunLimit slot, within BytesLimit. Ordinary adapters allocate
 * no label array. SubmitSelected/DrainSelected must be used for this mode; plain
 * Submit/Drain reject it atomically. Cancel/CancelJob/Close/Inspect/Destroy work
 * with both modes. No new timer, worker, registry or runtime is introduced. */
XCRON_API bool xcronExecutorCreateSelected(const xcronexecutorconfig* Config,xcronexecutor** Adapter,xcronexecutorissue* Issue);
/* Run is a lease from one originating selected scheduler. Copies the entire
 * ordinary run and its canonical floating Local/global Candidates before any
 * worker can execute. Success transfers only the completion duty; failure
 * transfers nothing and invokes no Proc. Caller may reuse Run storage after
 * return. Callback Data retains the scheduler's existing borrowed lifetime. */
XCRON_API bool xcronExecutorSubmitSelected(xcronexecutor* Adapter,const xcronselectedrun* Run,xcronexecutorissue* Issue);
/* Selected equivalent of SubmitBatch:1..BATCH_MAX leases, copied nominal
 * snapshots before any worker dispatch, all-or-none native acceptance, no
 * waiting to fill the batch and an independent physical receipt per lease.
 * Input must be disjoint from adapter storage. Failure transfers no duties;
 * the caller may retry/scalar-submit or Complete all rejected leases. */
XCRON_API bool xcronExecutorSubmitBatchSelected(xcronexecutor* Adapter,const xcronselectedrun* Runs,size_t Count,xcronexecutorissue* Issue);
/* Same bounded detached-batch/owner behavior as Drain. Copies the selected lease
 * metadata, even when native queued cancellation skipped Proc, before slot reuse.
 * Each Error transfers to the caller, which frees it with xrtErrorFree. Complete
 * the embedded ordinary run exactly once after receiving its physical receipt.
 * Adapter lifetime still extends through all outstanding Notify/publication tails. */
XCRON_API bool xcronExecutorDrainSelected(xcronexecutor* Adapter,xcronselectedcompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue);
/* Borrowed immutable view only during Proc, like Run and Execution. NULL for a
 * plain adapter or NULL token. No heap, lock, diagnostic, retention or copy.
 * Never retain/escape this pointer or token; it becomes invalid when Proc returns.
 * Use the completion's copied metadata after return. A callback's run and label
 * belong to the same slot, including after earlier slots have been reused. */
XCRON_API const xcronexecutionlabel* xcronExecutionSelected(const xcronexecution* Execution);
XRT_EXTERN_C_END
#endif
#endif
