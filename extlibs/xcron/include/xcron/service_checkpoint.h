#ifndef XCRON_SERVICE_CHECKPOINT_H
#define XCRON_SERVICE_CHECKPOINT_H
#include <xcron/features.h>
#include <xcron/checkpoint_writer.h>
#include <xcron/service_selected.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE_CHECKPOINT) && (!defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER) || !defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED))
#error "cron service checkpoints require the checkpoint writer and selected service modules"
#endif
#if defined(XCRON_FEATURE_XCRON_SERVICE_CHECKPOINT)
typedef struct xcronservicecheckpointjob {
    xcronjobconfig Config;
    xcroncheckpointkey Key;
} xcronservicecheckpointjob;
typedef struct xcronservicecheckpointgroup {
    const xcronservicecheckpointjob* Jobs;
    const xcroncheckpoint* Records;
    uint32 Count;
} xcronservicecheckpointgroup;
#define XCRON_SERVICE_CHECKPOINT_NO_INDEX XCRON_CHECKPOINT_NO_INDEX
typedef struct xcronservicecheckpointissue {
    xcronserviceissue Service;
    xcronschedulerissue Scheduler;
    uint32 Index;
} xcronservicecheckpointissue;

typedef struct xcronservicecheckpointreply {
    xcronrequestid Request;
    xcronserviceerror Code;
    xcronschedulerissue Scheduler;
    uint32 Index;
    xerror* Error;
} xcronservicecheckpointreply;

typedef struct xcronservicecheckpointcommit {
    xcroncheckpointwriter* Writer;
    const xcroncheckpointtarget* Targets;
    xcroncheckpoint* Scratch;
    uint64 ExpectedGeneration,TimeoutMilliseconds;
    uint32 Count;
} xcronservicecheckpointcommit;

typedef struct xcronservicecheckpointcommitreply {
    xcronservicecheckpointreply Command;
    bool StorageCompleted;
    xcroncheckpointstorestatus Store;
    uint64 Generation;
} xcronservicecheckpointcommitreply;

typedef struct xcronservicecheckpointperiodic {
    xcronservicecheckpointcommit Commit;
    uint64 EveryMilliseconds;
} xcronservicecheckpointperiodic;
typedef struct xcronservicecheckpointperiodicreport {
    xcronservicecheckpointcommitreply Last;
    uint64 Started,Finished,Committed,Skipped,Generation;
    bool Stopped;
} xcronservicecheckpointperiodicreport;

XRT_EXTERN_C_BEGIN
/* Match the ordinary constructors and additionally reserve one record and
 * key per command slot, charged to ArenaBytes and BytesLimit. Ordinary
 * constructors leave checkpoint admission disabled. */
