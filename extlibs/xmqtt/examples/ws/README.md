# MQTT5 WebSocket attachment example

Run from the repository root:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_ws_tests --no-single
```

This executable creates two local workers and a literal MQTT-over-Binary peer,
attaches native TCP on its owner worker, awaits CONNACK/PUBACK, sends MQTT
DISCONNECT then WS Close and joins Client/WS/TCP/Engine before releasing results.
It reuses the local test fixture to keep the attachment example deterministic.
It exercises the data phase after HTTP Upgrade and does not contact a broker or
perform an HTTP handshake. A real host must validate Upgrade and selected mqtt
before calling the factory; see [the contract](../../docs/client_ws.md).

The TLS test selector additionally exercises ownership-certified verified native
TLS, AUTH and Session recovery. Its historical local certificate verifier is a
fixture policy and is not a production trust example.
