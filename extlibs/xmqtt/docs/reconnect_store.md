# Durable MQTT reconnect

Select `XMQTT_MODULE_XMQTT_RECONNECT_STORE` to run the normal reconnect controller
over an semantic Writer. Set `xmqttreconnectconfig.Store` to a typed
`xmqttreconnectstoreplan` before calling `xmqttReconnectCreate`. On success the
controller takes ownership of the supplied disconnected Session. The optional
`Subscriptions` plan can be supplied in the same call. The controller waits
for both the committed OPEN and subscription restoration before issuing leases. Create a fresh Session for an empty store, or
restore the exact Session from the journal before transferring the recovered
state to `xmqttStoreWriteCreate`. Bind the Writer and CONNECT identity to the
same logical broker, principal and client slot across all attempts.

The application retains the running Engine, the explicit cleanup pool and the
semantic Writer. The Writer also requires its own running storage pool. Keep both
pools and the file/clock context alive until disposal. The worker callback uses
the supplied Session, store configuration and original attempt policy to choose
a compatible durable transport. Connect to a stable endpoint and keep the
callback context alive through controller destruction. Use a trusted,
nondecreasing wall-clock source for journal time and a stable, nonempty Client ID.

Wait for an online lease before submitting Client operations. Release every
lease, including on errors, so the controller can retire the Client. A replacement
waits for the prior Client and its storage effects to retire. It becomes online
only after its own OPEN is committed. Lease generation distinguishes fresh
admissions from outstanding work on old connections. An original Future keeps its
original typed result and storage checkpoints. It may resolve before the
controller observes retirement. When waiting for a replacement, check that
the controller has reached the next generation before acquiring a lease. Retransmission and completion of
durable obligations follow the journal across generations.

The Writer exposes new deliveries through the leased Client's
`xmqttClientTakeMessage`. Applications with existing Inbox journals may
use the [legacy migration example](../examples/store_inbox/README.md).
The controller reserves protocol mutations and Writer lifecycle until Destroy.
When policy allows another attempt, an unusable Writer stops it with
`XMQTT_STATUS_SESSION`.
Destroy the controller, dispose of the Writer and recover the journal before
starting a replacement controller. Reconcile broker subscriptions in the
application after session loss.

See [Reconnect](reconnect.md), [durable TCP Client](client_store.md), and
[API reference](api/reference.md).

## Run the example

Build with the extension manifest and select `xmqtt_reconnect_store_tests`.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_store_tests
```

Run the resulting `examples_reconnect_store_main` with `127.0.0.1 1883 5` or
`127.0.0.1 1883 311` against a local broker. Start with a fresh broker session
for the example's fixed Client ID. The example uses a temporary journal with
a QoS 1 publication and a graceful END. Append `expect-replay` when a test proxy
drops the first PUBACK and disconnects that connection. The example then waits
for generation two and checks that the journaled obligation was retired.

## TCP hostname and prepared-address factories

With `XMQTT_MODULE_XMQTT_CLIENT_DIAL` enabled, a factory can pass the original
Session and Writer to `xmqttClientDialStore` or `xmqttClientDialAddressesStore`.
Keep its Resolver or retained address list available for each attempt.
An accepted replacement still waits for committed OPEN before becoming
available through an online lease. The TCP constructors also support the
MQTT 5 enhanced authentication proof gate. [Durable TCP contract](client_store.md#hostnames-and-prepared-addresses).

## TLS and WebSocket recovery

The factory can issue a new `xmqttClientTlsDialStore`,
`xmqttClientWsConnectUriStore` or `xmqttClientWssConnectUriStore` for each
generation. Use the factory's Session and Writer with the original broker
identity. Keep the resolver, verifier and factory data alive through controller
destruction. Each secure attempt verifies the peer before MQTT OPEN. A valid
replacement transport still waits for OPEN and replays using the committed
MQTT state.

Build the multi-transport example with the complete feature set.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt,xmqtt_reconnect_store_tests --exclude-test "*" --no-single
```

The executable accepts these endpoint forms.

```text
tls HOST PORT 5|311 CA.pem
ws://HOST:PORT/mqtt 5|311
wss://HOST:PORT/mqtt 5|311 CA.pem
```

Append `expect-replay` to exercise QoS 1 or `replay-qos2` to exercise QoS 2
against a proxy that drops one acknowledgement and disconnects the transport.
The example checks the old Future and the committed end of the replacement.
TLS uses an explicit CA and hostname verification. The example opens a full
TLS handshake on each generation.

Run the independent [Mosquitto qualification](../tests/interop/README.md) to
check wire identities, actual subscriber deliveries, and both framing and
transport closure. It covers PUBLISH retransmission following lost PUBACK or
PUBREC and PUBREL recovery following lost PUBCOMP. See the retry rules in
[MQTT 5.0 §4.4](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
and [MQTT 3.1.1 §4.4](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html).


## Recovery from an online checkpoint

With the complete `xmqtt` feature set the example also accepts
`compact-cut DIRECTORY` and `compact-resume DIRECTORY`. These modes are
driven by the interop runner. It provisions the three journal slot filenames
in a private directory. The APP opens and locks those files and gives the Slot
borrowed handles. The runner tells the first publisher to compact while one
QoS 2 acknowledgement is withheld. After the selector has committed, the
runner terminates the process. The replacement opens the selected slot,
recovers the typed State, restores a fresh Session, and connects with the
same Client ID and Clean Start disabled. It verifies the replay, commits END,
and cold-reopens the journal again. [Run the checkpoint qualification](../tests/interop/README.md#online-checkpoint-and-native-client-restart).


The runner also uses `compact-auto-cut` and `compact-auto-resume` to
checkpoint after the APP commits a trigger delivery to `journal.app`.
With `compact-pressure-cut` and `compact-pressure-resume`, the APP selects
the smaller protocol journal before admitting the trigger. The broker
receives the network acknowledgement before termination. The restored
application reads and processes the pending record and commits the
application acknowledgement in `journal.app`.

The replay-pressure variants use `compact-replay-cut` and
`compact-replay-resume` followed by DIRECTORY, a quota name
(`transactions`, `operations` or `bytes`), and its positive integer limit.
Use the interop runner to select a limit that the old cut exactly exhausts.
It verifies recovery under that same limit through the final durable END.
