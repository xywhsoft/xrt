# Explicit Client Session recovery

From the repository root:

```sh
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_session --no-single
```

The offline literal peer withholds PUBCOMP on the first connection. The example
destroys that Client and Engine, then explicitly connects a new Client/Engine
using the same Session and Inbox. The resumed connection sends PUBREL only,
receives PUBCOMP and releases the retained packet. The original Future remains
the original connection's CLOSED result. Both passive owners can then be
destroyed before those original result objects.

The fixture simulates SessionPresent and acknowledgement cut points. It is not
a broker interoperability or crash-recovery test. See [the contract](../../docs/client_session.md).
