# Offline automatic Sentinel pool

Run `python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_sentinel_pool --no-single`.
Disposable local peers emulate changes to the endpoint announced by Sentinel.
The existing pool worker discovers and verifies the endpoint, retires the old
generation and supplies another native lease. The example runs without a Redis
installation or credentials.

The application-owned replica example queries `SENTINEL REPLICAS`,
verifies a connected replica with ROLE and builds a regular native pool.
The application prepares a replacement pool on refresh and closes the
old pool. The fixture verifies the old sent command keeps its native
delivery uncertainty and is never replayed. The real Redis runner
accepts `--replicas` to exercise the same application policy through
Sentinel promotion.

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
