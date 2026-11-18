# Durable MQTT reconnect

Select `XMQTT_MODULE_XMQTT_RECONNECT_STORE` to run the normal reconnect controller
over an Inbox Writer. `xmqttReconnectCreateStore` takes ownership of the supplied
disconnected Session on success. Create a fresh Session for an empty store, or
restore the exact Session from the journal before transferring the recovered
state to `xmqttStoreInboxCreate`. Bind the Writer and CONNECT identity to the
same logical broker, principal and client slot across all attempts.

The application retains the running Engine, the explicit cleanup pool and the
Inbox Writer. The Writer also requires its own running storage pool. Keep both
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
original typed result and storage checkpoints. Retransmission and completion of
durable obligations follow the journal across generations.

Use `xmqttStoreInboxTake`, `xmqttStoreInboxRelease` and `xmqttStoreInboxAckAsync`
on the same Writer while online, offline or reconnecting. After application
effects, wait for a COMMITTED Inbox ACK to confirm durable removal. The
controller reserves protocol mutations and Writer lifecycle until Destroy.
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
