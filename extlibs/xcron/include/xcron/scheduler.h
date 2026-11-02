#ifndef XCRON_SCHEDULER_H
#define XCRON_SCHEDULER_H
#include <xcron/features.h>
#include <xcron/zoned.h>
#if defined(XCRON_FEATURE_XCRON_SCHEDULER) && !defined(XCRON_FEATURE_XCRON_ZONED)
#error "cron scheduler requires zoned calculation"
#endif
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
typedef struct xcronscheduler xcronscheduler;
typedef uint64 xcronjobid;
typedef uint64 xcronrunid;
typedef enum xcronmisfire {XCRON_MISFIRE_CATCH_UP=0,XCRON_MISFIRE_SKIP,XCRON_MISFIRE_COALESCE} xcronmisfire;
typedef enum xcronoverlap {XCRON_OVERLAP_WAIT=0,XCRON_OVERLAP_ALLOW,XCRON_OVERLAP_SKIP} xcronoverlap;
typedef enum xcronjobstate {XCRON_JOB_SCHEDULED=0,XCRON_JOB_EXHAUSTED,XCRON_JOB_CANCELLED,XCRON_JOB_FAULTED} xcronjobstate;
typedef enum xcronrunresult {XCRON_RUN_SUCCEEDED=0,XCRON_RUN_FAILED,XCRON_RUN_CANCELLED} xcronrunresult;
typedef enum xcronschedulererror {
    XCRON_SCHEDULER_OK=0,XCRON_SCHEDULER_ARGUMENT=4001,XCRON_SCHEDULER_LIMIT,
    XCRON_SCHEDULER_MEMORY,XCRON_SCHEDULER_CLOSED,XCRON_SCHEDULER_STALE,
    XCRON_SCHEDULER_BUSY,XCRON_SCHEDULER_CLOCK,XCRON_SCHEDULER_CALCULATION
} xcronschedulererror;
typedef struct xcronschedulerissue {xcronschedulererror Code;xcronzonedissue Zoned;} xcronschedulerissue;
typedef struct xcronschedulerconfig {
    uint32 JobLimit,RunLimit,WorkLimit,Reserved;
    size_t BytesLimit;
} xcronschedulerconfig;
typedef struct xcronclock {xtime Wall;uint64 MonotonicMilliseconds;} xcronclock;
typedef struct xcronjobconfig {
    xcronexpression Expression;
    xcronzone* Zone;
    xtime Begin,End;
    uint32 SearchFlags;
    xcronmisfire Misfire;
    xcronoverlap Overlap;
    uint32 RunningLimit;
    uint64 GraceMilliseconds;
    ptr Data;
} xcronjobconfig;
typedef struct xcronjobinfo {
    xcronjobstate State;
    uint32 Running;
    bool HasNext;
    xtime Next;
    uint64 Dispatched,Succeeded,Failed,Cancelled,SkippedRanges,CoalescedRanges;
    xcronzonedissue LastIssue;
} xcronjobinfo;
#define XCRON_RUN_COALESCED UINT32_C(1)
typedef struct xcronrun {
    xcronrunid Id;
    xcronjobid Job;
    ptr Data;
    xtime ScheduledAt,Through;
    xcronclock Observed;
    uint32 Flags;
} xcronrun;
typedef struct xcronpoll {
    size_t Count;
    uint32 Work,Faults,SkippedRanges;
    bool More;
} xcronpoll;
typedef struct xcronwake {
    uint64 MonotonicMilliseconds,DelayMilliseconds;
    bool Immediate,WaitingForCompletion;
} xcronwake;
typedef struct xcronretired {xcronjobid Id;ptr Data;xcronjobinfo Info;} xcronretired;
typedef struct xcronschedulerstats {
    size_t Bytes;
    uint32 Jobs,Running,Heap,Ready,Blocked,Retired,PeakJobs,PeakRunning;
    bool Closed,HasClock;
    xcronclock LastClock;
    uint64 Dispatched,Completed,SkippedRanges,CoalescedRanges,Faults,ClockSamples,WallRegressions;
} xcronschedulerstats;
XRT_EXTERN_C_BEGIN
/* Single externally serialized owner; no timer, worker, callback or storage is
 * created. Limits must be nonzero, Reserved zero. One allocation preallocates
 * all job/run slots, heap and queues within BytesLimit (excluding retained zones
 * and XRT diagnostics). Normal operations and recorded job faults allocate none.
 * IDs are scoped to this scheduler. Job/run IDs have a generation; exhausted
 * generations use all bits not needed by the configured slot index, then retire
 * rather than wrap. Never use an ID with another instance.
 * Invalid argument/spans/aliases preserve every output. Other failures publish
 * typed Issue despite diagnostic OOM, preserving other outputs. Success preserves
 * ambient error. Writable outputs must be disjoint from scheduler-owned storage,
 * each other and input structs. The caller also keeps them disjoint from borrowed
 * job Data and retained immutable zones. Error domain: xrt.cron.scheduler. */
