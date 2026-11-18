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

The last example configures one seed by endpoint. After learning a second
Sentinel, it makes the configured seed unavailable and repairs a native
connection through the learned endpoint. The real Redis runner
`extlibs/xredis/tests/interop/run_redis_sentinel_seeds.py` also checks this
path with two cooperating Sentinel processes.
