# Live cron service

`XCRON_MODULE_XCRON_SERVICE` supplies a dedicated public-XRT owner thread with a
bounded command FIFO, the explicit-clock scheduler and a borrowed executor adapter.
It is a developing module. No new shared runtime, network callback, private Core
API, durable store or forced termination is used. The owner evaluates recurrences,
issues leases, consumes physical completions and waits on an auto-reset XRT event
through one reusable public `event_waiter` bound to the owner thread.
The borrowed Executor is never closed by this service.

## Commands and original business data

`AddAsync`, `CancelAsync`, `InfoAsync` and `ReapAsync` are thread-safe, bounded
admission operations. A generation-tagged ticket occupies one `CommandLimit` slot
from acceptance until exactly one `Collect`. Outstanding replies still count.
Generations use all bits outside the slot index and retire on exhaustion. Tickets,
job IDs and run IDs belong to their live originating service/kernel, not a global
capability namespace. Concurrent collection of one ticket transfers one reply;
other collectors get STALE. No Future allocation occurs for a command.

Accepted ADD copies the expression/config and retains its immutable zone. It does
not yet imply job admission. The owner admits a separate kernel zone reference
on success, then releases the command reference. A successful reply identifies the
job and has no Data; a failed reply returns original Data and an owned optional
diagnostic. Caller Data remains borrowed until that failure reply is collected,
or a successfully admitted job produces a retirement receipt. ReapAsync returns
the receipt while running; ReapStopped provides serialized kernel handoff after
owner stop. An empty successful reap preserves Retired and sets Found=false.
Cancelling a job stops future occurrences; `CancelRuns=true` additionally requests
cooperative accepted tokens and completes its unsent leases as cancelled.

`Collect` takes a finite nonnegative millisecond timeout. Ready wins before the
timeout/self-wait check. A timed-out accepted request remains live; its output and
Data duty are preserved. All condition waits and mutex contention share the
original elapsed monotonic budget; this is not a real-time OS scheduling bound.
Pending blocking Collect and positive WaitFor reject the native owner and **any**
callback of the borrowed Executor, including unrelated host work. Nonblocking
collection with Timeout=0 is allowed. This uses public `xrtExecutorIsCurrent`.

## Reserved completions and retry

The owner reserves output slots before SchedulerPoll. `CompletionLimit` covers
unsent, accepted and ready-undrained records. An active run-ID mapping uses the
kernel slot index and validates the full ID when a physical receipt arrives.
Output slots are independent of reusable kernel run slots. Full output stops
new dispatch; completing/closing never waits for a consumer to free output space.

`Drain` is a nonblocking bounded pull. Each record carries IDs, scheduled/covered
and observed clock values, coalescing flags, physical result, typed completion and
executor codes, attempt count and an optional owned error. It contains no borrowed
job Data pointer. Original Data can therefore be freed after a separate retirement
receipt even if older metadata remains queued. Worker FAILED diagnostics transfer
from that worker, not the submitting/collecting thread. Invalid result enums are
typed failures. Release each transferred Error with `xrtErrorFree`.

Only native rejection with adapter REJECTED and cause AGAIN is retried. This is
no-effect admission retry; it never replays an accepted business operation.
`RetryAttempts` includes the first attempt. AdmissionMilliseconds=0 disables retry.
Otherwise one saturating monotonic deadline starts with the original observed
sample; RetryIntervalMilliseconds is positive. A min-heap orders the earlier of
the next retry and deadline, with bounded attempts per owner turn. Attempt/elapsed
exhaustion yields EXPIRED/FAILED and retains the last owned rejection diagnostic.
Other failures yield REJECTED/FAILED. Counters distinguish attempts, rejected
submissions, retries and expiry. Close always completes unsent leases as CANCELLED.

## Clocks, wakes and shutdown

Clock=NULL samples UTC xrtNow and monotonic xrtTimer. A custom Clock runs only on
the owner, after Create publishes its output; it may run before Create returns.
Keep its context live through Destroy, return promptly, retain no output and end
all context-using tails before return. A read failure or monotonic regression
records the first typed sticky failure and owned optional cause, then initiates
controlled close. Diagnostic OOM cannot erase the typed failure. Wall jumps use
the existing kernel catch-up/skip/coalesce policy. Consumed cursors never rewind.
No clock callbacks are started after the owner observes closing.

Commands, close, result-consumer credit release and adapter physical-completion
notifications signal an auto-reset event. The owner never resets it after checking
work, preventing a lost wake. Event delivery preserves ambient error and counts
failures; a positive ResampleMilliseconds bounds fallback wall resampling. Sleep
also considers kernel projection and pending retry. Lack of completion/adapter
capacity suppresses an otherwise immediate dispatch timer. Completion notification
tails must leave adapter memory before the owner destroys it and publishes stop.

