# Selected service fixed-arrival measurements

`bench_service_arrivals.c` pre-admits explicit UTC jobs at immutable real second
boundaries. No custom clock is installed. Admission must finish before the first
arrival; a missed deadline invalidates the process instead of shifting the
schedule. Four periods of256 jobs give1024 expected callback/receipt pairs. Worker
counts1/2/4/8 and initial submission widths1/64 are separate explicit profiles.
All fifteen latency metrics retain the default noise and regression contracts.

Each job/second has a preallocated event with an atomic callback claim. Workers
validate their immutable nominal view, original Data, actual schedule, flags and
observation. The controller maps opaque Job IDs by binary search, rejects repeated
or missing callbacks/receipts, compares every public Run field, checks full nominal
metadata and computes the independent ordinal checksum524800. Full physical
retirement must return every original Data pointer with four successes per job.
No skipped/coalesced range, clock regression, rejected submission, retry or sticky
owner failure is accepted. Close/join/drain/reap/destroy and host pool join are
verified before a process may exit successfully.

The measurements include P50/P95/P99/P99.9/max for wall-clock scheduled-to-worker
start and scheduled-to-Drain-return latency, plus monotonic owner-sample-to-worker
delay. Wall measurements have millisecond precision; monotonic values retain the
service's integer-millisecond observation precision. A busy system may delay any
event, but every expected event remains in the data. The controller polls with
`xrtSleep(1)`; its polling/copying/validation work and result queue wait affect
receipt latency. Worker claims, a shared callback counter, two complete nominal
checks and the ID lookup add fixture costs. This is a fixed offered rate, not a
maximum-throughput test or a minimal-XRT baseline.

The `_slow` profile waits for all256 completion credits to contain physical
results, records that count, then pauses consumption for2200ms. WAIT overlap and
CATCH_UP preserve backlog. After the pause, all1024 events are still required;
the intentionally delayed start/receipt measurements remain in every quantile.
The observed pause duration and full-credit count are retained per process.

Per-process observations also retain actual service/fixture requested bytes,
native grouping counters, waiter backend and malloc/calloc/realloc/block request
deltas. Heap counting starts at its post-admission snapshot and ends after the
last receipt, without an intentional in-process workload warmup. A preceding
snapshot must show zero dispatches/callbacks. The checks and CPU/heap snapshots
are separate operations: if the first deadline is crossed between them, initial
work can precede the counter window. These observations do not prove complete
cold-path allocation/CPU accounting. The fixture uses the explicit `memory_stats`
suite. Native/bootstrap,
borrowed timezone storage, CRT allocations and OS handles are not implied to be
covered by these requested-byte/heap counters.

Process CPU is observed over the same post-admission-to-last-receipt window using
the sum of all process threads: Windows
[GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
and Linux [CLOCK_PROCESS_CPUTIME_ID](https://man7.org/linux/man-pages/man3/clock_gettime.3.html).
These native calls are benchmark instrumentation only. Windows' reported time
unit does not establish effective precision: short workloads can report a zero
delta. CPU is an observation, not a precision-qualified gate or a zero-CPU claim.
Creation/admission and shutdown CPU are outside this observation window.

Run a workload smoke from the repository root (Linux: `--arch x64`):

```sh
python tools/measure_performance.py --compiler gcc --arch native --manifest extlibs/xcron/config/modules.json --config extlibs/xcron/config/performance_profiles.json --profiles xcron_arrivals_scalar1,xcron_arrivals_batch4_slow --smoke --report out/xcron-arrivals-smoke.json
```

Omit `--smoke` for the unchanged five-repeat/one-warmup contract. A smoke uses two
periods and32 jobs; its64 callback/receipt pairs have checksum2080. Reports and
source/binary identities must be preserved even when quality gates fail. These
first unpinned measurements do not establish native Linux, mixed-load, absolute
P99 or>=72h production acceptance. Read the [service contract](service_selected.md)
for lifetime, byte limits and the remaining compensation/recovery scope.

## First measured evidence

[Stage record](../tests/results/service-arrivals-20261109.json) freezes2558 inputs,
seven modular/lean/single diagnostic executables and eighteen formal binaries.
Both platforms retain54 native process outputs (one warmup plus five measured
samples for each of nine profiles). Every process validates1024 pairs and
checksum524800; raw values are checked against every stored metric, observation
and stable fact. Diagnostics include ASAN/UBSAN/leak-halt and TSan for modular and
single forms, full-output catch-up, argument rejection and90 Windows tool tests.
Production Core/service and the prior drivers/profiles remain unchanged.

The first Windows normal start P99 medians are around1ms and receipt P99 mostly
16ms. WSL normal receipt P99 is1..4ms. The slow profile's start/receipt P99 medians
are1235/2219ms on Windows and1203/2201ms on WSL, retaining all1024 arrivals despite
the stalled output channel. Windows/WSL reports retain79/62 failed noise checks.
These observations motivate isolating the polling consumer wait, not changing
timer resolution globally or claiming qualified latency. Precise first-arrival
alignment of CPU/heap snapshots, real mixed-load/native acceptance and>=72h gates
remain required.
