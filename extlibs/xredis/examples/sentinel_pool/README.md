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

The last example queries `SENTINEL SENTINELS` using a short-lived Redis
client on the APP owner thread while the pool serves an existing lease.
The APP enables its preapproved loopback backup after checking the reply.
The pool discovers the same data node using that index when the first seed
becomes unavailable and a connection needs repair. The real Redis runner
`extlibs/xredis/tests/interop/run_redis_sentinel_seeds.py` exercises the same
APP policy with two cooperating Sentinel processes.
