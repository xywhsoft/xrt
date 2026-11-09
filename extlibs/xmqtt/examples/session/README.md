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
Existing Client factories do not yet attach this state; that integration and
durable recovery remain required work.
