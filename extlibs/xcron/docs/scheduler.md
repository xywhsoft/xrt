# Bounded explicit-clock scheduling kernel (developing)

Select `XCRON_MODULE_XCRON_SCHEDULER` and include `<xcron/scheduler.h>` or
`<xcron.h>`. The leaf depends on zoned/calendar/expression/timezone and public
XRT core/time/atomic. It creates no thread, timer, engine or executor. A serialized
owner supplies paired wall and monotonic millisecond samples and pulls run leases.
Worker completion must be delivered back to that owner. This is a usable,
deterministic scheduling kernel, not the complete live scheduler product.

## Admission, memory and ownership

`Create` requires positive JobLimit, RunLimit, WorkLimit and an adequate BytesLimit;
Reserved is zero. One allocation contains the owner, job/run slots, due min-heap,
free lists and intrusive ready/retirement queues. BytesLimit covers that allocation,
not immutable retained zones, caller Data, caller output arrays or XRT diagnostics.
`Inspect.Bytes` is its exact requested allocation size. JobLimit includes terminal
jobs not yet reaped; RunLimit includes published leases still awaiting completion.
No normal Add/Poll/Complete/Cancel/Reap/Wake/Inspect/Close operation allocates.

Add copies a validated compiled expression and retains the immutable Zone on
success only. Data is borrowed until the job's retirement receipt, including all
live executions after cancellation. An initial calculation error admits nothing;
an empty result admits an EXHAUSTED job ready for Reap. IDs are scoped to their
originating live scheduler. Each slot uses ceil(log2(Limit)) low index bits and all remaining high bits for
a generation, starting at one; zero is invalid. A one-slot table has all 64 bits
for its generation; 128 slots retain 57 generation bits. Reuse changes the
generation. At its maximum a slot permanently retires instead of wrapping.
This cannot give an old ID new meaning; there is no 32-bit hot-slot reuse cliff. Cross-instance IDs are a
caller error, not globally unique capabilities.

Every successful Poll lease is running immediately, even before executor admission.
Submit failure therefore needs a Complete with FAILED/CANCELLED. Complete is
exactly once and means the physical body **and all tails that can use Data have
stopped**. A failed/cancelled run counts that outcome but leaves future triggers
eligible. Cancel stops future dispatch and is idempotent while the ID exists;
an earlier EXHAUSTED/FAULTED reason is preserved. Close rejects Add and cancels
scheduled jobs in O(JobLimit), without joining bodies or discarding receipts.
Reap returns only terminal jobs with zero running leases, including original
Data and typed final information, then releases the zone and invalidates the ID.
The caller may then destroy Data. Destroy fails BUSY until all jobs are reaped.
There are no user callbacks or destructors inside the kernel.

All writable outputs, including Issue, must be disjoint from one another, input
structs and the scheduler allocation. Add also checks the zone-owned graph. Other
calls require the caller to keep output buffers disjoint from retained immutable
zones and borrowed Data; they do not scan every job's graph on a hot path.
As usual, all input pointers must designate live accessible objects. API argument,
span and alias errors preserve every output. Value failures preserve normal
outputs and publish typed Issue despite diagnostic OOM. Successful operations
preserve the caller's ambient error. Counters of lifetime events saturate at
UINT64_MAX; sizes/counts are bounded by the configured slots.

## Clock, recurrence and delivery policies

Begin/End are an explicit inclusive XRT millisecond window, with the usual
XCRON_SEARCH_EXCLUSIVE Begin option. XRT epoch is 0001-01-01; use
XRT_TIME_UNIX_EPOCH for Unix epoch. Recurrence uses actual zoned instants:
all folds participate in UTC order, missing labels do not fire. Configurable
nominal gap compensation and fold selection are still pending.

Poll accepts equal/increasing monotonic samples and rejects regression atomically.
Wall samples may jump either direction. A forward jump creates policy-controlled
backlog. A backward jump delays future work, including jobs already promoted to
ready; consumed/discarded cursors never rewind, so it does not replay a lease.
WallRegressions counts strictly decreasing consecutive wall samples, not inferred
NTP adjustment magnitudes. A clock sample is committed even when no lease is ready.

An instant is a misfire only when strictly older than
`cutoff = saturating_subtract(Wall, GraceMilliseconds)`. Grace is uint64 and the
cutoff saturates at INT64_MIN. Policy is explicit; there is no hidden smart policy.

