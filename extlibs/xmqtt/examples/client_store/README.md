# Durable numeric TCP Client example

Build from the repository root.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_tests
```

Start an MQTT broker on a trusted loopback listener. Run the resulting
`examples_client_store_main` executable (with `.exe` on Windows) from the
build output with `127.0.0.1 1883 5`. Use `311` for MQTT 3.1.1.
The example creates a temporary journal under a fixed demonstration Client ID,
publishes a QoS 1 message to `xrt/example/durable`, waits for committed PUBACK
retirement, and checks the graceful END checkpoint before retiring its owners
and deleting the temporary file.

See the [Client storage contract](../../docs/client_store.md) for recovery
and the [durable Inbox example](../store_inbox/README.md) for application
delivery leases.
