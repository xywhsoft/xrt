# Explicit TCP Dial/Auth Session (developing)

Select `xmqtt_client_session_dial` for `xmqttClientDialSession` and
`xmqttClientDialAddressesSession`. They combine the existing
[Session/Inbox](client_session.md) ownership and [Dial](client_dial.md) policies.
Also select `xmqtt_client_auth` for their `SessionAuth` variants. Numeric
`xmqttClientConnectSessionAuth` needs only `xmqtt_client_session` and
`xmqtt_client_auth`; it does not acquire Dial, Resolver or TLS dependencies.

Each factory captures one monotonic entry time. SetupTimeout spans admission,
DNS, connection attempts, CONNECT/CONNACK and application authentication proof.
Dial's optional additional timeout never extends it. Host/config/CONNECT are
copied and prepared endpoint lists retained before return. Resolver lifetime,
detached shared lookups, family filtering, fallback and MaxAttempts follow Dial.
Client-owned host scratch uses MaxPrivateBytes; native Resolver/Dial buffers
retain their separately bounded Core configurations.

Session/Inbox are exclusive through actual Client Destroy. Failed constructors
refund claims and leave output untouched. DNS/refusal, setup timeout/cancellation
and denied final proof preserve old offline obligations and expiry. Session
opening/reset and replay begin only after accepted MQTT/local proof gates.
After acceptance, a local replay failure may follow an authenticated reset/resume
even if Ready fails. Session expiry then follows actual connection termination.
Original Futures remain connection-scoped; reconnect is explicit after old
Client destruction and can use a new Engine. Bind every endpoint and both passive
owners to one logical broker and security principal.

MQTT5 enhanced authentication follows [OASIS MQTT5 section4.12](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).
Algorithms/credential generation belong to the application. Challenge responses
are immutable owned snapshots, and successful CONNACK proof must be accepted
locally before Ready/application delivery or Session mutation. Reauthentication
uses the verified connection without reopening Session; existing ticket, deadline,
traffic and physical-retirement rules apply. Credentials never enter passive
Session storage. TCP factories add no transport encryption; TLS factories remain
separate. Literal test tokens are assertions, not an authentication mechanism.

The [runnable example](../examples/client_session_dial/README.md) exercises copied
DNS names or prepared fallback, two Engines and QoS2 PUBREL-only recovery. The
benchmark uses the existing closed-loop Session workload with prepared Dial
setup outside its timer. New constructors add no protocol hot-path branch,
allocation, copy, synchronization or public/private state fields. Existing Session
metadata/gates, Auth state and native Dial setup remain real costs; this is not
zero-overhead or full performance acceptance. Assigned ids, automatic reconnect,
durable recovery, WebSocket and complete native/long-run gates remain open.

Validation and limits are recorded in the [stage result](../tests/results/session-dial-20261110.json).
All new constructors have modular/single, package, proof/failure and lifetime
coverage. Broker ACK-cut recovery covers plain Dial only; literal Auth fixtures
are not authentication-plugin interoperability. Performance noise/raw failures
remain recorded and the complete production gates remain open.
