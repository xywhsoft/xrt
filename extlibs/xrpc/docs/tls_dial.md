# Asynchronous TLS connection sources

The developing `xrpc_tls_dial` leaf adds `xrpcTlsDialConfigInit`, `xrpcTlsDial`
and `xrpcTlsDialAddresses` to the existing framed bidirectional TLS peer. It
borrows the host Engine/shared business Server and optional Resolver; it creates
no Engine, Resolver, runtime, thread or hot IO dispatch layer.

The DNS routing name is independent of explicit `Client.ServerName/VerifyName`
and verification/resumption policy. Factory return snapshots a native client
Session before async work: bounded names/ALPN are copied and native context,
verifier and resume references retained. Borrowed callback code/data remains
resident through Destroy. There is no inferred identity or cleartext fallback.
DNS accepts bounded nonempty names with no embedded NUL, default1024/hard65535
bytes and no IDNA conversion. Prepared immutable lists retain one alias before
return, filter family and preserve selected nonzero ports, at most64 endpoints.
Defaults are dual stack,250ms fallback and8 attempts, hard64 attempts.

One `Config.Stream.ConnectTimeout` deadline starts at original factory entry,
including policy snapshot, queue, DNS, candidate race/loser retirement and verified
TLS OPEN. Zero expires immediately and -1 disables this overall deadline. Native
TLS handshake timeout remains effective. Call/notify/batch keep their independent
entry deadlines and cancellation while queued. Unsent call expiry does not poison
a healthy connection attempt; uncertain operations are never replayed.

Candidates have no facade callbacks or TLS Session. The transferred TCP winner
gets a temporary owned binding, then hands off the precreated Session on its
Worker; existing encrypted IO/framing and the borrowed business executor drive
normal work. Only native verified TLS OPEN publishes readiness and enables RPC
writes. Both winner publication and OPEN preserve concurrent Close under the
facade gate. Already buffered native bytes follow the existing handoff path.

One product-private cold allocation holds plan/name/config and native list/dial
owner references; Core Dial/Resolver own their separate metadata/name copies.
Creator and owned Done context have actual references. Owned callback retirement
releases the cold source/controller; it is not kept for the full TLS connection.
Close cancels pending setup and waits for source/native raw/TLS/TCP/message duties.
Native cleanup uses the existing bounded backoff timer; a pending DNS source alone
adds no polling loop. Cancelled dial callbacks can retire while an independent
shared host lookup still runs. Keep Resolver valid through peer retirement and
lookup code/data through the Resolver's separate actual worker/thread join.

Local deadlines, cancellation, native admission/memory/transport/handshake failure
remain distinct peer reasons and typed call outcomes, with copied native metadata
in TLS Info. Native full cause graphs are not retained by this facade. Existing
frame/ciphertext/metadata/submitter/turn bounds, result ownership and physical
Close/Poll/Wait/Destroy contracts remain unchanged. Shared Server, Engine and
external callers have separate joins before code unload.

The [local example](../examples/tls_dial/README.md) uses OS localhost routing and
an explicit public fixture pin. Production PKIX/trust/cipher/resume matrix,
broader failure/performance/fixed-arrival/connection-scale/native Linux/72h gates
remain required. stdio/WebSocket, negotiated deadline/cancel, credentials and
recovery remain in the complete four-product goal.
