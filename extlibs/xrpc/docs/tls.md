# Framed bidirectional TLS transport

`xrpc_tls` is developing. It shares the existing duplex/frame runtime and borrowed
business Server, owns no new Engine/thread, and uses public native TLS/TCP APIs.
`xrpcTlsConfigInit` initializes `xrpctlsconfig`; `xrpcTlsInfo` snapshots
`xrpctlsinfo`, `xrpctlsstate` and `xrpctlsreason` including copied TLS version,
cipher, opaque negotiated ALPN, resumption and native error kind/code/domain.

`xrpcTlsConnect` snapshots a native client Session synchronously and connects a
numeric address on the selected host Worker. Native verification/resume policy
is explicit; an identity name is required. Configured names/ALPN are copied and
native context/verifier/resume references are retained by Core. The original
entry deadline covers construction, TCP and handshake. `xrpcTlsAccept` snapshots
server policy and consumes exactly one accepted TCP reference only on success.
`xrpcTlsAttach` adopts an already OPEN owned TLS stream once, from its Worker
outside a callback; post from Open first. Failure transfers nothing. Original
session callback contexts remain alive until final native destruction.

`xrpcTlsCall`, `xrpcTlsNotify` and `xrpcTlsBatch` preserve API-entry deadlines,
copy-before-return bounded concurrent admission, typed native Futures and
independent retained results. Frame formats remain explicit Content-Length or
binary32; no ALPN or framing standard is invented or auto-detected. Native ALPN
selection is copied into Info, with no secret/session-key material exposed.

Sends use immutable header/body spans and advance by the native accepted prefix,
including positive prefixes with AGAIN. Native SendVec may produce two records;
there is no full-frame adapter staging copy and no end-to-end zero-copy claim.
MaxCipherBytes gates RPC offers against total observed native ciphertext pending;
Core control traffic has separate Context.SendLimit + TCP bounds. Parsing,
read/write bytes, frames, native metadata and submitters have explicit limits.
Short Writes, blocked writes, overload and scheduling failures are counted.

`xrpcTlsClose` aborts; it never replays uncertain calls. Notification completion
means whole plaintext accepted by the native stream, not delivery/business ACK.
Final buffered plaintext is parsed before EOF failure. A partial unsent frame,
protocol violation, incoming overload or scheduler failure closes the channel.

`xrpcTlsPollRetired` composes duplex/facade/post/timer and actual public TLS/TCP
retirement. Native TLS ownership is consumed on the Worker only after its barrier,
then its owned event/session data credit is released. Closing cleanup uses one
bounded timer with backoff; exhausted timer quota relies on Poll/WaitFor nudges.
`xrpcTlsWaitFor` rejects every same-Engine Worker, with one remaining timeout.
`xrpcTlsDestroy` never blocks and retains ownership on failure. Stop new API
entries and keep the sole caller owner stable before treating success as final.
The Engine remains pinned through Destroy; shared business Server, Engine and
external callers join separately before code unload. Retained results survive.

The [local example](../examples/tls/README.md) uses an explicit pin to the existing
public test fixture, not a production PKIX policy. Full native interop, all fault
boundaries and production throughput/tails/72h gates require their own evidence.

The optional [TLS dial leaf](tls_dial.md) adds asynchronous host DNS and prepared
TCP candidates with an explicit snapshotted client identity/trust policy. The
numeric TLS leaf remains independent of Resolver and candidate dialing.
