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
one APP job in flight and exits when the peer ends the TCP stream. Framing,
socket and thread pool lifetime stay in the example.

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_examples --no-single
```