Close rejects all new commands. Already queued Adds not started by the owner
receive CLOSED plus their Data duty; an Add already being evaluated may succeed
and then be cancelled, retaining its correct successful reply/retirement duty.
Other accepted commands finish. Close(false) permits accepted bodies to finish;
Close(true) can upgrade it to cooperative cancellation. Neither is a synchronous
no-new-body barrier. There is no safe forced destruction of live business work.

WaitFor includes the public XRT thread's physical/context cleanup. It can finish
before replies, completion metadata and retirement receipts are consumed. Destroy
is caller-exclusive and BUSY until stop/join, zero jobs, zero ticket duties and
zero completion reservations. It implicitly consumes no receipt or business Data.
Keep borrowed pool/code/contexts live through Destroy; before unloading extension
callback/module code, the host must physically drain/join its borrowed executor.

## Bounds, cost and evidence

One arena preallocates commands/replies, completion reservations, mapping/retry
heap and batches. All counts are positive; BatchLimit cannot exceed RunLimit or
CompletionLimit; reserved fields are zero. Checked arithmetic reserves
`arena + Scheduler.BytesLimit + AdapterBytesLimit <= BytesLimit` before allocation.
Stats.Bytes reports the currently retained requested arena/kernel/adapter bytes.
The initial complete allocation snapshot is published before Create exposes the
service, independently of whether the first Clock callback has returned. After
physical adapter destruction, AdapterBytes and Stats.Executor.Bytes become zero,
and Bytes equals ArenaBytes + KernelBytes before stop is published. Other executor
fields retain their final lifetime counters and configured limit. Kernel job/clock
and attempt/retry fields remain owner-turn snapshots and may lag ready outputs.
All component fields are published together under the service mutex; their sum is
consistent in every snapshot. This replaces the earlier provisioned/sampled byte
semantics without changing layouts or signatures. These are component requested
bytes, not total allocation-budget charge, live process heap or RSS. Borrowed zones,
Data, pool, diagnostics, callbacks and native/Core bootstrap are outside this cap.
Optional InitializationBudget charges the direct creator allocations and explicit
owner kernel/adapter startup; its pointer is borrowed only during Create. Blocks
retain the quota until their cross-thread cleanup. No business allocation budget
or total process-RSS guarantee is implied.

Normal warmed control/dispatch uses no dynamic requested heap allocation. Hot
lease lookup is direct; retry operations are O(log RunLimit). Cold CancelRuns
scans active kernel slots, and kernel Close scans jobs. FIFO command work, poll
actions and completion/retry batches are bounded per turn. A single recurrence
query can still traverse deep history/a Gregorian cycle: hard owner CPU and P99
bounds are outstanding. Thread-safe hot APIs use a mutex; no lock-free claim is made.
Outputs reject range overflow, mutual/input aliases and owned service arena
overlap; caller keeps them disjoint from all borrowed Data/contexts/zones and
private component storage. Error domain is `xrt.cron.service`; structural argument
errors preserve every output. Typed value failures survive diagnostic OOM.

Run [the real UTC example](../examples/service/README.md). Tests cover finite
timeouts/ticket generations, held-owner close, output credits, physical results,
cooperative cancellation, no-effect retry/expiry, owner/unrelated-pool self waits,
clock failure/regression, wall jumps, concurrent readers and racing collectors,
observed cold allocation prefixes and warmed dispatch. The fixed-arrival benchmark
records scheduled-to-worker start lateness without deleting slow samples, plus
owner-sample-to-worker delay, at millisecond clock precision. Its rate is an input,
not measured maximum throughput. Unpinned smoke is no performance acceptance.
Named-zone database/reload, durable recovery, nominal DST compensation, bounded
recurrence CPU, native Linux and the full long-run/performance gates remain open.

## Reusable deadline backend

The owner initializes the waiter after the scheduler and adapter. Native startup
failure rolls back both components and leaves the caller output unchanged.
Windows prefers one high-resolution nonperiodic timer and uses a standard timer
only when that mechanism is unsupported. Resource failures are reported, rather
than treated as unsupported. POSIX reuses the event condition. Signals still use
EventSet; the owner never resets the event after checking work. No spinning,
periodic timer, resolution change or new shared runtime is introduced.

`Stats.Waiter` records the selected backend, inline storage and native timer
capacity. `ActiveNativeTimers` reports live handles, including a failed cleanup.
Normal stop releases the waiter before publishing STOPPED, even while replies,
completion metadata or retirement duties remain. A native release failure
publishes FAILED with a typed SYSTEM failure; after physical join, caller-exclusive
Destroy retries cleanup before destroying the kernel. A failed retry preserves
the service for another attempt. Backend metadata persists after cleanup.

The developing stats layout gains these fields; configuration and function
signatures are unchanged. Consumers must rebuild with matching generated headers.
Waiter storage is part of ArenaBytes, never counted twice. The native timer handle,
OS scheduling and diagnostics remain outside the requested-byte cap. Backend
selection does not guarantee a maximum lateness. Wake, deadline clock reads and
Windows timer arm/cancel calls are explicit per-wait costs. Fixed-arrival,
CPU/power, pinned throughput and tail-latency qualification remain required.
