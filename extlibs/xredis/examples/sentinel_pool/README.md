# Offline automatic Sentinel pool

Run `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_sentinel_pool --no-single`.
Disposable local peers emulate changes to the endpoint announced by Sentinel.
The existing pool worker discovers and verifies the endpoint, retires the old
generation and supplies another native lease. The example runs without a Redis
installation or credentials.

The example also runs the replica pool through the same generation
switch. For a real failover fixture, pass `--replicas` to
`extlibs/xredis/tests/interop/run_redis_sentinel_pool.py` with the native
probe. It verifies the new replica identity and the original blocking read's
native outcome across Sentinel promotion.

The example issues `xredisSentinelPoolRefresh` after the fixture changes the
announced endpoint. The old lease stays held until verified replacement
fences it. The queued native acquisition then receives the new generation.
