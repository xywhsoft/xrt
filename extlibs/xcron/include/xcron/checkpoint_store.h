#ifndef XCRON_CHECKPOINT_STORE_H
#define XCRON_CHECKPOINT_STORE_H
#include <xcron/features.h>
#include <xcron/checkpoint.h>
#include <xrt/file.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_STORE) && (!defined(XCRON_FEATURE_XCRON_CHECKPOINT) || !defined(XRT_FEATURE_FILE_ROOT) || !defined(XRT_FEATURE_FILE_LOCK))
#error "cron checkpoint store requires checkpoint and rooted native file locks"
#endif
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_STORE)
typedef struct xcroncheckpointstore xcroncheckpointstore;
typedef enum xcroncheckpointstorestatus {
    XCRON_CHECKPOINT_STORE_OK=0,XCRON_CHECKPOINT_STORE_EMPTY,
    XCRON_CHECKPOINT_STORE_ARGUMENT=7001,XCRON_CHECKPOINT_STORE_MEMORY,
    XCRON_CHECKPOINT_STORE_IO,XCRON_CHECKPOINT_STORE_LOCK,
    XCRON_CHECKPOINT_STORE_CORRUPT,XCRON_CHECKPOINT_STORE_MISMATCH,
    XCRON_CHECKPOINT_STORE_STALE,XCRON_CHECKPOINT_STORE_LIMIT,
    XCRON_CHECKPOINT_STORE_UNCERTAIN
} xcroncheckpointstorestatus;
typedef struct xcroncheckpointstoreinfo {
    uint64 Generation;
    uint32 ValidSlots;
} xcroncheckpointstoreinfo;

XRT_EXTERN_C_BEGIN
/* A caller-serialized store owns one exclusive native file lock until Close.
 * Create is exclusive and initializes an empty generation zero. Open existing
 * validates both slots and synchronizes the selected image before publication.
 * A failed Create can leave an unusable file to inspect. Open never truncates
 * or reinitializes an existing file. Root and Path are borrowed for this call.
 * Keep the path stable in a trusted directory while the store is in use. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreOpen(xroot Root,cstr Path,
    xcroncheckpointkey Key,bool Create,xcroncheckpointstore** Store);
/* Read the cached committed image. EMPTY publishes Info, leaving Record
 * untouched. Errors leave both outputs untouched. Outputs must be distinct
 * and outside the store object. Info is required; Record may be NULL. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreRead(const xcroncheckpointstore* Store,
    xcroncheckpoint* Record,xcroncheckpointstoreinfo* Info);
/* Compare ExpectedGeneration before I/O; generation zero means empty.
 * Success advances the generation by one after one native flush. An uncertain
 * write or flush poisons this handle. The caller keeps Record stable through
 * the call. Close an uncertain handle and reopen to discover the
 * durable generation. Serialize calls outside network and live owner turns. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreCommit(xcroncheckpointstore* Store,
    uint64 ExpectedGeneration,const xcroncheckpoint* Record);
/* Consumes Store and releases the lock even when reporting a close error. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointStoreClose(xcroncheckpointstore* Store);
XRT_EXTERN_C_END
#endif
#endif
