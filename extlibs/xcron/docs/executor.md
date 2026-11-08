# XRT execution adapter (developing)

Select `XCRON_MODULE_XCRON_EXECUTOR`. This leaf adds an actual execution path for
scheduler leases through a borrowed public XRT Executor. It creates no runtime,
thread, timer or executor. Its creating native thread owns Submit, Cancel,
CancelJob, Drain, Close, Inspect and Destroy. Callback/token inspection is the
only worker-side surface. A future live driver can use a dedicated scheduler
owner or precomputed recurrence work; full history searches must not block
latency-sensitive network callbacks. Timer integration and durability remain.

## Lease and data flow

1. Owner polls the scheduler, obtaining running leases.
2. Submit copies a lease to a preallocated slot and submits that slot to XRT.
   Acceptance transfers the duty to return a receipt. Rejection transfers
   nothing: the owner must Complete the lease or explicitly retry its unsent work.
3. XRT calls Proc on a worker with borrowed Run, token and callback Context.
   Proc must finish every data-using tail before returning. The token cannot
   escape that scope. Invalid result enums normalize to FAILED/RESULT. FAILED
   takes the worker's ambient error into an owned completion; successful and
   cancelled results do not borrow an unrelated owner/previous-work error.
4. XRT's completion/drop callback publishes one receipt even when XRT cancels
   queued work without calling Proc. Drain transfers the receipt and Error
   reference to the owner; it releases adapter capacity, not the scheduler lease.
5. Owner calls SchedulerComplete exactly once, frees Error, and eventually Reap
   returns the job's original Data. Only then may that job Data be destroyed.

IDs belong to one originating scheduler per adapter. The adapter cannot verify
external lease authenticity. Active duplicate IDs are rejected; a completed
lease must not be submitted again after Drain. It provides no business-effect
exactly-once claim. Executor, callbacks, Context and NotifyContext are borrowed
through successful adapter destruction. Never destroy a borrowed executor while
it can still hold adapter submissions. Callback/library code must remain resident
through that executor's physical drain/join before unloading the extension.

## Capacity, cancellation and completion tails

One allocation contains the adapter, RunLimit slots and a power-of-two lease-ID
hash with load <=50%. Checked sizes obey BytesLimit. Backward-shift deletion
avoids tombstone accumulation during reuse. An owner free list and bounded
single-consumer completion batches require no per-job heap allocation. Kernel
and XRT executor limits also apply; XRT QueueLimit is per worker, not a global
queue. Retained zones/job data/executor storage and callback/diagnostic allocations
are outside this adapter's byte cap. A shared-executor rejection retains its XRT
error cause and publishes a typed adapter issue, including under diagnostic OOM.

Cancel requests an atomic per-slot token. Before Proc, a queued request suppresses
the call and returns CANCELLED. A running Proc cooperatively observes cancellation
and chooses its result; a late request cannot rewrite an already finished result.
CancelJob counts newly requested tokens, not inferred stopped bodies. Close stops
Submit; its CancelRuns option can upgrade existing token requests. Neither Close
nor Destroy closes/cancels a borrowed shared executor. Coordinate SchedulerCancel
or SchedulerClose separately to stop new recurrence leases.

Producers push completion nodes with release CAS; the owner detaches batches with
acquire exchange. Producers never read older nodes, and never access their own
slot/Data/Error after publication. The owner may therefore reuse a drained slot
even while its previous drop callback is notifying the host. Detached finite
batches drain before newer batches, without scanning all slots or reversing a
whole batch in one call. Completion order across workers is unspecified.

A separate producer reference remains live through publication and optional
Notify. Destroy fails BUSY until both all receipts are drained and these tails
are zero. Thus user Data can retire after its physical result, while adapter and
notification context remain protected independently. Notify runs on the actual
drop thread: a worker, or the owner when XRT cancels queued work there. It must
not block or enter adapter operations. Owner notification reentry and other-thread
API calls fail with typed errors. If XRT selects Proc on the adapter's owner
thread, it is refused as FAILED/OWNER_CONFLICT, preventing owner-state reentry.

Notify can signal a caller-owned event or post a wake. Its resource must live
through adapter Destroy. Any asynchronous work started by Notify needs a separate
host lifetime and must drain before its own context is released; the producer
reference covers the Notify call, not an untracked asynchronous callback. Notifications can coalesce or arrive after a receipt
was drained; Drain is authoritative. Use bounded wall/monotonic resampling to
recover from failed wake delivery. No blocking wait API is added here, so the
adapter cannot accidentally wait for its own executor worker. Timer/engine and
durable hand-off contracts are separate unfinished work.

