#ifndef XCRON_SCHEDULER_H
#define XCRON_SCHEDULER_H

#include <xcron/calendar.h>

#if defined(XCRON_FEATURE_XCRON_SCHEDULER) && \
    (!defined(XCRON_FEATURE_XCRON_CALENDAR) || !defined(XRT_FEATURE_EVENT_WAITER))
#error "xcron scheduler requires calendar and XRT event waiter"
#endif

#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
typedef struct xcronscheduler xcronscheduler;
typedef uint64 xcronjobid;
typedef void (*xcroncallback)(void* Context);

typedef enum xcronschedulestatus {
    XCRON_SCHEDULE_ERROR = -1,
    XCRON_SCHEDULE_IDLE = 0,
    XCRON_SCHEDULE_DISPATCHED,
    XCRON_SCHEDULE_TIMEOUT,
    XCRON_SCHEDULE_WOKEN
} xcronschedulestatus;

typedef struct xcronpoll {
    size_t Count;
    bool HasNext;
    xtime Next;
} xcronpoll;

XRT_EXTERN_C_BEGIN

/* Use one APP thread for create/add/remove/poll/wait/destroy. Callbacks execute
 * inline on that thread and must return before any of those operations on the
 * same scheduler. Wake is safe on other threads while the handle is live. Join
 * all Wake callers before destroying it. There is no scheduler worker. */
XCRON_API bool xcronSchedulerCreate(size_t Capacity, xcrondialect Dialect,
    xcronscheduler** Output);

/* Compile once. The first UTC occurrence is strictly after registration.
 * The string is borrowed only during Add. Context is borrowed until removal or
 * destruction. Failure preserves Output and leaves no job. A zero ID is invalid. */
XCRON_API bool xcronSchedulerAdd(xcronscheduler* Scheduler, xstrview Cron,
    xcroncallback Callback, void* Context, xcronjobid* Output);
XCRON_API bool xcronSchedulerRemove(xcronscheduler* Scheduler, xcronjobid Job);

/* Dispatch every job due at the sampled wall time once. Compute the next
 * occurrence strictly after that sample. Output may be NULL. Next is meaningful
 * only when HasNext is true. Poll never waits. */
XCRON_API xcronschedulestatus xcronSchedulerPoll(xcronscheduler* Scheduler,
    xcronpoll* Output);

/* Relative milliseconds. XRT_WAIT_FOREVER keeps waiting until due or woken.
 * A zero timeout dispatches due jobs and otherwise returns TIMEOUT. An explicit
 * wake interrupts the wait; the owner can drive again to recheck the wall clock. */
XCRON_API xcronschedulestatus xcronSchedulerWait(xcronscheduler* Scheduler,
    int64 Timeout, xcronpoll* Output);
XCRON_API bool xcronSchedulerWake(xcronscheduler* Scheduler);
XCRON_API bool xcronSchedulerDestroy(xcronscheduler* Scheduler);

XRT_EXTERN_C_END
#endif
#endif
