# In-memory MQTT session state

Build/run from the repository root:

```
python tools/build.py --compiler gcc --arch native --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_session --cflag=-O2
```

The example simulates authenticated connection boundaries and completed writes.
It abandons a selected PUBLISH, resumes the same session, checks DUP/original
identifier, completes QoS2 and retains immutable views after session destruction.
It performs no network I/O and does not prove broker receipt or disk durability.
See [the contract](../../docs/session.md) before attaching a custom transport.
The optional Client Session factories attach this state to TCP/TLS. Durable
recovery remains separate.

Run the built `examples_session_main` with `--assigned` to simulate an empty first CONNECT
and accepted server-assigned id (64-byte reservation), obtain its immutable view,
and reconnect using that id before the same QoS2 replay. The default mode retains
a fixed caller-supplied id. Both modes simulate authenticated boundaries; they
do not authenticate a network connection or prove broker delivery.
