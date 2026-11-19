# Negotiated remote deadlines

A finite RPC timeout starts at caller API entry. Duplex can also send
the remaining time to an opted-in peer. Each direction negotiates
independently. Legacy peers handle the reserved offer as a normal
JSON-RPC Method Not Found response. [JSON-RPC 2.0 extensions](https://www.jsonrpc.org/specification#extensions).

## Host API

Call xrpcDuplexEnableDeadline on each serialized owner, then call
xrpcDuplexNegotiateDeadline on the sending owner. The returned ordinary
RPC Future carries the capability reply. A successful boolean true
activates propagation for subsequent admissions. An application can
submit from the successful Future watch or from another thread after
it observes the reply. Offers may be retried after a failure.

Use xrpcStdioEnableDeadline and xrpcStdioNegotiateDeadline for a framed
stdio owner. They also work on xrpcStdioProcessEndpoint. The normal
call, notify and batch APIs continue to accept millisecond timeouts.
Finite document admissions after activation get the envelope. Existing
queued work and infinite timeouts use the ordinary wire format. Deadline
and [cancellation](cancellation.md) offers can be negotiated independently.

## Wire protocol

The standalone rpc.xrt.deadline.v1 call omits params. An enabled peer
replies true if it has a server, or false if it is client-only. A
positive response authorizes receipt only after its complete send.
Both directions use ordinary JSON-RPC responses.

A timed document is the second item of the reserved notification's
params. The first item is a fixed-width string of 19 ASCII decimal
digits from 0000000000000000000 through 9223372036854775807. The outer
envelope has the exact key order, separators and quoting below. The
inner document is one valid JSON-RPC call, notification or request
batch. The entire batch shares one budget.

```json
{"jsonrpc":"2.0","method":"rpc.xrt.deadline","params":["0000000000000000150",{"jsonrpc":"2.0","method":"lookup","id":"11"}]}
```

The sender stamps whole remaining milliseconds rounded down before
Next exposes the immutable wire. Its original local deadline still
governs transport acceptance and the Future. Select as close as
possible to the first transport offer. Sender-side delay after selection
and transit before the first received byte have no shared clock
measurement. The receiver uses the earlier of its configured
IncomingTimeout and the advertised budget. In framed stdio this clock
starts at the first input fragment and survives frame assembly and
admission backpressure. Worker queue time counts too.

An enabled endpoint treats a pre-agreement envelope, invalid metadata,
nested control or enclosed response as a protocol error. The outer
envelope consumes the normal wire and frame byte limits while existing
decode and dispatch limits still govern the inner document. The
sender reserves the envelope in the same charged wire allocation and
encodes the document directly into it. The receiver borrows the inner
span until ordinary server admission copies it.

## Expiry and delivery

A worker checks the deadline before entering business, after each
handler and while building responses. Long-running handlers can
compare xrtTimer with xrpcservercontext.Deadline. At expiry the server
discards any partial batch output and emits error -32001 with message
Deadline exceeded for each original call ID. Notifications have no
reply. The response remains subject to existing output and result
bounds. A bounded reply failure can retire the connection.

An ordinary call that receives this error before its own deadline
resolves as XRPC_CALL_REMOTE_ERROR with XRPC_REPLIED delivery. A batch
resolves as XRPC_CALL_BATCH_DONE with deadline errors at the mapped
call indices. A local deadline still produces XRPC_CALL_TIMEOUT with
the usual transport delivery evidence. Already entered business may
have taken effect and must stop cooperatively.

The [duplex example](../examples/duplex/README.md) demonstrates the
remote error and continued bidirectional calls. TCP, TLS and WebSocket
have [network opt-in APIs](network_extensions.md). Native sustained
qualification remains open.

## Benchmark

The paired profile compares finite calls across two in-process endpoints
and the same shared executor. It records throughput, process CPU time
and P50/P99/P99.9 from the same build with propagation enabled or disabled.

```sh
python tools/measure_performance.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_duplex_propagation
```
