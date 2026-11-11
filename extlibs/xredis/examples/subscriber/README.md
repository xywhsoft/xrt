# Native subscriber example

From the repository root:

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_subscriber --exclude-test "*" --no-single
```

The example owns a local deterministic protocol peer and Engine. It dedicates
an idle ready Client, subscribes to a binary name, consumes a separately owned
message, unsubscribes all and closes every transport. The retained message is
still valid after Engine destruction. It contacts no external service.
The peer is a fixture, not a Redis interoperability or performance comparator.
Real TCP/TLS/RESP2/RESP3 scenarios live in tests/interop/redis_subscriber.c.
