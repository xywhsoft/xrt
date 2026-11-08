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
all folds participate in UTC order, missing labels do not fire. The optional
[selected mode](scheduler_selected.md) adds recurring fold/missing selection
through separate factories and five-budget polling. Nominal gap compensation
is still pending. The
separate [nominal-label selection primitive](delivery.md) resolves explicit
single labels; it does not reconfigure this kernel's existing jobs.

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

Leap-aware conversion, nominal DST delivery choices and durable crash recovery
remain incomplete. The [named database](tzdb.md) supplies explicit snapshots/reload;
existing jobs keep their retained rules. The borrowed Executor adapter and
live owner have separate [execution](executor.md) and [service](service.md)
contracts, including cancellation and no-effect rejection recovery. The module
and root stay developing; no scheduler-product completion is claimed.

## Budgeted recurrence work

`xcronSchedulerPollBudgeted` preserves lease/misfire/overlap/clock semantics and
adds independent global segment/month/rule-year limits. Every field is positive;
the sum of all jobs' traversal in one call stays within its limit. WorkLimit
continues to cap heap/queue actions. A40-byte report embeds the existing Poll and
adds actual Used work and the remaining Searching count. Capacity0/NULL Runs is
a calculation-only turn, useful when business output or leases are full.

Heap promotion, ready evaluation and search work rotate across calls. Pending
queries form a FIFO and take one-unit quanta. A ready evaluation may probe the
earliest rounded instant, charging the same global budget; an immediate hit
needs no full query-context initialization or recurrence loop. A miss seeds the
resumable query without repeating its charged state lookup. Already queued
queries reserve the last segment/month credit: an eager ready probe
cannot exhaust it ahead of an older pending query, even with output capacity1.
Queries that cannot use the remaining resource budget are rotated once and deferred, avoiding
repeated no-progress work in the same call.

SEARCHING is an appended nonterminal job state with HasNext=false. Live leases
and original Data stay valid. Complete cannot mistake SEARCHING for retirement.
Cancel removes a queued query in constant queue work; no remaining calendar or
history is evaluated. A later calculation fault records LastIssue and Poll.Faults
while previously published leases remain valid. Skip/coalesce range boundaries
are fixed when their decision is made; resumptions never rebase them on a new
wall sample. Once a result is known, placement uses the latest sample, preserving
wall rollback and avoiding replay of consumed instants.

The original Add remains a synchronous transactional calculation. Poll may
synchronously drain work left by a budgeted call; mixing it back in gives up the
recurrence bound. The live service uses provisional admission and the budgeted
entry point throughout. No scheduler callback or new shared runtime is involved.
Work units do not guarantee elapsed CPU or OS scheduling; Close is still O(JobLimit).

One416-byte typed query slot per configured job is reserved in the scheduler's
single allocation, with alignment padding and header bytes charged to BytesLimit.
The query array is separate from hot job slots; adding the provisional bit uses
existing padding instead of enlarging their stride. Normal immediate successors
avoid this query array. No extra allocation/lock/opaque wrapper copy occurs on
steady calculation paths. Existing config/job-info/poll/stats layouts remain;
the new report is separate. Tight byte caps must accommodate the additional
reserve. The initial allocation/zeroing and each actual pending query's state
setup/bookkeeping remain explicit costs.

The finite policy model runs another92160 samples through budgeted calls plus
calculation-only drains. Additional tests cover global limits, mixed heavy/light
fairness, full run capacity, cancellation/physical retirement, unpublished
admission IDs, rollback/typed OOM, future faults with live leases and128 sparse
lifecycles with more than10000 actual yields under a one-byte quota. Separate
dense1/128-job budgeted profiles include recurrence and physical Complete with
whole-batch P99. They are measurements, not production qualification.

## Control-path measurement

Budgeted class selection implements the same three cyclic orders with direct
branches, avoiding the per-action availability array and modulo-index scan.
Ready-query results also replace LastIssue directly instead of clearing it
immediately before overwriting it. Global limits, reserved query credit and
cross-call fairness are unchanged; no new state or ABI is introduced.

The [hot-path evidence](../tests/results/budget-hotpath-20261105.json) retains
original benchmark comparisons, archived pre-budget/budgeted binaries, class-only
prototypes and a separate bulk CPU diagnostic. The latter omits per-batch timer
reads AND sample-array writes/allocation, and reports no latency/P99. It still
executes real kernel Poll/advancement/Complete, with no executor or body work.
Its CPU observations cannot replace the original wall-clock performance gate.
Windows QPC reports10MHz here; coarse single-job P99 and failed noise gates
remain limitations, never exceptions to unchanged acceptance thresholds. Some
WSL original-benchmark samples exceed regression limits even when CPU diagnostics
improve. Qualified native performance and full service tail latency remain open.
