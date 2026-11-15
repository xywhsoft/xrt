# WS/WSS client establishment (developing)

`xrpc_ws_dial` composes numeric TCP, asynchronous DNS or immutable prepared
endpoints with the existing HTTP Upgrade and bidirectional RPC peer. Select
`xrpc_ws_dial_tls` for an explicit verified TLS session before HTTP. Both use the
same `xrpcws` handle, ordinary call/batch/notification and close/retirement APIs.

`xrpcWsDialConfigInit` supplies native TCP buffer/limit defaults, dual stack,
250ms fallback, eight candidates,1024 copied DNS bytes and16KiB aggregate TLS
name/ALPN metadata. Hard limits are64 candidates/endpoints,65535 DNS bytes and
16KiB metadata. `xrpcWsConnect` takes a numeric endpoint, `xrpcWsDial` takes a
borrowed host Resolver/name/port and `xrpcWsDialAddresses` retains an immutable
prepared list. Prepared selected nonzero ports are preserved; Family filters.
TLS counterparts are `xrpcWsConnectTls`, `xrpcWsDialTls` and
`xrpcWsDialAddressesTls`, with explicit native client and optional TLS limits.

HTTP Host/Target, DNS routing and TLS verification identity are separate explicit
inputs. The native TLS client snapshot holds context/verifier/resume references
and copies names/ALPN before returning. Keep callback data/code alive through
physical Destroy. Negotiated ALPN must be empty or http/1.1. No cleartext fallback,
credential retry, replay, redirect, implicit reconnect or IDNA conversion occurs.
The developing [URI input layer](ws_uri.md) composes these same factories.

HTTP SetupTimeout begins at factory entry and includes validation/copy, owner
queue, DNS, candidate retirement, verified TLS, Upgrade and authorization. Zero
expires immediately and -1 is infinite. Native TCP/TLS caps remain effective.
Each queued RPC operation keeps its own API-entry deadline/cancellation. No RPC
bytes or business dispatch occur before the checked101 barrier. Setup failure
completes queued operations with NOT_SENT and typed local/native diagnostics.

Factories are concurrent and return one creator owner only on success; failure
preserves output and consumes no input aliases. Close/WaitFor/Destroy includes
the actual owned native Done/Drop and raw/TLS/WS duties. Keep the borrowed Resolver
alive through peer retirement, then close/join it separately; a cancelled shared
backend lookup can outlive this dial's owned callback. No new Engine, Resolver,
executor, thread or shared runtime is created. Cold controller/name/list/session
ownership retires after actual transfer and owned callback tails. HTTP ColdBytes
counts its own control/cache/auth and dial controller storage, excluding native
Core session/crypto/buffer allocations and shared caches; these remain real costs.

A freshly created TLS stream has a root data owner, distinct from its replaceable
event slot. After verified Open, the peer posts out of the native driver and binds
one separate HTTP event owner before reading/writing HTTP. Checked101 then uses
the existing transactional HTTP-to-WS event exchange. Both real ownership credits
retire through native destruction; no guard is bypassed and no TLS alias is kept
only to make inspection convenient.

Windows/WSL Linux x64 probes measure existing config528/info680 and private peer
1568/1776 bytes respectively, unchanged across base/plain-dial/TLS-dial selection.
Selecting dialing grows the cold HTTP control872 to888 bytes. The plain controller
is280 bytes, TLS controller320, plus copied DNS bytes and one NUL; one cold
allocation holds each controller and its copied name. The new public dial config is104 bytes on both hosts. These
figures exclude all native dial candidates, TLS sessions/crypto/buffers and allocator
headers/caches. Controllers, source lists and snapshots are not kept on the hot
message path. No end-to-end zero-copy or performance-acceptance claim is made.

An owned source context may retain its real credit past a successful WS handoff.
Its cold header/controller stay alive until the original Drop is delegated; then
they retire even though the native stream has already moved into WS. Completed
setup is not timed out again while that context is retiring. Close still waits
for the actual retained owner. Run the production-TU lifetime regression with
`python tools/test_xrpc_ws_dial_drop.py --compiler gcc`; Linux also supports
`--sanitize address` or `--sanitize thread` with separate output directories.
The fixture moves one actual context credit into a holder, inspects it on the
owning Worker, communicates past the setup deadline, and checks physical Close
before/after release. It does not invent references, native results or IO.

See [the runnable numeric example](../examples/ws_dial/README.md). Modular and
single tests cover real numeric/DNS/prepared TCP/TLS traffic, bidirectional calls,
batches/notifications, held DNS/deadlines/cancel/close and physical owner joins.
Additional cases mutate routing/HTTP/TLS/context inputs after return, release the
client verifier alias, force real constructor allocation refusals and a refused
TCP candidate, stop an actual ClientHello, and reject TLS identity/ALPN before
HTTP. The independent stdlib/OpenSSL runner's `--dial` switch uses the new numeric
client factories; its native server cases retain the ordinary attach API. It
checks actual TCP/TLS Upgrade, protocol failures, compression/context/windows,
large exact IDs, acknowledged close and original native-owner joins. Without the
switch it continues to test the original attach factories.
This leaf remains developing; broader faults/OOM/fuzz, controlled CPU/throughput/
tail/scale and native Windows/Linux72h production qualification remain required.

Protocol sources: [RFC6455](https://www.rfc-editor.org/rfc/rfc6455.html) and
[ALPN](https://www.rfc-editor.org/rfc/rfc7301.html).
