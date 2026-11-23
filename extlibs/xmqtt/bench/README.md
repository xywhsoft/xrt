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

## MQTT 5 publisher reference

The bench_client_reference.c driver runs the same fixed-arrival schedule for
xmqtt and Paho C. Both publish 32-byte MQTT 5 messages over one plaintext TCP
connection to Mosquitto 2.1.2. Both use Clean Start, no persistence, the same
outstanding window and the default TCP Nagle setting on the publisher socket.
Each process warms up with 128 publications. Latency begins at the scheduled
arrival, including time waiting for a window slot, and ends when the driver
observes the terminal QoS 1 or QoS 2 result. Every planned operation must
complete by its deadline. CPU totals include the generator and client workers.

The independent QoS 0 subscriber verifies the topic, exact sequence set
and every payload byte, including warmup. Subscription, broker startup,
warmup, observer drain and disconnect are outside the publisher timing window.

Use the codeload archive of Paho C v1.3.15 with SHA-256
60ce2cfdc146fcb81c621cb8b45874d2eb1d4693105d048f60e31b8f3468be90.
Windows extraction may omit the unused TLS test symlinks under
test/ssl/capath. [Paho C release](https://github.com/eclipse-paho/paho.mqtt.c/releases/tag/v1.3.15)

Set PAHO_SRC and PAHO_BUILD to the extracted source and its build directory.
Use the same compiler and optimization level in both publisher binaries.

    cmake -S "$PAHO_SRC" -B "$PAHO_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DPAHO_BUILD_SHARED=OFF \
      -DPAHO_BUILD_STATIC=ON -DPAHO_ENABLE_TESTING=OFF -DPAHO_ENABLE_CPACK=OFF \
      -DPAHO_HIGH_PERFORMANCE=ON -DPAHO_WITH_SSL=OFF
    cmake --build "$PAHO_BUILD" --target paho-mqtt3a-static
    flags=(-std=gnu11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror
      -DXMQTT_MODULE_XMQTT_CLIENT -DXRT_IMPLEMENTATION -DXMQTT_IMPLEMENTATION
      -Iinclude -Iextlibs/xmqtt/include -include xmqtt/features.h
      -include ./single/xrt.h -include ./single/extlibs/xmqtt.h)
    source=extlibs/xmqtt/bench/bench_client_reference.c
    gcc "${flags[@]}" "$source" -o out/xmqtt-publish -pthread -lm
    gcc "${flags[@]}" -DXMQTT_BENCH_PAHO -DPAHO_MQTT_STATIC=1 -I"$PAHO_SRC/src" \
      "$source" "$PAHO_BUILD/src/libpaho-mqtt3a.a" -o out/paho-publish -pthread -lrt -ldl -lm

On Windows with MinGW, set WIN32_LEAN_AND_MEAN in place of _GNU_SOURCE.
Link the xmqtt binary with ws2_32. Link the Paho binary with
libpaho-mqtt3a-static.a, ws2_32, crypt32, rpcrt4 and bcrypt. Select
CPUs on separate physical cores where possible. Each pair alternates
process order and keeps the raw samples, hashes and variation results.

    python3 extlibs/xmqtt/bench/compare_client_reference.py \
      --broker /path/to/mosquitto-2.1.2 --xmqtt out/xmqtt-publish --paho out/paho-publish \
      --broker-cpu 0 --client-cpus 4,8 --qos 2 --window 16 --rate 3000 --count 4500 \
      --repeats 5 --report out/mqtt-client-reference.json

The comparison measures publisher-side latency at the scheduled offered
rate. It reports count, P50/P95/P99/P99.9, submission lag and process CPU.
Saturation, receiver overhead, TLS, durable sessions and the native
72-hour mixed workloads have separate qualification.


### Native Linux comparison in CI

Enable `xmqtt_comparison` in a manual service-extensions workflow run.
The job builds Paho C 1.3.15 and Mosquitto 2.1.2 from checksum-pinned
source archives. It samples the MQTT 5 QoS 1 and QoS 2 publishers
against one Mosquitto listener and verifies the binary payload set
with an independent subscriber. The broker comparison covers QoS 0
exact and wildcard routing and QoS 2 wildcard and shared delivery.
Both use plaintext connections and memory-only state. The broker
process gets one core, and each client or load generator gets the
same two other cores.

The job alternates implementation order for five measured repetitions
and saves reports, binary hashes, executable files, revision, and host
and toolchain identifiers. The reports include per-run latency and
throughput and their repeatability checks. Encrypted transport,
restart, and sustained-load qualification use their existing workloads.


## MQTT 5 receiver reference

The receiver comparison sends the same 32-byte payloads to xmqtt and Paho C
1.3.15 from an independent plaintext TCP peer. It requires a clean MQTT 5
connection, the requested Receive Maximum, and a matching SUBSCRIBE. The
peer schedules publications at fixed arrival times and never exceeds the
requested window. It verifies every PUBACK for QoS 1 and every PUBREC and
PUBCOMP exchange for QoS 2.

Both clients validate the topic, delivery order and binary contents as
the application receives the message. The peer releases the client for
a graceful DISCONNECT after all reliable wire exchanges finish. The
report includes application latency, terminal ACK latency, time from
actual send attempt to terminal ACK, and sender scheduling lag. Both
MQTT 5 clients can deliver to the APP before QoS 2 wire completion.
Compare terminal ACK latency for the complete protocol exchange.

The default comparison disables `TCP_NODELAY` on the xmqtt socket to match
the Paho C build's socket setting. Add `--xmqtt-default-tcp` to keep the
settings from `xmqttClientConfigInit`, which enable `TCP_NODELAY` through
XRT. Paho keeps its default for both runs. The JSON report records this
choice in `xmqtt_default_tcp`.

Add `--broker /path/to/mosquitto --broker-cpu 10` to use standalone
Mosquitto 2.1.2 on a separate CPU. A raw MQTT 5 publisher makes the same
fixed-arrival offers through a dedicated loopback listener. The peer relays
the subscriber connection, verifies the broker's forwarded payloads and
Receive Maximum, and tracks publisher and subscriber acknowledgements
by their separate packet IDs. The report includes the broker's CPU and
memory and `subscriber_leg` percentiles from relay forwarding to the
subscriber's terminal acknowledgement. The end-to-end times include
the relay, which runs on the peer CPU with the publisher. The broker mode
uses plaintext TCP with persistence disabled.

Reuse the publisher comparison's compiler flags and pinned Paho builds.
Add `-DXRT_MODULE_EVENT` for the receiver driver's completion event.

    source=extlibs/xmqtt/bench/bench_client_receive_reference.c
    gcc "${flags[@]}" -DXRT_MODULE_EVENT "$source" -o out/xmqtt-receive -pthread -lm
    gcc "${flags[@]}" -DXRT_MODULE_EVENT -DXMQTT_BENCH_PAHO -DPAHO_MQTT_STATIC=1 \
      -I"$PAHO_SRC/src" "$source" "$PAHO_BUILD/src/libpaho-mqtt3a.a" \
      -o out/paho-receive -pthread -lrt -ldl -lm

    python3 extlibs/xmqtt/bench/compare_client_receivers.py \
      --xmqtt out/xmqtt-receive --paho out/paho-receive --peer-cpu 0 --client-cpus 4,8 \
      --qos 2 --window 16 --rate 3000 --count 4500 --repeats 5 \
      --report out/mqtt-receiver-reference.json

Use the same Windows compiler and linker substitutions as the publisher
build and append .exe to the receiver executables. Each measured process
warms up with 128 publications. The runner alternates client order,
verifies CPU affinity and retains raw runs and variation results.
The dedicated peer CPU polls the socket during the final two milliseconds
before an offer when it has credit. The reports record the peer CPU time.

### Direct TLS receivers through Mosquitto

Add `-DXMQTT_BENCH_RECEIVER_TLS` to both receiver builds. Select
`-DXMQTT_MODULE_XMQTT_CLIENT_TLS` for the xmqtt build. Configure the pinned
Paho build with `PAHO_WITH_SSL=ON` and `PAHO_HIGH_PERFORMANCE=ON`, then link
its static `paho-mqtt3as` target with the matching OpenSSL libraries. On
Windows, place the matching OpenSSL DLLs beside the receiver executable.

Add `--broker /path/to/mosquitto --broker-cpu 10 --tls-credentials DIRECTORY`
to the receiver command. The directory contains `ca.pem`, `certificate.pem`
and `key.pem` for a localhost test identity. Keep the private key in the
runtime directory. Both receivers validate the CA and hostname and connect
directly to the TLS 1.3 listener with `TLS_AES_128_GCM_SHA256`. The publisher
uses a second plaintext loopback listener.

Mosquitto's debug trace independently verifies negotiated TLS, forwarded
publications and both directions of the subscription's QoS acknowledgements.
The publisher limits total outstanding publications to 128 and waits for
subscriber completion credit before offering more. Time spent waiting is
included in application and publisher latency. The report includes verified
application and publisher rates, application latency percentiles, publisher
ACK latency and a hash of the broker trace saved alongside the JSON. The
measured workload includes broker tracing and excludes TLS handshake timing.

The manual service-extensions comparison runs direct TLS receivers at QoS 1
and 2 with windows 1 and 16. It requires four logical CPUs on at least three
cores and records the affinity of the broker, publisher and receivers. With
three cores, the broker and publisher use different logical CPUs of one core.
[Service extensions workflow](../../../.github/workflows/service-extensions.yml).

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
64 subscribers, with one exact, wildcard or shared filter per subscriber.
Each repeat warms the established connections, then sends at fixed
scheduled times. The observer checks each expected payload and sequence
and checks for extra deliveries after draining. The report
keeps missed or failed repeats. Latency starts at the scheduled send time,
and the separate offer-lag metric records producer scheduling delay.

Use `--workload shared:8:3000` for two shared subscriptions, `$share/a/+`
and `$share/b/+`, with members assigned alternately. Every publication
must reach exactly one member of each group. The observer checks each
group's publication set through warmup and measurement, claims each
sequence before acknowledging it and waits for terminal delivery in both
groups before draining every connection with PING. A member can receive
no publications. Shared readers wait for socket readiness and check for
cancellation while idle. Their latency includes this observer work.
`share_min_deliveries` and `share_max_deliveries` report
the observed allocation among members during measurement. The shared
workload requires at least two subscribers and covers connected sessions.
[MQTT 5.0, section 4.8.2](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

Select `--qos 1` or `--qos 2` with `--window 1..64` for reliable tests.
The observer requires the broker's advertised Receive Maximum to cover the
publisher window and releases a slot on PUBACK or PUBCOMP. Each subscriber advertises
its own Receive Maximum, set by `--subscriber-window` or inherited from
`--window` by default. Subscribers use the broker-assigned packet identifiers.
The runner requires every publisher confirmation, every subscriber delivery
and a final PING drain on both sides. Each subscriber has 256 total pending
slots, counting the active window and the queue. The flow-control rules
are in [MQTT 5.0, section 4.9](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

End-to-end latency starts at the scheduled arrival. Subscriber latency ends
after the observer sends PUBACK or PUBCOMP. Publisher latency ends when it
reads the matching terminal acknowledgement. The JSON keeps time from the
scheduled arrival to the pacer reaching it, time from that observation to
acquiring the publisher slot, and time from the PUBLISH send attempt to
its terminal acknowledgement. The pacer can inherit delay from preceding
arrivals. The current-slot wait includes lock and wake-up time. The
`publisher_credit_stalls` count identifies arrivals that actually saw an
occupied slot. Each P99 comes from that interval's own samples.

Build the broker with the `xmqtt_server` suite and build the observer
with the registered `xmqtt_server_routes` smoke profile. Use the resulting
`examples_server_main` and `bench_server` executable paths.

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server --arch x64 --cflag=-O2
python tools/measure_performance.py --config extlibs/xmqtt/config/performance_profiles.json --profiles xmqtt_server_routes --smoke --report out/server-routing-smoke.json
python extlibs/xmqtt/bench/compare_broker.py --xmqtt /path/to/examples_server_main --mosquitto /path/to/mosquitto --observer /path/to/bench_server --broker-cpu 2 --load-cpus 4,6,8 --workload exact:1:3000 --workload wildcard:8:10000 --report out/broker-compare.json
```


On Linux, set ``XMQTT_EXAMPLE_BACKEND`` to ``epoll`` or ``uring`` when
starting the plaintext broker example or this comparison driver.
``auto`` selects the platform default. Build with the XRT
``net_port_uring`` module to try ``uring``.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server,net_port_uring --no-single --exclude-test '*' --cflag=-O2
XMQTT_EXAMPLE_BACKEND=uring /path/to/examples_server_main --compare
```

For a single-header build, select ``XRT_MODULE_NET_PORT_URING``.
The selected backend must start successfully on the host. The WSL
[backend comparison](../../../docs/development/service-extensions-progress.md#2026-11-21-mqtt-epoll-and-io_uring-on-wsl)
covers 64-subscriber fan-out, higher offered rates with eight
subscribers, and 1,000 idle connections.

For the eight-subscriber QoS 2 diagnostic, keep the offered rate at
3,000/s and rerun with each pair of publisher and subscriber windows
(1/1, 1/16, 16/1, 16/16). Select separate reports for each run. For example,
append these arguments to the command above after removing the example
workload and report arguments.

```text
--qos 2 --window 1 --subscriber-window 16 --workload wildcard:8:3000 --rounds 3000 --warmup 300 --repeats 7 --report out/broker-qos2-p1-s16.json
```

Use [Mosquitto 2.1.2](https://github.com/eclipse-mosquitto/mosquitto/releases/tag/v2.1.2)
and choose disjoint logical CPUs available to the caller. On Windows, the
official 2.1.2 x64 installer can be extracted to a test directory with its
adjacent DLLs. The default comparison starts both brokers on loopback with one broker
logical CPU, plaintext transport and in-memory state. It uses one engine
worker for xmqtt and verifies each child's CPU mask. The order alternates
by repeat. The JSON retains executable and runner digests, throughput,
P50/P95/P99/P99.9, offer lag, broker CPU through connection and warmup,
and peak resident memory. Each throughput and P99 series has a sample
noise verdict using 3% relative MAD and 6% trimmed central range.

For a network worker comparison, use the same CPU set for each worker
count. With the plaintext server example, run separate cohorts with
`--xmqtt-workers 1`, `--xmqtt-workers 2` and `--xmqtt-workers 4`.
Replace the CPU, workload and report arguments in the command above.
Use a distinct report for each worker count and QoS.

```text
--broker-cpus 0,2,4,6 --load-cpus 8,10 --xmqtt-workers 4 --qos 2 --workload wildcard:8:2800 --workload wildcard:32:850 --rounds 1800 --warmup 240 --repeats 5 --report out/broker-workers4-qos2.json
```

The APP option is `--compare --workers NUMBER`. It uses the XRT
listener's round-robin connection distribution. The fixed-arrival observer
has one publisher and measures delivery to subscribers on multiple workers.

For a reliable fan-out rate sweep, keep the window sizes, payload and CPU
placement fixed. Run the same number of repeats for both brokers at each
scheduled rate. A short trial helps choose rates to repeat with more
publications. This example uses an approximately four-second offer interval.

```sh
rate=6000
python extlibs/xmqtt/bench/compare_broker.py \
  --xmqtt out/examples_server_main --mosquitto /path/to/mosquitto \
  --observer out/bench_server --broker-cpus 0,2,4,6 --load-cpus 8,10 \
  --xmqtt-workers 2 --qos 2 --window 16 --subscriber-window 16 \
  --workload "wildcard:8:$rate" --rounds "$((4 * rate))" --warmup 600 \
  --repeats 5 --report "out/broker-qos2-rate-$rate.json"
```

Keep the JSON from unsuccessful rates. Count a rate as completed only when
both publication and delivery ledgers match. Compare achieved ingress to the
scheduled rate for each repetition. Evaluate P99 repeat variation separately.

The scripted observer regression checks negative PUBREC with its reason code,
early PUBCOMP, a wrong PUBCOMP identifier and duplicate subscriber delivery. It also exercises bounded
socket retirement after failure.

```text
python extlibs/xmqtt/bench/test_broker_observer.py /path/to/bench_server
```

This profile covers connected sessions at specified arrival rates. Saturation,
persistent state and mixed-load duration have separate acceptance workloads.

### Verified TLS broker comparison

Build a TLS broker and an observer with the following flags from the
repository root. On MinGW, replace `-D_GNU_SOURCE` with
`-DWIN32_LEAN_AND_MEAN` and `-pthread -lm` with `-lws2_32`. If GCC 16.2
fails in its IPA constant propagation pass for the Windows TLS example,
add `-fno-ipa-cp` to that build.

```bash
mkdir -p out
flags=(-std=gnu11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror
  -DXMQTT_MODULE_XMQTT_SERVER -DXRT_IMPLEMENTATION -DXMQTT_IMPLEMENTATION
  -Iinclude -Iextlibs/xmqtt/include
  -include xmqtt/features.h -include ./single/xrt.h -include ./single/extlibs/xmqtt.h)
gcc "${flags[@]}" -DXMQTT_MODULE_XMQTT_SERVER_TLS -DXRT_MODULE_TLS_IDENTITY_RSA \
  extlibs/xmqtt/examples/server/tls.c -o out/compare-server-tls -pthread -lm
gcc "${flags[@]}" -DXRT_MODULE_MEMORY_STATS -DXMQTT_BENCH_TLS \
  -DXRT_MODULE_TLS_CLIENT_VERIFY -DXRT_MODULE_TLS_STREAM_FUTURE -DXRT_MODULE_TLS_STREAM_RETIRE \
  -DXRT_MODULE_TLS_RECORD_AES -DXRT_MODULE_TLS_SCHEDULE_SHA256 -DXRT_MODULE_TLS_KEY_EXCHANGE_X25519 \
  extlibs/xmqtt/bench/bench_server.c -o out/compare-observer-tls -pthread -lm
python extlibs/xmqtt/bench/compare_broker.py --xmqtt out/compare-server-tls \
  --mosquitto /path/to/mosquitto --observer out/compare-observer-tls \
  --tls-credentials /path/to/local-test-identity --broker-cpu 0 --load-cpus 4,6,8 \
  --qos 2 --workload exact:1:1000 --workload shared:8:2000 --report out/broker-tls-q2.json
```

Supply `ca.pem`, the same RSA test certificate as `certificate.pem` and
`certificate.der`, and its private key as `key.pem` and PKCS#1 `key.der`.
The certificate needs a DNS SAN for `localhost` and must chain to the
supplied local CA. Generate these files for the test and keep the private
key out of the repository. The runner checks identity, leaf certificate,
TLS 1.3 and AES-128-GCM on both loopback listeners before every sample. The
same C observer connects directly to each broker over TLS 1.3 with X25519,
RSA-PSS and CA/DNS verification. The report verifies the authenticated
connection count and keeps hashes of the public CA and leaf.
[Mosquitto listener TLS options](https://mosquitto.org/man/mosquitto-conf-5.html).

Repeat with `--qos 0` and `--qos 1` for the corresponding MQTT flows.
TLS handshakes finish before the measured interval. Broker process CPU
includes connection setup and warmup. Both measurements include the same
observer TLS worker, Futures and plaintext copies. Compare the brokers
within a TLS workload. Run the scripted observer cases with the same
`--tls-credentials` directory and an additional unrelated CA in
`wrong.pem` to cover idle and duplicate shared delivery over TLS and
rejection of an unrelated CA or a wrong DNS name before any MQTT traffic.

### Persistent-session restart comparison

Build `examples_server_checkpoint` with the `xmqtt_server_checkpoint_example`
suite. Run the comparison from an ordinary user account so both brokers
can read and write the same temporary directory with their normal privileges.

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server_checkpoint_example --arch x64 --cflag=-O2
python extlibs/xmqtt/bench/compare_broker_restart.py --xmqtt /path/to/examples_server_checkpoint --mosquitto /path/to/mosquitto --broker-cpu 0 --load-cpu 4 --workload 1:32:1 --workload 1:32:2 --workload 8:128:1 --workload 8:128:2 --report out/broker-restart.json
```

Each repeat uses an empty temporary directory and three fresh broker
processes. The observer creates each subscriber with Clean Start 0 and
Session Expiry Interval 600 s and checks that its first CONNACK has Session
Present 0. It parks the subscribers after SUBACK, confirms the
publisher's terminal QoS 1 or 2 acknowledgements, and requests a clean
checkpoint. On the first restart, it resumes each subscriber with Receive
Maximum 1. It checks Session Present, topic, all 128 payload bytes, sequence,
QoS, the terminal acknowledgement exchange and a PING barrier. After a
second checkpoint and restart, the observer resumes those same identities
again, checks for stale acknowledgements or replay, and delivers a new
publication through their restored subscriptions. The broker and observer
use separate logical CPUs. The xmqtt APP uses one network worker and a
checkpoint on exit. Mosquitto uses its built-in persistence at shutdown,
requested by its process-specific Windows event or POSIX SIGTERM.
[Mosquitto persistence settings](https://mosquitto.org/man/mosquitto-conf-5.html),
[Mosquitto 2.1.2 shutdown implementation](https://github.com/eclipse-mosquitto/mosquitto/blob/v2.1.2/src/signals.c),
[MQTT 5 session state](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

The JSON retains each outcome, broker logs, image sizes, publisher
acknowledgement latency, checkpoint-to-exit duration, time from spawn to
listener readiness, time from each resumed CONNECT to first delivery and
the total confirmed drain duration. Drain includes sequential reconnects,
wire exchanges and the observer's validation. Checkpoint and restore
measurements include process signalling, startup and readiness observation.
The comparison covers graceful process restarts and serialized replay at
Receive Maximum 1. Per-ACK commits, abrupt process termination and combined
TLS/persistence have separate acceptance workloads.

The manual `xmqtt_comparison` workflow runs these four workloads on Linux
and preserves the broker executables, their identities and the complete report.

### Idle connections and reconnect waves

Build `examples_server_main` with the `xmqtt_server` suite. The
`--compare MAX_CONNECTIONS` argument selects a connection limit and one
network worker. The observer uses Python 3.11 or later and disjoint
logical CPUs for the broker and observer.

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_server --arch x64 --cflag=-O2
python extlibs/xmqtt/bench/compare_broker_connections.py --xmqtt /path/to/examples_server_main --mosquitto /path/to/mosquitto --broker-cpu 0 --load-cpu 4 --clients 1 --clients 100 --clients 1000 --clients 8000 --clients 10000 --report out/broker-connections.json
```

Each repeat starts a fresh loopback broker with the same connection
limit and verifies readiness with a separate clean MQTT 5 handshake
and disconnect. It admits the complete cohort as clean MQTT 5 connections and
tests one excess connection. After the idle interval, the observer
requires a PINGRESP on every socket. It retires 10% of the connections
(at least one), reconnects the same identities, retests the admission
limit and checks every surviving and replacement socket. The final
drain must retire every connection and allow a new client. CONNECT and
PING latencies include each attempted socket or protocol operation.
[Mosquitto listener limits](https://mosquitto.org/man/mosquitto-conf-5.html).

Use `--concurrency` to set the rolling number of in-flight observer
operations, default 32. Throughput uses the whole phase wall interval
and per-operation percentiles start when an operation enters that
window. The report retains partially completed phases and failures.
It records broker CPU, working set or RSS, peak RSS and operating
system handle or descriptor counts before filling, at either end of
the idle period and after the final drain. Observer CPU is reported
separately. The POSIX runner raises only its process descriptor limit
and inherits it into the test broker. Use a host with enough descriptors
and loopback ports for the largest cohort.

The manual `xmqtt_comparison` workflow runs the full cohort matrix on Linux
and archives `broker-connections.json` with the executable and runner hashes.
It also repeats the 1,000- and 10,000-client cohorts with descriptor preparation
and saves both brokers' results in `broker-connections-prepared.json`.

Add `--trace-connect` to record TCP dial and MQTT handshake times for
each successful initial or replacement connection. Each sample gives
the connection index and the two durations in microseconds. Segment
percentiles summarize each distribution independently. Run the same
observer mode and CPU placement on both brokers when comparing timings.

On Linux, `--prepare-xmqtt-fds` enables the xmqtt example's
`--compare MAX_CONNECTIONS --prepare-fds` mode. It duplicates standard
input above the expected connection range and closes the duplicate
before starting the network worker. This expands the process descriptor
table before it becomes shared with the worker. The example uses a
64-slot allowance for listener and other process descriptors and uses
the existing process descriptor limit. Run the same workload with and
without the option to measure its effect on xmqtt. The reference
configuration is identical in the two runs.
[WSL 6.18 descriptor table expansion](https://github.com/microsoft/WSL2-Linux-Kernel/blob/linux-msft-wsl-6.18.y/fs/file.c).

Use the 8,000-client group for the near-capacity Windows comparison.
Mosquitto 2.1.2 initializes its Windows polling table for 8,192 sockets
and places the listener in the same table as client sockets. The
10,000-client group preserves the reference timeouts and the xmqtt
outcomes. A requested group with a failed peer gives the run a nonzero
exit status while the JSON retains each completed series.
[Mosquitto Windows startup](https://github.com/eclipse-mosquitto/mosquitto/blob/v2.1.2/src/mosquitto.c#L412-L419),
[Mosquitto polling table](https://github.com/eclipse-mosquitto/mosquitto/blob/v2.1.2/src/mux_poll.c#L59-L94).

This workload covers idle clean sessions and bounded reconnect waves.
Full-rate reconnect storms, authenticated transports, loaded subscriber
cohorts and durable recovery have separate workloads.

### Live quota at connection capacity

Use the independent MQTT 5 peer to hold a full connection cohort and
exhaust one subscriber's reliable delivery quota. The subscriber
advertises Receive Maximum one and withholds its first QoS 2 PUBREC.
The broker refuses an excess connection. Other clients answer PING
while the peer retires and replaces 1% of the cohort, with at least
one replacement. A peer can observe EOF before its slot is reusable.
The probe retries pre-CONNACK refusals within each replacement's
five-second budget and records the refusal count and longest replacement.
Two publications to the full recipient are rejected
on either side of the replacement wave. A disjoint topic continues
delivering verified QoS 2 messages. The peer then drains the slow
subscriber in order and reuses the rejected publisher packet ID.

    python extlibs/xmqtt/tests/interop/run_server_handshake.py \
      /path/to/examples_server_main out/broker-active-pressure.json --pressure-clients 10000

Choose a cohort from 5 to 10,000 using the one-worker server example.
The peer checks PING in groups of at most 128 sockets. The report
records verified connections and replacements and records the failed
phase if the workload stops early. The P50, P95 and P99 cover 256
serial healthy-topic exchanges before, during and after the quota
hold. Timing runs from the publisher's PUBLISH send attempt to the
subscriber's PUBCOMP send return. A final PING confirms the broker
received the last PUBCOMP.

### Fixed-arrival QoS 2 traffic during quota pressure

The benchmark keeps one broker alive through three healthy traffic
phases. Each offers the same fixed-arrival MQTT 5 QoS 2 stream to eight
wildcard subscribers. During the middle phase, a subscriber on a
separate topic withholds its first PUBREC and fills its 256-delivery
quota. A second publisher schedules new publications to that topic at
250/s and verifies PUBREC reason 0x97. It reuses the packet identifier
after receiving each negative acknowledgement. [MQTT 5.0, sections 3.5.2 and 4.3.3](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

The peer drains all queued payloads in order and verifies a new
publication on the reclaimed credit before the final healthy phase.
The report includes healthy throughput and tail latency, broker CPU and
resident memory, and verified refusals and their largest gap during the
observer lifetime. Each repeat opens a fresh broker. The default uses
1,000 additional idle connections and 4,000 healthy publications/s for
three seconds per phase.

    python extlibs/xmqtt/bench/compare_broker_pressure.py \
      /path/to/examples_server_main /path/to/bench_server out/broker-pressure.json \
      --broker-cpus 0,2 --load-cpus 8,10 --control-cpu 6

Use disjoint available CPUs and a fresh report path. Append `.exe` to the
binary paths on Windows. The broker and observer builds match the
plaintext external comparison above.

## Continuous MQTT 5 mixed load

The interop workload runs the xmqtt client and broker in two supervised
processes on a private IPv4 loopback listener. The client keeps one
publisher, one ordinary wildcard subscriber and two members of a shared
wildcard subscription. Each round admits six publications in a burst,
one per QoS for each route. It checks terminal publisher outcomes, the
full binary payload, delivered QoS, and exactly one recipient in the
shared group for each shared publication. The publisher also has a
No Local subscription to the ordinary route and verifies zero echoes.
The payload is 32 bytes, with a 256-byte payload every fourth round.

Every 16 rounds, the publisher replaces one retained record, opens a
temporary client and verifies the retained replay before retiring that
client. At completion, a tombstone removes the retained record. The
supervisor compares the broker's accepted connections and forwarded
deliveries with the client's exact round and replay counts. Both sides
verify their protocol and transport ownership counters at shutdown.

Build from the repo root using the checked-in single headers. Use GCC
on Linux or MinGW GCC on Windows. On Windows give the binary an `.exe`
suffix. Keep the binary's `.build.json` receipt with the run.

    python3 tools/build_service_mixed.py xmqtt --output out/mqtt-mixed
    python3 extlibs/xmqtt/tests/interop/run_mqtt_mixed.py \
      --binary out/mqtt-mixed --seconds 90 --pause-ms 3 --report out/mqtt-mixed-smoke.json

Use a new report name for every run. The JSONL journal keeps five-second
progress windows with round latency, client process CPU, wall and
monotonic time continuity, and both processes' private-memory and
handle/descriptor snapshots. Round latency includes all six terminal
outcomes and received deliveries, plus the retained replay when due.
Pacing sleeps contribute to the run duration. Growth above the first
resource sample is guarded at 64 MiB of private memory and 32 handles or
descriptors per process. The Windows supervisor and the C workload use
QueryPerformanceCounter for elapsed time. New journals also record
uptime and awake time. The observer checks the requested awake
duration, observation gaps and the accumulated suspend difference.
UTC offset remains in the journal for diagnosing host time steps.

For a 72-hour run use --seconds 259200. The native_72h result requires both
live process resource ledgers, uninterrupted duration, graceful teardown
and matching counters on native Windows or Linux. WSL runs retain their
own environment label. For a separate fault run use --seconds 20 and
--kill-broker-after 5. The supervisor kills only its spawned broker
after a verified window and records the interrupted workload as failed.
Persistent-session recovery, TLS/WebSocket, and sustained saturation
have separate workloads.

Keep the binary, its `.build.json`, and both report files. Reconcile them
after the run with the offline checker. Use `--require-native-72h` for
release evidence and omit it for diagnostic runs.

    python3 tools/check_service_mixed.py xmqtt --binary out/mqtt-mixed \
      --report out/mqtt-mixed-72h.json --require-native-72h
