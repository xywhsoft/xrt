# xrpc

xrpc implements JSON-RPC 2.0 messages and call state in C. It accepts complete
messages from an APP and produces complete output messages for that APP to send.
The codec, exact ID comparison, synchronous dispatch and request matching are
selectable separately. The duplex module composes them into an endpoint with
typed Futures, notifications, batches, local deadlines and cancellation.

1. Submit an outbound call or feed a complete inbound document.
2. Drive queued submissions and timeouts on the endpoint owner.
3. Give inbound requests to the APP through its request handler. The APP may
   process them later and reply using the original token.
4. Select an output and report when the transport begins and completes its send.

The APP owns framing, I/O, worker execution, authentication and shutdown
policy. See the [transport-free example](examples/duplex/README.md), the
[stdio APP](examples/stdio/main.c), [endpoint contract](docs/duplex.md) and
[API reference](docs/api/reference.md).
