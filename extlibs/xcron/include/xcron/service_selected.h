#ifndef XCRON_SERVICE_SELECTED_H
#define XCRON_SERVICE_SELECTED_H
#include <xcron/service.h>
#include <xcron/executor_selected.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED) && (!defined(XCRON_FEATURE_XCRON_SERVICE) || !defined(XCRON_FEATURE_XCRON_EXECUTOR_SELECTED))
#error "selected service requires service and selected execution"
#endif
#if defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED)
typedef struct xcronselectedserviceconfig {
    xcronserviceconfig Service;uint32 SubmissionBatchLimit,Reserved;
} xcronselectedserviceconfig;
typedef struct xcronselectedreply {
    xcronrequestid Request;xcronrequestkind Kind;xcronserviceerror Code;
    xcronjobid Job;ptr Data;bool Found;
    xcronselectedjobinfo Info;xcronselectedretired Retired;
    xcronschedulerissue Scheduler;xcronexecutorissue Executor;xerror* Error;
} xcronselectedreply;
typedef struct xcronselectedservicecompletion {
    xcronservicecompletion Completion;xdatetime Local;size_t Candidates;
} xcronselectedservicecompletion;
typedef struct xcronselectedservicestats {
    xcronservicestats Service;xcronselectedpoll LastPoll;
    uint64 BatchSubmissions,BatchedAttempts;
} xcronselectedservicestats;
XRT_EXTERN_C_BEGIN
/* Optional mode of the same owner thread/event waiter, tickets, output credits
 * and retry heap. SubmissionBatchLimit is1..EXECUTOR_BATCH_MAX; Reserved=0.
 * It caps atomic native groups of ALREADY-ready leases, never a fill wait. Every
 * group member counts one attempt; AGAIN enters the per-lease retry heap and is
 * retried individually under its original deadline/attempt cap. A native atomic
 * group must fit one worker queue, even if scalar calls could use aggregate
 * capacity. Batch rejection transfers no physical execution duties. Callback
 * uses ExecutionSelected for its immutable nominal view. Ordinary service config
 * and retained-byte/startup/borrowed-lifetime/stop contracts apply.
 * Five global search budgets each equal Scheduler.WorkLimit per owner poll.
 * Use Selected Add/Collect/Drain/ReapStopped/Inspect on this mode. CancelAsync,
 * InfoAsync, ReapAsync, Close/WaitFor/Failure/Wake/Destroy work in both modes. */
XCRON_API bool xcronServiceCreateSelected(const xcronselectedserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
/* Copies Policy/config and retains the command Zone on queue acceptance only.
 * Owner admission does no recurrence traversal: successful reply publishes an
 * admitted SEARCHING/EXHAUSTED job. Initial or later fold/missing/UNKNOWN faults
 * appear in Info/Retired.LastIssue and keep Data borrowed through retirement.
 * Failed ADD returns original Data through exactly one collected reply. Queue
 * acceptance, job admission, physical execution and retirement remain distinct.
 * No gap compensation or durable recovery is implied by fold/missing selection. */
XCRON_API bool xcronServiceAddSelectedAsync(xcronservice* Service,const xcronjobconfig* Config,xcrondeliverypolicy Policy,xcronrequestid* Request,xcronserviceissue* Issue);
/* Same total timeout, generation, Error/Data ownership and owner/borrowed-pool
 * self-wait rules as Collect; preserves complete selected Info/Retired fields. */
XCRON_API xwaitresult xcronServiceCollectSelected(xcronservice* Service,xcronrequestid Request,int64 Timeout,xcronselectedreply* Reply,xcronserviceissue* Issue);
/* Copies nominal metadata with every reserved physical or unsent outcome,
 * including cancellation, rejected/expired admission and queued native cancel.
 * Contains no borrowed job Data; metadata remains valid after separate Reap.
 * Each Completion.Error transfers to the caller for xrtErrorFree. */
XCRON_API bool xcronServiceDrainSelected(xcronservice* Service,xcronselectedservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue);
XCRON_API bool xcronServiceReapStoppedSelected(xcronservice* Service,xcronselectedretired* Retired,bool* Found,xcronserviceissue* Issue);
/* Complete five-budget historical poll. Service.LastPoll is zero in this mode;
 * the actual-only three-budget view is not a truncated selected poll. Batch
 * counters count native calls with>1 lease and their per-lease initial attempts;
 * retries are individual. Other snapshot/byte/timer contracts match Inspect. */
XCRON_API bool xcronServiceInspectSelected(xcronservice* Service,xcronselectedservicestats* Stats,xcronserviceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
