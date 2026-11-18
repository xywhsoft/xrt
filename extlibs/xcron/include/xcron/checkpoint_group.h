#ifndef XCRON_CHECKPOINT_GROUP_H
#define XCRON_CHECKPOINT_GROUP_H
#include <xcron/checkpoint_store.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_GROUP) && !defined(XCRON_FEATURE_XCRON_CHECKPOINT_STORE)
#error "cron checkpoint groups require the rooted checkpoint store"
#endif
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_GROUP)
typedef struct xcroncheckpointgroup xcroncheckpointgroup;
typedef struct xcroncheckpointgroupview {
    const xcroncheckpoint* Records;
    uint32 Count;
    xcroncheckpointstoreinfo Info;
} xcroncheckpointgroupview;

XRT_EXTERN_C_BEGIN
/* A serialized owner holds an exclusive file lock until Close. Group and
 * Capacity are nonzero and fixed for the lifetime of the file. Capacity
 * limits both slot payloads and the single allocation for the handle.
 * Create is exclusive; a failed Create may leave an unusable file.
 * Reopen validates both slots and synchronizes before publishing the view. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupOpen(xroot Root,cstr Path,uint64 Group,
    uint32 Capacity,bool Create,xcroncheckpointgroup** Store);
/* EMPTY publishes empty generation zero. A committed empty group has OK.
 * Only success or EMPTY publishes a view. Records are immutable, sorted
 * by stable Job ID, and borrowed until the next Commit or Close. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupRead(const xcroncheckpointgroup* Store,
    xcroncheckpointgroupview* View);
/* Replaces the whole group in a single generation. Records must be valid,
 * sorted strictly by Job ID, with at most Capacity records. Count may be
 * zero. A current borrowed view is valid input. Keep input stable until
 * return. One alternate-slot write and one flush precede success. Failed
 * I/O makes the handle uncertain. Close and reopen before retrying. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupCommit(xcroncheckpointgroup* Store,
    uint64 ExpectedGeneration,const xcroncheckpoint* Records,uint32 Count);
/* Consumes Store. */
XCRON_API xcroncheckpointstorestatus xcronCheckpointGroupClose(xcroncheckpointgroup* Store);
XRT_EXTERN_C_END
#endif
#endif
