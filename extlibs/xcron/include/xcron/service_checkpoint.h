#ifndef XCRON_SERVICE_CHECKPOINT_H
#define XCRON_SERVICE_CHECKPOINT_H
#include <xcron/features.h>
#include <xcron/checkpoint.h>
#include <xcron/service_selected.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE_CHECKPOINT) && (!defined(XCRON_FEATURE_XCRON_CHECKPOINT) || !defined(XCRON_FEATURE_XCRON_SERVICE_SELECTED))
#error "cron service checkpoints require the checkpoint and selected service modules"
#endif
#if defined(XCRON_FEATURE_XCRON_SERVICE_CHECKPOINT)
XRT_EXTERN_C_BEGIN
/* Match the ordinary constructors and additionally reserve one record and
 * key per command slot, charged to ArenaBytes and BytesLimit. Ordinary
 * constructors leave checkpoint admission disabled. */
XCRON_API bool xcronServiceCreateCheckpointed(const xcronserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
XCRON_API bool xcronServiceCreateSelectedCheckpointed(const xcronselectedserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
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
/* Collects either checkpoint kind in either service mode. Record is required
 * for save and is assigned only on successful save. Restore returns Job or
 * the original Data on failure. Error transfers as in ServiceCollect.
 * Ordinary collectors reject these tickets and preserve them. This call
 * rejects ordinary tickets. All outputs must be disjoint. */
XCRON_API xwaitresult xcronServiceCollectCheckpoint(xcronservice* Service,xcronrequestid Request,int64 Timeout,
    xcronreply* Reply,xcroncheckpoint* Record,xcronserviceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
