# APP-owned Inbox

Run from the repository root.

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client
```

The loopback peer echoes one QoS 1 publication on each of two connections.
The APP takes each received message with `xmqttClientTakeMessage` and stores the
owned reference in a two-entry array. It verifies both payloads after both
clients and their network engines have been destroyed, then releases the
references. The APP queue takes no copy of the packet or its payload.

In a longer-running APP, set a count and byte budget for retained messages.
Stop the connection and drain any queued messages that should outlive it
before calling Destroy. Use a caller-owned [Session](../../docs/client_session.md)
when the MQTT exchange itself should resume across connections.
