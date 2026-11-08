# HTTP benchmark timing

The matched TCP/TLS benchmark records four separate monotonic instants for every
offer: planned arrival, actual submission, terminal Watch observation and result
collection. Pending Watch callbacks capture their clock reading without waiting,
then publish it with release/acquire ordering. The caller retains Future and
Watch storage through WatchRemove, which joins in-flight notifications before
reuse/free. READY registrations cannot recover a historical timestamp; their
caller observation is an explicit upper bound, counted separately.

Planned-to-notification latency includes late arrivals and queueing. Submit-to-
notification retains admission/encoding/network/server/Future work. Collector lag
is measured separately and does not inflate those two populations. Refusals enter
both latency populations at admission return. Every accepted terminal result is
checked; failures and refusals remain counted. P50/P95/P99/P99.9 use nearest-rank
percentiles over the complete offer population.

The default `yield` mode uses a yielding load generator instead of Sleep(1).
`sleep` remains available for diagnosis with the same completion observer. Yield
can consume a CPU core, and scheduling contention/late offers remain visible.
Native process user+kernel CPU includes ALL process threads and the load generator,
as specified by [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
and [getrusage RUSAGE_SELF](https://man7.org/linux/man-pages/man2/getrusage.2.html);
it is not isolated protocol CPU. These benchmark-only Windows GetProcessTimes /
Linux getrusage calls do not add a production Core API or runtime. Array allocation,
setup/handshake/readiness, sorting, reporting, optional trace and shutdown are
outside timing. Watch registration, clock/atomic work and result validation /
destruction during the run are included. Harness memory is reported explicitly.
Short low-CPU/bursty Windows samples in this run reported zero CPU deltas.
The native counters are not fine-grained instrumentation; zero must not be
interpreted as zero protocol CPU. The dedicated busy-work test checks counter progress separately. These
values are not hardware cycle counts or per-operation CPU precision claims.

CLI: `COUNT RATE MODE SECURE [yield|sleep [TRACE.csv]]`, where MODE is 0 for a
call, 1 for notification or the batch size; SECURE is 0/1. An optional CSV contains
one row per offer with index, four timestamps relative to timed start, admission,
status, delivery and READY provenance. It is written after timing. A trace I/O
failure makes the invocation fail. Standard profile reports retain all aggregate
repeat samples; they do not persist per-offer traces unless a path is supplied.

The dedicated measurement test checks a deliberately delayed collector, READY
upper bounds, cancellation without completion, detach-before-free, publication /
removal races, a one-byte allocation quota and process CPU progress. Production
protocol code, public API and runtime ownership are unchanged. Performance still
requires stable pinned environments, native Linux, all worker/connection/payload/
fault/slow/cold variants, representative comparators and 72-hour qualification.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_http_measure_tests --cflag=-O2
python tools/measure_performance.py --compiler gcc --arch native --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_http_server_tls --smoke
python extlibs/xrpc/tests/interop/http_measurement.py
python extlibs/xrpc/tests/interop/http_measurement.py --burst --label windows-burst
```

Use `--arch x64` for the Linux binaries selected by the peer script, and use a
distinct `--label` for every retained diagnostic variant. `--binary` overrides
the default benchmark path, including for an ASan/UBSan build. The independent
CSV oracle recomputes counts/provenance/percentiles/elapsed/CPU/harness memory,
checks trace I/O failure and can exercise actual burst admission refusals. Run
performance measurements serially after compilers, sanitizers and diagnostic
loads have stopped. A smoke or diagnostic trace is not a stable baseline.
