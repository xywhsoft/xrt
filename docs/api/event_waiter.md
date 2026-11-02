# Event deadline waiter

Select `XRT_MODULE_EVENT_WAITER` before including `<xrt.h>`, or include
`<xrt/features.h>` before the direct `<xrt/event_waiter.h>` header. The leaf depends
only on event/sync/wait/time/Core, not thread creation, executors or networking.
It provides an explicitly owned reusable deadline context for an existing event.
There is no timer thread, callback, shared runtime, busy poll or requested heap
allocation. On Windows it owns one native nonperiodic timer handle; POSIX reuses
the borrowed event condition. It calls no global/process timer-resolution API;
kernel clock/power effects are not claimed to be zero or qualified here.

The native prototype motivating this addition compares ordinary XRT events,
native event timeouts, ordinary timers and high-resolution timers. A pinned-CPU,
256-sample group with 1ms requests observed median extra wait about14.7ms for the
ordinary event and0.96ms for the precise timer. These are measured local samples,
not a scheduling SLA, CPU/power acceptance or a portable latency guarantee.
The [Microsoft CreateWaitableTimerEx documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)
defines the high-resolution flag (Windows10 version1803+). The
[waitable timer lifecycle](https://learn.microsoft.com/en-us/windows/win32/sync/waitable-timer-objects)
defines rearming/reset and cancellation. No implementation source was copied.

## Types and capability

```c
typedef union xeventwaiter {
    uint64 Alignment;
    uint8 Storage[XRT_EVENT_WAITER_STORAGE_SIZE];
} xeventwaiter;
typedef enum xeventwaiterbackend {
    XEVENT_WAITER_PLATFORM=0,
    XEVENT_WAITER_WINDOWS_HIGH_RESOLUTION,
    XEVENT_WAITER_WINDOWS_STANDARD,
    XEVENT_WAITER_POSIX_MONOTONIC
} xeventwaiterbackend;
typedef struct xeventwaiterinfo {
    size_t StorageBytes;
    xeventwaiterbackend Backend;
    uint32 NativeTimers;
} xeventwaiterinfo;
```

`XRT_EVENT_WAITER_STORAGE_SIZE` is40 bytes on Windows and32 on POSIX, aligned by
Alignment; no feature selection changes that platform's layout. `Storage` is
opaque. `StorageBytes` reports that context size, `Backend` reports the actual
mechanism, and `NativeTimers` is1 on Windows,0 on POSIX. The separate borrowed
event/native handle is not counted as waiter-owned storage/timers. Metadata is
immutable until caller-exclusive Unit, so another thread can inspect it while
the owner waits, provided no Init/Unit races with it.

`XRT_EVENT_WAITER_REQUIRE_PRECISE` is flag bit1. Flags0 prefers the precise backend
but permits a reported fallback: Windows standard waitable timer or POSIX platform
event condition. REQUIRE_PRECISE rejects a missing high-resolution Windows timer
or non-monotonic POSIX condition with UNSUPPORTED. Native resource/permission
errors never trigger silent fallback. This is a backend selection contract,
not an upper bound on hardware, kernel scheduling, wake or application latency.

## API and ownership

```c
bool xrtEventWaiterInit(xeventwaiter* Waiter,xevent* Event,uint32 Flags);
bool xrtEventWaiterUnit(xeventwaiter* Waiter);
xwaitresult xrtEventWaiterWaitFor(xeventwaiter* Waiter,int64 Timeout);
bool xrtEventWaiterGet(const xeventwaiter* Waiter,xeventwaiterinfo* Info);
```

Init borrows an initialized Event through successful Unit. Waiter storage must be
uninitialized and disjoint from Event; unknown flags/invalid spans fail without
writing it. Success consumes only the new native timer, not Event. Failure
preserves storage and releases new resources. Do not initialize live storage
again. WaitFor belongs to the native Init thread. Signal/reset the event with
ordinary EventSet/EventReset; manual/auto-reset behavior and competing waiters
remain ordinary event semantics. A shared auto event is not a broadcast channel.

Timeout is relative signed milliseconds: -1 forever,0 poll, positive finite.
Values below-1 and foreign-owner calls fail without consuming a signal. A ready
event wins over timeout. One original monotonic budget survives backend/spurious
wakes; no restart of the full interval. Very large finite budgets are chunked
before outward conversion to signed100ns durations, avoiding overflow. Timer
signals do not consume Event. The event is first in the native wait set; after an
event wins the timer is cancelled. Each next finite wait resets/rearms the timer,
so a late/cancelled timer signal cannot shorten its new budget. Infinite waits
need no timer arming. Backend failure is observable and may have consumed a ready
event; errors are not misreported as normal timeouts.

Unit is caller-exclusive with no live WaitFor. It can run on a different thread
after a physical handoff/join. It releases only the owned timer, clears storage
on success, and leaves a valid context intact on native close failure for retry.
Owner error-handler reentry while actively waiting is rejected as STATE/busy.
No API makes concurrent Unit/reinitialization or destruction/reuse of borrowed
Event safe. Get rejects output aliases with either public storage object.

Success and normal timeout preserve ambient XRT error. Failures set structured
errors in `xrt.event_waiter` with Operation/SystemCode; clock failures may retain
`xrt.time`. ARGUMENT covers invalid flags/spans/outputs/timeouts; STATE covers
uninitialized context/event, foreign owner or active cleanup; UNSUPPORTED covers
strict precision rejection; IO covers native operations. Diagnostic allocation
failure may replace a diagnostic with MEMORY, consistently with other Core APIs.

## Runnable example, costs and qualification

```sh
python tools/build.py --compiler gcc --suite event_waiter --cflag=-O2
python tools/build.py --compiler gcc --suite event_waiter_tests --cflag=-O2
python tools/package.py --compiler gcc --suite event_waiter --kind static --verify
python tools/package.py --compiler gcc --suite event_waiter --kind shared --verify
python tools/measure_performance.py --compiler gcc --profiles event_waiter --smoke
```

The `concurrency_event_waiter_main` example signals/polls, waits5ms, releases the
context, then proves the borrowed event is still usable. Expected `[PASS]` reports
the backend/timer count/storage size. It needs no external service/files and
releases all resources. No observed overshoot is asserted as a tight CI bound.
Windows finite waits add ready polling, monotonic clock reads, timer arm,
two-object wait and event-won timer cancel; this is not zero syscall/call overhead.
POSIX delegates to the existing event absolute condition wait. There are no
per-wait copies/allocations of business data or per-wait native handle creation.
One native handle/context per waiter is explicit; benchmark CPU counters exclude
system-wide interrupt/power costs, and a measured zero CPU delta is not proof of
zero CPU work.

Public tests cover auto/manual reset, outward deadlines, event interruption,
large/infinite budgets, owner guard and physical cleanup handoff, alias/span
rejection,20,000 ready cycles,32 armed cycles and1,000 requested-heap-free context
lifetimes. Native Windows fault tests cover unsupported/fallback/resource errors,
diagnostic OOM, arm/wait/cancel/unit errors, retry and active cleanup reentry;
handle counts return to baseline. POSIX tests construct a real realtime-condition
fallback and prove strict rejection/default behavior. Modular/single, sanitizer,
static/shared consumer, trim, size and timing evidence are recorded separately.
This prerequisite does not itself integrate xcron, qualify native Linux, or
complete pinned throughput/P99/CPU/power/72h production acceptance.
