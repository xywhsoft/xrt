# Client fixed-arrival workload

`bench_client_openloop.c` complements the closed-loop pipeline and cold setup
workloads. It uses the public client API and contains no production runtime or
new public ABI. This is measurement infrastructure for the full performance
contract; a successful smoke run is not a production or parity claim.

## Arrival and outcome contract

Offer `i` has an absolute monotonic time `start + floor(i * 1e9 / rate)`, evaluated
with bounded integer arithmetic. The generator never moves that time after a
stall, waits for the previous reply before planning another arrival, or retries
a rejected offer. Work is submitted in bounded passes of at most 128 due offers.
Every planned offer gets one record, including:

- admitted requests, with every typed client terminal status;
- observer-slot capacity rejection, separate from client overload/closed/error;
- intended deadlines already expired before admission.

The slot cap bounds retained Futures and callbacks. It is a **harness limit**,
not proof that Redis or the client reached their own capacity. All scheduled
records remain present even when the harness cannot issue them. The final
identity is total offers = admitted outcomes + all admission/driver rejections.
A generator delay contributes to the offered latency and lag distributions.
Overload is reported, not converted into a new closed-loop arrival schedule.

ECHO carries a binary payload with an eight-byte unique ID and deterministic
data, including NUL/CR/LF/high bytes. Every successful reply is validated in the
generator thread. A short Future notification records the observation time;
its Release callback publishes slot reuse only after the watch's notification
accounting. A Future already ready at registration is handled explicitly.
Neither network callbacks nor a held reply block the producer thread.

Successful and all-outcome latency distributions start at **planned arrival**.
Generator lag is actual admission attempt minus planned arrival, including
payload construction. Completion observation is notification entry, or the
registering thread's observation for an already-ready Future. Full payload
validation, watch retirement and result destruction consume time/CPU and can
delay later offers; their post-observation cost also appears in total elapsed
time. Percentiles use nearest rank; a 16-sample P99.9 includes the maximum.
An empty success set prints zero percentiles together with successful=0, and a
normal-mode run with no successes is invalid. Fast rejection must never be
interpreted as successful low latency. Inspect failure counts and throughput
together with both latency distributions.

Each offer has an intended deadline from its planned time. Admission passes
the remaining whole milliseconds, rounded down, to the existing relative client
API. This can expire a request early by less than a millisecond. Preemption
between the clock read and API entry can also delay it; completion at or past
the intended deadline is independently counted regardless of client status.
The benchmark does not pretend that a relative API supplies atomic absolute
deadline admission.

## Limits and instrumentation

The harness allocates its bounded record, slot, free-index, payload and sample
buffers before the measured window. Count is at most 1,000,000; at most 4096
observer slots; payload 8..256 KiB; slot-count times payload at most 64 MiB.
The arrival horizon is at most 30 seconds, intended timeout at most 10 seconds,
and fault injection stall at most 5 seconds. Held-fixture responses additionally
fit its 64 KiB buffer. One client uses at most min(slots,128) in flight and a
client request quota equal to the slot count. Existing client command/response
byte limits still apply. Physical Close verifies zero client requests/command/
initialization bytes before teardown.

The local fixture uses two engine workers (client and peer), plus the generator.
An external loopback Redis uses one client engine worker plus the generator.
Managed HELLO 3 and one PING warmup precede the measured window; warm request
and setup storage retire before measurement starts. Reported backend, mode,
payload, slots, rate, deadline and memory-statistics state are checked facts.

CPU time is the entire benchmark process from measurement start through engine
retirement. Thus local results include peer work; external results do not
include Redis process CPU. The generator polls/yields; its CPU and callback/
validation bookkeeping are included and must not be attributed solely to the
library. Peak RSS/working set is a process-lifetime high-water
mark including warmup and harness storage. Sampled request/command peaks can
miss shorter peaks. Native handles/Linux descriptors are reported before setup
and after teardown; these observations are not a full resource-baseline audit.

The separate `xredis_openloop_stats` profile compiles/enables XRT memory counters.
It reports malloc/calloc/realloc/memdup, heap-block and backing-allocation calls
and bytes from measurement start through teardown. These are process-window
calls, including fixture and cleanup activity. They exclude harness libc
allocation, external-library allocations and server-process memory; they are
not exact incremental client allocations. Instrumented timings are distinct
from the profile without memory-statistics code. CLI counter zeros when statistics
are disabled mean unmeasured; the external diagnostic JSON uses null for those
counters. Payload-copy byte counts are
explicit lower bounds from accepted command and successful response payloads;
they omit vector growth, metadata, other replies and transport/OS copies.
Copies, locking and syscalls are not yet fully traced or accepted.

## Reproduction

```sh
python tools/measure_performance.py --compiler gcc --arch native --config extlibs/xredis/config/performance_profiles.json --profiles xredis_openloop,xredis_openloop_stats,xredis_openloop_stall,xredis_openloop_hold --smoke --repeats 1 --report out/service-extensions/openloop-win-smoke.json
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --test test_openloop --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_tests --single-test test_single_xredis_openloop --cflag=-O2
```

Use separate output paths for concurrent Windows/WSL builds (WSL `--arch x64`).
Each profile explicitly pins its benchmark helper/peer headers as inputs.
CLI positional arguments are count, rate/s, slots, payload bytes, timeout ms,
mode, stall ms, loopback Redis port, and statistics flag. Mode 0 is normal;
mode 1 stalls the generator initially; mode 2 holds fixture ECHO replies until
close. Port zero selects the self-contained fixture. Statistics require the
instrumented executable. The stall/hold profiles verify accounting under faults,
not service throughput.

For a real server, build the WSL performance profiles, then run the disposable
service supervisor with the corresponding executable and build report:

```sh
python3 tools/measure_performance.py --compiler gcc --arch x64 --config extlibs/xredis/config/performance_profiles.json --profiles xredis_openloop,xredis_openloop_stats --smoke --repeats 1 --report out/service-extensions/openloop-wsl-smoke.json
python3 extlibs/xredis/tests/interop/run_redis_openloop.py --client out/performance/gnu/x64/xredis/xredis_openloop --build-report out/service-extensions/openloop-wsl-smoke.json --report out/service-extensions/openloop-redis.json
python3 extlibs/xredis/tests/interop/run_redis_openloop.py --client out/performance/gnu/x64/xredis/xredis_openloop_stats --stats --build-report out/service-extensions/openloop-wsl-smoke.json --report out/service-extensions/openloop-redis-stats.json
```

The supervisor starts only its own loopback Redis with persistence disabled,
uses the same startup/cleanup rules as initialization interoperability, and
records actual executable, source/fixture and supplied-build-report hashes.
The build report describes compilation; the external run records its own
arguments/facts and does not inherit local-fixture measurements.

The deterministic tests cover rational arrival times, non-shifting schedules,
overflow, nearest-rank tails, exactly-once ledger accounting and deadline
equality. Real TCP cases cover binary success, generator-induced expiry without
sending business commands, bounded held-reply overload/timeouts, callback reuse
and enabled allocation counters. Remaining acceptance includes native platform
long runs, stable pinned repeat measurements, larger worker/connection/payload
matrices, matched Hiredis workloads, server CPU/resource budgets, exact copy/
synchronization/syscall accounting and the original four-product release gates.