XCRON_API bool xcronServiceCreateCheckpointed(const xcronserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
XCRON_API bool xcronServiceCreateSelectedCheckpointed(const xcronselectedserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
/* Installs a whole group before service publication, in Jobs order. Supply
 * independent stable keys and matching configurations in ascending Job order.
 * Inputs are borrowed through return. JobIds has Count slots and is optional
 * only for Count zero. Outputs must be mutually disjoint and disjoint from all
 * inputs and borrowed zones, data and contexts. Each restored job uses its
 * saved mode and policy. Scheduled jobs start paused; terminal jobs can be
 * reaped. The service and all IDs are published together before the owner
 * invokes the clock or dispatches. On failure neither output is modified,
 * and the caller retains all Data duties. A member failure reports its index
 * and typed scheduler issue; other startup failures use NO_INDEX.
 * Startup uses O(Count) extra ID bytes from BytesLimit and InitializationBudget.
 * Those bytes are released before publication. Use the usual checkpoint APIs
 * to save and SetPausedAsync to release recovered jobs. File I/O is external. */
XCRON_API bool xcronServiceCreateCheckpointedFromGroup(const xcronserviceconfig* Config,
    const xcronservicecheckpointgroup* Group,xcronservice** Service,xcronjobid* JobIds,xcronservicecheckpointissue* Issue);
XCRON_API bool xcronServiceCreateSelectedCheckpointedFromGroup(const xcronselectedserviceconfig* Config,
    const xcronservicecheckpointgroup* Group,xcronservice** Service,xcronjobid* JobIds,xcronservicecheckpointissue* Issue);
/* Share CommandLimit and FIFO with normal requests. Save snapshots the job on
 * the owner when quiescent, including a cancelled/exhausted job before reap.
 * A live lease or unfinished search can return a kernel BUSY reply. It does
 * not pause dispatch. Serialize durable commits for the same persistent key. */
XCRON_API bool xcronServiceSaveCheckpointAsync(xcronservice* Service,xcronjobid Job,xcroncheckpointkey Key,
    xcronrequestid* Request,xcronserviceissue* Issue);
/* Copies Config and the record before return, retains Zone on acceptance,
 * and borrows Data under the same rules as AddAsync. The owner checks the
 * configuration, mode, and one saved occurrence before admitting the job.
 * Selected policy comes from the record. The caller controls file I/O and
 * must prevent multiple live restores of the same persistent identity. */
XCRON_API bool xcronServiceRestoreCheckpointAsync(xcronservice* Service,const xcronjobconfig* Config,
    xcroncheckpointkey Key,const xcroncheckpoint* Record,xcronrequestid* Request,xcronserviceissue* Issue);
/* Restores a scheduled job with new lease publication held before admission.
 * Use SetPausedAsync to release it after the caller is ready. */
XCRON_API bool xcronServiceRestorePausedCheckpointAsync(xcronservice* Service,const xcronjobconfig* Config,
    xcroncheckpointkey Key,const xcroncheckpoint* Record,xcronrequestid* Request,xcronserviceissue* Issue);
/* Collects checkpoint commands in either service mode. Record is required
 * for save and is assigned only on successful save. Restore returns Job or
 * the original Data on failure. Error transfers as in ServiceCollect.
 * Ordinary collectors reject these tickets and preserve them. This call
 * rejects ordinary tickets. All outputs must be disjoint. */
XCRON_API xwaitresult xcronServiceCollectCheckpoint(xcronservice* Service,xcronrequestid Request,int64 Timeout,
    xcronreply* Reply,xcroncheckpoint* Record,xcronserviceissue* Issue);
/* One command captures and holds a quiescent group at an owner boundary.
 * Count must fit JobLimit and CommandWorkLimit; a nonempty group consumes
 * Count work units in its owner turn. Targets and Records are borrowed
 * until successful collection, including on close or collection timeout.
 * Keep Targets immutable and Records exclusively available to this command.
 * A rejected admission transfers no borrow. An unsuccessful command leaves
 * the records and dispatch holds intact. Use disjoint spans, outside all
 * borrowed zones, contexts and service storage. A caller may pause busy
 * jobs via SetPausedAsync before requesting capture. File I/O is external. */
XCRON_API bool xcronServiceSaveCheckpointGroupAsync(xcronservice* Service,const xcroncheckpointtarget* Targets,
    uint32 Count,xcroncheckpoint* Records,xcronrequestid* Request,xcronserviceissue* Issue);
/* Reserves one group, prevents new leases and waits for the issued work and
 * any pending recurrence searches. Uses the same array borrow and limits
 * as SaveCheckpointGroupAsync. A positive monotonic timeout starts at
 * submission, including queue time. Only one quiescence command can own
 * holds at a time. Conflicting quiescence, pause, resume or cancel returns
 * BUSY and reaping skips the reserved members. On success the holds become
 * ordinary pauses. Failure, timeout or service close releases the temporary
 * holds and preserves Records. File commit remains with the caller. */
XCRON_API bool xcronServiceQuiesceCheckpointGroupAsync(xcronservice* Service,const xcroncheckpointtarget* Targets,
    uint32 Count,xcroncheckpoint* Records,uint64 TimeoutMilliseconds,xcronrequestid* Request,xcronserviceissue* Issue);
/* Uses the normal timeout and Error transfer rules. Only XWAIT_OK ends the
 * array borrow. Check Reply.Code to decide whether Records were published.
 * Index identifies the failed member or is NO_INDEX. */
XCRON_API xwaitresult xcronServiceCollectCheckpointGroup(xcronservice* Service,xcronrequestid Request,int64 Timeout,
    xcronservicecheckpointreply* Reply,xcronserviceissue* Issue);
/* Reserves/captures as QuiesceCheckpointGroupAsync and keeps the reservations
 * until the off-owner writer finishes. A positive timeout covers quiescence
 * through writer admission. A submitted write drains during service close.
 * A definite outcome restores previous dispatch eligibility. UNCERTAIN
 * leaves scheduled members paused for explicit reconciliation. Copying the
 * descriptor completes at admission. Keep Writer and both arrays live until
 * successful collection. Scratch is working storage; only the reply reports
 * durability. All spans must be disjoint from Writer, store and executor.
 * The host owns Writer and its executor. Join the service from outside that
 * executor while a durable request is outstanding. */
XCRON_API bool xcronServiceCommitCheckpointGroupAsync(xcronservice* Service,const xcronservicecheckpointcommit* Commit,
    xcronrequestid* Request,xcronserviceissue* Issue);
/* Only XWAIT_OK writes Reply and releases the borrows. Command.Error transfers
 * the owned diagnostic. Store and Generation apply when StorageCompleted is
 * true; Generation is nonzero on a stored generation. STORAGE with
 * StorageCompleted false indicates writer submission rejection. A wait from
 * the writer executor is refused while the request depends on it. */
XCRON_API xwaitresult xcronServiceCollectCheckpointCommit(xcronservice* Service,xcronrequestid Request,int64 Timeout,
    xcronservicecheckpointcommitreply* Reply,xcronserviceissue* Issue);
/* Register one periodic group on the same live owner. It owns one command
 * ticket and borrows the writer and arrays through successful final collection.
 * The first tick is one positive interval after the owner arms the request.
 * Host monotonic time drives fixed-rate ticks. Missed ticks are skipped and
 * never queue extra writes. The capture timeout applies separately to each
 * attempt. Success advances the expected generation. Capture timeout and
 * temporary storage queue pressure retry on a later tick. Other failures
 * stop the schedule. UNCERTAIN also leaves the scheduled members paused.
 * Use a dedicated writer for the group while the registration is live. */
XCRON_API bool xcronServiceStartCheckpointPeriodicAsync(xcronservice* Service,const xcronservicecheckpointperiodic* Periodic,
    xcronrequestid* Request,xcronserviceissue* Issue);
/* Request stop without a second ticket, even if the command queue is full.
 * A write already accepted by storage drains. Service Close also stops the
 * schedule. Poll or collect the original ticket to observe completion. */
XCRON_API bool xcronServiceStopCheckpointPeriodic(xcronservice* Service,xcronrequestid Request,xcronserviceissue* Issue);
/* Copy current counters and the latest finished attempt. Before Finished is
 * nonzero, Last identifies the ticket and can report registration failure.
 * A returned Last.Command.Error is an owned reference. This call retains the
 * ticket and all borrows. Generation starts at Commit.ExpectedGeneration and
 * advances after a successful write. */
XCRON_API bool xcronServiceInspectCheckpointPeriodic(xcronservice* Service,xcronrequestid Request,
    xcronservicecheckpointperiodicreport* Report,xcronserviceissue* Issue);
/* Collect once after stop, ending the array and writer borrows. As for a
 * single commit, a positive wait from the storage executor is refused. */
XCRON_API xwaitresult xcronServiceCollectCheckpointPeriodic(xcronservice* Service,xcronrequestid Request,int64 Timeout,
    xcronservicecheckpointperiodicreport* Report,xcronserviceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
