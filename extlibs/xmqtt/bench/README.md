# MQTT client QoS workload

`bench_client_qos2.c` is a closed-loop, one-connection TCP workload with one
observer and two host IO workers. It supports MQTT 5.0/3.1.1, QoS 1/2, sender-only
or duplex publish/echo, window 1..64, cold or internally warmed connection state,
and separately selected XRT heap instrumentation. The in-process peer is a small
healthy fixture, not a broker, independent protocol oracle or comparator.

Run the registered QoS-2 profiles:

```text
python tools/measure_performance.py --compiler gcc --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_qos2_sender,xmqtt_qos2_echo --smoke --report out/qos2-smoke.json
python tools/measure_performance.py --compiler gcc --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_qos2_sender,xmqtt_qos2_echo --report out/qos2-measured.json
```

Create `out/` first if needed. The tool compiles the selected modules, captures
source/tool/environment fingerprints and retains all measured samples. The
second command uses the frozen five repeats / one process warmup, relative MAD
3%, central range 6%, and existing 5% throughput / 10% P99 regression limits.
A smoke pass verifies build/workload shape. It is not performance acceptance.
Noise failures stay failed; the profile policy remains `baseline-unpinned`.
No threshold is changed for this workload. Windows and WSL runs do not establish
native Linux or pinned broker/production qualification.

The generated benchmark executables also accept:

```text
COUNT VERSION QOS WINDOW ECHO INSTRUMENT WARMUP
200000 5       2   32     1    0          4096
```

COUNT is 1..1000000; VERSION is 4 (3.1.1) or 5; QOS is 1 or 2; ECHO and
INSTRUMENT are 0 or 1; WARMUP is 0..1000000. Instrumentation requires a binary
built with the memory-stats module. QoS 1 uses the same fixture/observer for
functional and workload comparisons; it performs a different protocol and is
not a QoS-2 regression baseline. Existing QoS-1 benchmarks remain available.

The registered hot runs measure 200000 operations after 4096 internal warmup
operations. Cold/hot instrumented runs measure 20000 with internal warmup 0/4096.
The tool's separate process warmup launches a new process and does not warm the
next process's client tables. Cold means first client/state use after handshake,
not cold executable pages, CPU caches, OS routing or machine power state. A
32-byte payload carries slot/sequence values, a checked checksum and unique echo
correlation. The native ID can wrap; fixture request ids and outbound echo ids
use separate namespaces. Every typed outcome and ACK phase is checked.

Sender latency ends when the observer sees the final ACK result (PUBACK for
QoS1, PUBCOMP for QoS2). Duplex latency additionally waits for the owned echo
message and the peer's atomic publication that it actually observed the final
receiver ACK. Taking a message alone is insufficient. Sequence stamps publish
fixture state across threads; slots are not reused while either exchange lives.
The phase wall/CPU endpoint additionally destroys observations and waits for
physical client request counters to return to zero. Per-operation latency does
not claim that every native reference tail has retired at that same instant.
Copied-ACK native tails can remain until later; full connection teardown is
recorded separately. `latency_completion` and `phase_completion` facts distinguish
these endpoints. P50 is the upper middle sorted value; P99 uses floor((N-1)*.99).

This is bounded closed-loop traffic. Queueing/admission retries share the
operation's original five-second budget, and overload attempts are printed.
It does not model an externally paced arrival stream, connection fan-out,
production saturation/overload distributions, TLS/WebSocket, persistence,
recovery or arbitrary message sizes. Throughput is exchanges/s: one publish in
sender mode, two in duplex mode. The printed frame/byte facts count MQTT logical
wire only, excluding CONNECT/CONNACK and TCP/IP/kernel framing.

The fixture decodes publications, re-encodes echoes with its own ids and copies
native ACK/echo output. Client encoding, owned ACK/message copies, Future/token/
quota work, reference counts, scheduling and locking remain included. Observer
payload packing, sequence polling, latency writes, sleeps and release work are
also included. A new QoS-2 receiver table is charged only in the cold duplex
phase; sender-only does not allocate it. Inherited process affinity is unchanged;
there is no OS power/frequency/isolation tuning. Peer/observer are part of these
numbers, so they are not isolated client or minimal handwritten-C overhead.

Process CPU is the sum of the process threads, using GetProcessTimes on Windows
or CLOCK_PROCESS_CPUTIME_ID on Linux. Windows values have 100ns units but the
OS accounting resolution can make short runs zero/quantized. Zero is not a
zero-CPU claim. This omits system-wide interrupt/DPC/softirq CPU not charged to
the process. Setup, warmup, measurement, teardown and connection-lifecycle
wall/CPU are printed separately. Observer sorting/reporting and final standard
latency-buffer free are outside that lifecycle interval; its allocation size is
printed. Fixture stack storage is also printed and is not XRT heap traffic.

