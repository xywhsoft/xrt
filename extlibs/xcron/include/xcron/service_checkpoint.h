#ifndef XCRON_SERVICE_CHECKPOINT_H
#define XCRON_SERVICE_CHECKPOINT_H
#include <xcron/features.h>
#include <xcron/checkpoint.h>
#include <xcron/service_selected.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE_CHECKPOINT) && (!defined(XCRON_FEATURE_XCRON_CHECKPOINT) || !defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED))
#error "cron service checkpoints require the checkpoint and selected service modules"
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
#define XCRON_SERVICE_CHECKPOINT_NO_INDEX UINT32_MAX
typedef struct xcronservicecheckpointissue {
    xcronserviceissue Service;
    xcronschedulerissue Scheduler;
    uint32 Index;
} xcronservicecheckpointissue;

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
XRT_EXTERN_C_END
#endif
#endif
