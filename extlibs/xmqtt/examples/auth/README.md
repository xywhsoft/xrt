# MQTT5 enhanced authentication example

From the repository root, with Python and GCC available:

```text
python tools/build.py --compiler gcc --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_client_auth --no-single
```

No installed broker, credentials or external service is required. The process
starts a two-worker loopback peer, performs two initial AUTH challenges, verifies
the final CONNACK proof, repeats two-round reauthentication and verifies its final
AUTH proof. Every ticket is released; Client, peer, listener and Engine are
physically retired. The final typed result is checked after destruction and all
Futures are released. The process exits and closes its ephemeral port itself.

Expected output includes `MQTT5: two-round initial authentication and
reauthentication, explicit local final proof, physical shutdown verified`.

The fixed binary tokens exercise protocol/ownership only. Replace their handling
with the mutually agreed authentication algorithm and validate the server's final
proof. For expensive algorithms, move the unique ticket to an application worker;
Notify must stay nonblocking. The example is a protocol fixture, not a broker or
cryptographic interoperability test. See [contracts](../../docs/client_auth.md).
