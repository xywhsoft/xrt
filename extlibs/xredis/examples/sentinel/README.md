# Offline Sentinel discovery example

Run with `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_sentinel --no-single`.
The example uses disposable local deterministic Sentinel/master peers, discovers
and verifies a master, transfers the native client and issues PING. No external
Redis installation or credentials are required. This is a runnable protocol
example, not real Sentinel failover/performance acceptance.

The APP also selects and verifies a connected replica from a RESP3 Sentinel list,
transfers the selected native Client, and verifies PING and connection close.
The real Redis probe is `extlibs/xredis/tests/interop/run_redis_sentinel.py`.
