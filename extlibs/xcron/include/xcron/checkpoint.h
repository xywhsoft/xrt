#ifndef XCRON_CHECKPOINT_H
#define XCRON_CHECKPOINT_H
#include <xcron/features.h>
#include <xcron/scheduler_selected.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT) && !defined(XCRON_FEATURE_XCRON_SCHEDULER_SELECTED)
#error "cron checkpoints require the scheduler in both modes"
#endif
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT)
#define XCRON_CHECKPOINT_BYTES 180u
typedef struct xcroncheckpointkey {
    uint64 Job;
    uint64 Revision;
} xcroncheckpointkey;
typedef struct xcroncheckpoint {
    uint8 Bytes[XCRON_CHECKPOINT_BYTES];
} xcroncheckpoint;

XRT_EXTERN_C_BEGIN
/* Validates the v1 envelope and returns its stable application identity.
 * The record and output must be disjoint. Failure preserves Key. */
XCRON_API bool xcronCheckpointGetKey(const xcroncheckpoint* Checkpoint,xcroncheckpointkey* Key);
/* Capture one quiescent job on the serialized scheduler owner. Every run has
 * physically completed and the next occurrence is known, or the job is an
 * exhausted/explicitly cancelled tombstone. SEARCHING/live leases are BUSY;
 * a faulted job reports CALCULATION. Capture before Close/Reap. A new record
 * is published to caller storage only on success. No I/O or allocation on
 * success. Job and Revision are nonzero stable application identities;
 * change Revision whenever the job's zone rules or intended behavior change.
 * The caller commits the record to durable storage at its chosen boundary. */
XCRON_API bool xcronSchedulerSaveCheckpoint(const xcronscheduler* Scheduler,xcronjobid Job,
    xcroncheckpointkey Key,xcroncheckpoint* Checkpoint,xcronschedulerissue* Issue);
/* Admit from a complete v1 record into an open scheduler of the saved mode.
 * Recreate the same configuration/zone and supply new process-local Data;
 * configured fields and Key must match. Revalidate a scheduled instant and
 * restore the selected mode's saved fold/missing policy and local metadata.
 * Terminal records restore as reapable tombstones. Per-job counters continue;
 * scheduler-wide counters, monotonic samples, leases and old IDs are local.
 * Only success retains Zone and borrows Data. Config/record are borrowed for
 * the call. Checksum detects storage corruption; it authenticates nothing.
 * Replay may occur after the last committed checkpoint. Serializing a newer
 * record does not itself make the record durable or commit business effects. */
XCRON_API bool xcronSchedulerRestoreCheckpoint(xcronscheduler* Scheduler,const xcronjobconfig* Config,
    xcroncheckpointkey Key,xbytesview Checkpoint,xcronjobid* Job,xcronschedulerissue* Issue);
/* Same admission and record validation as RestoreCheckpoint. Scheduled jobs
 * enter with lease publication paused atomically. The wire format is shared
 * with normal restore. Release the hold with SchedulerSetPaused. */
XCRON_API bool xcronSchedulerRestorePausedCheckpoint(xcronscheduler* Scheduler,const xcronjobconfig* Config,
    xcroncheckpointkey Key,xbytesview Checkpoint,xcronjobid* Job,xcronschedulerissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