Heap variants print phase and complete connection-lifecycle XRT block counts/
bytes, frees and selected malloc/calloc/backing allocation counters. These are
XRT counters for client plus fixture, not all libc/OS allocations, RSS, copies,
locks or syscalls. The first lazy sender/receiver allocations are included in
cold phase counters and cached before hot ones. A phase can have live cached
blocks/native tails; the full instrumented connection lifetime is checked for
balanced blocks/bytes by the validation matrix. Instrumented timing includes
counter overhead and must not be compared as uninstrumented throughput.

Detailed synchronization/copy/syscall and whole-system CPU profiles, open-loop
load, pinned reference clients/brokers, native Linux and >=72h mixed/fault runs
remain required. No zero-overhead, comparator parity or production acceptance
is established by these workloads. Generated reports/binaries can be removed
after inspection. All sockets/threads/client/stream/listener/engine and retained
message/Future owners are released on successful completion.

`xmqtt_client_session` uses the combined QoS 1 echo workload with an explicit
protocol Session and a connection-local receive queue. Archived shared queue
measurements refer to the removed Inbox feature.
Unaliased QoS 1 shares one
snapshot; independent request/packet headers still require two allocations. The
benchmark does not exercise outgoing aliases. Compare
uninstrumented timing separately from instrumented heap counts; recovery itself
is covered by fault tests rather than this steady-connection echo workload.


The [2026-11-02 local observations](results/qos2-local-20261102.json) retain all
measured arrays and diagnostics. Both platform noise gates failed; the file is
an experimental observation, not an accepted performance baseline. WSL Git
revision/dirty fields were unavailable; runtime and workload hashes are retained.
The worktree progress ledger records exact validation and the Windows CPU
quantization seen in those runs.


## Longer instrumented measurement

`xmqtt_qos2_resolution` adds eight instrumented variants with 200000 operations,
10x the original 20000. The original profiles remain unchanged. The workload
source, runtime, window, payload, warmup, heap instrumentation and gate thresholds
are identical; only the operation count and variant name differ. The
[predeclared plan](results/qos2-long-plan-20261102.json) was frozen before the
single serial Windows/WSL collection. Reproduce with:

```text
python tools/measure_performance.py --compiler gcc --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_qos2_resolution --report out/qos2-long.json
```

The [long observations](results/qos2-long-20261102.json) retain all measured
samples/diagnostics beside the original observations. Windows' observed CPU
counter step amortizes from 781.25 to 78.125 ns/exchange. Corresponding CPU series
failing noise quality fall from 6/8 to 2/8 there, and remain 1/8 in WSL. Both
complete collectors still fail their unchanged gates. This improves a precision
limitation without establishing stable performance, source optimization or
release acceptance. No series was retried to seek a favorable result.

More operations also amortize first-use allocations, enlarge the observer's
latency array and cross more automatic-id wraps. Cold per-operation heap changes
are not an allocator improvement; raw lifecycle costs remain separate. Run
duration/host load can affect CPU/latency, so observed differences cannot all be
attributed to clock quantization. Full production qualification remains pending.

`xmqtt_client_session_alias` adds MQTT5 alias echo cases using the same observer,
payload32 and window32: the first admission binds both directions, subsequent
requests use an empty topic. Alias transaction/gather, incoming alias resolution,
peer and observer costs are included. This is a steady-connection closed-loop
workload, not broker recovery/fixed-arrival/native qualification.

## Native Store Writer capacity and online compaction

`bench_store_capacity.c` sends a fixed schedule of typed writes into one native
journal slot on one host storage worker. Each transaction combines acceptance
and acknowledgement of a 256-byte QoS 0 payload. The settled model keeps no
live Inbox entries, so a pressure checkpoint retains identity, session metadata
and the key fence. The registered profiles send 1,024 arrivals at 300/s with
16 pending observation slots. Each MQTT version runs with maintenance disabled
and with the APP scheduling compaction at a 64-transaction quota. The APP
drains already accepted requests before checking whether it needs to compact.

```text
python tools/measure_performance.py --compiler gcc --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_store_capacity --smoke --report out/store-capacity-smoke.json
python tools/measure_performance.py --compiler gcc --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_store_capacity --report out/store-capacity.json
```

Choose an existing private scratch parent on the intended filesystem with
`XMQTT_BENCH_DIRECTORY`. The benchmark uses the platform temporary directory
when the variable is unset. Each successful trial provisions and removes its
own three slot files. Record the filesystem and virtualization layer with the
report. Avoid running the two operating-system workload groups concurrently
on a shared host when measuring.

