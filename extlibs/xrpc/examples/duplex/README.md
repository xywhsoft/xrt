# APP-owned JSON-RPC processing

This example uses two endpoints in memory. The receiving endpoint copies a
bounded job in OnRequest. The APP drives that job afterwards, dispatches it
and returns the resulting wire document through its token. app_dispatch.h
also serves the [stdio example](../stdio/main.c). The APP can replace the
one-item queue with its chosen XRT task pool and post completed replies back
to the endpoint owner.

The stdio APP accepts `--serve` and frames one JSON document per LF-terminated
input record. The APP owns its record format, output flushing and process
lifetime. When used over HTTP or WebSocket, feed a complete body or message
into the same Receive entry point. Use XRT transport APIs for networking,
TLS and authentication.

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xrpc/config/modules.json --suite xrpc_examples --no-single
```
