# Negotiated extensions on network peers

TCP, TLS and WebSocket peers use the same connection-local
[cancellation](cancellation.md) and [deadline](deadline.md) wire as
duplex and framed stdio. WebSocket uses Text messages through either
TCP or TLS, including peers created by the HTTP Upgrade and dial
factories. The RPC exchange begins once the stream is open, including
completion of any TLS handshake and authorized HTTP Upgrade.

## Enable and offer

Each side enables the extension flags it accepts. Choose
XRPC_DUPLEX_EXTENSION_CANCELLATION, XRPC_DUPLEX_EXTENSION_DEADLINE,
or their bitwise OR. The TCP API is xrpcTcpEnableExtensions and
xrpcTcpNegotiateExtension. TLS and WebSocket expose the same operations
with xrpcTls and xrpcWs prefixes.

Enable uses the existing concurrent admission and retirement contract.
ACCEPTED means the selected policy is installed. Repeating a flag
leaves its prior enablement in place. A combined opt-in reserves any
needed cancellation index once and installs the flags together.
A failure returns without enabling additional flags. Each peer
may enable the receive policy before its own connection or setup opens.

Negotiate takes exactly one flag and sends one ordinary capability call.
The original timeout includes facade admission, setup, encoding and
queueing. Only ACCEPTED writes the owned Future. One offer per extension
may construct or wait for a reply. Read its typed result and require a
boolean true. A false or Method Not Found reply preserves ordinary calls
and permits retry. An agreed capability lasts through the connection.
An application can submit finite work from the successful Future watch
or after observing the reply on another thread.

## Resource and send boundaries

The adapter uses its existing submitter reference, owner wake, native
write quota and retirement path. There is no additional posted task or
thread per offer. The receive side uses its normal frame and decode
limits. On send, both control messages and deadline envelopes share
the existing wire quota and must also fit the configured transport
body or message size. Finite calls that would exceed that size fail
admission and leave the Future output unchanged.

The owner checks native write credit before selecting an unexposed
deadline envelope. TCP checks writable bytes, TLS checks ciphertext
offer bounds, and WebSocket checks the work budget and native frame
credit. Once selected, the wire stays immutable through any native
borrow or retry. Remaining delay until the first observed receive
fragment still has no common clock. TCP and TLS use the framed
input origin, while WS starts at native MessageBegin.

See the [TCP example](../examples/tcp/README.md) for an opt-in, remote
deadline error and later calls on the same connection. The standalone
HTTP POST adapter and native sustained production qualification remain open.

## Loopback benchmark

The TCP propagation profile measures finite calls with the same payload
and connection setup. Use window one for P99 and P99.9, and window 32
for batched throughput. The capability exchange runs before warmup.
The closed-loop samples include caller collection time.

```sh
python tools/measure_performance.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_tcp_propagation
```
