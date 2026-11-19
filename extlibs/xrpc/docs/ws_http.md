# HTTP Upgrade on the RPC peer

The developing `xrpc_ws_http` leaf adds checked HTTP/1.1 Upgrade to the existing
WS/Duplex peer. `xrpc_ws_http_tls` also selects owned native TLS event exchange.
Select either with the corresponding `XRPC_MODULE_` macro in single-header
builds. No new Engine, executor, reader thread or shared runtime is created.

The four factories take an already OPEN native stream on its owning Worker:

| API | Native input | Endpoint role |
|---|---|---|
| `xrpcWsHttpClientTcp` | `xnetstream*` | HTTP client / masked WS output |
| `xrpcWsHttpServerTcp` | `xnetstream*` | HTTP server / unmasked WS output |
| `xrpcWsHttpClientTls` | authenticated `xtlsstream*` | HTTPS client |
| `xrpcWsHttpServerTls` | authenticated `xtlsstream*` | HTTPS server |

TLS attachment must run outside the native Open/Read driver; post a host node
from Open as in [the example](../examples/ws_http/README.md). The original TLS
session/configuration owner remains alive. Successful attachment consumes one
native caller reference and returns the ordinary `xrpcws*` in
`XRPC_WS_UPGRADING`. False preserves native binding/reference and output. Stop
all old readers before transfer and keep callback policies/code resident through
physical retirement. Already validated attach APIs retain their previous scope.

Calls, batches and notifications may queue immediately on that handle. Only
checked GET/101, key/Accept and subprotocol permit WS parsing and JSON dispatch.
Existing buffered bytes are accepted; consume precisely the HTTP header and
leave a coalesced first WS frame for Core. Client nonces use Core secure random.
No redirect, credential retry, connection replay or implicit downgrade occurs.

## Configuration and bounded storage

`xrpcWsHttpConfigInit` initializes `xrpcwshttpconfig`; initialize its checked
`Size` and the normal `xrpcwsconfig` before a factory call. Configuration views
and extra fields are copied before return. Output must be disjoint from input
descriptors and views. An invalid managed header or injected field/challenge is
rejected before native ownership moves.

| Setting | Default and contract |
|---|---|
| `Head` | Core limits capped at16KiB and128 fields; smaller nonzero limits allowed |
| `MaxWriteBytes` | 32KiB;256..65536 bytes; partial native writes retain exact offset |
| `SetupTimeout` | 10000ms from factory entry;0 immediate, `XRT_WAIT_FOREVER` allowed |
| `Host` | Required client authority; optional server allow value, ASCII case-insensitive exact match;<=1024 bytes |
| `Target` | `/rpc`; origin-form exact path including query;<=4096 bytes, no fragment |
| `Origin` | Optional client field/server exact allow value;<=2048 bytes; empty server value imposes no browser-origin rule |
| `Fields` / `FieldCount` | <=32 request/response extras,<=16KiB total;<=256-byte name and4096-byte value |
| `UnauthorizedChallenge` | `Bearer`;<=128 bytes, valid HTTP field value |
| `Client` / `Server` | Core strict handshake policies; protocol lists<=1024 bytes; selected protocol<=255 bytes |
| `RequireProtocol` | true; default offer/allow list `jsonrpc` |
| `MaxAuthorizationContextBytes` | 0..16384 bytes reserved for subsequent handler context |

Managed Host/Origin/Connection/Upgrade/framing/handshake/WWW-Authenticate fields
cannot be overridden with extras. Duplicate Authorization extras are refused.
Compression policies require the optional [HTTP compression leaf](ws_deflate.md).
Without that leaf, enable/require/accept-deflate configuration is rejected.
The existing validated WS attach retains its explicit negotiation contract.

HTTP control, field descriptors, copied credentials and output are cold storage.
They retire after handoff and the old event-owner tail. An exceptional native
TLS partial prefix with no published original Read, or insufficient full-record
growth space, uses one additional bounded Head cache. Partial/cross-block TCP
headers use the same cache; complete contiguous TCP heads parse a const span.
The adapter never mutates a borrowed TCP receive buffer: IOCP can own an active
write reservation for its next receive. Consume exactly through CRLFCRLF,
preserve the WS suffix, and erase/release that cache after checking.
The cache tracks its scan offset and current line across fragments. Each new
byte is probed once (a split trailing CR is revisited); incomplete valid prefixes
do not repeatedly invoke the full parser. Bad CR/LF and start/field-line limits
still invoke Core immediately for its authoritative error. Complete heads retain
all Core syntax, field-count and handshake validation. The exact retained head
cap still rejects an unterminated head. Three private size counters add24 bytes
to cold setup control on measured x64 hosts; no public layout, new allocation,
thread or shared runtime is added. The ordinary native TLS parsing path remains
unchanged; the optimization applies when either transport uses this cache.
Auth snapshots have separate
bounded storage and actual executor credits. Cold storage is securely erased at
release. Selecting HTTP also adds a permanent private copied HTTP-info snapshot;
this stage makes no zero-overhead or performance-acceptance claim.

