# HTTP/1 JSON-RPC server

The server composes public Core HTTP/1 parsers, TCP/listener/engine APIs and the
bounded xrpc server executor. Create copies configuration and route path but does
not bind or run handlers. Start is a serialized creator operation: application
state must already be initialized, because accepts/business work can begin before
Start returns. Numeric plain TCP is supported; TLS/authentication and other
transports remain product work. The host engine stays pinned until Destroy.

Core accepts distribute connections across network workers. Each mutable peer is
owned by its stream worker. Whole request bodies go to the separate execution
pool; business handlers never run on network workers. Future Watch notification
posts an embedded peer continuation. A separate operation allocation owns each
watch/header/result lifetime, avoiding reuse of watch storage while a foreign
publication/release tail can still be active. Accepted stream/listener event-data
owners carry actual references; opaque graphs are not collector admission.

The local endpoint accepts POST at the configured raw path, ignores a syntactically
valid query and accepts valid absolute http targets as well as origin targets.
HTTP/1.1 needs one valid Host. Content-Type is application/json, with validated
parameters and an optional unique UTF-8 charset. Unsupported content codings are
rejected. Core validates framing/CL/TE and chunk/trailer syntax; the server additionally
uses the Core trailer-section policy validator before RPC submission;
declared trailer names and expectations are checked before business submission.
100-continue is sent for a valid pending body; unsupported expectations are 417.
Invalid HTTP framing/policy is rejected without invoking business handlers.

Complete JSON documents use the execution component's protocol rules. Valid RPC
responses use HTTP 200 and application/json; notification-only results use empty
204 without Content-Length. HTTP-local failures use empty error responses and
close, rather than fabricated RPC Invalid Request responses. Overload or allocation
failure that cannot reserve even an operation/header may abort the connection.
No operation is replayed, and local/transport failure is not business rollback.

Each connection processes one HTTP exchange at a time and preserves pipelined
bytes for the next request. It resumes parsing after response references leave
the Core send queue, retaining order. Network read/write work has per-turn byte
budgets. Global send credit uses per-worker waiting-writer lists and embedded
shard kicks, so a peer blocked on another connection's bytes wakes when credit
returns. No backpressure polling timer is added. Responses use bounded reference
sends; the Future and header allocation stay alive until every reference releases.
This avoids copying response payloads into a second application send buffer.
It is not an end-to-end zero-copy claim.

Monotonic request time begins with the earliest consumed/buffered input event,
including retained pipelined bytes. Prefetched bytes conservatively share their
earlier buffer epoch. The same exact origin reaches execution through a private
entry helper; it is not restarted after body reading or rounded by subtracting
milliseconds. A peer timer covers header/body/execution/send waiting; an idle
timer covers periods between requests. Expiry aborts the peer and requests
cooperative cancellation. Noncooperative business code can delay physical drain,
but finite operation/connection/storage limits bound further admission.

Private buffer quotas cover server/shard/route, peer/header descriptor/input/body
storage and transport operation blocks, including old-plus-new realloc peaks.
Referenced queued bytes have a separate global limit. Execution request/result
limits also apply, including retained results. Core Future/queue/stream metadata,
allocator rounding, socket buffers and thread stacks are separate: connection,
operation, per-stream and execution limits bound their counts/capacities; the
private-buffer limit is not total process RSS. Large copied request-body staging
is released after execution submission; large empty input scratch is trimmed on
reuse. Partial/malformed request bodies are released before error response drain.

Close stops accepts, posts peer aborts and requests execution cancellation. It
may run on any thread; user token watches run on their requesting thread and
must not block. Wait closes admission and observes listener/peer/timer/post/watch/
send duties plus executor publication/drop tails under one monotonic deadline.
It rejects network workers and its own execution/drop callbacks. Destroy requires
stopped concurrent native API entries, joins execution workers, releases the
listener/engine pin and consumes the unique owner only on success. Mechanical
callback return tails finish when the host engine joins. Retained client response
Futures do not pin this server. A timeout leaves the handle valid for retry.

This stage is not final interoperability, performance, native Linux or long-run
production acceptance. API/packaging and actual evidence are recorded in the
project's service-extension progress ledger.

Protocol references: [JSON-RPC 2.0](https://www.jsonrpc.org/specification) and
[HTTP/1.1 framing](https://www.rfc-editor.org/rfc/rfc9112.html). HTTP status/media/
lifecycle choices above are local binding policy, not extra JSON-RPC requirements.


Run the minimal example with no arguments for a short lifecycle smoke, or with
`8080 60000` to expose loopback `/rpc` for 60000 ms. A request such as
`{"jsonrpc":"2.0","id":1,"method":"echo","params":[7]}` returns `[7]`.
The example echoes any method; applications should supply their method policy.
The performance profile `xrpc_http_server` drives real client/server ordinary,
notification and batch16 traffic at fixed planned arrivals. Offered, accepted,
rejected, completed and failed counts are retained; latency includes queueing,
late offers and polling observation. The client and server share a host engine,
so these measurements are compositions, not isolated server capacity results.

For independent HTTP/JSON interoperability, run
`python -X utf8 extlibs/xrpc/tests/interop/http_server.py BINARY [RECEIPT]`
against the built server example. It uses Python stdlib HTTP framing, chunked
requests and eight persistent connections, then checks a truncated-body EOF.
This is not independent JSON-RPC-library parity.
