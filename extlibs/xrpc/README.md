# xrpc (developing)

Independent pure-C JSON-RPC 2.0 client/server extension using public XRT APIs.
**Bounded envelope decoding/encoding, synchronous dispatch, immutable method
indexing, exact ID comparison keys, bounded pending calls, immutable response
snapshots, complete response-batch correlation, native typed call Futures and a
bounded ordinary HTTP/TCP call client are implemented.**
Remaining client/server runtime, bidirectional transport, WebSocket/framed stdio,
deadline/cancellation negotiation and production acceptance remain pending.

Select `XRPC_MODULE_XRPC_DECODE`. `xrpcDecode` scans one complete document into
caller-owned message scratch. It classifies calls, notifications, success/error
responses and invalid objects/items. A separate document result is published
only after complete JSON validation. Do not use scratch before DONE; a later
syntax error invalidates every earlier item. There are no business callbacks.

The protocol follows the [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification)
independently. Request/response modes validate their respective envelope and
ignore opposite-role extra fields. Duplex classifies method-bearing requests
and rejects ambiguous request/response envelopes. Relevant envelope/error keys
reject duplicates after JSON-escape interpretation; user payload dictionaries
retain their own application policy. Keys and version comparisons are case
sensitive and recognize escaped ASCII forms.

Views borrow the original input. Method/message values retain JSON quotes and
escapes; this leaf does not create decoded strings or owned result objects.
String, number and null identifiers stay exact, including huge/fractional/
scientific numbers and embedded-NUL string escapes. Missing id identifies a
valid notification; explicit null remains a call. Error codes accept integral
numeric values without native rounding. The optional ID key leaf supplies exact
semantic comparison; the pending-call leaf adds serialized admission/removal.
The first ordinary HTTP/TCP call runtime is described below; other transports
and executor/server integration remain pending.

Nonempty batches retain one descriptor per input item, including invalid ones.
An empty batch yields one invalid descriptor with Batch=false for a single
Invalid Request response. Scalar roots and nested-array batch items are invalid
envelopes. Malformed JSON is PARSE_ERROR; scratch/admission/resource/API/OOM
failures are LOCAL_ERROR. Local overload is never mislabeled Invalid Request.
Only valid notifications suppress a response; application dispatch must enforce
this and the reserved `rpc.` method namespace (the descriptor flags it).

Limits cover input bytes, depth, decoded string bytes, total values, direct
container items, messages, and id/method token bytes. Config is copied. The call
is synchronous on its caller thread, retains no inputs, allocates/copies no
payload on DONE and preserves ambient error. Caller scratch may be smaller
than the configured batch cap. Source spans and config must not overlap outputs.
Cold XRT diagnostics may allocate; an OOM diagnostic becomes a local error.

Select `XRPC_MODULE_XRPC_ENCODE` for `xrpcEncodeSize`/`xrpcEncodeWrite`. Outgoing
descriptors distinguish calls, notifications, results and errors. Method and
error message are decoded UTF-8 (including embedded NUL) and are quoted with
the public XRT JSON writer. Id, params, result, error code and optional data
are complete raw JSON values: validated once per preflight and copied exactly,
without numeric conversion. Surrounding JSON whitespace is allowed and retained.
Size=0 means an absent raw field; explicit null requires the `null` token.
Required and unused fields are checked, including integer error codes and
string/number/null ids. Payload duplicate keys retain application policy.

Batch shape is explicit and never inferred from Count. Non-batch requires one
message; a batch requires at least one and preserves brackets for one message.
Request batches can mix calls/notifications; response batches contain only
results/errors. Mixed request/response direction is rejected. The encoder does
not dispatch handlers or select replies: a runtime must suppress notification
responses and send nothing when an entire response batch would be empty.

Encode limits apply to the **complete output**, including fixed envelope keys,
container depth/items and total values across all messages. Config is copied.
Size and Write independently preflight all inputs; every failure preserves the
output buffer and size outputs. Inputs must stay alive/unchanged during a call.
The written range and size output cannot overlap descriptors, config or any
input span. Success writes exactly the measured bytes, with no terminating NUL,
allocates nothing and preserves ambient error. Malformed local JSON/UTF-8 is a
local failure; the encoder never turns it into an automatic protocol response.

Write scans each raw JSON field once before copying it once. Each decoded text
field passes through XRT quoting in both preflight and emission; each quote
currently validates UTF-8 then traverses for escaping. Thus Write has four text
traversals, rather than an unmeasured zero-overhead claim. A separate Size call
adds its own validation/measurement. Fixed syntax is copied in small segments;
there is no DOM, payload ownership, descriptor allocation, retained input,
transport copy or synchronization added by this codec. XRT diagnostic save/
restore and error reference operations still occur on public JSON API calls.

```sh
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc --no-single --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc --trim-only
python tools/package.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc --kind static --verify
python tools/package.py --compiler gcc --manifest extlibs/xrpc/config/modules.json --suite xrpc --kind shared --verify
python tools/measure_performance.py --config extlibs/xrpc/config/performance_profiles.json --smoke
```

