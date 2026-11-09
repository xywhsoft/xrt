# Completion readiness

`xcronServiceWaitReadable(Service, Timeout, Issue)` works with ordinary and selected
services. It waits for completion metadata using the existing service mutex and
condition. It consumes no record, reserved credit, owned Error or borrowed Data.
Use the appropriate Drain API afterwards; a competing consumer may have drained
the indication in the meantime. Recheck and wait again if needed. This API does
not reserve the next record or authorize concurrent Destroy.

Timeout must be finite and nonnegative;0 polls. One monotonic budget includes
mutex contention and unrelated/spurious condition wakeups. Ready metadata wins
over timeout or terminal failure. Stopped+empty returns ERROR with typed CLOSED,
or the sticky service failure Code/Cause. Closing alone is not EOF while physical
or unsent outcomes can still arrive. A pending positive wait rejects the owner
and any borrowed-executor callback; polling and observing already-ready metadata
remain permitted. Success/normal timeout preserve caller ambient error. Finish
all calling threads before caller-exclusive Destroy.

The owner registers existing result publication under its mutex. An empty-to-
nonempty transition broadcasts only when result readers actually wait. A native
notification failure preserves the already-published physical result, records
sticky SYSTEM failure and starts controlled shutdown. Wait errors release the
registration. Total-time waits use bounded condition chunks, so unrelated replies
and missing notifications cannot restart the deadline. The waiter counter rejects
saturation rather than wrapping. No runtime, queue or per-wait allocation is added.
Mode checks, mutex/condition operations, counter updates and any actual broadcast
remain costs; no zero-overhead or hard whole-owner CPU bound is claimed.

The ordinary/selected examples and root/static/shared consumers use readiness.
Tests distinguish queue acceptance, native cancellation, physical return, metadata
readability and retirement. They cover competing consumers, spurious wakeups,
ambient errors, owner/pool self-waits, close/failure, diagnostic OOM, counter
saturation and warmed no-requested-heap paths. A single-header diagnostic observes
actual blocked registrations and injects condition notification/wait failures.

The separate `bench_service_readable.c` retains the full expected event map,
nominal checks, physical receipts and Data retirement of the original fixed-
arrival fixture. Runtime argument6 chooses polling0 or readiness1. All original
drivers/profiles remain intact. The new matched controls both establish CPU/heap
baselines before a final positive first-arrival margin check; a crossed deadline
invalidates the sample. `counter_window_guard` and consumer policy are stable
facts, while the actual remaining margin is retained per process. This closes the
separate-snapshot boundary limitation recorded by the earlier polling fixture.
Creation/admission/shutdown, CRT/OS/borrowed storage and Windows CPU clock precision
remain outside the corresponding counter claims.

Run from the root (Linux: `--arch x64`):

```sh
python tools/measure_performance.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --config extlibs/xcron/config/performance_profiles.json --profiles xcron_readable_poll4,xcron_readable_wait4 --smoke --report out/xcron-readable-smoke.json
```

Formal profiles retain one warmup and five runs, P50/P95/P99/P99.9/max start,
owner-to-worker and receipt latency, complete accounting and the unchanged noise
and regression limits. Consumer policy is an explicitly differing workload fact;
do not force it into the same-fact historical regression comparator. First
unpinned/WSL component observations do not establish native production tails,
maximum throughput, mixed load or>=72h acceptance. Compensation, durable recovery
and the other products' full scope remain in the active development goal.

## Validation and first measurements

[Stage evidence](../tests/results/service-readable-20261109.json) records both
34-program root suites, modular/single ASAN+UBSAN/leak-halt and TSan, deterministic
condition failure/coalescing diagnostics, ordinary/selected isolated and root
static/shared consumers, unchanged422-zone oracle totals and90 tool tests.
The final test refinement retains caller error references through identity
comparison; all five affected programs were revalidated without changing
production/config/generated inputs or removing assertions. Controlled preserved-
source replay binaries fail before the sticky-error fix and pass afterwards.

All old public sizes remain unchanged. The new4-byte counter occupies existing
private padding: Windows offset820 before Now824, Linux/WSL1100 before Now1104.
Ordinary owner sizes remain880/1160 bytes, extended888/1168; command440 and hot
reservation184 are unchanged. No retained arena/scratch bytes are added.

Each of36 matched native processes per platform validates1024 pairs/checksum524800
and a positive first-arrival counter margin. Windows four-worker polling/readiness
receipt P99 medians are15/1ms; WSL2/1ms. Readiness1/2/4/8-worker medians are1ms in
both reports. Slow-consumer start/receipt P99 medians are1221/2221ms and1201/2201ms,
with all events preserved. Windows/WSL retain39/31 failed latency noise checks.
Millisecond precision means a zero sample is below measurement resolution.

Unchanged-driver no-readiness throughput controls retain another24 native outputs
and four binaries per platform, with6/14 failed noise checks. A separate WSL
four-worker scalar before/current ABBA replay retains24 outputs and two binaries:
medians193144.472/188165.5225 executions/s, about-2.6%, with relative MAD7.08%/3.61%
and central ranges13.75%/7.22%. Both are too noisy to establish a qualified
regression or absence of overhead. Source-side synchronization, borrowed lifetime,
native mixed-load/endurance and all remaining release gates remain explicit.
