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
library. The benchmark also reports generator-thread CPU for that window.
On Windows and Linux the observer records user and kernel CPU for both scopes.
The comparator checks each scope's split against its total and reports all
available fields. Linux can adjust process and thread CPU splits independently.
Use process total minus generator total for CPU outside the generator.
[Linux CPU accounting](https://github.com/torvalds/linux/blob/v6.18/kernel/sched/cputime.c).
For successful replies, `response_p*_us` measures directly from the timestamp
immediately before submission to the observation used for `success_p*_us`.
The response interval includes submission, transport, the server and reply
delivery. `generator_p*_us` measures offer-to-submission delay for all scheduled
offers, so the two percentile series have different populations.
Peak RSS/working set is a process-lifetime high-water
mark including warmup and harness storage. Sampled request/command peaks can
miss shorter peaks. xredis takes these snapshots at a 1 ms target cadence to
limit contention with the client worker. Its exact observer window peak and
final physical-ownership checks are independent of these snapshots.
Native handles/Linux descriptors are reported before setup
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
and enabled allocation counters.

## Hiredis reference comparison

Build the same observer with and without REDIS_OPENLOOP_HIREDIS for the
single-connection RESP3 TCP ECHO comparison. Both paths negotiate HELLO 3
and warm up with PING. They use the same scheduled arrivals, binary payload,
credit limit, timeout and reply validation. Hiredis drives its nonblocking
socket and reply parser on the generator thread. xredis uses one XRT I/O
worker and the generator. Latency ends when the Future notification or
Hiredis reply parser exposes the reply to the observer. CPU measurements
include both the generator and any client worker.

The comparator rejects local/mock peers, fault mode 2 and allocator
instrumentation. It fails a run if an outstanding Hiredis command does not
drain by its scheduled deadline. A command already on that FIFO connection
cannot be discarded independently. The paired runner checks every offer,
reply and deadline, alternates client order, and keeps the warmups, raw output,
and executable and source hashes. The report distinguishes verified reply
counts from variation checks. With a dedicated Redis fixture, the command
ledger compares the peer's counts with admitted commands that returned valid
replies. Offers rejected locally can still fail the full comparison while
the peer command counts reconcile. Variation requires five or more measured
samples, at most 3% relative MAD and at most 6% relative central range.

Build Hiredis 1.3.0 from its codeload tag archive. The archive SHA-256 used
in this comparison is 25cee4500f359cf5cad3b51ed62059aadfc0939b05150c1f19c7e2829123631c.
[Hiredis release](https://github.com/redis/hiredis/releases/tag/v1.3.0)
From the repository root, point HIREDIS_SRC at the extracted source, and
HIREDIS_BUILD at an output directory.

    cmake -S "$HIREDIS_SRC" -B "$HIREDIS_BUILD" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
      -DBUILD_SHARED_LIBS=OFF -DDISABLE_TESTS=ON -DENABLE_SSL=OFF
    cmake --build "$HIREDIS_BUILD" --target hiredis

These GNU/Linux commands build both observers from the checked-in single
headers. On Windows with MinGW GCC use WIN32_LEAN_AND_MEAN in place of
_GNU_SOURCE and link with -lws2_32 in place of -pthread -lm. Add -lcrypt32
after the Hiredis archive on Windows. With CMake 4, use
-DCMAKE_POLICY_VERSION_MINIMUM=3.5 for the Hiredis configuration.

    flags=(-std=gnu11 -O2 -Wall -Wextra -Werror -D_GNU_SOURCE
      -DXREDIS_MODULE_XREDIS_CLIENT -DXRT_IMPLEMENTATION -DXREDIS_IMPLEMENTATION
      -Iinclude -Iextlibs/xredis/include -include xredis/features.h
      -include ./single/xrt.h -include ./single/extlibs/xredis.h)
    observer=extlibs/xredis/bench/bench_client_openloop.c
    gcc "${flags[@]}" "$observer" -o out/xredis-echo -pthread -lm
    gcc "${flags[@]}" -DREDIS_OPENLOOP_HIREDIS -I"$HIREDIS_SRC" "$observer" \
      "$HIREDIS_BUILD/libhiredis.a" -o out/hiredis-echo -pthread -lm
    taskset -c 0 redis-server --bind 127.0.0.1 --port 16379 \
      --save "" --appendonly no --io-threads 1

Use a dedicated unused port and select available CPUs on separate physical
cores for the clients and fixture. In a second terminal, run the pair and
retain Redis INFO commandstats and INFO cpu before and after the run.

    taskset -c 4,8 python3 extlibs/xredis/bench/compare_hiredis.py \
      --xredis out/xredis-echo --hiredis out/hiredis-echo --port 16379 \
      --count 20000 --rate 5000 --window 16 --payload 32 --repeats 7 \
      --report out/xredis-echo-comparison.json

The offered rate is fixed. Successful throughput measures how much of that
rate the pair delivered at this CPU and connection budget.

On Linux, set `XREDIS_OPENLOOP_BACKEND=epoll` or
`XREDIS_OPENLOOP_BACKEND=uring` on the paired command to select the
xredis observer's external TCP engine. Build the xredis observer with
`-DXRT_MODULE_NET_PORT_URING` to enable io_uring. Unset the variable or
use `auto` to use XRT's default. The JSON records the active xredis
backend under `network_backend` in each run's facts.

Set `XREDIS_OPENLOOP_WORKERS` to a count from 1 to 8 for the external
xredis observer. The default is one. The count must fit the number of
connections, which are assigned round-robin to the workers. The output
reports the engine's actual worker count as `workers`. Keep the observer
CPU budget, connections and window fixed when comparing worker counts.
With the paired runner, use `--xredis-workers` to set the count. The Hiredis
observer uses its single event loop.

Add `--raw-dir out/redis-echo-traces` to keep one CSV for each observer
process, including warmup runs. Use a new directory for each comparison.
The paired JSON records each file name and SHA-256. Rows follow scheduled
offer order and include the due, submit and finish timestamps in monotonic
nanoseconds, the admission outcome, request status and reply-validity flag.
Export happens after timing and peak memory sampling.
To attribute tail latency, select the slowest offers by finish minus due
and sum the two intervals for those same offers. Submit through finish
includes socket, server and client reply handling.
[Numeric outcome definitions](../bench/openloop_schedule.h).

At high connection counts, `xredisclientconfig.Replies.Reader.MaxDepth`
controls the memory reserved for reply nesting. It also bounds the nesting
the client accepts. On the Ubuntu epoll lifecycle fixture, a depth of 8
reduced the median idle RSS growth by about 34.5 MiB across 10,000
connections compared with the default of 64. Fixed-arrival GET/SET runs
at depth 8 had higher median P99 and greater P99 variation in both
measured cohorts. [Capacity and latency measurements](../../../docs/development/service-extensions-progress.md#2026-11-24-redis-client-parser-depth-and-connection-memory).

### Native Linux fixture and CI

On Linux, the paired runner can start a disposable loopback Redis process
using `--server /path/to/redis-server --fixture-cpus 0,2,4`. Use three
available logical CPUs from separate cores. The first runs Redis and the
other two form the shared CPU budget for the client observers. The runner
checks the spawned PID, CPU masks and Redis command counts, including the
setup traffic. Use `--fixture-tls` with the TLS observer builds to give the
fixture temporary certificates. Its logs and the public certificate hashes
are retained with the report.

Enable `xredis_hiredis` when manually dispatching the service-extensions
workflow to collect native Linux samples. The Redis job uses checksum-pinned
Redis 8.0.5 and Hiredis 1.3.0. Each transport runs five alternating measured
pairs after warmup, with four RESP3 connections, 2,000 mixed GET/SET offers
per second, 1,024-byte values and a 128-offer window. The artifact contains
both binaries per transport, revision, host and toolchain evidence, paired
reports and per-offer CSV. The reports retain the measured variation result
for each client. Repeatable release baselines require measurements on a
controlled host.

### Multiple connections and mixed GET/SET

The same binaries support up to eight independent connections with a shared
observer window. Append the two switches below to the paired command.

    --connections 4 --mixed

One quarter of the planned offers on each connection are SETs, with three
GETs after each SET. Both paths assign offers round-robin by index. The
clients negotiate RESP3 and warm up each connection. One setup SET seeds a
binary read key before timing starts. Measured writes use a separate binary
key and include the offer index in their payload. Each admitted operation
checks the exact seeded GET value or the SET status reply.

The report includes accepted GET and SET ledgers, observer window peaks and
separate rejection causes for every scheduled offer. A pair is verified
when every planned operation finishes successfully within its deadline.
Redis commandstats can be checked against the accepted counts plus one seed
SET per mixed sample and one HELLO and PING per connection. The report includes
submission-to-observation P99, generator-thread CPU and total process CPU.
Hiredis runs its nonblocking socket and parser on the generator thread.
xredis samples native request and command-byte counters across the selected
connections.

### Plaintext socket I/O accounting

Run each existing observer under the same tracer against the dedicated
Redis fixture. The arguments below offer 4,000 mixed operations at
2,000/s on four RESP3 connections with 1,024-byte values.

    strace -ff -qq -yy -s 1 -e trace=%network,read,write,readv,writev \
      -o /tmp/xredis-io -- out/xredis-echo 4000 2000 128 1024 1000 0 0 "$redis_port" 0 4 1

Choose a distinct trace prefix for the Hiredis observer. Write the
traces on the Linux filesystem and copy them after the run. Count I/O
only on the TCP descriptors connected to the fixture port across all
traced threads. Record successful byte transfers and EAGAIN separately.
The totals include connection setup, the seed write and retirement.
Verify the per-response results and the fixture's GET, SET, HELLO and
PING counters before comparing the syscall totals. Use untraced runs
for throughput and tail validation.

### Verified TLS comparison

Use the same open-loop observer with `REDIS_OPENLOOP_EXTERNAL_TLS` defined
for both builds. Select `XREDIS_MODULE_XREDIS_TLS` for the xredis binary. For
the Hiredis binary, also compile the pinned Hiredis `ssl.c` and link that
object with `libhiredis.a` and matching OpenSSL 3 or later SSL and crypto
libraries.
The Hiredis observer uses XRT's clock and runner utilities. Its build can
omit `XREDIS_IMPLEMENTATION` and the xredis single-header forced include.
Record the OpenSSL runtime version and library hashes along with the binary
hashes. This OpenSSL dependency is specific to the reference observer.

Use an isolated Redis instance with plaintext disabled and configure
`tls-protocols TLSv1.3`, `tls-ciphersuites TLS_AES_128_GCM_SHA256`, and
`tls-auth-clients no`. Use an RSA server certificate with a DNS SAN and a
PEM CA file. The example fixture can generate those certificates.
[Redis TLS interop fixture](../tests/interop/run_redis_tls.py)

Append the CA and expected DNS name to the paired run.

    --tls-ca /path/to/ca.pem --tls-server-name localhost

Both clients verify the chain and DNS name and constrain the handshake to
TLS 1.3, AES-128-GCM and X25519. They negotiate RESP3 and verify PING before
timing starts. Preserve the server command ledger and retain failed offers in
the latency report. The reported CPU and throughput are for established
connections. Handshake, mutual TLS, TLS 1.2 performance, native Linux,
stable release tails, exact syscall/copy counts and 72-hour acceptance
have separate evidence requirements.

## Continuous mixed workload

The mixed workload holds two shared pool connections, one Tracking
connection and one Pub/Sub subscriber on two XRT I/O workers. Each
round pipelines eight SETs, pipelines eight GETs and verifies every
binary reply. It reads and replaces one tracked key and verifies the
invalidation's key and receive order. Then it sends one publication and
checks one recipient and the exact binary payload. Payloads alternate
between 32 and 1,024 bytes, with one larger payload every four rounds.
The same eight disposable keys and one private channel are reused.
This is a paced closed-loop workload for continuous validation.

Build the host against the checked-in single headers from the repo root.
Use GCC on Linux or MinGW GCC on Windows. On Windows give the binary an
`.exe` suffix. Keep the binary's `.build.json` receipt with the run.

    python3 tools/build_service_mixed.py xredis --output out/redis-mixed

Run the supervisor on the candidate client OS using a local disposable
Redis. The server must be dedicated to this test because the observer
reconciles the entire Redis command delta. Use a new report name for each
attempt.

    python3 extlibs/xredis/tests/interop/run_redis_mixed.py \
      --server /usr/bin/redis-server --client out/redis-mixed \
      --seconds 60 --pause-ms 3 --report out/redis-mixed-smoke.json

Use --port instead of --server for an already isolated loopback service.
Record the Redis host and build in that case. The supervisor checks server
identity and connection count before and after the workload. JSONL windows
record round counts, CPU, P50/P95/P99/P99.9, and client memory and handle
counts. A supervised Redis also gets process resource samples. Round latency
includes all 19 commands and both delivery validations. Pacing sleeps are
outside that latency and inside the run's wall time. Resource-growth guards
are 64 MiB of private memory and 32 handles/descriptors above the first
sample. The client verifies zero outstanding work at physical close.

For the 72-hour run use --seconds 259200. The native-client result requires
successful teardown, exact command accounting, continuous clock progress
and resource observations on Windows or Linux. New runs verify the
requested interval using both monotonic and awake time. The journal
keeps UTC corrections and checks the difference between uptime and
awake time for suspension. WSL and shorter runs
remain diagnostic. A server crash or process interruption terminates the
run without a successful result. Recovery scenarios have separate tests
and evidence. This workload covers the TCP RESP3 shared-pool, Tracking
and direct Pub/Sub paths. Keep separate qualification for other
deployment topologies, larger loads, and injected failures.

Keep the client binary, its `.build.json`, and both report files. Reconcile
them after the run with the offline checker. Use `--require-native-72h`
for release evidence and omit it for diagnostic runs.

    python3 tools/check_service_mixed.py xredis --binary out/redis-mixed \
      --report out/redis-mixed-72h.json --require-native-72h

### The same workload over verified TLS

Build a fresh client with `--tls` on the build command. Supply
`--tls-directory` with `ca.pem` and `server.pem` for the `localhost` identity.
With `--server`, also provide `server.key`. The supervisor starts a
TLS-only listener or connects to the TLS endpoint supplied by `--port`.

    python3 tools/build_service_mixed.py xredis --tls --output out/redis-mixed-tls
    python3 extlibs/xredis/tests/interop/run_redis_mixed.py \
      --server /usr/bin/redis-server --client out/redis-mixed-tls \
      --tls-directory /path/to/fixture --seconds 60 --pause-ms 3 \
      --report out/redis-mixed-tls.json

The four workload connections and the independent INFO observer use
TLS 1.3 with AES-128-GCM and verify the certificate. The observer also
checks the expected leaf before and after the run. The same Redis
command, latency, duration and resource checks apply to both transports.
Use certificates valid through the final observation for an endurance run.

## Recovery across a Redis process restart

This separate integration probe keeps one xredis process, shared pool and
subscription session alive while replacing a disposable Redis server on the
same loopback port. The harness observes both the original blocked pool call and a pipeline on the
Tracking connection before stopping its Redis process. The pipeline has a
completed binary `ECHO`, a blocked `BLPOP` and an `INCR` queued behind it.
`CLIENT LIST` confirms the full mutation frame is buffered and the command
ledger confirms it has not executed. The received binary reply retains its
sequence. Both pending calls report `DISCONNECTED` and `MAY_HAVE_EXECUTED`
with no reply sequence. The probe checks all three results after engine teardown. The probe checks a disconnected
MAY_HAVE_EXECUTED outcome, preserves it across recovery and observes rejection
while both pool members are gone. After replacement it checks both restored
members, automatic replay of the binary subscription intent, fresh Tracking
setup on a new APP-owned connection, an ordered invalidation, one matching
binary publication and 16 pipelined binary replies. The replacement Redis command ledger records zero BLPOP, INCR and PING
executions.

Run the Python harness on the same host as the disposable Redis server.
For MinGW, use `-DWIN32_LEAN_AND_MEAN` in place of `-D_GNU_SOURCE`, an `.exe`
output and `-lws2_32` in place of `-pthread -lm`. If the TLS module is enabled,
link `-liphlpapi -lbcrypt -ladvapi32` as well. The WSL supervisor can launch
the Windows probe. Pass fresh screened loopback ports with `--port-candidate`
when using that setup. The reports preserve server identities, phase results
and command ledgers alongside each process log.

    mkdir -p out
    flags=(-std=gnu11 -O2 -Wall -Wextra -Werror -D_GNU_SOURCE
      -DXREDIS_MODULE_XREDIS_POOL_SHARED_CONNECT -DXREDIS_MODULE_XREDIS_TRACKING
      -DXREDIS_MODULE_XREDIS_SUBSCRIBER_NOTIFY -DXREDIS_MODULE_XREDIS_SUBSCRIPTION_SESSION
      -DXRT_IMPLEMENTATION -DXREDIS_IMPLEMENTATION -Iinclude -Iextlibs/xredis/include
      -include xredis/features.h -include ./single/xrt.h -include ./single/extlibs/xredis.h)
    gcc "${flags[@]}" extlibs/xredis/tests/interop/redis_restart.c \
      -o out/redis-restart -pthread -lm
    python3 -B extlibs/xredis/tests/interop/run_redis_restart.py \
      --server /usr/bin/redis-server --client out/redis-restart \
      --report out/redis-restart.json

Recovery time runs from the client's observation of the old physical closes
to verification of resumed traffic. It includes the forced outage barrier,
fixture restart and workload verification. It is diagnostic for this local
setup. The default profile uses standalone RESP3 over plaintext TCP.

Run the separate `--lost-reply` case to exercise an already executed
mutation. A single-use loopback relay forwards a binary `ECHO`, `INCR` and
binary `ECHO` to the real Redis fixture. It forwards the first reply
and the selected number of bytes from the integer reply. An independent
`GET` verifies the mutation before the relay sends EOF.

Use `--reply-bytes` from 0 to 4. With zero bytes the two pending calls
report `DISCONNECTED` and `MAY_HAVE_EXECUTED`. A partial integer reply
reports `PROTOCOL` with the same delivery certainty. With all four
bytes the increment keeps its validated integer result, and the
remaining call reports `DISCONNECTED` and `MAY_HAVE_EXECUTED`.
The client verifies its retained results after reconnect and engine
teardown. Redis records exactly one `INCR` and preserves the value
`1` in every case. The default cut is three bytes. Use two screened
`--port-candidate` values when supplying ports for this case.

    python3 -B extlibs/xredis/tests/interop/run_redis_restart.py \
      --server /usr/bin/redis-server --client out/redis-restart \
      --report out/redis-lost-reply.json --lost-reply --reply-bytes 3

Add `--tls` and use a TLS-enabled probe and Redis to repeat these cuts
after RESP3 negotiation. Both loopback legs use verified TLS 1.3
with AES-128-GCM. The relay and Redis have distinct localhost
certificates from the temporary test CA. The relay forwards the
selected RESP bytes in authenticated TLS records, then sends TLS
close_notify. The command ledger includes one HELLO through the
relay and one HELLO on the direct reconnect.

Pass `--edit-subscriptions` to exercise a four-edit sequence during the
outage. The replacement exposes the final subscription revision. Its
new channel receives a binary message and the removed original and
intermediate channels have zero subscribers. The independent observer
uses `PUBSUB NUMSUB` before teardown and again after teardown. The old
generation's retained Tracking reply, invalidation and Pub/Sub message
are checked after engine destruction. Tracking results and invalidations
are paired within their own generation.
[Redis PUBSUB NUMSUB](https://redis.io/docs/latest/commands/pubsub-numsub/)

For verified TLS, add `-DXREDIS_MODULE_XREDIS_TLS` to the build and pass
`--tls` to the harness. Use TLS-enabled Redis and OpenSSL on the fixture
host. The harness generates temporary keys and three distinct localhost
certificates. The original and final Redis instances use certificates from
the same CA. The intermediate instance uses a separate CA. All three
listen on the same loopback port with TLS 1.3 and AES-128-GCM.

The C probe requires a typed certificate verification failure, failed pool
and subscription reconnects, and continued unavailability during the
untrusted interval. The independent Redis ledger requires zero application
commands on that listener. After another restart, the C client verifies
the new trusted certificate and exercises the same pool, Tracking, Pub/Sub
and pipeline workload. TLS recovery time includes the untrusted interval
and the final restart. Public certificate fingerprints are recorded and
the temporary identity directory is removed at exit.
