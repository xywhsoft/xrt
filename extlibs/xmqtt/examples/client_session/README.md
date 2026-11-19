# Explicit Client Session recovery

From the repository root:

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_session --no-single
```

The offline literal peer withholds PUBCOMP on the first connection. The example
takes one received message after the connection closes and retains its reference.
It destroys that Client and Engine, then explicitly reconnects with the same
Session. The new connection sends PUBREL, receives PUBCOMP and releases the
protocol packet. The retained message and the original CLOSED Future remain
valid after both Clients, both Engines and the Session are destroyed.

The fixture simulates SessionPresent and acknowledgement cut points. It is not
a broker interoperability or crash-recovery test. See [the contract](../../docs/client_session.md).
