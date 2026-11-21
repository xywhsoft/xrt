# APP-owned JSON-RPC processing

This example uses two endpoints in memory. The receiving endpoint copies a
bounded job in OnRequest. The APP drives that job afterwards, dispatches it
and returns the resulting wire document through its token. app_dispatch.h
also serves the [stdio example](../stdio/main.c) and
[TCP and executor example](../tcp_worker/main.c).

The stdio APP accepts `--serve` and frames one JSON document per LF-terminated
input record. The TCP APP accepts `--serve`, prints its ephemeral loopback
endpoint, and accepts one client. It reassembles records of up to 4096 bytes
across reads. An XRT executor worker handles the copied request and signals the
owner, which publishes the result with the original token. The example keeps
one APP job in flight and exits when the peer ends the TCP stream. The socket
disables Nagle's algorithm and sends the reply and its LF delimiter together
using XRT's scatter/gather API. Framing, socket and thread pool lifetime stay
in the example.

Pass `--serve CLIENTS WORKERS` to share one bounded XRT executor across
up to 32 connections and 8 executor workers. The APP accepts the requested
number of loopback connections and gives each connection its own endpoint
owner. Each connection queues one copied job at a time. The owner shuts
down its output when it finishes, and the listener owner retains the
socket until that thread retires. The APP checks the executor completion
ledger before freeing job storage. With explicit counts, it prints
connections, workers and completed requests after shutdown.

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_examples --no-single
```

Run the independent peer driver without timing options to exercise real TCP
backpressure and peer reset. Two connections share one executor worker.
The driver leaves one connection's large replies unread until its send window
stalls. It verifies 128 ordered replies on the healthy connection, fills the
slow connection again, and resets it with zero linger. The healthy connection
completes another 128 replies and retires cleanly. Both APP modes run this case.

```sh
python extlibs/xrpc/tests/interop/run_tcp_worker.py out/gcc/native/xrpc_examples/examples_tcp_worker_main out/xrpc-tcp-peer.json
```

## TCP performance comparison

The TCP APP also accepts `--dispatch-only`. This calls the same xrpc dispatch
handler on the same XRT executor through the same TCP framing. It skips the
duplex endpoint, so it serves as the baseline for measuring the endpoint's
additional work.

Build with optimization for repeatable measurements.

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_examples --no-single --cflag=-O2
python extlibs/xrpc/tests/interop/run_tcp_worker.py out/gcc/native/xrpc_examples/examples_tcp_worker_main out/xrpc-tcp.json --measure
```

Append `.exe` to the executable path on Windows. Each mode reopens a single
loopback connection per sample. The peer sends batches of 1, 8 and 32 requests
and verifies every reply. Latency runs from the start of each batch send to
receipt of the individual reply, including its queue wait. The report contains
throughput and P50/P95/P99/P99.9 for each repetition and the median of each
metric. The client waits for the previous batch to finish before sending the
next one. These results describe that arrival pattern with one connection.

The concurrent comparison uses fixed-arrival batches of eight requests
per connection. Each connection warms up with 64 calls and then begins
at the same scheduled time. All operation latencies start at their
batch's scheduled arrival. The report also records delay before send,
so contention and observer scheduling time remain visible. The
independent peer verifies every reply and reconciles the APP's completion
ledger across 1, 4 and 8 concurrent connections.

```sh
python extlibs/xrpc/tests/interop/run_tcp_worker.py out/gcc/native/xrpc_examples/examples_tcp_worker_main out/xrpc-tcp-concurrent.json --concurrent --calls 3200 --rate 1600 --repeats 5 --cpus 4,6,8,10
```

The rate is per connection. CPU placement is inherited by the APP.
The report keeps all repetitions and applies the 3% relative MAD and 6%
central-range checks to throughput, P99 and P99.9. The test uses a bounded
one-job-per-connection APP and captures TCP, executor, and observer
costs together.

## TCP saturation sweep

