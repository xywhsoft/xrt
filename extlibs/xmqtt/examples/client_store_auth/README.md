# Authenticated durable Client example

From the repository root, build and run the example and its fixture with

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_store_auth_tests
```

The example starts a loopback MQTT 5 peer and uses a temporary native journal.
It answers two challenges, checks the final proof through the ticket API,
waits for committed OPEN and verifies graceful END before cleanup. The fixed
tokens and clock are deterministic local test values. The application supplies
its own algorithm, credentials and trusted clock for a real deployment.
[API and ownership contract](../../docs/client_store_auth.md).
