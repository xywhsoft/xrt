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

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_examples --no-single
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