The saturation observer schedules groups of eight requests per connection
from one monotonic clock. One event loop drives nonblocking sends and replies
across all sockets. It carries partial writes and reads across readiness
notifications. A group frees its slot after its final matching reply. The
default 16-group window admits up to 128 requests per connection. Every
scheduled reply has a five-second deadline, and the APP completion ledger
includes the warmup and measured requests. The report retains failed samples.

    python extlibs/xrpc/tests/interop/run_tcp_worker.py \
      out/gcc/native/xrpc_examples/examples_tcp_worker_main out/xrpc-tcp-saturation.json \
      --saturation --connections 8 --workers 2 --window-batches 16 --calls 4800 \
      --rates 1600,4000,8000,16000 --repeats 5 --app-cpus 2,4 --cpus 6,8,10

Select disjoint CPU sets available to the caller and verify their physical
core placement on the test host. The observer checks the APP process's
inherited affinity each time it starts. Append .exe to the executable on
Windows. For executor scaling, run the same case with `--workers` set to
1, 2, 4 and 8 in separate reports. Keep the APP and observer masks fixed.
The APP mask covers both connection owners and executor workers.
For load scaling, change `--rates` and keep the other parameters fixed.
Use `--calls` equal to the per-connection rate times the desired duration
in seconds.

Use `--observer-shards 3` to split the observer across the first three
CPUs in the sorted observer mask. The default is one process. Each
shard drives a disjoint subset of connections and uses the same initial
timestamp, full connection phase offsets and per-connection credit
limit. The coordinator merges timestamps after all shards complete.
The report records CPU time for each shard and its sum. The coordinator's
CPU is outside that measurement. For a comparison, keep the APP, CPU
masks, framing, rates and windows fixed and save each run to its own
report. Choose at most one shard per observer CPU and connection.

Use `--observer-native <library>` with one observer process to measure how
the Python event loop affects the result. The same Python coordinator
prepares requests and expected replies and verifies the APP ledger.
The native loop paces the same groups, verifies replies byte for byte,
and timestamps each call at its last observed reply byte. Use the same
CPU masks and workload for the Python comparison. Build the observer
from the repository root with GCC on Linux or MinGW-w64 on Windows.

    gcc -O2 -std=c11 -fPIC -shared extlibs/xrpc/tests/interop/tcp_native_observer.c -o out/xrpc-observer.so

    gcc -O2 -std=c11 -shared extlibs/xrpc/tests/interop/tcp_native_observer.c -o out/xrpc-observer.dll -lws2_32

Set `XRPC_TCP_OBSERVER_NATIVE` to the built library path and run
`python extlibs/xrpc/tests/interop/test_tcp_observer.py` to include the
native reply-verification tests.

The default aligns every connection's groups on the same schedule.
Append `--stagger-arrivals` to space their offers evenly within one group
period. The per-connection rate, number of calls and credit window stay
the same. Use distinct reports for the two schedules. Each sample
reports its phase offsets in nanoseconds and preserves the original
scheduled time even when credit delays an offer.

Throughput is the count of verified replies from the planned start to the
final observed reply, including any drain time. Latency starts at the
scheduled batch arrival for every request. Separate percentiles cover the
delay until submission, the observer's wait for a window slot, and the
interval from the actual send attempt to the reply. The report also
records the largest in-flight window, the number of batches that found it
full and observer CPU time. For the same slowest one percent of replies,
it sums the offer delay outside observed credit wait, observed credit wait,
and time from the first send attempt through receipt. Credit wait begins
when the observer first finds the scheduled group blocked by the full
window. Time behind earlier unsent groups is part of the offer delay. The observer compares received bytes against a
precomputed wire sequence for each group. Within a split group, each reply
takes the timestamp of the read that supplies its final byte. A JSON-RPC
batch document supplies one timestamp for all calls in the document.
The saturation summary checks variation across both P99 and P99.9.
Compare tail medians only when the repeated samples meet the variation gate.

The default framing sends a separate JSON-RPC record for each call.
With `--batch-documents` the same calls travel in one JSON-RPC request
array per scheduled group. Each response array is checked byte for byte
in the order produced by this APP. Its arrival time counts for each call
in the array, including calls in a partial final group. The
`app_documents` count includes warmups and reconciles the APP's executor
ledger. A document corresponds to one submitted job.

For a six-second scheduled load of 64,000 calls/s across eight peers, run
the same command twice with `--calls 48000 --rates 8000` and distinct
reports. Enable `--batch-documents` for the second run. Compare both
framing modes at the same offer rate, request window, executor size and
CPU placement.
