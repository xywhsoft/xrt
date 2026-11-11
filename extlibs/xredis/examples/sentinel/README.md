# Offline Sentinel discovery example

Run with `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_sentinel --no-single`.
The example uses disposable local deterministic Sentinel/master peers, discovers
and verifies a master, transfers the native client and issues PING. No external
Redis installation or credentials are required. This is a runnable protocol
example, not real Sentinel failover/performance acceptance.
