# Negotiated remote cancellation

The xrpc v1 abandonment extension uses reserved JSON-RPC 2.0 method names
on an ordered duplex connection. Select it explicitly on each endpoint.
Ordinary JSON-RPC behavior is the default. The two directions negotiate
independently. [JSON-RPC 2.0 extensions](https://www.jsonrpc.org/specification#extensions).

## Host API

On the serialized owner, call xrpcDuplexEnableCancellation to enable
recognition of the control methods. Server peers reserve the incoming ID
index immediately. Calling it again returns the existing enablement.
xrpcDuplexNegotiateCancellation submits the capability call and returns a
normal xrpc call Future. Drive it through the existing Step, Next, Started,
Sent and Receive APIs. Check its typed outcome and its boolean result.
A local timeout, cancellation, remote error or false reply leaves outbound
wire cancellation disabled. One offer may be pending. After a failed offer
retires, the same owner may retry. Successful negotiation lasts through the
connection lifetime.

For framed streams call xrpcStdioEnableCancellation and
xrpcStdioNegotiateCancellation, then continue to drive the stdio owner.
These functions also work on the endpoint borrowed from
xrpcStdioProcessEndpoint. Use xrpcStdioCall or xrpcDuplexCall and their batch
variants normally. Cancel their Future with xrtFutureCancel. The local
monotonic deadline can initiate the same abandonment path. User callbacks
continue to observe the existing cancellation token.

## Wire protocol

- The capability request is a standalone JSON-RPC call to
  rpc.xrt.cancel.v1 with no params. Its JSON-RPC ID follows the normal request
  and response rules. An enabled responder sends result true if it can
  track incoming work, or false for a client-only endpoint. A standard
  Method Not Found response is ordinary unsupported-feature fallback.
- The initiator activates outbound abandonment after its pending capability
  Future resolves successfully with result true. The responder activates
  reception after the positive reply's complete send. Work whose first send
  begins afterward is eligible.
- Abandonment is a standalone notification to rpc.xrt.cancel. Its params are
  an array with one JSON string, the ID of the first call in the original
  document. The string contains a canonical nonzero decimal uint64 value.
  The receiver interprets JSON string escapes. The params representation
  may occupy at most 256 bytes. A batch uses the first call in batch order
  and abandons the whole document. Notification-only documents have no
  cancellation target. Senders keep these IDs unique through the connection.
- The notification declares that the sender's local operation is terminal
  and relinquishes its reply. The receiver requests cooperative cancellation
  on the matching document's Future, suppresses any response it has yet to
  queue, and waits for physical handler retirement. A response already
  committed to output can still arrive. The caller keeps its original typed
  timeout or cancellation and delivery uncertainty. This exchange provides
  no acknowledgement of business effects or rollback.
- An enabled receiver ignores a valid notification received before its
  positive acknowledgement completes. After negotiation an unknown or
  repeated target has no additional effect. A duplicate active target ID,
  an invalid control shape in a valid envelope or a control in a batch seals the connection
  with a protocol error before invoking other work in that document.

## Bounds and ordering

The receiver allocates one xrpcPending index with at most MaxIncoming
entries out of MaxFixedBytes. It tracks only valid documents whose first
call has an eligible string ID. Lookup uses its balanced ID index. A
tracked job's token belongs to the existing control quota, and leaves the
index when the job physically retires. Allocation or admission failure
rolls back the reservation. Other valid JSON-RPC IDs continue to work as
ordinary calls.

Only a fully sent request or batch can create a cancellation notification.
An unsent operation retires locally. An incomplete write still retires
the transport for framing integrity. If the response was already consumed,
the caller has no remaining remote abandonment duty. The cancellation
notification uses the existing wire quota and moves ahead of unsent work,
preserving an already selected physical output. If required control output
cannot be built within its quota, the connection seals so the peer can
retire connection-local work through normal transport teardown.

The [duplex example](../examples/duplex/README.md) observes cancellation on
the executor and then makes new calls on the same connection. The
subprocess regression exercises this wire exchange under Content-Length and
UINT32_BE with an independent wire fixture. [Remote deadline negotiation](deadline.md)
is independent of cancellation. TCP, TLS and WebSocket use
[network opt-in APIs](network_extensions.md).
