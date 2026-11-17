# Client storage integration under development

This module currently has no public factory. It adds a private path through the
actual numeric TCP Client to validate accepted CONNACK -> durable BIND/OPEN
-> Ready ordering,with native-pool Future notification and physical Client duties.
It records END at the actual native close edge. The borrowed Engine and task pool
remain running through Client retirement; Writer,file/provider and callback code
outlive their physical duties. All ordinary Client selections remain independent.

The receive driver journals each accepted PUBLISH, including QoS 0. It queues
PUBACK or PUBREC only after commit. MQTT 5 alias binds and lookups follow the
existing connection rules; the persisted packet has a full topic and no alias
property. Durable payloads are available to the committed store and never enter
the volatile Inbox. Use a Writer created in [durable Inbox mode](store_inbox.md)
to claim and acknowledge these deliveries while the Client is connected.

For QoS 2, a duplicate PUBLISH repeats PUBREC without another acceptance.
A known PUBREL commits the release and identifier retirement together before
PUBCOMP, retaining the unacknowledged delivery. If the connection closes after
that commit but before PUBCOMP, the next connection answers retried PUBREL with
0x92 in MQTT 5 or the four-byte PUBCOMP in MQTT 3.1.1. A fresh PUBLISH can then
reuse the identifier. See [MQTT 5.0 Sections 3.7.2.1 and 4.3.3](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1 Section 4.3.3](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html).

One receive transaction can be pending per Client. Existing committed ACKs and
keepalive may progress during storage. The current private path gives the
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

The private API reports network delivery on the original connection. In this
stage, a NOT_SENT result can accompany a committed obligation when Close or
timeout wins before the first byte. Resume the authoritative journal rather
than automatically submitting another application request. Uncertain/failed
storage stops the owner for recovery. A recovered protocol obligation never
creates a replacement application Future.

The public durable factory, subscription intents, durable graceful disconnect,
full transport and authentication support, and transport fencing across
independent hosts remain open.


Run the private integration fixture from the repository root:

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tests
```

This selects modular and single-header fixtures; it adds no public API/example.
The root module includes this developing feature. Narrow Client/Session/Writer
selections remain independent. The private context and encoded transaction buffer
are charged to Client MaxPrivateBytes; the staged CONNACK uses the existing object
quota and transfers into Ready without another message copy. The Writer/worker
still copy encoded transaction bytes under their own bounded admission contract.
No zero-copy or measured throughput/tail-latency claim is made.