XCRON_API bool xcronSchedulerCreate(const xcronschedulerconfig* Config,xcronscheduler** Scheduler,xcronschedulerissue* Issue);
/* Copies Expression and retains Zone on success only; borrows Data until Reap.
 * Begin/End and SearchFlags have ZonedNext semantics. An empty window result is
 * a successful EXHAUSTED job awaiting Reap; a calculation error admits nothing.
 * WAIT/SKIP require RunningLimit=1; ALLOW uses its explicit limit <=RunLimit.
 * SKIP overlap discards due work through the wall sample when evaluated while
 * a lease is live. WAIT/ALLOW preserve backlog while at their running limit.
 * Misfires are due instants strictly older than Wall-GraceMilliseconds (saturates
 * at INT64_MIN). SKIP seeks at that cutoff; COALESCE emits the oldest due instant
 * with Through=min(Wall,End), then seeks strictly after Through; CATCH_UP keeps
 * every actual instant. Range counters do not pretend to count lost occurrences.
 * Actual folds all participate; missing labels never fire. No nominal compensation. */
XCRON_API bool xcronSchedulerAdd(xcronscheduler* Scheduler,const xcronjobconfig* Config,xcronjobid* Id,xcronschedulerissue* Issue);
/* Samples must have nondecreasing MonotonicMilliseconds; wall rollback is legal.
 * Already consumed cursors never rewind. Capacity must be positive. Pulls at most
 * Capacity leases with at most WorkLimit queue/heap steps; calculation inside one
 * step may traverse a full Gregorian cycle/history. Round-robin ready jobs and
 * alternating heap promotion prevent a catch-up job monopolizing ready work.
 * A lease is running immediately upon publication, including executor queue wait.
 * The caller must eventually Complete every lease, even if executor submission
 * fails. A future calculation fault terminates that job, but previously published
 * leases remain valid; Poll succeeds, records typed LastIssue and reports Faults.
 * More requests another poll at the same sample if output/work budgets stopped
 * immediately actionable work. All outputs are caller storage; none are retained. */
XCRON_API bool xcronSchedulerPoll(xcronscheduler* Scheduler,xcronclock Now,xcronrun* Runs,size_t Capacity,xcronpoll* Poll,xcronschedulerissue* Issue);
/* Completion means the physical body and all its data-using tails have stopped.
 * It is exactly once; stale/duplicate IDs fail without changing state.
 * Worker threads must deliver completions to the serialized scheduler owner.
 * Execution outcomes are counted; they do not implicitly stop future triggers. */
XCRON_API bool xcronSchedulerComplete(xcronscheduler* Scheduler,xcronrunid Id,xcronrunresult Result,xcronschedulerissue* Issue);
/* Cancels future dispatch only. Live leases remain valid and retain job storage.
 * Cancel is idempotent while the job exists; an already terminal reason remains. */
XCRON_API bool xcronSchedulerCancel(xcronscheduler* Scheduler,xcronjobid Id,xcronschedulerissue* Issue);
XCRON_API bool xcronSchedulerJob(const xcronscheduler* Scheduler,xcronjobid Id,xcronjobinfo* Info,xcronschedulerissue* Issue);
/* Successful empty reap sets Found=false and preserves Retired. Only terminal
 * jobs with zero running leases can be reaped; returns original borrowed Data,
 * releases the scheduler's zone reference, invalidates the ID and frees the slot.
 * Caller may destroy Data after this receipt. No destruction callbacks run here. */
XCRON_API bool xcronSchedulerReap(xcronscheduler* Scheduler,xcronretired* Retired,bool* Found,xcronschedulerissue* Issue);
/* O(JobLimit), idempotent: rejects Add and cancels scheduled jobs; does not join
 * bodies or discard completions/receipts. Reap all jobs before Destroy. */
XCRON_API bool xcronSchedulerClose(xcronscheduler* Scheduler,xcronschedulerissue* Issue);
/* Pure projection from explicit sample, which must not precede the last Poll's
 * monotonic sample. MaxWaitMilliseconds must be positive: bounds wall resampling
 * even while idle/blocked. Addition saturates at UINT64_MAX. WaitingForCompletion
 * tells an adapter to also wake on Complete, Add, Cancel or Close. No busy timer
 * loop for jobs blocked by running limits; Immediate means useful owner work. */
XCRON_API bool xcronSchedulerWake(const xcronscheduler* Scheduler,xcronclock Now,uint64 MaxWaitMilliseconds,xcronwake* Wake,xcronschedulerissue* Issue);
XCRON_API bool xcronSchedulerInspect(const xcronscheduler* Scheduler,xcronschedulerstats* Stats,xcronschedulerissue* Issue);
/* Requires zero jobs and leases. BUSY leaves the scheduler live and unchanged. */
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* Scheduler,xcronschedulerissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
