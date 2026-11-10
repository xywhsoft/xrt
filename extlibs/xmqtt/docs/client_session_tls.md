# Explicit TLS Client Session (developing)

Select `xmqtt_client_session_tls` / `XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS`.
The numeric, hostname and prepared-address factories combine existing
[Session/Inbox](client_session.md) ownership with [TLS](client_tls.md).
`xmqttClientTlsConnectSession`, `xmqttClientTlsDialSession` and
`xmqttClientTlsDialAddressesSession` require both passive owners. When
`xmqtt_client_auth` is also selected, their `SessionAuth` variants support MQTT5
challenge, final proof and subsequent reauthentication. TLS client-certificate
authentication remains independent and follows the existing TLS config.

The caller binds one Session/Inbox and ClientId/version to one logical broker
and security principal. Credentials, verifier handles and TLS sessions are not
persisted into MQTT protocol state. Metadata is snapshotted by the native TLS
session before DNS. Hostname inference, explicit numeric/prepared identities,
strict verification, ALPN and optional TLS resumption follow Client TLS.
One entry setup deadline covers metadata admission, DNS, TCP, TLS and MQTT proof.
Old protocol state is opened/replayed only after accepted MQTT readiness gates.
A failed handshake/proof preserves the old offline obligations and expiry.
Once MQTT/proof gates accept the connection, Session reflects that authenticated
reset/resume even if later local replay construction fails before Ready succeeds.
Its subsequent expiry starts at that accepted connection's actual termination.
With enhanced authentication the original CONNECT snapshot stays charged to
the existing setup/private bounds until local final-proof acceptance or failure.
Coalesced post-CONNACK frames are backpressured before receive preflight or
application delivery. Proof rejection cannot reset, open or replay old state.

Reconnect is explicit after physical Client Destroy. A new Engine is permitted.
Both passive objects stay exclusively claimed through Destroy; constructor
failure changes no output and refunds every claim. Original Futures never migrate
to a later connection. The application must handle original uncertain outcomes.

QoS1/2 admission keeps one immutable canonical plaintext snapshot. Shared alias
output gathers at most four spans; compact aliases use two. TLS encryption
copies plaintext into authenticated record buffers and holds an aligned native
retirement fence per request, including recovery wrappers. MaxPrivateBytes
covers both Session driver and TLS request/state metadata. Native TLS/ciphertext
buffers keep their separate bounded Core policy. These costs are not zero-copy
or zero-overhead. QoS0 still does not prove peer receipt or durable storage.

The [example](../examples/client_session_tls/README.md) performs two explicit
MQTT5 connections to a trusted external TLS broker using one Session/Inbox.
Native TLS resumption is separate from MQTT SessionPresent. No automatic
reconnect, durable storage, assigned-id adoption, WebSocket or production/
performance acceptance is implied.