## Cost, errors and validation

Steady submission copies one64-byte lease, resets one cancellation atomic, adds
one producer reference, hashes the ID and invokes bounded public XRT Submit.
Each worker checks its native thread identity and token before invoking Proc.
Completion uses a shared CAS head, one producer decrement and optional Notify;
Drain copies one80-byte completion and deletes its hash entry. Owner operations
check native thread identity. Hash probing/CAS contention, copies, synchronization,
XRT bookkeeping and cold allocation are real costs, not zero-overhead claims.
Typed Issue is8 bytes, config56, completion80 and stats64 on current x64 targets.

API argument/span/output-alias errors preserve every output. Other failures
publish Issue even if allocating diagnostics fails. Successful owner operations
preserve its ambient error. All output arrays must be disjoint from adapter/input
storage and each other; caller also keeps them disjoint from borrowed context,
job data and scheduler storage. Counters saturate at UINT64_MAX. CancelJob and
Close(cancel=true) are O(RunLimit); hash operations have bounded worst-case probes.

The [offline example](../examples/executor/main.c) executes on real XRT workers
and returns completion/retirement to the owner, with full shutdown:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_executor --no-single --cflag=-O2
python tools/measure_performance.py --config extlibs/xcron/config/performance_profiles.json --profiles xcron_executor_1,xcron_executor_4 --smoke
```

Tests cover real queue saturation/native queued cancellation, worker errors,
invalid results, callback/owner-notify reentry, close and notification tails,
reuse of a slot before its previous notification exits, real scheduler/Data
retirement, randomized63-slot hashing and32256 many-worker receipts per program,
OOM/aliases/overflow and10000 warmed allocation-free cycles. These do not establish
native Linux, production or72h acceptance. The WSL TSan-instrumented complete unit
also runs without a reported race. GCC warns that atomic_thread_fence is not
supported by its TSan instrumentation; the warning is retained, so this is not
proof about fence-only synchronization or the entire XRT runtime. Reproduce the
separate instrumented unit in a Linux checkout with:

```sh
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tests --test test_executor --no-single --no-examples --cflag=-O1 --cflag=-fsanitize=thread --cflag=-fno-omit-frame-pointer --cflag=-Wno-error=tsan --ldflag=-fsanitize=thread
```

Benchmarks include1/4-worker64-item batches and batch P99, plus an explicitly
minimal XRT submit/counter baseline. That baseline lacks lease copies, token,
ID hash, owner completion records and error transport; it is an algorithm-cost
reference, not a feature-equivalent competitor. Smoke runs do not establish
accepted throughput or tail latency. Mixed arrivals, slow bodies, queue wait,
adversarial contention and end-to-end scheduling latency remain to qualify.

## Bounded atomic admission

SubmitBatch accepts1..XCRON_EXECUTOR_BATCH_MAX (64) already-ready leases. It never
waits to collect more work. All inputs are checked, slots and duplicate-ID entries
are reserved, and one public XRT SubmitBatch call copies their descriptors. A
duplicate anywhere, capacity/closed failure or native rejection returns every
new reservation and transfers no physical completion duties. Existing leases
survive rollback. Caller arrays must also be disjoint from adapter-owned storage.
Each admitted lease retains its independent token, result, Error and Drain duty;
there is no completion ordering promise. Counters count leases rather than calls.

Native atomic batch capacity is one worker queue. A rejection can occur even when
the pool has enough aggregate space for separate scalar submissions. The caller
still owns every rejected lease and may retry, fall back to scalar submission,
or Complete them itself. Typed native causes survive diagnostic allocation failure.
No extra instance storage is allocated. At most64 native descriptors (2048 bytes
on verified x64) occupy the call's stack, plus ordinary locals/callee frames.
Temporary reservation links reuse inactive slot Next fields; they are traversed
only on rejection, when native acceptance guarantees no callback. After success
the owner never reads those links, which Drop may immediately overwrite.

The [two-fold batch example](../examples/executor_batch/README.md) demonstrates
the selected equivalent. The ready-array benchmark separates scalar and batch
APIs using the same complete payload/nominal checks, actual workers and shared
callback counters. Batch size is an explicit fact; it does not rewrite or replace
the original scalar driver or its unqualified historical reports.

The [batch stage evidence](../tests/results/executor-batches-20261109.json)
retains complete validation, rejected runtime-tuning experiments, exact mutex
call counts, all scalar/batch samples and unqualified noise failures. It does not
establish linear scaling of arbitrary scalar callbacks or production acceptance.
