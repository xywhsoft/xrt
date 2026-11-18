# Offline automatic Sentinel pool

Run `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_sentinel_pool --no-single`.
Disposable local peers emulate a changing announced master. The existing pool
worker discovers/verifies the first master, retires a connection, automatically
rediscovers the new endpoint and supplies another native lease. No external
Redis or credentials are needed. This is protocol/lifecycle example evidence,
not real Redis failover or performance acceptance.

The example also runs the replica pool through the same generation
switch. For a real failover fixture, pass `--replicas` to
`extlibs/xredis/tests/interop/run_redis_sentinel_pool.py` with the native
probe. It verifies the new replica identity and the original blocking read's
native outcome across Sentinel promotion.
