# Durable TCP Client under development

The developing `xmqtt_client_store` module exports
`xmqttClientConnectStore` for numeric TCP in MQTT 5.0 and 3.1.1.
Create the Writer with `xmqttStoreWriteCreate` and supply the matching fresh
or recovered disconnected Session. Accepted CONNACK precedes the BIND/OPEN
commit, and Ready follows it. Keep the borrowed Engine and storage task pool
running through actual Client retirement. Destroy the Client before destroying
its Session or Writer, then close the journal file after Writer destruction.
Use a trusted endpoint bound to the journal's broker, principal and Client slot.
The nonblocking clock callback runs on the Engine worker and supplies trusted,
nondecreasing Unix milliseconds.

Set `xmqttstorewriteconfig.ProtocolOnly` before creating the Writer to deliver
new incoming publications through `xmqttClientTakeMessage`. QoS 0 and 1 use
the ordinary bounded receive queue. Before accepting QoS 2, the Client reserves
room for the live message, commits its receive identity without payload, then
queues the message and PUBREC in that order. A failed commit does not publish
the message. A late confirmed commit after local close still publishes it to
the retiring Client queue.

The queue is volatile. Recovery preserves QoS 2 deduplication, including when
the process exits after commit and before the APP takes the message. The APP
owns any durability of business processing. Earlier journal entries remain
readable. With the default Writer configuration the receive driver journals
each accepted PUBLISH, including QoS 0, before any ACK. Its persisted packet
has a full topic and no alias property, and the APP can
[read and retire pending publications](store_inbox.md) while connected.

For QoS 2, a duplicate PUBLISH repeats PUBREC without another acceptance.
A known PUBREL commits the release and identifier retirement together before
PUBCOMP. A previously journaled application delivery remains readable until
its separate retirement. If the connection closes after
that commit but before PUBCOMP, the next connection answers retried PUBREL with
0x92 in MQTT 5 or the four-byte PUBCOMP in MQTT 3.1.1. A fresh PUBLISH can then
reuse the identifier. See [MQTT 5.0 Sections 3.7.2.1 and 4.3.3](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1 Section 4.3.3](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).

One receive transaction can be pending per Client. Existing committed ACKs and
keepalive may progress during storage. The driver gives the
received frame a commit budget of CloseTimeout from dispatch. A close or local
failure suppresses the pending network response, while any actual commit still
updates the corresponding Session state before it closes. Recover and import a
fresh Session before reattachment. The Writer and native Client must already
have retired.

QoS 1 and QoS 2 PUBLISH use the existing Session snapshot and native output
queue. A new packet's canonical full topic, identifier and attempt are committed
together before first output. Its first frame has DUP clear. On same-session
resume, a recorded attempt has DUP set. A restored packet with no attempt first
commits OUT_ATTEMPT and sends with DUP clear. QoS 0 uses ordinary volatile output.
See [MQTT 5.0 Sections 3.3.1.1 and 4.4](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1 Section 4.4](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).

Successful PUBREC advances the durable flow to PUBREL. PUBACK, negative PUBREC
and PUBCOMP commit retirement before the request completes or releases its id.
Observed broker ACKs use the bounded CloseTimeout storage budget. A request's
original deadline and cancellation still determine its local outcome while the
native write settles. If commit completes late, the confirmed broker facts
survive. If metadata cannot be copied, the durable transition and scalar reason
survive while the connection reports the local error.

Call `xmqttClientStoreOutcome` on a live resolved durable Publish or
Disconnect result for the independent storage effects. The copy includes an
epoch, the outgoing key where applicable, and sequence/effect pairs for
Attempt, Pubrel, Completion and End. A MAY_BE_COMMITTED sequence is a
candidate and requires recovery. NOT_SENT on the original connection can
accompany a committed obligation when Close or timeout wins before the first
byte. Recover the authoritative journal before deciding whether to resubmit.
Recovered obligations have no replacement application Future. QoS 0 Publish
has only the ordinary network result.

SUBSCRIBE and UNSUBSCRIBE return the usual per-filter broker outcomes. The
application reconciles its desired subscriptions after a broker reset. The
store has no durable subscription intent registry.

Graceful Disconnect drains outstanding traffic, closes the native transport and
waits for END to settle. A successful result requires committed END. A complete
local native disconnect records the requested reason, effective expiry and
native-close wall time. The broker supplies no receipt for DISCONNECT. A
deadline or cancellation can determine the local status even if END later
commits. If the native transport fails after accepting a complete DISCONNECT
that changed expiry, the journal remains live at cut for recovery. For MQTT 5,
the expiry override is validated against the original CONNECT. See
[MQTT 5.0 Section 3.14.2.2.2](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

Run the integration fixture and build the
[public TCP example](../examples/client_store/README.md) from the repository root.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tests
```

This builds the example and runs the modular and single-header fixtures.
The private context and encoded transaction buffer are charged to Client
MaxPrivateBytes. The staged CONNACK transfers into Ready using the existing
object quota. Encoded transaction bytes are copied under the Writer/worker's
bounded admission contract.

## Hostnames and prepared addresses

Select both `XMQTT_MODULE_XMQTT_CLIENT_STORE` and `XMQTT_MODULE_XMQTT_CLIENT_DIAL`
for `xmqttClientDialStore` and `xmqttClientDialAddressesStore`. They share the
same durable protocol and use the existing [Client dial policy](client_dial.md).
Keep the Resolver alive through Client retirement. A DNS lookup shared by
the Resolver can outlive the Client. A prepared address list is immutable
and can be released after successful admission. Its endpoints must belong
to the same trusted broker identity. The application can reuse either
factory on each [durable reconnect](reconnect_store.md) attempt.

The original `SetupTimeout` includes DNS, TCP, the MQTT handshake and durable
OPEN. Failures before OPEN leave the committed journal unchanged. Select
`XMQTT_MODULE_XMQTT_CLIENT_AUTH` for the `xmqttClientDialStoreAuth` and `xmqttClientDialAddressesStoreAuth`
variants. With enhanced authentication, BIND/OPEN also waits for accepted
final broker proof. Callers keep DNS policy and authentication credentials
outside the journal.

The [verified TLS adapter](client_store_tls.md) supports hostname dialing.
The [enhanced authentication adapter](client_store_auth.md) supports MQTT 5
over direct TCP or verified TLS. The [durable WebSocket adapter](client_store_ws.md)
supports both WS and WSS URI endpoints. Cross-host fencing and production throughput and tail-latency
qualification are pending.
