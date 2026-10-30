# Bounded HTTP/TCP ordinary calls

`xrpc_http_client` is the first network client slice. It sends one ordinary
JSON-RPC call per HTTP POST over one persistent numeric plain-TCP connection,
using a caller-supplied running XRT engine and fixed worker affinity. It uses
public Core HTTP/1 head/body parsers and writer, not an HTTP parser of its own.
There is one active exchange; concurrent submissions wait in bounded queues.
Notifications, batches, connection pooling/pipelining, DNS, TLS, authentication,
WebSocket/stdio, server execution and negotiated extensions remain product work.

Host is an explicit nonempty authority and Target is origin-form. Connect copies
both, the endpoint and config before asynchronously starting TCP. The user must
Close and Release the client. Constructor and call outputs stay unchanged on
failure/refusal. Retain/Release are concurrent; Info is a protected snapshot.
No business callbacks run under the submission gate. Future completion Watches
run synchronously on publication and must not block an I/O worker.

Call inputs are borrowed only during caller-thread preflight/encoding. Method
is decoded UTF-8; Params is absent or an entire raw JSON array/object. One owned
HTTP/JSON wire copy is built before publication. Each admitted call uses a fresh
decimal-string identifier; gaps from refused construction are never reused and
64-bit serial exhaustion refuses further calls. Replies must be complete single
response objects, with an exactly equivalent string ID (JSON escapes accepted).
Unknown/mismatched IDs and unexpected bytes retire the connection. No operation
is replayed. New connection objects do not share an old TCP response stream.

The deadline starts at the public CallAsync entry and includes input encoding,
admission, worker wait, connection setup and response processing. A product-private
constructor passes that exact monotonic origin to the existing call implementation;
there is no millisecond subtraction/rounding workaround or new Core API. A queued
stop removes unsent work. Once any request bytes were accepted by TCP, local
timeout/cancel retires this connection and closes its other queued calls rather
than silently reusing HTTP response state. NOT_SENT/MAY_HAVE_EXECUTED/REPLIED
retain the existing call module's knowledge semantics, not business-effect proof.

Explicit Future cancellation posts a bounded, allocation-free embedded owner
notification. A timer holds a real request reference until its terminal callback.
Captured cancellation tails hold the call context, request reservation and client
reference until physically retired. Raw-token cancellation alone does not wake
the runtime; use Future cancellation. Dropping a Future does not cancel work.
Closed resolves only after stream, request construction, timers/posts and captured
request tails retire. Its publishing stack still has a mechanical client-reference
tail; join the engine before asserting the final allocation baseline. No further
transport duties run after Closed. Retained response Futures own no
request/client/engine refs.

HTTP fixed, chunked (including extensions/trailers), close-delimited bodies and
bounded informational responses use the Core state machine. Upgrade/tunnel and
unsupported content/transfer codings are rejected. Requests explicitly send
`Accept-Encoding: identity`; absence would permit arbitrary response codings
under RFC 9110. No compression or redirects are implicit. A final non-2xx HTTP status can carry a valid matching RPC error
or result: this initial RPC API judges the complete matching RPC response, not
a transport-specific HTTP status mapping. There is no per-call HTTP metadata
result API yet. A call with no valid RPC response is not successful.

Limits include live request reservations (also concurrent construction and tail
references), per-request and total owned wire bytes, Core stream read/write hard
bounds, HTTP heads/fields, body/chunk/trailer sizes, total response wire bytes
(including informational heads and chunk framing/extensions), informational count, JSON
decode/owned snapshot bounds and work/read bytes per turn. Count/byte overload is
an ordinary admission result and preserves the output and ambient diagnostic.
Limit, memory, protocol, transport, close, cancellation and timeout outcomes stay
typed. Caller-retained completed snapshots are outside pending-wire admission;
aggregate retained-result quotas remain required product work.

Costs include request/call/result/Future/cancel creation, one encoded wire block,
timer bookkeeping, gate/atomic operations and TCP's bounded copy-send storage.
The reusable bounded receive buffer copies TCP input; decoded body bytes append
to a geometrically grown bounded payload buffer, and the immutable response owns
its further wire copy. Header delimiters are scanned incrementally before one
Core head parse. The Core body reader may rescan an incomplete trailer prefix.
Complete JSON validation and snapshot copying still execute on the owning worker,
bounded by response/JSON limits rather than the per-turn wire-read budget. The
default receive scratch reserves 128 KiB per client before payload growth. Large
payload tail latency and high-connection-count idle memory remain performance
work, alongside aggregate retained-result accounting.
No zero-copy, zero-allocation, stable performance or production claim follows.

The [offline example](../examples/http_client/main.c) creates a deterministic
local C TCP peer, asserts the typed result, closes/joins all resources and reads
the retained response afterward. Run README's build command with suite
`xrpc_http_client`. The peer is a fixture, not independent interoperability proof.
The separate benchmark schedules absolute arrivals, records every offered call
and refusal, and measures observed latency from planned arrival (including late
offers and result polling). It includes wire/runtime/Future work, uses a separate
peer worker, and preserves overload/failure counts. Timed completion is logical
Future observation; final close/join is outside timed arrivals. This is not final acceptance.

Protocol references: [JSON-RPC 2.0](https://www.jsonrpc.org/specification) and
[RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) and
[RFC 9110 content negotiation](https://www.rfc-editor.org/rfc/rfc9110.html#section-12.5.3). The initial HTTP binding
and retirement policy above are local xrpc policy, not additional JSON-RPC rules.