The executable accepts `COUNT RATE WINDOW TRANSACTION_CAP VERSION INSTRUMENT`.
A zero cap disables maintenance and sets the transaction bound to COUNT plus
the seed transaction. Positive caps start at two. VERSION 4 selects MQTT 3.1.1,
and VERSION 5 selects MQTT 5.0. Set INSTRUMENT to 1 to collect XRT block allocation
counts and bytes in a separate run. A useful accounting stress run is
`4096 50000 1 16 5 1`.

Every scheduled arrival gets a terminal accounting category. A full observation
window produces a drop. The Writer can independently refuse bounded admission.
Accepted work uses the remaining five-second arrival deadline and records a late
durable commit separately from a definite timeout. The next arrival is always
computed from the original time origin. Committed percentiles span scheduled
arrival to Future notification and therefore include dispatch lag, encoding,
admission, queue time and any required checkpoint. All-arrival P99 also includes
rejections and timeouts. Already-terminal Futures are counted and stamped at
registration. Process CPU includes the actively yielding pacer, observation
and native storage tasks. Heap counters exclude C observer arrays, which are
reported separately.

Throughput counts durable commits through the physical Writer drain. The final
state checks the recovered sequence, highest key and Inbox count using the same
quota. Recovery timing measures the selector and typed reopen after the Writer
has closed, with the filesystem cache in its resulting state. This is a
component workload on the recorded host. Broker traffic, populated checkpoints,
production CPU and tail-latency acceptance, and device power-loss qualification
require separate workloads.

## External broker comparison

The `--remote` mode of `bench_server` sends MQTT 5 publications with
a 128-byte payload to a loopback broker. It uses one publisher and up to
64 subscribers, with one exact filter or one wildcard per subscriber.
Each repeat warms the established connections, then sends at fixed
scheduled times. The observer checks every payload and sequence for every
subscriber and checks for extra deliveries after draining. The report
keeps missed or failed repeats. Latency starts at the scheduled send time,
and the separate offer-lag metric records producer scheduling delay.

Select `--qos 1` or `--qos 2` and `--window 1..64` for reliable tests.
The publisher enforces the same window against the broker's advertised
Receive Maximum. It advances only on PUBACK or PUBCOMP. Each subscriber
advertises the requested window, uses its broker-assigned identifiers and
completes the full exchange. The runner requires every publisher confirmation
and every subscriber delivery. Subscriber latency ends when the observer has
sent PUBACK or PUBCOMP. Publisher latency ends when it receives the terminal
acknowledgement. Every latency starts at the scheduled arrival, including
any time spent waiting for credit. The connection drain checks for excess
packets on both sides. The reliable profile configures 256 total pending
deliveries per subscriber, counting the active window and the queue.

Build the broker with the `xmqtt_server` suite and build the observer
with the registered `xmqtt_server_routes` smoke profile. Use the resulting
`examples_server_main` and `bench_server` executable paths.

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server --arch x64 --cflag=-O2
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_server_routes --smoke --report out/server-routing-smoke.json
python extlibs/xmqtt/bench/compare_broker.py --xmqtt /path/to/examples_server_main --mosquitto /path/to/mosquitto --observer /path/to/bench_server --broker-cpu 2 --load-cpus 4,6,8 --workload exact:1:3000 --workload wildcard:8:10000 --report out/broker-compare.json
```

Use [Mosquitto 2.1.2](https://github.com/eclipse-mosquitto/mosquitto/releases/tag/v2.1.2)
and choose disjoint logical CPUs available to the caller. On Windows, the
official 2.1.2 x64 installer can be extracted to a test directory with its
adjacent DLLs. The runner starts both brokers on loopback with one broker
logical CPU, plaintext transport and in-memory state. It uses one engine
worker for xmqtt and verifies each child's CPU mask. The order alternates
by repeat. The JSON retains executable and runner digests, throughput,
P50/P95/P99/P99.9, offer lag, broker CPU through connection and warmup,
and peak resident memory. Each throughput and P99 series has a sample
noise verdict using 3% relative MAD and 6% trimmed central range.

The scripted observer regression checks early PUBCOMP, a wrong PUBCOMP
identifier and duplicate subscriber delivery. It also exercises bounded
socket retirement after failure.

```text
python extlibs/xmqtt/bench/test_broker_observer.py /path/to/bench_server
```

This profile covers traffic at specified arrival rates. Saturation,
TLS, shared subscriptions, persistent state and
mixed-load duration have separate acceptance workloads.
