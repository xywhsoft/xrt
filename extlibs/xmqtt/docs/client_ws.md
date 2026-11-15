# MQTT WS/WSS client attachment (developing)

The client_ws leaf attaches an already checked, exclusively owned native OPEN
TCP stream; client_ws_tls additionally supports verified OPEN client TLS. The
native TLS must come from an OwnedV1 factory without a prior additional event
binding, active driver/callback or pending read. Its original configuration owner
remains retained. The host configures peer trust and performs HTTP Upgrade and supplies its exact negotiated mqtt/client policy.
These attachment factories enforce the metadata and OPEN/client native state; they do not
re-verify peer credentials or
perform/attest earlier host network setup. The separate developing
[native connection factories](client_ws_connect.md) perform numeric/DNS/prepared
network setup and HTTP Upgrade. URI input and reconnect integration remain required.

One ordinary Client owns all existing packet/QoS/auth/Session/Inbox state. Binary
bytes span arbitrary WS frames/messages; Text closes the transport. WS control
frames stay with Core. CONNECT/config/auth snapshot before native handoff; false
leaves output/native/read-ahead ownership unchanged. Native attachment success
is final: later prefix-activation errors use typed Ready/close outcomes.

The common incoming path copies once into normal MQTT packet storage. Only an
unread callback suffix uses separately bounded, charged reusable staging, including
old+new growth peak; consumed staging is wiped. Each WS data chunk pauses native
application delivery, then resumes on the existing owner. Packet/read and native
send-byte/call work stay bounded. MaxPrivateBytes charges MQTT state/staging;
native WS/masking/TLS/buffer/allocator storage remains separately bounded/counted.

WS requests reuse the 24-byte aligned prefix fence tail, including Session/alias
gather paths. Capture a complete native cut only after Staged clears; older cuts
remain comparable. QoS0 waits for native completion, not broker effects. Early
ACKs may resolve a result but keep physical request/id/quota credit. Close aborts
or completes bounded MQTT DISCONNECT plus WS/TLS close, polls real native retirement
and destroys the WS owner before its owned Client reference can retire. Borrowed
native aliases can delay this barrier. The host separately joins Engine/code.

The 176-byte public WS config layout is independent of optional Auth/Session/
Inbox and plain/WSS selection on x64. Selecting WS adds an 8-byte pointer to the
private Client even when the connection uses TCP; an attached WS client adds a
288-byte cold state plus bounded staging (296 bytes when the optional HTTP
connection factories are selected). These are real costs. The existing
Client public config272/info160 layouts stay unchanged on both tested x64 hosts.
The request fence is24 bytes plus its existing alignment. A valid UINT64_MAX cut
uses the explicit pending-owner identity and can complete while still open.

Run the separate functional/fault suites and local example from the repository:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_tests
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_tls_tests
```

Windows select/IOCP and Linux WSL select/epoll pass ordinary and adversarial C
executions, plain/full/root singles, full stats-off, ASan/UBSan/leaks and separate
TSan. Coverage includes tiny work budgets, split/coalesced messages, QoS0/1/2,
subscription and incoming-QoS1 metadata, Text rejection, timeout, constructor OOM
and transactional retries, actual alias-held retirement, AUTH/final proof/reauth,
and explicit Session/Inbox outgoing replay across connections. Static/shared and
standalone declaration consumers use separate XRT/MQTT libraries; all three APIs
export and the internal bridge stays hidden. These tests use a literal local
post-Upgrade data peer, not an external HTTP/broker interoperability witness.

The registered loopback QoS1 performance profiles run the actual exchange,
packet-ID checksum and physical shutdown, with caller-observed latency and XRT
allocation instrumentation. Three measured samples fail several quality limits
on the shared host; no threshold is relaxed. The preserved pre-exact-max-fix
measurements are diagnostics, not performance acceptance. Native masking/frame
allocations, extra frames for gather spans, TLS work, admission/result copies,
receive/staging copies and allocation traffic still require controlled analysis.

All new leaves remain developing. See the source-bound
[stage report](../../../docs/development/xmqtt-websocket-20261116.json).
The later [native connection stage](client_ws_connect.md) separately records
numeric/DNS/prepared HTTP Upgrade and independent peer/broker evidence.
URI input, automatic reconnect transport factories, durable state, strict clean
broker interoperability, broader async-OOM/slow-peer/
malformed/load tests, controlled throughput/tails/CPU and native Linux/Windows
soak qualification remain required. No complete WS/WSS product acceptance is claimed.