## Authorization and close

`Authorize` requires a borrowed shared `RPC.Duplex.Server` and an empty initial
`RPC.RequestContext`. `AuthorizationUser` and callback code remain resident until
peer retirement. The `xrpcwshttpauthorize` callback runs on that Server's existing
executor under its Operations/RequestBytes limits, outside IO Workers.
`xrpcwshttpauthrequest` borrows an immutable owned `Head` snapshot, `Remote`,
`Secure`, poll-only `Cancel` and the original monotonic `Deadline` during the
callback. Keep no borrowed pointer after returning.

Return `XRPC_WS_HTTP_ALLOW` with a context length within the supplied capacity
to make those bytes the connection's later handler context. Return
`XRPC_WS_HTTP_UNAUTHORIZED`, `XRPC_WS_HTTP_FORBIDDEN` or
`XRPC_WS_HTTP_AUTH_ERROR` for401/403/503. Invalid callback result/length yields
500. Poll cancellation/deadline in long work. Timeout does not forcefully stop
foreign code; noncooperative auth delays physical peer retirement, and Close/
WaitFor/Destroy joins its actual executor tail.

Routing/method/version checks produce404/405/426; malformed or duplicate auth/
Origin fields produce400, excessive headers431. Rejections have empty bodies
and close the native connection. No handler runs on a rejected connection.
The original setup clock spans queued work, auth, response writes and handoff;
queued RPC operations retain their own API-entry clocks. Handshake failure keeps
`XRPC_NOT_SENT`; HTTP/auth rejection maps to `XRPC_CALL_HTTP_ERROR`, malformed
handshake to `XRPC_CALL_PROTOCOL`, setup expiry to `XRPC_CALL_TIMEOUT`.
`xrpcDuplexClose` now accepts HTTP_ERROR and TIMEOUT alongside its documented
prior close reasons, preserving delivery knowledge and each operation's clock.

A terminal unsent RPC result does not prove the HTTP rejection has drained.
Let orderly peer retirement finish before an explicit host `xrpcWsClose` if the
response must reach its peer; explicit Close may abort pending native output.
Keep the shared Engine running through retirement, then join Server/Engine
separately. Typed results may outlive those joins.

## Inspection, validation and remaining scope

`xrpcWsHttpInfo` copies `xrpcwshttpinfo` under the peer gate, including after cold
storage has retired. Its `xrpcwshttpoutcome` records the first setup decision:
`XRPC_WS_HTTP_PENDING`, `XRPC_WS_HTTP_OPEN`, `XRPC_WS_HTTP_REJECTED`,
`XRPC_WS_HTTP_FAILED`, `XRPC_WS_HTTP_TIMED_OUT` or `XRPC_WS_HTTP_CLOSED`.
Later transport/close failures remain in normal `xrpcWsInfo`; OPEN/REJECTED is
not rewritten by later teardown. `Status` is checked/chosen HTTP status,
`AuthorizationStatus` is the executor result, `HeaderBytes` is parsed input and
`WriteBytes` is native acceptance, not remote acknowledgement. `ColdBytes`,
`PeakColdBytes`, `HeaderCopyBytes` expose setup storage/copies, including actual
auth final release; the reserved handler context is part of normal FixedBytes.
Legacy validated-attach peers return STATE and preserve this output.

Modular/single suites cover queued bidirectional RPC, batches/notifications,
held auth, snapshot lifetime, queued [extension offers](network_extensions.md),
cancellation/deadlines, HTTP401, real OOM credits,
prebuffered TLS and physical joins. The independent stdlib/OpenSSL peer checks
both roles over TCP/TLS, byte-split and coalesced101/first frame, exact typed
failure classes, strict key/protocol/framing failures and acknowledged close.
Its ephemeral local keys stay in the supplied test directory and are removed.

Run the separate structural/IO regression with
`python tools/test_xrpc_ws_http_scan.py --compiler gcc`. On Linux add
`--sanitize address` or `--sanitize thread` in separate output directories.
It compares every prefix of adversarial inputs with the public Core parser,
then paces real TCP fragments from actual receive consumption on select and
IOCP/epoll. It counts actual production-TU parser input, checks101 and the first
WS Ping/Pong suffix, and joins peer/native/Engine duties. Those paced wall times
are not a throughput or tail-latency benchmark. A larger native TLS incomplete
prefix outside the cache still needs separate performance evaluation.

These attach factories begin after native establishment. For numeric, DNS and
prepared client setup, select the [WS/WSS dial leaf](ws_dial.md). URI input,
broader fault/fuzz/scale validation, controlled native throughput/
tail latency and native Windows/Linux72h qualification remain required. The
four-product and production goals are incomplete.

Normative sources: [RFC6455](https://www.rfc-editor.org/rfc/rfc6455.html),
[HTTP semantics](https://www.rfc-editor.org/rfc/rfc9110.html),
[JSON-RPC2.0](https://www.jsonrpc.org/specification).
