# MQTT native WS/WSS connection factories (developing)

`xmqtt_client_ws_connect` builds one ordinary Client from a numeric address,
counted DNS name or retained prepared-address list. The optional
`xmqtt_client_ws_connect_tls` creates the existing trusted native TLS Session
before DNS and performs the same HTTP/MQTT setup over WSS. These use public XRT
interfaces and the existing Client post/timer, not a separate runtime.

The original Client.SetupTimeout starts at API entry and covers policy/CONNECT/
auth snapshots, candidates, TCP, TLS, HTTP Upgrade and CONNACK/local proof. Queued
request deadlines also retain their original API-entry times. No MQTT output is
sent before a strict101 response bound to the fresh random Key, correct Accept
and exact mqtt subprotocol. Binary parsing then uses the existing QoS/aliases/
subscriptions, authentication and protocol Session logic. No redirect, proxy, credential retry,
compression offer, fallback or implicit replay is added.

## Configuration and lifetime

Initialize `xmqttclientwsconnectconfig`, set its required HTTP Host authority and
optional Target (default /mqtt), Origin and extra Fields. Authority includes any
nondefault port and brackets IPv6. Network Dial Host is a distinct counted name,
used for DNS and default TLS SNI/identity; numeric/prepared WSS requires an
explicit native TLS peer name. ConnectConfig.Dial controls candidates in both
transports; the TLS argument supplies Session/Stream/metadata/client-proof policy.
Original native shared-object and callback/code lifetimes continue to apply.

The complete request wire snapshots strings/fields before return. Managed
Upgrade/framing/Origin fields and repeated Authorization are rejected; remaining
extras use strict Core syntax validation. Defaults/hard caps:16KiB response head,
128 descriptors,64KiB request output; <=32 extras/16KiB names+values. Four interim
responses are accepted by default (hard maximum16);100/103 do not refresh time.
Non101 statuses resolve Ready as REJECTED, while malformed negotiation uses a
protocol outcome. There is no authentication challenge retry or redirect follow.

Optional WebSocket.Auth and WebSocket.Session use their existing explicit
feature and ownership contracts. On a false return the caller retains the
prepared address list and the output is unchanged. Success returns the unique Client in CONNECTING;
Ready is a typed outcome and may still fail. Close/Wait/Destroy preserve actual
candidate/raw/TLS/WS duties, including pre-Upgrade aliases and original TLS owner
credits. The borrowed Resolver remains alive through Client retirement; its own
Destroy joins any detached lookup. The host separately joins Engine/code.

WSS has no cleartext fallback. Its negotiated ALPN must be empty or http/1.1.
A supplied native Resume can be used. Select the separate
[WSS credential handoff](client_ws_tls_resume.md) leaf to capture newly issued
credentials after WS event handoff with the existing bounded cache APIs.
Without that leaf, TLS.Session.ResumeLimit must explicitly be zero.

## Resources and validation

The response fast path borrows a contiguous native prefix. A partial head uses
one lazily charged16KiB cache with an incremental delimiter/line-limit probe;
Core remains authoritative for complete semantics. The cache may preview some
WS suffix bytes but consumes exactly the header, leaving the native suffix
untouched. Info.HeadCopyBytes records those actual copies. No mutable Pullup is
required on an IOCP receive reservation. Request/descriptor/cache storage and the
Key are wiped/freed after handoff or setup stop; small HTTP diagnostic state stays
through Client destruction.

Earlier x64 builds measured 80 bytes for connection info, 240 bytes for private
HTTP state and an 8-byte pointer in the optional WS connection state. HTTP
state, request, descriptors and cache
are charged to MaxPrivateBytes. Native Dial/Resolver/WS/TLS/TCP/buffer/allocator
storage and host CA-file I/O remain separate real costs.

[Runnable CLI](../examples/ws_connect/README.md) performs native DNS, actual
Upgrade and optional CA verification. Plain/full/root and stats-off singles,
modular tests, separate static/shared protocol consumers and declaration consumers
pass. Positive, fault, atomicity, authentication and Session tests cover numeric/DNS/prepared,
MQTT5/3.1.1, tiny work quotas, split records, interim responses, rejected/malformed
headers, setup/queued-request deadlines, detached DNS, constructor OOM and real
pre-Upgrade native aliases. Windows select/IOCP and WSL select/epoll pass the
functional suites; WSL also passes ASan/UBSan/leaks and independent TSan.
A discovered post-publication owner-only pointer
read is corrected; final TSan positive/fault/state suites pass on both backends.
WSL does not prove native Linux production acceptance.

An independent Python stdlib/OpenSSL peer passes12 Windows/WSL MQTT5/3.1.1 cases
across WS/TLS1.2/TLS1.3, including103/101, independently computed Accept, fragmented
Binary MQTT, Ping/Pong, real CA trust, close echo and physical shutdown. A pinned
Mosquitto2.0.22 package completes all8 WS/WSS data exchanges and actual joins, but
all8 CLI commands fail strict clean Disconnect. An independent plain byte relay
records client CONNECT/PUBLISH/DISCONNECT/Close1000, server CONNACK/PUBACK and EOF
without a returned WS Close. The native success predicate is unchanged: these
are retained interoperability failures, not clean-success evidence.

Cold setup and HTTP-established steady QoS1 profiles run actual exchanges,
checksum and physical joins. Three samples on the shared unpinned host fail
several quality limits; no thresholds are lowered and no accepted performance
comparison is claimed. Native masking/frame allocation, copies, TLS work and
retained message/result traffic remain real. See the source-bound
[stage report](../../../docs/development/xmqtt-websocket-connect-20261116.json).

## Remaining delivery

The later [URI factories](client_ws_uri.md) compose strict ws/wss authority and
resource mapping. [Automatic WS/WSS recovery](reconnect_ws.md) now has separate
controller/generation-lease and actual broker/gateway evidence. Remaining delivery:
post-Upgrade credential capture, durable protocol state, remaining asynchronous
OOM/partial-write/load cases, broker-close qualification/compatibility policy,
controlled throughput/tails/CPU and native Windows/Linux soak remain required.
All leaves remain developing; the four-product release gate remains closed.

Protocol references: [MQTT5 chapter6](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [RFC6455](https://www.rfc-editor.org/rfc/rfc6455.html). No reference client
implementation is copied.