| Misfire | Behavior when an eligible due instant is older than cutoff |
| --- | --- |
| CATCH_UP | Publish each actual instant in order, subject to all limits |
| SKIP | Discard the old range and search inclusively at cutoff |
| COALESCE | Publish oldest due instant once with COALESCED and Through=min(Wall,End); continue strictly after Through |

Through equals ScheduledAt on a normal lease. A coalesced range describes coverage,
not an exact missed-run count. SkippedRanges and CoalescedRanges count decisions,
not occurrences. Selecting a range does not enumerate the omitted instants.
Semantics are library-specific. Quartz documents analogous do-nothing/fire-now
misfire choices, but this API does not claim Quartz dialect or policy compatibility:
[official CronTrigger tutorial](https://www.quartz-scheduler.org/documentation/quartz-2.3.0/tutorials/tutorial-lesson-06.html).

| Overlap | Behavior at evaluation |
| --- | --- |
| WAIT | RunningLimit must be 1; retain backlog while a lease lives |
| ALLOW | Publish up to explicit RunningLimit (<=global RunLimit); retain excess |
| SKIP | RunningLimit must be 1; while a lease lives discard all due work through Wall |

Overlap is evaluated using live leases at owner evaluation, including executor
queue time. It is not inferred retrospectively from unobserved wall time between
Poll/Complete calls. SKIP overlap takes precedence over misfire policy. WAIT/ALLOW
evaluate misfires when capacity becomes available. Global capacity keeps pending
work bounded; the caller must notify/wake the owner on completions.

## Work, fairness, faults and waiting

Poll publishes at most caller Capacity and performs at most WorkLimit heap/queue
steps. Capacity is positive. Due jobs are promoted from a min-heap (due instant,
then numeric ID tie-break) into a FIFO. Promotion and ready evaluation alternate
across calls. A catch-up job returns to the FIFO tail; it cannot indefinitely
monopolize an older due-heap key. Per-job running limits detach blocked jobs until
Complete reactivates them. More requests another same-sample poll when useful
immediate work remains after a budget or output limit.

WorkLimit bounds owner actions, **not CPU time of one recurrence search**. A search
may traverse all relevant TZif history or a complete Gregorian cycle. This stage
does not claim a hard callback-time bound or safe embedding of such an unbounded
search in a latency-sensitive NetEngine callback. Incremental/precomputed query
work and the runtime adapter need further design and measurement.

A calculation fault after admission marks only that job FAULTED, records a typed
LastIssue and increments Poll.Faults. Already published leases remain valid. Poll
still succeeds with explicit partial progress; the error cannot be mistaken for
an all-or-nothing batch failure. Product-private pure zoned value calculation
avoids manufacturing ambient diagnostic errors or allocations for recorded job
faults. Initial calculation failures instead fail Add transactionally.

Wake is a pure projection from an explicit sample, no earlier than last Poll's
monotonic sample. It returns the earliest heap deadline bounded by positive
MaxWaitMilliseconds for wall resampling. Ready actionable work is immediate;
jobs blocked by live running limits wait for completion/resampling instead of a
zero-delay busy loop. A due heap item can request one promotion poll even at the
global run limit. Add, Cancel, Close and Complete are external wake events for a
future adapter. Monotonic deadline addition saturates at UINT64_MAX; DelayMilliseconds
records the projected duration before that saturation.

## Execution and evidence

The [offline example](../examples/scheduler/main.c) demonstrates coalescing and
physical retirement with explicit clock samples and full cleanup:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_scheduler --no-single --cflag=-O2
python tools/measure_performance.py --config extlibs/xcron/config/performance_profiles.json --profiles xcron_scheduler_1,xcron_scheduler_128 --smoke
```

Tests include adversarial lifetime/limits/aliases, all policy combinations, work=1
fairness, clock jumps/endpoints, unknown TZif coverage after publishing leases,
OOM, repeated generation reuse and zero steady allocations. An independent finite
occurrence-list model checks 92,160 deterministic samples across 576 policy traces.
Benchmark profiles measure preallocated pull dispatch/completion for 1/128 jobs,
including batch P99 (the whole batch, not divided into fictitious per-job latency).
They exclude threads/timers/storage and do not establish production/performance
acceptance. Native Linux, mixed load, adversarial P99 and >=72h qualification remain.

Named database/reload and leap-aware conversion, nominal DST delivery choices,
NetEngine/Executor integration, callback/cancellation-token contracts, admission
hand-off/rejection recovery and durable crash recovery remain incomplete. The
module and root stay developing; no scheduler-product completion is claimed.
