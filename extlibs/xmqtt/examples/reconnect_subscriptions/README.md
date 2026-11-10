# Automatic subscription restoration

This offline loopback example starts a bounded local peer, Engine and cleanup
pool. It copies an initial `example/+` subscription, waits for confirmation, sends
a normal publication, and forces a real transport outage while holding the old
Client lease. After the old native peer physically retires, releasing the lease
allows automatic replacement. The fresh session is subscribed again before the
new ONLINE lease becomes available. The example verifies both control packets,
per-filter confirmation, retained old message/Future and complete shutdown.

The local peer acknowledges controls and echoes publications. It is a fixture,
not a broker implementation, routing oracle or interoperability comparator. No
network service, certificate or credential is required.

From the repository root:

```text
python tools/build.py --manifest extlibs/xmqtt/config/modules.json --suite xmqtt_reconnect_subscriptions,memory_stats,time,atomic --jobs 3 --cflag=-O2
```

The final line is:

```text
MQTT5 subscription recovery: initial intent, actual outage, lost-session restoration, retained outcomes and physical joins verified
```

With an external broker use the same public API and replace `connectAttempt` with
the chosen Session numeric/Dial/TLS/Auth factory. Retained sessions carry confirmed
subscriptions forward; lost sessions rebuild the desired registry. An initial
volatile registry cannot discover subscriptions from a previous process: select
Clean Start when that state must be discarded. Raw leased Client controls bypass
the registry. See [the contract](../../docs/reconnect_subscriptions.md).
