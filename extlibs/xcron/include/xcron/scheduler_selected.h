#ifndef XCRON_SCHEDULER_SELECTED_H
#define XCRON_SCHEDULER_SELECTED_H
#include <xcron/scheduler.h>
#include <xcron/occurrence.h>
#if defined(XCRON_FEATURE_XCRON_SCHEDULER_SELECTED) && (!defined(XCRON_FEATURE_XCRON_SCHEDULER) || !defined(XCRON_FEATURE_XCRON_OCCURRENCE))
#error "selected scheduler requires scheduler and complete occurrence calculation"
#endif
#if defined(XCRON_FEATURE_XCRON_SCHEDULER_SELECTED)
typedef struct xcronselectedrun {xcronrun Run;xdatetime Local;size_t Candidates;} xcronselectedrun;
typedef struct xcronselectedpoll {xcronpoll Poll;xcronoccurrencebudget Used;uint32 Searching;} xcronselectedpoll;
typedef struct xcronselectedjobinfo {
    xcronjobstate State;uint32 Running;bool HasNext;xtime Next;
    uint64 Dispatched,Succeeded,Failed,Cancelled,SkippedRanges,CoalescedRanges;
    xcrondeliverypolicy Policy;xcronoccurrenceissue LastIssue;
} xcronselectedjobinfo;
typedef struct xcronselectedretired {xcronjobid Id;ptr Data;xcronselectedjobinfo Info;} xcronselectedretired;
XRT_EXTERN_C_BEGIN
/* Optional policy-aware mode of the same externally serialized lease kernel.
 * One bounded allocation preallocates all selected query storage (792 bytes/job
 * on verified x64). Actual-mode factories keep their smaller query storage.
 * Config/byte limits, zone/Data ownership and physical completion contracts match
 * SchedulerCreate. Use the Selected Add/Poll/Job/Reap entry points on this mode;
 * the actual-mode counterparts reject it atomically. Complete/Cancel/Wake/
 * Inspect/Close/Destroy work with both modes. No callback/thread/runtime is added. */
XCRON_API bool xcronSchedulerCreateSelected(const xcronschedulerconfig* Config,xcronscheduler** Scheduler,xcronschedulerissue* Issue);
/* Copies Expression/Policy/bounds and retains Zone only on success. No recurrence
 * traversal: an admitted job starts SEARCHING, or EXHAUSTED for an empty rounded
 * window. Data is borrowed from success through Reap, including initial faults.
 * Invalid expression/policy/window fails admission; zone/fold/missing calculation
 * faults are discovered by Poll and recorded in LastIssue without diagnostics.
 * ALL/FIRST/LAST/REJECT and SKIP/REJECT use complete global occurrence semantics.
 * There is no nominal gap shift/compensation. Other job policies match Add. */
XCRON_API bool xcronSchedulerAddSelected(xcronscheduler* Scheduler,const xcronjobconfig* Config,xcrondeliverypolicy Policy,xcronjobid* Id,xcronschedulerissue* Issue);
/* Five independent nonnegative global traversal budgets; zeros are allowed.
 * WorkLimit bounds heap/queue actions. Pending queries receive FIFO one-unit
 * quanta. A ready successor may eagerly use one quantum only if no older query
 * waits. Capacity=0 allows calculation while lease/completion output is full.
 * Known ready leases may publish even at zero search budget, leaving successors
 * SEARCHING. Used deltas never exceed their own budgets. Local/Candidates describe
 * the selected ScheduledAt label before the next search changes job metadata.
 * Poll faults preserve published leases and are reported per job, not as an
 * all-or-nothing call error. Misfire/overlap range discards do not enumerate or
 * validate omitted labels: missing REJECT applies to each retained search window.
 * CATCH_UP retains every selected occurrence; no consumed cursor rewinds. */
XCRON_API bool xcronSchedulerPollSelected(xcronscheduler* Scheduler,xcronclock Now,xcronoccurrencebudget SearchBudget,
    xcronselectedrun* Runs,size_t Capacity,xcronselectedpoll* Poll,xcronschedulerissue* Issue);
/* Outputs avoid the owner allocation, input/other outputs and retained zones/Data
 * as for SchedulerJob/Reap. LastIssue is the latest query's complete occurrence
 * issue; pending exposes traversal counters only. Policy is the admitted copy.
 * A successful empty Reap sets Found=false and preserves Retired. Reap releases
 * Zone and returns Data only after a terminal job has no live physical leases. */
XCRON_API bool xcronSchedulerJobSelected(const xcronscheduler* Scheduler,xcronjobid Id,xcronselectedjobinfo* Info,xcronschedulerissue* Issue);
XCRON_API bool xcronSchedulerReapSelected(xcronscheduler* Scheduler,xcronselectedretired* Retired,bool* Found,xcronschedulerissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
