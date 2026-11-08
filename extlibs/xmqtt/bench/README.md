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
