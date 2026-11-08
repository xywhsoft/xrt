#ifndef XRT_EVENT_WAITER_H
#define XRT_EVENT_WAITER_H
#include <xrt/sync.h>
#if defined(XRT_FEATURE_EVENT_WAITER) && !defined(XRT_FEATURE_EVENT)
#error "XRT_FEATURE_EVENT_WAITER requires XRT_FEATURE_EVENT"
#endif
#if defined(XRT_FEATURE_EVENT_WAITER)
#if defined(_WIN32) || defined(_WIN64)
#define XRT_EVENT_WAITER_STORAGE_SIZE 40u
#else
#define XRT_EVENT_WAITER_STORAGE_SIZE 32u
#endif
typedef union xeventwaiter {uint64 Alignment;uint8 Storage[XRT_EVENT_WAITER_STORAGE_SIZE];} xeventwaiter;
typedef enum xeventwaiterbackend {
    XEVENT_WAITER_PLATFORM=0,XEVENT_WAITER_WINDOWS_HIGH_RESOLUTION,
    XEVENT_WAITER_WINDOWS_STANDARD,XEVENT_WAITER_POSIX_MONOTONIC
} xeventwaiterbackend;
#define XRT_EVENT_WAITER_REQUIRE_PRECISE UINT32_C(1)
typedef struct xeventwaiterinfo {size_t StorageBytes;xeventwaiterbackend Backend;uint32 NativeTimers;} xeventwaiterinfo;
XRT_EXTERN_C_BEGIN
/* Caller-owned reusable deadline context borrowing one initialized event through
 * Unit. Init succeeds only into uninitialized storage, which must not overlap the
 * event. Failure preserves storage/borrowed event and releases all new resources.
 * WaitFor belongs to the native Init thread; signals use ordinary EventSet from
 * any thread. No new thread, callback, shared runtime or requested heap allocation.
 * Windows owns one reusable nonperiodic timer, preferring a high-resolution timer
 * without changing global/process timer resolution. Unsupported precise backends
 * fall back only with Flags=0; REQUIRE_PRECISE rejects that fallback. POSIX reuses
 * the event's monotonic condition where available. Inspect the actual backend;
 * precision selection is not a bound on OS scheduling/wake latency.
 * Other flag bits are invalid. Success/normal timeout preserve ambient error.
 * Diagnostics can fall back to MEMORY under diagnostic OOM. Error domain is
 * xrt.event_waiter (clock failures may retain xrt.time). No API grants concurrent
 * Unit/reinitialization, destruction of the borrowed event or escaping its storage. */
XRT_API bool xrtEventWaiterInit(xeventwaiter* Waiter,xevent* Event,uint32 Flags);
/* Caller-exclusive, no live WaitFor. May run on another thread after a physical
 * handoff/join. Releases only its timer/context, never the borrowed event. Owner
 * error-handler reentry during a wait is BUSY. Successful Unit clears storage. */
XRT_API bool xrtEventWaiterUnit(xeventwaiter* Waiter);
/* Relative signed milliseconds, -1 forever, 0 poll. One original monotonic budget
 * survives backend/spurious wakeups. Ready event wins over timeout, and automatic
 * reset consumes exactly one signal. Windows timer signals never consume the
 * borrowed event. Timers are rearmed/reset per finite wait and cancelled after an
 * event wins. Invalid timeout/owner transfers no event signal. Backend errors may
 * consume a ready signal; they are observable errors, not a successful timeout. */
XRT_API xwaitresult xrtEventWaiterWaitFor(xeventwaiter* Waiter,int64 Timeout);
/* Immutable capability/layout metadata; output must be disjoint from both public
 * storage objects. No concurrent Init/Unit; metadata can otherwise be inspected
 * by another thread while the owner waits. */
XRT_API bool xrtEventWaiterGet(const xeventwaiter* Waiter,xeventwaiterinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