See the [decode example](examples/decode/main.c), [encode example](examples/encode/main.c), [API inventory](docs/api/reference.md),
[coverage matrix](docs/coverage.md) and [live progress](../../docs/development/service-extensions-progress.md).
The product remains developing. A codec smoke is not transport throughput,
dispatch latency, interoperability or production acceptance.

Select `XRPC_MODULE_XRPC_DISPATCH` for the [synchronous dispatch leaf](docs/dispatch.md)
and its [batch assembly example](examples/dispatch/main.c). It validates a whole
request document before any user code, suppresses every valid notification reply,
forwards exact IDs, supplies standard errors and bounds response count/bytes.
Handler-local output is consumed synchronously. Callback/limit failures can leave
business effects and partial sink output; no replay or rollback is implied.
Async scheduling and transports remain separate pending work.

Select `XRPC_MODULE_XRPC_METHODS` for the [immutable method index](docs/methods.md)
and its [escaped-name example](examples/methods/main.c). It copies bounded UTF-8
registrations once, validates complete raw JSON query names, matches semantic
escapes without a decoded query allocation, and resolves collisions using full
names in binary search. Hold a strong index reference through every dispatch
using `xrpcMethodsCall`; callback code/context remain application owned.

Select `XRPC_MODULE_XRPC_ID` for [exact identifier comparison keys](docs/id.md)
and the [huge-exponent/NUL example](examples/id/main.c). Keys distinguish JSON
types, decode string escapes and normalize decimal values without floating-point
or exponent narrowing. Keep original raw IDs for wire encoding; keys do not
implement a pending map, runtime correlation or duplicate-response policy.

Select `XRPC_MODULE_XRPC_PENDING` for [bounded pending calls](docs/pending.md):
exact ID admission, response/token removal, absolute monotonic deadlines and
shutdown draining. A preallocated AVL index and deadline heap bound memory and
lookup depth. The owner serializes all operations. Local tokens resist slot reuse;
wire ID reuse still needs runtime policy. [The example](examples/pending/main.c)
runs without a server. Private structural/exhaustion checks run separately:

```sh
python tools/test_xrpc_pending_structure.py
# GCC/Clang Linux ASan/UBSan; use a separate output directory per platform:
python tools/test_xrpc_pending_structure.py --sanitize --output out/service-extensions/xrpc-pending/linux-structure
```

This table does not supply asynchronous completion, whole-response validation,
transport cancellation, connection epochs or an executor. Those remain product work.

Select `XRPC_MODULE_XRPC_RESPONSE` for [immutable response snapshots](docs/response.md)
and `XRPC_MODULE_XRPC_RECEIVE` for [complete batch correlation](docs/receive.md).
The reader owns a complete validated document, exposes lossless rebased views and
supports retained concurrent readers. Receive preflights every envelope and ID
before removing any call; matched/repeated/unknown outcomes remain distinct.
Neither leaf completes futures or supplies an executor/transport. The examples
in `examples/response` and `examples/receive` require no server and release their
resources. Private lifetime/overflow checks run separately:

```sh
python tools/test_xrpc_response_lifetime.py
python tools/test_xrpc_response_lifetime.py --sanitize --output out/service-extensions/xrpc-response/linux-lifetime
```

Snapshot storage is bounded per document; aggregate retained-result quotas,
connection epochs, queue-inclusive deadlines and async completion remain runtime
work. A snapshot makes one allocation and one wire copy; no end-to-end zero-copy
or production-performance acceptance is implied.

Select `XRPC_MODULE_XRPC_CALL` for [native call/Future lifetime](docs/call.md).
A serialized producer owns preallocated terminal storage and a cooperative cancel
producer binding. Outcomes distinguish remote errors, timeout/cancel/close/failure and
NOT_SENT/MAY_HAVE_EXECUTED/REPLIED knowledge; retained results outlive the producer.
The [example](examples/call/main.c) has no network prerequisite and releases both
endpoints. A separate private payload-reference overflow/lifetime probe runs via:

```sh
python tools/test_xrpc_call_lifetime.py
python tools/test_xrpc_call_lifetime.py --sanitize --output out/service-extensions/xrpc-call/linux-lifetime
```

This operation component creates no timer/thread/queue/connection. HTTP exchange
and XRT Future primitives are available; the network client/server integration,
executor, aggregate quotas and transport acceptance remain pending.

Select `XRPC_MODULE_XRPC_HTTP_CLIENT` for the [HTTP/TCP client](docs/http_client.md)
and its [offline loopback example](examples/http_client/main.c). It uses a running
XRT engine, bounded cross-thread submissions, fresh string IDs, monotonic deadlines
and typed call Futures. One persistent numeric plain-TCP connection runs one
HTTP exchange at a time. Calls, notifications and atomic batches are available; DNS/TLS, HTTP metadata policy,
other transports and the server remain pending. The separate fixed-arrival
benchmark records every offer, refusal and observed queue-inclusive completion;
local fixture measurements do not establish production performance.

The [batch example](examples/http_batch/main.c) maps mixed call/notification/error
items after the peer reverses its response order. NotifyAsync completes the HTTP
exchange without claiming business execution. Batch Futures own one snapshot and
a request-order map, including a typed empty map on stop/failure. The HTTP
benchmark includes ordinary calls, notifications and batches of 16/64; its rates
and observed latencies count HTTP operations, not individual batch items.
