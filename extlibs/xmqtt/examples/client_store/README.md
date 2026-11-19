# Durable TCP Client example

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tests
```

Start an MQTT broker on a trusted loopback listener. Run the resulting
`examples_client_store_main` executable (with `.exe` on Windows) from the
build output with `localhost 1883 5` or `127.0.0.1 1883 5`. Use `311` for MQTT 3.1.1.
The example creates a temporary journal under a fixed demonstration Client ID,
publishes a QoS 1 message to `xrt/example/durable`, waits for committed PUBACK
retirement, and checks the graceful END checkpoint before retiring its owners
and deleting the temporary file.

See the [Client storage contract](../../docs/client_store.md) for recovery
and the [durable Inbox example](../store_inbox/README.md) for application
delivery leases.

## Verified TLS

Build the same example via the `xmqtt_client_store_tls_tests` suite. Run it
with `localhost 8883 5 SERVER_CA.pem` or use `311` for MQTT 3.1.1. The named
CA file is the example's trust store. The server certificate must match the
given hostname. The result includes a committed END after authenticated TLS
closure. [TLS API and lifetime contract](../../docs/client_store_tls.md).

## WebSocket

Build through the `xmqtt_client_store_ws_tests` suite. Pass the complete URI
and protocol level instead of a separate port. Use the broker's trusted CA
file for WSS.

```text
examples_client_store_main ws://127.0.0.1:9001/mqtt 5
examples_client_store_main wss://localhost:9002/mqtt 311 SERVER_CA.pem
```

The example expects a complete WebSocket close handshake. Its failure output
includes the independent local Disconnect status and journal END effect.
[Durable WebSocket contract](../../docs/client_store_ws.md).
